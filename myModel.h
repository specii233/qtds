#ifndef MY_MODEL_H
#define MY_MODEL_H

#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

#include <memory>

class MyMesh;  // 前置声明：本类只持有网格引用，不需要网格定义（避免把 GL 头文件传染给所有包含者）

// 模型 = 对一份网格的引用 + 自己的变换（位置/旋转/缩放）+ 自转速度。
// 多个模型可以共享同一份网格（显存只占一份），但各自的位置/旋转完全独立。
//
// 纯数学类，header-only（类内定义即隐式 inline，多 TU 包含不会重复定义），
// 不碰任何 GL 调用 —— 因此可以直接单元测试。
// 变换顺序固定为：缩放 → 旋转 → 平移（顺序错了会出现"绕远处某点转"的怪现象）。
class MyModel
{
public:
    MyModel() = default;
    explicit MyModel(std::shared_ptr<MyMesh> mesh) : m_mesh(std::move(mesh)) {}

    // ---------- 位置 ----------
    void setPosition(const QVector3D &position) { m_position = position; }
    QVector3D position() const { return m_position; }
    void translate(const QVector3D &delta) { m_position += delta; }

    // ---------- 缩放 ----------
    void setScale(const QVector3D &scale) { m_scale = scale; }
    void setUniformScale(float scale) { m_scale = QVector3D(scale, scale, scale); }
    QVector3D scale() const { return m_scale; }

    // ---------- 旋转 ----------
    void setRotation(const QQuaternion &rotation) { m_rotation = rotation; }
    void setRotationDegrees(float degrees, const QVector3D &axis)
    {
        m_rotation = QQuaternion::fromAxisAndAngle(axis, degrees);
    }
    QQuaternion rotation() const { return m_rotation; }

    // 在当前旋转基础上再转（后乘：新旋转作用于局部坐标系）
    void rotateBy(float degrees, const QVector3D &axis)
    {
        m_rotation = QQuaternion::fromAxisAndAngle(axis, degrees) * m_rotation;
    }

    // ---------- 自转（由 MyGLWidget 每帧按 dt 驱动）----------
    void setSpin(float degreesPerSecond, const QVector3D &axis = QVector3D(0.0f, 1.0f, 0.0f))
    {
        m_spin_degrees_per_second = degreesPerSecond;
        m_spin_axis = axis;
    }
    float spinDegreesPerSecond() const { return m_spin_degrees_per_second; }
    QVector3D spinAxis() const { return m_spin_axis; }

    void updateSpin(float dt)
    {
        if (m_spin_degrees_per_second != 0.0f && dt > 0.0f) {
            rotateBy(m_spin_degrees_per_second * dt, m_spin_axis);
        }
    }

    // ---------- 模型矩阵 ----------
    QMatrix4x4 modelMatrix() const
    {
        QMatrix4x4 matrix;
        matrix.translate(m_position);  // ① 平移
        matrix.rotate(m_rotation);     // ② 旋转
        matrix.scale(m_scale);         // ③ 缩放
        return matrix;
    }

    // ---------- 几何 ----------
    MyMesh *mesh() const { return m_mesh.get(); }  // 方法 const，但返回可绘制指针（智能指针惯例）
    bool hasMesh() const { return m_mesh != nullptr; }
    void setMesh(std::shared_ptr<MyMesh> mesh) { m_mesh = std::move(mesh); }

private:
    std::shared_ptr<MyMesh> m_mesh;  // 共享几何：多个模型可指向同一份 VBO/VAO
    QVector3D m_position{0.0f, 0.0f, 0.0f};
    QQuaternion m_rotation;  // 默认单位四元数 = 无旋转
    QVector3D m_scale{1.0f, 1.0f, 1.0f};

    float m_spin_degrees_per_second = 0.0f;
    QVector3D m_spin_axis{0.0f, 1.0f, 0.0f};
};

#endif  // MY_MODEL_H
