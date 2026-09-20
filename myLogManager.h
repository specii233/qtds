#ifndef MY_LOG_MANAGER_H
#define MY_LOG_MANAGER_H

#include <QString>
#include <QtGlobal>

// 全局日志管理器：把 Qt 的全部日志（qDebug / qInfo / qWarning / qCritical / qFatal）
// 写入 exe 同目录下的 qtds.log。
//
// 用法（见 main.cpp）：
//     if (!MyLogManager::init()) return 1;   // 打开文件 + 安装消息处理器
//     ...
//     MyLogManager::shutdown();              // 还原处理器 + 关闭文件
//
// 实现全部放在 myLogManager.cpp：头文件被多个 TU 包含也不会重复定义（ODR），
// 文件对象也不暴露给外部，无法被误改。

class MyLogManager
{
public:
    // 打开日志文件（追加模式）并安装消息处理器；成功返回 true
    static bool init();

    // 还原默认消息处理器并关闭文件；可重复调用
    static void shutdown();

    // 日志文件完整路径：<exe 所在目录>/qtds.log
    static QString logFilePath();

    // 是否已经就绪（文件已打开且处理器已安装）
    static bool isReady();

private:
    // Qt 消息处理器：输出格式 时间戳 [级别] 内容
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message);

    MyLogManager() = delete;  // 纯静态工具类，禁止实例化
};

#endif  // MY_LOG_MANAGER_H
