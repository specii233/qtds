#ifndef MY_CAMERA_H
#define MY_CAMERA_H

#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

#include <algorithm>
#include <cmath>

// 人称视图模式（FPV = 第一人称 / TPV = 第三人称轨道相机）
enum class CameraView
{
    FPV,
    TPV  // orbitCamera
};

// 摄像机：纯数学模型，**不是 QWidget**，也不碰任何 OpenGL 调用。
//
// 朝向用【单位四元数】表示，**不保存欧拉角（yaw/pitch/roll）也不保存 target/up**：
//   - 四元数把"相机局部坐标"旋到"世界坐标"，局部轴固定为
//         前 = (0, 0, -1)   右 = (1, 0, 0)   上 = (0, 1, 0)   （OpenGL 相机约定）
//   - forward()/right()/up() 都是"用四元数旋转局部轴"现算出来的，天然正交、天然无万向锁
//   - 俯仰限位不需要存欧拉角：由"当前俯仰角（由 forward 现算）+ 请求量"钳制出实际转角
//
// 状态只有：位置 + 朝向四元数 + 透视参数。矩阵每次现算，不存在"改了参数忘更新矩阵"的隐患。
// 本类全部函数在类内定义 → 隐式 inline，头文件被多个 TU 包含也不会重复定义。

class MyCamera
{
public:
    MyCamera() = default;

    MyCamera(const QVector3D &position, const QQuaternion &orientation)
        : m_position(position), m_orientation(orientation.normalized())
    {
    }

    // ---------- 位置 ----------
    void setPosition(const QVector3D &position) { m_position = position; }
    QVector3D position() const { return m_position; }

    // ---------- 朝向（单位四元数）----------
    void setOrientation(const QQuaternion &orientation)
    {
        const QQuaternion normalized = orientation.normalized();
        m_orientation = normalized.isNull() ? QQuaternion() : normalized;
    }
    QQuaternion orientation() const { return m_orientation; }

    // 让相机看向某点：内部换算成四元数，不保存 target/up
    void lookAt(const QVector3D &target, const QVector3D &worldUp = QVector3D(0.0f, 1.0f, 0.0f))
    {
        QVector3D forward_dir = target - m_position;
        if (forward_dir.lengthSquared() <= 0.0f) {
            return;  // 退化：目标与相机重合
        }
        forward_dir.normalize();

        QVector3D up_axis = worldUp.normalized();
        if (std::fabs(QVector3D::dotProduct(forward_dir, up_axis)) > 0.999f) {
            // 视线与参考上方向几乎平行：换一个参考轴，避免叉乘退化为零向量
            up_axis = (forward_dir.y() > 0.0f) ? QVector3D(0.0f, 0.0f, 1.0f) : QVector3D(0.0f, 0.0f, -1.0f);
        }

        const QVector3D right_dir = QVector3D::crossProduct(forward_dir, up_axis).normalized();
        const QVector3D true_up = QVector3D::crossProduct(right_dir, forward_dir);

        // 旋转矩阵的每一列 = 局部基向量在世界中的方向：X=右, Y=上, Z=-前
        QMatrix3x3 basis;
        basis(0, 0) = right_dir.x();
        basis(1, 0) = right_dir.y();
        basis(2, 0) = right_dir.z();

        basis(0, 1) = true_up.x();
        basis(1, 1) = true_up.y();
        basis(2, 1) = true_up.z();

        basis(0, 2) = -forward_dir.x();
        basis(1, 2) = -forward_dir.y();
        basis(2, 2) = -forward_dir.z();

        m_orientation = QQuaternion::fromRotationMatrix(basis).normalized();
    }

    // ---------- 旋转 ----------
    // 绕世界轴（前乘）：常用于偏航（绕世界 Y）
    void rotateWorld(float degrees, const QVector3D &worldAxis)
    {
        if (degrees == 0.0f || worldAxis.isNull()) {
            return;
        }
        m_orientation = QQuaternion::fromAxisAndAngle(worldAxis.normalized(), degrees) * m_orientation;
        m_orientation.normalize();
    }

