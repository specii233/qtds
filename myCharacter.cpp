#include "myCharacter.h"

#include <algorithm>
#include <cmath>

namespace {
// 俯仰限位：[-89°, 89°] 避免正对头顶（视线与 up 平行会退化）
constexpr float kMinPitchDegrees = -89.0f;
constexpr float kMaxPitchDegrees = 89.0f;
constexpr float kMinCameraDistance = 0.5f;
constexpr float kMaxCameraDistance = 100.0f;
constexpr float kPi = 3.14159265358979323846f;
}  // namespace

// ---------------- MyCharacterController ----------------

QVector3D MyCharacterController::movementDirection(const MyCamera &camera) const
{
    // 把视线方向压到水平面：这样抬头/低头只影响视线，不会改变角色的移动平面
    QVector3D forward_dir = camera.forward();
    forward_dir.setY(0.0f);
    if (forward_dir.lengthSquared() <= 0.0f) {
        return QVector3D();  // 摄像机垂直向下/向上看，水平前进方向无法确定
    }
    forward_dir.normalize();

    // cross(forward, up)：forward=(0,0,-1)、up=(0,1,0) → right=(1,0,0)
    QVector3D right_dir = QVector3D::crossProduct(forward_dir, QVector3D(0.0f, 1.0f, 0.0f));
    if (right_dir.lengthSquared() <= 0.0f) {
        return QVector3D();
    }
    right_dir.normalize();

    QVector3D direction;
    if (m_input.forward) {
        direction += forward_dir;
    }
    if (m_input.backward) {
        direction -= forward_dir;
    }
    if (m_input.right) {
        direction += right_dir;
    }
    if (m_input.left) {
        direction -= right_dir;
    }

    if (direction.lengthSquared() <= 0.0f) {
        return QVector3D();  // 无输入，或相反方向互相抵消
    }
    return direction.normalized();  // 归一化：斜向移动不会比直线更快
}

void MyCharacterController::update(QVector3D &position, const MyCamera &camera, float dt) const
{
    if (dt <= 0.0f) {
        return;
    }

    const QVector3D direction = movementDirection(camera);
    if (direction.isNull()) {
        return;
    }

    position += direction * (currentSpeed() * dt);
}

// ---------------- MyCharacter ----------------

MyCharacter::MyCharacter()
{
    syncCamera();
}

void MyCharacter::update(float dt)
{
    m_controller.update(m_position, m_camera, dt);
    syncCamera();  // 角色动完，摄像机跟上（按当前人称模式）
}

void MyCharacter::setView(CameraView view)
{
    if (m_view == view) {
        return;
    }
    m_view = view;
    syncCamera();
}

void MyCharacter::syncCamera()
{
    if (m_view == CameraView::FPV) {
        // 第一人称：摄像机放在眼睛高度；朝向完全由鼠标控制，这里**不能**覆盖四元数
        m_camera.setPosition(m_position + QVector3D(0.0f, m_eye_height, 0.0f));
        return;
    }

    // 第三人称：摄像机在"角色 + 偏移"处，看向角色（注视点高度由 m_camera_target_height 控制，
    // 默认 1.6 米 = 头部；设为 0 就是看向脚底）。朝向由 lookAt 现算成四元数。
    m_camera.setPosition(m_position + m_camera_offset);
    m_camera.lookAt(m_position + QVector3D(0.0f, m_camera_target_height, 0.0f));
}

void MyCharacter::orbitCamera(float yawDegrees, float pitchDegrees)
{
    if (m_camera_offset.lengthSquared() <= 0.0f) {
        return;  // 退化：偏移为零，无法环绕
    }

    // ① 水平环绕：用四元数把偏移向量绕世界 Y 轴旋转（只改方向，长度不变）
    QVector3D offset = m_camera_offset;
    if (yawDegrees != 0.0f) {
        offset = QQuaternion::fromAxisAndAngle(QVector3D(0.0f, 1.0f, 0.0f), yawDegrees)
                     .rotatedVector(offset);
    }

    // ② 俯仰：绕"水平右方"轴旋转；偏移与水平面的夹角钳制在 [5°, 85°]（相机不会钻到脚下/翻到头顶）
    if (pitchDegrees != 0.0f) {
        const float horizontal = std::hypot(offset.x(), offset.z());
        const float current_pitch = std::atan2(offset.y(), horizontal) * 180.0f / kPi;
        const float target_pitch = std::clamp(current_pitch + pitchDegrees,
                                              kMinPitchDegrees, kMaxPitchDegrees);
        const float applied = target_pitch - current_pitch;

        // cross(offset, up) 指向"从相机看向角色的右方"；绕它正向旋转 = 相机升高
        QVector3D right_axis = QVector3D::crossProduct(offset, QVector3D(0.0f, 1.0f, 0.0f));
        if (!right_axis.isNull()) {
            offset = QQuaternion::fromAxisAndAngle(right_axis.normalized(), applied).rotatedVector(offset);
        }
    }

    m_camera_offset = offset;
    syncCamera();
}

void MyCharacter::setCameraDistance(float distance)
{
    if (distance < kMinCameraDistance) {
        distance = kMinCameraDistance;  // 太近会穿模，钳制到下限
    }
    if (distance > kMaxCameraDistance) {
        distance = kMaxCameraDistance;
    }

    const float current = m_camera_offset.length();
    if (current <= 0.0f) {
        m_camera_offset = QVector3D(0.0f, 2.0f, distance);
    } else {
        m_camera_offset *= (distance / current);  // 只改长度，方向不变
    }

    syncCamera();
}
