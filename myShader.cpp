#include "myShader.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QOpenGLContext>

#include <utility>

// ---------------- 构造 / 析构 / 移动 ----------------

MyShaderProgram::~MyShaderProgram()
{
    // 注意：这里要求调用方保证"当前有 GL 上下文"（MyGLWidget 在 makeCurrent() 之后析构）
    destroy();
}

MyShaderProgram::MyShaderProgram(MyShaderProgram &&other) noexcept
    : m_program(other.m_program), m_uniform_cache(std::move(other.m_uniform_cache))
{
    other.m_program = 0;  // 句柄所有权转移，避免源对象析构时删除
}

MyShaderProgram &MyShaderProgram::operator=(MyShaderProgram &&other) noexcept
{
    if (this != &other) {
        destroy();

        m_program = other.m_program;
        m_uniform_cache = std::move(other.m_uniform_cache);

        other.m_program = 0;
        other.m_uniform_cache.clear();
    }
    return *this;
}

// ---------------- 文件读取 ----------------

bool MyShaderProgram::readTextFile(const QString &path, QString &outText)
{
    QFile file(path);  // QFile 原生支持 Qt 资源路径（":/shaders/basic.vert"）
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical().noquote() << "着色器文件无法打开:" << path;
        return false;
    }

    outText = QString::fromUtf8(file.readAll());
    if (outText.trimmed().isEmpty()) {
        qCritical().noquote() << "着色器文件为空:" << path;
        return false;
    }
    return true;
}

// ---------------- 创建 / 释放 ----------------

bool MyShaderProgram::createFromFiles(const QString &vertexPath,
                                      const QString &fragmentPath,
                                      const std::vector<AttributeBinding> &attributeBindings)
{
    QString vertex_source;
    QString fragment_source;

    if (!readTextFile(vertexPath, vertex_source) || !readTextFile(fragmentPath, fragment_source)) {
        return false;  // 读文件失败时不会碰任何 GL 状态
    }

    if (!createFromSource(vertex_source, fragment_source, attributeBindings)) {
        return false;
    }

    qInfo().noquote() << "shader program ready: id =" << m_program
                      << "| vertex:" << vertexPath << "| fragment:" << fragmentPath;
    return true;
}

bool MyShaderProgram::createFromSource(const QString &vertexSource,
                                       const QString &fragmentSource,
                                       const std::vector<AttributeBinding> &attributeBindings)
{
    // 先显式确认"当前有 GL 上下文"：没有上下文时直接调 initializeOpenGLFunctions()
    // 会踩到 Qt 内部的空指针（实测会崩溃），所以这里必须自己拦住
    if (QOpenGLContext::currentContext() == nullptr) {
        qCritical("没有当前 OpenGL 上下文，无法创建着色器程序（请在 initializeGL() 里创建）");
        return false;
    }

    if (!initializeOpenGLFunctions()) {
        qCritical("无法加载 OpenGL 4.3 Core 函数（上下文版本过低）");
        return false;
    }

    destroy();  // 重复创建时先释放旧的

    const GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSource, "vertex shader");
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "fragment shader");
    if (vs == 0 || fs == 0) {
        return false;
    }

    const bool linked = linkProgram(vs, fs, attributeBindings);

    // 无论成败都清理 shader 对象（链接后它们已无用）
    glDetachShader(m_program, vs);
    glDetachShader(m_program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!linked) {
        return false;
    }

    m_uniform_cache.clear();  // 新程序的 uniform 位置需要重新查询
    return true;
}

void MyShaderProgram::destroy()
{
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_uniform_cache.clear();
}

// ---------------- 编译 / 链接 ----------------

GLuint MyShaderProgram::compileShader(GLenum type, const QString &source, const QString &label)
{
    const QByteArray utf8 = source.toUtf8();  // GLSL 用字节流，Qt 字符串先转 UTF-8

    const GLuint shader = glCreateShader(type);
    const char *raw = utf8.constData();
    glShaderSource(shader, 1, &raw, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        QByteArray info(length > 0 ? length : 1, '\0');
        glGetShaderInfoLog(shader, length, nullptr, info.data());
        qCritical().noquote() << label << "编译失败:" << QString::fromUtf8(info).trimmed();
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool MyShaderProgram::linkProgram(GLuint vertexShader, GLuint fragmentShader,
                                  const std::vector<AttributeBinding> &attributeBindings)
{
    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);

    // 显式绑定属性号（与 shader 里的 layout(location = N) 双保险）
    for (const AttributeBinding &binding : attributeBindings) {
        glBindAttribLocation(m_program, binding.location, binding.name);
    }

    glLinkProgram(m_program);

    GLint linked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &length);
        QByteArray info(length > 0 ? length : 1, '\0');
        glGetProgramInfoLog(m_program, length, nullptr, info.data());
        qCritical().noquote() << "program 链接失败:" << QString::fromUtf8(info).trimmed();
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }
    return true;
}

// ---------------- 使用 ----------------

void MyShaderProgram::bind()
{
    if (isValid()) {
        glUseProgram(m_program);
    }
}

void MyShaderProgram::release()
{
    glUseProgram(0);
}

// ---------------- uniform ----------------

GLint MyShaderProgram::uniformLocation(const char *name)
{
    if (!isValid() || name == nullptr) {
        return -1;
    }

    const std::string key(name);
    const auto found = m_uniform_cache.find(key);
    if (found != m_uniform_cache.end()) {
        return found->second;  // 命中缓存，不再访问 GL
    }

    const GLint location = glGetUniformLocation(m_program, name);
    if (location < 0) {
        qWarning().noquote() << "uniform 未找到（可能被编译器优化掉）:" << name;
    }
    m_uniform_cache.emplace(key, location);
    return location;
}

bool MyShaderProgram::setMat4(const char *name, const QMatrix4x4 &value)
{
    const GLint location = uniformLocation(name);
    if (location < 0) {
        return false;
    }
    glUniformMatrix4fv(location, 1, GL_FALSE, value.constData());
    return true;
}

bool MyShaderProgram::setVec3(const char *name, const QVector3D &value)
{
    const GLint location = uniformLocation(name);
    if (location < 0) {
        return false;
    }
    glUniform3f(location, value.x(), value.y(), value.z());
    return true;
}

bool MyShaderProgram::setFloat(const char *name, float value)
{
    const GLint location = uniformLocation(name);
    if (location < 0) {
        return false;
    }
    glUniform1f(location, value);
    return true;
}
