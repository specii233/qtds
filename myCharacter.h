#ifndef MY_CHARACTER_H
#define MY_CHARACTER_H

#include <QVector3D>

#include "myCamera.h"

// 角色控制器：把"按键意图"翻译成"世界位移"。
// 控制器自身**不持有位置**，位置由调用者传入引用（显式、无隐藏状态），
// 这样同一个控制器可以驱动任意角色，也便于单元测试。
class MyCharacterController
{
public:
    struct InputState
    {
        bool forward = false;
        bool backward = false;
        bool left = false;
        bool right = false;
        bool jump = false;
        bool sprint = false;
    };

    struct Speeds
    {
        float walk = 5.0f;    // m/s
        float sprint = 9.0f;  // m/s
    };

    void setInput(const InputState &input) { m_input = input; }
    const InputState &input() const { return m_input; }

    void setSpeeds(const Speeds &speeds) { m_speeds = speeds; }
    const Speeds &speeds() const { return m_speeds; }

    // 当前目标速度（按住加速键用 sprint）
    float currentSpeed() const { return m_input.sprint ? m_speeds.sprint : m_speeds.walk; }

    // 依据摄像机朝向算出世界空间移动方向（单位向量；无输入时为 (0,0,0)）
    // 前后取"视线在水平面上的投影"，左右取该方向的右方 —— 抬头低头不会让角色飞起来
    QVector3D movementDirection(const MyCamera &camera) const;

    // 把本帧输入应用到位置
    void update(QVector3D &position, const MyCamera &camera, float dt) const;

private:
    InputState m_input;
    Speeds m_speeds;
};

// 角色：持有位置 + 摄像机 + 控制器；每帧由控制器推进位置，然后按人称模式摆放摄像机
class MyCharacter
{
public:
    MyCharacter();

    // dt 单位：秒
    void update(float dt);

    // ---- 人称模式 ----
    // FPV：摄像机位于角色眼睛高度，朝向完全由鼠标控制（syncCamera 不会覆盖朝向）
    // TPV：摄像机位于"角色 + 偏移"，由 lookAt 现算朝向，鼠标拖拽 = orbitCamera
    void setView(CameraView view);
    CameraView view() const { return m_view; }

    // FPV 的眼睛高度（米，相对角色脚底）
    void setEyeHeight(float height)
    {
        m_eye_height = height;
        syncCamera();
    }
    float eyeHeight() const { return m_eye_height; }

    // 按当前人称模式摆放摄像机
    void syncCamera();

    QVector3D position() const { return m_position; }
    void setPosition(const QVector3D &position)
    {
        m_position = position;
        syncCamera();
    }

    MyCamera &camera() { return m_camera; }
    const MyCamera &camera() const { return m_camera; }

    MyCharacterController &controller() { return m_controller; }
    const MyCharacterController &controller() const { return m_controller; }

    // 摄像机相对角色的偏移（第三人称：角色后上方）
    void setCameraOffset(const QVector3D &offset)
    {
        m_camera_offset = offset;
        syncCamera();
    }
    QVector3D cameraOffset() const { return m_camera_offset; }

    // 鼠标拖拽（TPV）：绕角色水平环绕 + 俯仰（四元数旋转偏移向量，俯仰角限制在 [5°, 85°]）
    void orbitCamera(float yawDegrees, float pitchDegrees);

    // 滚轮（TPV）：拉近/拉远（限制在 [0.5, 100] 米）
    void setCameraDistance(float distance);
    float cameraDistance() const { return m_camera_offset.length(); }

private:
    QVector3D m_position{0.0f, 0.0f, 0.0f};
    QVector3D m_camera_offset{0.0f, 2.0f, 5.0f};
    float m_camera_target_height = 1.6f;  // TPV 视线落点高度（角色胸口/头部）
    float m_eye_height = 1.6f;            // FPV 眼睛高度（相对脚底）
    CameraView m_view = CameraView::TPV;

    MyCamera m_camera;
    MyCharacterController m_controller;
};

#endif  // MY_CHARACTER_H
