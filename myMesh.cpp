#include "myMesh.h"

#include <QDebug>
#include <QOpenGLContext>

#include <utility>

// ---------------- 构造 / 析构 / 移动 ----------------

MyMesh::~MyMesh()
{
    // 注意：这里要求调用方保证"当前有 GL 上下文"（MyGLWidget 在 makeCurrent() 之后析构）
    destroy();
}

MyMesh::MyMesh(MyMesh &&other) noexcept
    : m_vao(other.m_vao), m_vbo(other.m_vbo), m_ebo(other.m_ebo), m_index_count(other.m_index_count)
{
    // 句柄所有权转移：源对象置空，避免它析构时把资源删掉
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
    other.m_index_count = 0;
}

MyMesh &MyMesh::operator=(MyMesh &&other) noexcept
{
    if (this != &other) {
        destroy();  // 先释放自己手里的资源

        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_ebo = other.m_ebo;
        m_index_count = other.m_index_count;

        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_ebo = 0;
        other.m_index_count = 0;
    }
    return *this;
}

// ---------------- 创建 / 释放 ----------------

void MyMesh::create(const std::vector<MyVertex> &vertices, const std::vector<GLuint> &indices)
{
    if (vertices.empty() || indices.empty()) {
        return;  // 空网格：不创建任何 GL 对象
    }

    // 先显式确认"当前有 GL 上下文"：没有上下文时直接调 initializeOpenGLFunctions()
    // 会踩到 Qt 内部的空指针，所以必须自己拦住
    if (QOpenGLContext::currentContext() == nullptr) {
        qCritical("没有当前 OpenGL 上下文，无法创建网格（请在 initializeGL() 里创建）");
        return;
    }

    // 本类有自己的 GL 函数表（QOpenGLFunctions 是按上下文初始化的）
    if (!initializeOpenGLFunctions()) {
        return;
    }

    destroy();  // 重复 create 时先清掉旧的

    // core profile 下所有缓冲绑定都需要先绑定 VAO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(MyVertex)),
                 vertices.data(),
                 GL_STATIC_DRAW);

    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(GLuint)),
                 indices.data(),
                 GL_STATIC_DRAW);

    // ① 属性格式：属性号、分量数、类型、是否归一化、字节偏移（offsetof 显式给出）
    glVertexAttribFormat(kAttribPos, 3, GL_FLOAT, GL_FALSE,
                         static_cast<GLuint>(offsetof(MyVertex, position)));
    glVertexAttribFormat(kAttribColor, 3, GL_FLOAT, GL_FALSE,
                         static_cast<GLuint>(offsetof(MyVertex, color)));

    // ② 属性 → 绑定索引
    glVertexAttribBinding(kAttribPos, kBindingInterleaved);
    glVertexAttribBinding(kAttribColor, kBindingInterleaved);

    glEnableVertexAttribArray(kAttribPos);
    glEnableVertexAttribArray(kAttribColor);

    // ③ 绑定索引 → 具体缓冲 + 起始偏移 + 跨距
    glBindVertexBuffer(kBindingInterleaved, m_vbo, 0, vertexStride());

    m_index_count = static_cast<GLsizei>(indices.size());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void MyMesh::destroy()
{
    // 句柄为 0 时直接跳过：既可重复调用，也允许在上下文已销毁后安全空转
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    m_index_count = 0;
}

// ---------------- 绘制 ----------------

void MyMesh::draw()
{
    if (!isValid()) {
        return;
    }

    // 显式绑定绘制所需的全部状态，不依赖 VAO 里记录的那一份
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBindVertexBuffer(kBindingInterleaved, m_vbo, 0, vertexStride());

    glDrawElements(GL_TRIANGLES, m_index_count, GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);
}

// ---------------- 几何工厂 ----------------

namespace MyMeshFactory
{

void makeCube(std::vector<MyVertex> &vertices, std::vector<GLuint> &indices, float size)
{
    const float h = size * 0.5f;  // 半边长

    vertices = {
        // 位置                  // 颜色
        {{-h, -h, -h}, {0.2f, 0.3f, 0.8f}},
        {{ h, -h, -h}, {0.2f, 0.8f, 0.3f}},
        {{ h,  h, -h}, {0.8f, 0.8f, 0.2f}},
        {{-h,  h, -h}, {0.8f, 0.2f, 0.3f}},
        {{-h, -h,  h}, {0.3f, 0.8f, 0.8f}},
        {{ h, -h,  h}, {0.8f, 0.3f, 0.8f}},
        {{ h,  h,  h}, {0.9f, 0.9f, 0.9f}},
        {{-h,  h,  h}, {0.3f, 0.3f, 0.3f}},
    };

    indices = {
        0, 1, 2, 2, 3, 0,  // 后
        4, 5, 6, 6, 7, 4,  // 前
        0, 4, 7, 7, 3, 0,  // 左
        1, 5, 6, 6, 2, 1,  // 右
        3, 2, 6, 6, 7, 3,  // 上
        0, 1, 5, 5, 4, 0,  // 下
    };
}

}  // namespace MyMeshFactory