    // 绕相机自身轴（后乘）：常用于俯仰（绕自身 X = 右方）
    void rotateLocal(float degrees, const QVector3D &localAxis)
    {
        if (degrees == 0.0f || localAxis.isNull()) {
            return;
        }
        m_orientation = m_orientation * QQuaternion::fromAxisAndAngle(localAxis.normalized(), degrees);
        m_orientation.normalize();
    }

    // 鼠标视角（FPV）：yaw 绕世界 Y，pitch 绕自身 X；俯仰钳制在 [min, max] 度
    void yawPitch(float yawDeltaDegrees, float pitchDeltaDegrees,
                  float minPitchDegrees = -85.0f, float maxPitchDegrees = 85.0f)
    {
        rotateWorld(yawDeltaDegrees, QVector3D(0.0f, 1.0f, 0.0f));

        if (pitchDeltaDegrees == 0.0f) {
            return;
        }

        // 俯仰限位：钳制"目标俯仰角"，只转差值 —— 全程不保存欧拉角
        const float current = pitchDegrees();
        const float target = std::clamp(current + pitchDeltaDegrees, minPitchDegrees, maxPitchDegrees);
        rotateLocal(target - current, QVector3D(1.0f, 0.0f, 0.0f));
    }

    // ---------- 局部轴（由四元数现算，永远正交）----------
    QVector3D forward() const { return m_orientation.rotatedVector(QVector3D(0.0f, 0.0f, -1.0f)); }
    QVector3D right() const { return m_orientation.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f)); }
    QVector3D up() const { return m_orientation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f)); }

    // 俯仰角（度）：+ 表示抬头；由 forward 现算，仅用于限位与调试
    float pitchDegrees() const
    {
        const float y = std::clamp(forward().y(), -1.0f, 1.0f);
        return std::asin(y) * 180.0f / kPi;
    }

    // 偏航角（度）：+ 表示向左转（默认朝 -Z 时为 0）
    float yawDegrees() const
    {
        const QVector3D heading = -forward();
        return std::atan2(heading.x(), heading.z()) * 180.0f / kPi;
    }

    // ---------- 移动 ----------
    void move(const QVector3D &worldDelta) { m_position += worldDelta; }

    // 相机自身坐标系移动：forward 沿视线、right 沿右方、upward 沿头顶
    void moveLocal(float forwardAmount, float rightAmount, float upwardAmount)
    {
        m_position += forward() * forwardAmount + right() * rightAmount + up() * upwardAmount;
    }

    // ---------- 透视参数 ----------
    void setPerspective(float fovYDegrees, float nearPlane, float farPlane)
    {
        m_fov_y = std::clamp(fovYDegrees, 1.0f, 179.0f);
        m_near = nearPlane;
        m_far = farPlane;
    }

    void setViewportAspect(float aspect) { m_aspect = (aspect > 0.0f) ? aspect : 1.0f; }

    float fovYDegrees() const { return m_fov_y; }
    float aspect() const { return m_aspect; }
    float nearPlane() const { return m_near; }
    float farPlane() const { return m_far; }

    // ---------- 矩阵（每次现算）----------
    // 视图矩阵 = 旋转的逆 · 平移的逆：把世界坐标变到相机坐标
    QMatrix4x4 viewMatrix() const
    {
        QMatrix4x4 view;
        view.rotate(m_orientation.conjugated());  // 世界 → 相机
        view.translate(-m_position);              // 再把相机位置移到原点
        return view;
    }

    QMatrix4x4 projectionMatrix() const
    {
        QMatrix4x4 projection;
        projection.perspective(m_fov_y, m_aspect, m_near, m_far);
        return projection;
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    QVector3D m_position{0.0f, 0.0f, 3.0f};
    QQuaternion m_orientation;  // 单位四元数：默认看向 -Z、头顶 +Y（与之前的默认机位一致）

    float m_fov_y = 90.0f;
    float m_aspect = 1.0f;
    float m_near = 0.1f;
    float m_far = 100.0f;
};

#endif  // MY_CAMERA_H
