#ifndef MY3D_H
#define MY3D_H

#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLWidget>
#include <QPoint>
#include <QVector3D>
#include <QWheelEvent>

#include <cstdint>
#include <memory>
#include <vector>

#include "gameLoop.h"
#include "myCamera.h"
#include "myCharacter.h"
#include "myMesh.h"
#include "myModel.h"
#include "myShader.h"

// 本类不使用 Qt 的 GL 封装类（QOpenGLShaderProgram / QOpenGLBuffer / QOpenGLVertexArrayObject），
// 全部直接调用原生 OpenGL 4.3 函数，"顶点数据从哪里来"这件事完全显式：
//
//   glVertexAttribFormat(属性号, 分量数, 类型, 归一化, 字节偏移)   ← 显式描述属性格式
//   glVertexAttribBinding(属性号, 绑定索引)                      ← 显式把属性挂到某绑定索引
//   glBindVertexBuffer(绑定索引, VBO, 起始偏移, 跨距)             ← 显式把绑定索引指向缓冲
//
// 职责划分：
//   MyMesh  —— 几何 + 它的 GL 资源（可被多个模型共享）
//   MyModel —— 对网格的引用 + 独立变换（位置/旋转/缩放/自转）
//   MyCamera—— 视图矩阵 + 投影矩阵
//   本类    —— 输入 → 摄像机；每帧遍历模型列表绘制（一个模型一次 draw call）
//
// 注意：需要 OpenGL 4.3 上下文（见 main.cpp 的 QSurfaceFormat 设置）。

class MyGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_3_Core
{
    Q_OBJECT

public:
    explicit MyGLWidget(QWidget *parent = nullptr);
    ~MyGLWidget() override;

    // ---- 人称切换器 ----
    // FPV：第一人称 —— 摄像机位于角色眼睛处，鼠标自由视角（四元数 yaw/pitch）
    // TPV：第三人称 —— 摄像机绕角色轨道（MyCharacter::orbitCamera）
    // CameraView 定义在 myCamera.h；这里的别名让 MyGLWidget::CameraView::FPV 这种写法同样成立
    using CameraView = ::CameraView;

    CameraView cameraView() const { return m_camera_view; }
    void setCameraView(CameraView view);

    // ---- 主循环（外部可挂任务：gameLoop().enqueue(...) / scheduler()）----
    GameLoop &gameLoop() { return m_game_loop; }

    // ---- 角色（摄像机挂在角色上，两种人称共用同一台）----
    MyCharacter &character() { return m_character; }
    const MyCharacter &character() const { return m_character; }

    // ---- 摄像机 ----
    void setCamera(const QVector3D &eye, const QVector3D &target,
                   const QVector3D &up = QVector3D(0.0f, 1.0f, 0.0f));
    void resetCamera();  // 回到默认机位

    MyCamera &camera() { return m_character.camera(); }
    const MyCamera &camera() const { return m_character.camera(); }

    // ---- 模型列表（只读访问，便于外部检查/测试）----
    const std::vector<MyModel> &models() const { return m_models; }
    std::size_t meshCount() const { return m_meshes.size(); }

    // ---- 输入状态（由事件填充，也可代码直接设置，便于测试/脚本驱动）----
    struct CameraInput
    {
        bool forward = false;   // W / ↑
        bool backward = false;  // S / ↓
        bool left = false;      // A / ←
        bool right = false;     // D / →
        bool up = false;        // Space / E
        bool down = false;      // C / Q
        bool fast = false;      // Shift（加速）
    };

    void setCameraInput(const CameraInput &input) { m_input = input; }
    const CameraInput &cameraInput() const { return m_input; }

    void setMoveSpeed(float metersPerSecond) { m_move_speed = metersPerSecond; }
    float moveSpeed() const { return m_move_speed; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // 输入事件
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    // ---- 着色器程序（源码在 shaders/*.vert|frag，经 qtds.qrc 嵌进 exe；所有模型共用一份）----
    MyShaderProgram m_program;

    // ---- 场景内容：网格库 + 模型列表 ----
    std::vector<std::shared_ptr<MyMesh>> m_meshes;  // 拥有几何（shared_ptr 使模型可共享同一份）
    std::vector<MyModel> m_models;                  // 每个元素是一个独立模型（自转的立方体）
    MyModel m_character_model;                      // 角色的占位模型（跟着角色位置走）

    MyCharacter m_character;  // 角色（摄像机就挂在它身上，FPV/TPV 共用）
    CameraView m_camera_view = CameraView::TPV;

    GameLoop m_game_loop{this};  // 主循环：固定步长逻辑刻 + 每帧渲染，取代内部的 QTimer

    // ---- 输入状态与参数 ----
    CameraInput m_input;
    float m_move_speed = 5.0f;       // 米/秒
    float m_fast_multiplier = 3.0f;  // 按住 Shift 的倍率
    float m_orbit_speed = 0.4f;      // 度/像素（鼠标拖拽转视角）
    float m_wheel_step = 0.5f;       // 米/格（滚轮推拉）
    QPoint m_last_mouse_pos;
    bool m_orbiting = false;

    // 摄像机状态变化日志（位置或朝向变化超过阈值才写一行，避免刷屏）
    QVector3D m_last_logged_camera_position;
    QVector3D m_last_logged_camera_forward;
    float m_log_move_threshold = 0.5f;

    // ---- 显式划分的初始化 / 清理步骤 ----
    void createShaderProgram();        // 从资源加载并链接着色器（shaders/basic.vert|frag）
    void createScene();                // 造几何（网格）+ 摆放模型
    void releaseGlResources();         // 释放程序与所有网格（需要当前上下文）

    // ---- 主循环回调 ----
    void onGameTick();          // 固定步长：逻辑更新（模型自转、角色推进、摄像机键盘移动）
    void onGameFrame();         // 每帧：请求重绘

    // ---- 每帧 ----
    void updateCharacter(float dt);         // 输入 → 角色 → 摄像机（两种人称共用）
    MyCharacterController::InputState characterInputFromKeys() const;
    void syncCharacterModel();              // 占位模型跟随角色位置
    void logCameraPositionIfMoved();
    void clearCameraInput();
};

#endif  // MY3D_H
