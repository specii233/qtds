#include <QApplication>
#include <QSurfaceFormat>

#include <cpr/cpr.h>

#include "myLogManager.h"
#include "my3d.h"


int main(int argc, char* argv[]) {
    // 必须在创建 QApplication 之前设置默认 surface 格式：
    // my3d.cpp 使用了 glVertexAttribFormat / glVertexAttribBinding / glBindVertexBuffer，
    // 这些是 OpenGL 4.3 才进入核心的显式顶点属性绑定 API。
    QSurfaceFormat fmt;
    fmt.setVersion(4, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);

    // 日志：打开 <exe 目录>/qtds.log 并把 Qt 全部日志重定向过去（命令行不再输出任何内容）
    if (!MyLogManager::init()) {
        return 1;  // 日志都写不了就没必要继续
    }

    // 主窗口创建
    MyGLWidget window;
    window.resize(800, 600);
    window.setWindowTitle(QStringLiteral("qtds"));
    window.show();

    // 网络请求测试
    const cpr::Response r = cpr::Get(cpr::Url{"https://httpbin.org/get"});
    qInfo().noquote() << "Status:" << r.status_code;
    qInfo().noquote() << "Content:" << QString::fromStdString(r.text);

    const int exit_code = app.exec();

    // 日志终止处理
    qInfo().noquote() << "===== qtds exit, code =" << exit_code << "=====";
    MyLogManager::shutdown();
    return exit_code;
}