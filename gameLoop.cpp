#include "gameLoop.h"

namespace {
constexpr int kDefaultFrameIntervalMs = 16;  // 帧间隔默认 16ms ≈ 60fps
}  // namespace

// ---------------- TaskScheduler ----------------

void TaskScheduler::addTickTask(Task task) { m_tick_tasks.push_back(std::move(task)); }
void TaskScheduler::addFrameTask(Task task) { m_frame_tasks.push_back(std::move(task)); }
void TaskScheduler::addRealTimeTask(Task task) { m_real_time_tasks.push_back(std::move(task)); }

void TaskScheduler::onTick(std::uint64_t tickCount)
{
    (void)tickCount;  // 目前任务不关心刻号，保留参数以便将来使用
    for (const Task &task : m_tick_tasks) {
        if (task) {
            task();
        }
    }
}

void TaskScheduler::onFrame(float dt)
{
    (void)dt;
    for (const Task &task : m_frame_tasks) {
        if (task) {
            task();
        }
    }
}

void TaskScheduler::onRealTime(double nowSeconds)
{
    (void)nowSeconds;
    for (const Task &task : m_real_time_tasks) {
        if (task) {
            task();
        }
    }
}

void TaskScheduler::clear()
{
    m_tick_tasks.clear();
    m_frame_tasks.clear();
    m_real_time_tasks.clear();
}

// ---------------- GameLoop ----------------

GameLoop::GameLoop(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(kDefaultFrameIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &GameLoop::mainLoop);
}

void GameLoop::init()
{
    m_loop_state |= GameLoopFlags::INITIALIZED;
    m_loop_state |= GameLoopFlags::OVERLOAD;  // 本项目暂无需异步加载的资源
    m_tick_count = 0;
    m_tick_accumulator = 0.0;
    m_clock.start();
}

void GameLoop::start()
{
    if ((m_loop_state & GameLoopFlags::INITIALIZED) == 0) {
        init();
    }

    m_loop_state |= GameLoopFlags::EVENT_ACTIVE;
    m_loop_state |= GameLoopFlags::TIME_ACTIVE;

    m_clock.restart();
    m_timer.start();
}

void GameLoop::pause()
{
    m_loop_state &= static_cast<GameLoopState>(~GameLoopFlags::TIME_ACTIVE);
}

void GameLoop::exit()
{
    m_timer.stop();
    m_task_queue.clear();
    m_scheduler.clear();
    m_loop_state = 0;
}

void GameLoop::setFrameInterval(int milliseconds)
{
    m_timer.setInterval(milliseconds > 0 ? milliseconds : kDefaultFrameIntervalMs);
}

void GameLoop::setFixedTickSeconds(double seconds)
{
    if (seconds > 0.0) {
        m_fixed_tick_seconds = seconds;
    }
}

void GameLoop::enqueue(Task task)
{
    if (task) {
        m_task_queue.push_back(std::move(task));
    }
}

void GameLoop::mainLoop()
{
    // ① 待办队列：本帧开始前一次性执行完
    while (!m_task_queue.empty()) {
        Task task = std::move(m_task_queue.front());
        m_task_queue.pop_front();
        if (task) {
            task();
        }
    }

    if (!isActive()) {
        return;
    }

    // ② 真实经过时间（秒）
    const double elapsed_seconds = static_cast<double>(m_clock.restart()) / 1000.0;

    // ③ 时间未激活时只跑帧任务（例如暂停逻辑但仍渲染）
    if ((m_loop_state & GameLoopFlags::TIME_ACTIVE) != 0) {
        m_tick_accumulator += elapsed_seconds;

        // 固定步长推进逻辑：一帧内可能补跑多个逻辑刻，且限制最大补跑次数防止"死亡螺旋"
        constexpr int kMaxSubSteps = 5;
        int steps = 0;
        while (m_tick_accumulator >= m_fixed_tick_seconds && steps < kMaxSubSteps) {
            m_tick_accumulator -= m_fixed_tick_seconds;
            ++m_tick_count;
            m_scheduler.onTick(m_tick_count);
            emit ticked(m_tick_count);
            ++steps;
        }
        if (steps == kMaxSubSteps) {
            m_tick_accumulator = 0.0;  // 落后太多就丢弃积压，避免越追越慢
        }
    }

    // ④ 每帧任务
    const float dt = static_cast<float>(elapsed_seconds);
    m_scheduler.onFrame(dt);
    m_scheduler.onRealTime(elapsed_seconds);
    emit frameStepped(dt);
}
