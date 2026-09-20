#ifndef MY_MESH_H
#define MY_MESH_H

#include <QOpenGLFunctions_4_3_Core>

#include <cstddef>
#include <vector>

// 顶点：位置(3 float) + 颜色(3 float)，交错存放。
// 用 POD 是为了能用 offsetof 显式给出属性偏移（与 shader 的 layout(location=N) 对应）。
struct MyVertex
{
    float position[3];
    float color[3];
};

// 一个网格 = 一套 VAO/VBO/EBO + 索引数量，即"几何本身"。
//
// 生命周期规则：
//   ★ 所有 GL 调用都要求"当前有有效上下文"：请在 initializeGL() 里 create()，
//     在 makeCurrent() 之后 destroy()（MyGLWidget 的析构已经是这个模式）。
//   ★ 禁拷贝（否则两个对象持有同一批 GLuint → 双重 glDelete），允许移动（可放进 std::vector）。
//   ★ 析构会调用 destroy()，所以析构时同样必须有当前上下文。
class MyMesh : protected QOpenGLFunctions_4_3_Core
{
public:
    // 显式属性号与绑定索引（与 my3d.cpp 里 shader 的 layout(location = N) 一一对应）
    static constexpr GLuint kAttribPos = 0;
    static constexpr GLuint kAttribColor = 1;
    static constexpr GLuint kBindingInterleaved = 0;  // 位置+颜色交错在同一个 VBO，共用一个绑定索引

    MyMesh() = default;
    ~MyMesh();

    MyMesh(const MyMesh &) = delete;
    MyMesh &operator=(const MyMesh &) = delete;
    MyMesh(MyMesh &&other) noexcept;
    MyMesh &operator=(MyMesh &&other) noexcept;

    // 上传几何并建立显式的"属性号 ↔ 绑定索引 ↔ 缓冲"关联
    void create(const std::vector<MyVertex> &vertices, const std::vector<GLuint> &indices);

    // 释放 GL 对象（可重复调用；需要当前上下文）
    void destroy();

    bool isValid() const { return m_vao != 0 && m_index_count > 0; }
    GLsizei indexCount() const { return m_index_count; }
    static constexpr GLsizei vertexStride() { return static_cast<GLsizei>(sizeof(MyVertex)); }

    // 绘制：显式重放全部绑定状态（不依赖 VAO 里"记着的那一份"）
    // 调用者负责 glUseProgram 并设置好 uniform
    void draw();

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    GLsizei m_index_count = 0;
};

// 几何工厂：造常见形状，避免到处手写顶点数组
namespace MyMeshFactory
{
// 立方体：8 个顶点（每个角一种颜色）+ 36 个索引（12 个三角形），size 为边长
void makeCube(std::vector<MyVertex> &vertices, std::vector<GLuint> &indices, float size = 1.0f);
}  // namespace MyMeshFactory

#endif  // MY_MESH_H
