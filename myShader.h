#ifndef MY_SHADER_H
#define MY_SHADER_H

#include <QMatrix4x4>
#include <QOpenGLFunctions_4_3_Core>
#include <QString>
#include <QVector3D>

#include <string>
#include <unordered_map>
#include <vector>

// 着色器程序：封装"编译顶点/片元着色器 → 链接 → 查 uniform 位置并缓存"。
//
// 与项目其余部分保持同一风格：
//   ★ 直接用原生 GL 调用，不用 QOpenGLShaderProgram
//   ★ 显式的错误日志（编译/链接失败时把 GL 的 info log 写进 qtds.log）
//   ★ 禁拷贝、可移动（GL 句柄只能有一个主人）
//   ★ 所有 GL 调用都要求"当前有有效上下文"：在 initializeGL() 里 create，析构前 makeCurrent()
//
// 着色器源码来自文件（可以是 Qt 资源路径 ":/shaders/basic.vert"，也可以是磁盘路径）；
// 资源通过 qtds.qrc 编译进 exe，因此发布时不需要额外拷贝 shader 文件。
class MyShaderProgram : protected QOpenGLFunctions_4_3_Core
{
public:
    // 显式属性号绑定：location 与 shader 里的 layout(location = N) 对应
    struct AttributeBinding
    {
        GLuint location;
        const char *name;
    };

    MyShaderProgram() = default;
    ~MyShaderProgram();

    MyShaderProgram(const MyShaderProgram &) = delete;
    MyShaderProgram &operator=(const MyShaderProgram &) = delete;
    MyShaderProgram(MyShaderProgram &&other) noexcept;
    MyShaderProgram &operator=(MyShaderProgram &&other) noexcept;

    // 从文件加载并创建（自动处理 Qt 资源路径 :/...）
    bool createFromFiles(const QString &vertexPath,
                         const QString &fragmentPath,
                         const std::vector<AttributeBinding> &attributeBindings = {});

    // 直接给源码创建（便于单元测试或运行时拼接）
    bool createFromSource(const QString &vertexSource,
                          const QString &fragmentSource,
                          const std::vector<AttributeBinding> &attributeBindings = {});

    // 释放程序（可重复调用；需要当前上下文）
    void destroy();

    bool isValid() const { return m_program != 0; }
    GLuint programId() const { return m_program; }

    // 启用/停用程序
    void bind();
    void release();

    // ---------- uniform ----------
    // 位置查询一次后缓存（重复调用不会再次访问 GL）
    GLint uniformLocation(const char *name);

    bool setMat4(const char *name, const QMatrix4x4 &value);
    bool setVec3(const char *name, const QVector3D &value);
    bool setFloat(const char *name, float value);

    // 读取文本文件（支持 ":/..." 资源路径）；失败返回 false 并写日志
    static bool readTextFile(const QString &path, QString &outText);

private:
    GLuint compileShader(GLenum type, const QString &source, const QString &label);
    bool linkProgram(GLuint vertexShader, GLuint fragmentShader,
                     const std::vector<AttributeBinding> &attributeBindings);

    GLuint m_program = 0;
    std::unordered_map<std::string, GLint> m_uniform_cache;  // uniform 名 → 位置
};

#endif  // MY_SHADER_H
