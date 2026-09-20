#ifndef GAME_LOOP_H
#define GAME_LOOP_H

#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

// 游戏主循环状态位（用一个字节的位域表示循环当前处于什么状态）
using GameLoopState = std::uint8_t;
namespace GameLoopFlags
{
    using T = GameLoopState;
    constexpr T EVENT_ACTIVE = 1U << 0;  // 事件活动
    constexpr T TIME_ACTIVE  = 1U << 1;  // 时间活动
    constexpr T INITIALIZED  = 1U << 2;  // 初始化完成
    constexpr T OVERLOAD     = 1U << 3;  // 资源加载完成
    constexpr T _UNDEF_4     = 1U << 4;  // 空预留（原来写的 0U << 4 恒为 0，占位无意义）
    constexpr T _UNDEF_5     = 1U << 5;  // 空预留
    constexpr T _UNDEF_6     = 1U << 6;  // 空预留
    constexpr T _UNDEF_7     = 1U << 7;  // 空预留
}  // namespace GameLoopFlags

using Task = std::function<void()>;

// 任务调度器：三类任务分别按"游戏刻 / 帧 / 现实时间"触发
class TaskScheduler
{
public:
    // 游戏刻任务：固定步长调用（逻辑更新）
    void onTick(std::uint64_t tickCount);

    // 帧任务：每帧调用（渲染插值等）
    void onFrame(float dt);

    // 现实时间任务：每帧检查是否到点（定时器、心跳）
    void onRealTime(double nowSeconds);

    void addTickTask(Task task);
    void addFrameTask(Task task);
    void addRealTimeTask(Task task);

    void clear();

private:
    std::vector<Task> m_frame_tasks;
    std::vector<Task> m_tick_tasks;
    std::vector<Task> m_real_time_tasks;
};

// 游戏主循环。
//
// 注意：这里**不再**用 `while (状态) { ... }` 阻塞循环 —— Qt 的事件循环（app.exec()）
// 必须持续运行才能处理窗口/输入事件，阻塞式 while 会把界面冻死。
// 因此 mainLoop() 被实现为"推进一帧"，由内部 QTimer 驱动（固定步长累加器保证逻辑与帧率无关）。
class GameLoop : public QObject
{
    Q_OBJECT  // ← 必须有：否则无法使用信号槽，且 connect 编译失败

public:
    explicit GameLoop(QObject *parent = nullptr);

    void init();   // 初始化状态位与时钟
    void start();  // 启动（必要时先 init）
    void pause();  // 暂停时间推进（保留状态）
    void exit();   // 停止定时器并清空任务

    GameLoopState state() const { return m_loop_state; }
    bool isActive() const { return (m_loop_state & GameLoopFlags::EVENT_ACTIVE) != 0; }
    std::uint64_t tickCount() const { return m_tick_count; }

    TaskScheduler &scheduler() { return m_scheduler; }

    // 帧间隔（毫秒），默认 16ms ≈ 60fps
    void setFrameInterval(int milliseconds);
    int frameInterval() const { return m_timer.interval(); }

    // 逻辑刻固定步长（秒），默认 1/60。物理/角色/动画都应按这个步长推进，才能与帧率无关
    double fixedTickSeconds() const { return m_fixed_tick_seconds; }
    void setFixedTickSeconds(double seconds);

    // 待办队列：在当前帧开始时依次执行
    void enqueue(Task task);

signals:
    void ticked(std::uint64_t tickCount);  // 每个逻辑刻
    void frameStepped(float dt);           // 每帧

private:
    void mainLoop();  // 推进一帧（私有：由内部定时器驱动）

    GameLoopState m_loop_state = 0;
    std::deque<Task> m_task_queue;
    TaskScheduler m_scheduler;
    QTimer m_timer{this};  // 值成员 + Qt 父子关系：不再用裸指针 new（原来会泄漏）
    QElapsedTimer m_clock;
    std::uint64_t m_tick_count = 0;
    double m_tick_accumulator = 0.0;
    double m_fixed_tick_seconds = 1.0 / 60.0;
};

#endif  // GAME_LOOP_H
