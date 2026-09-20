#include "myLogManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>

namespace {

// 文件对象与互斥锁放在 .cpp 内部：外部拿不到，只能通过 init()/shutdown() 控制生命周期
QFile *g_log_file = nullptr;
QMutex g_log_mutex;

}  // namespace

QString MyLogManager::logFilePath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/qtds.log");
}

bool MyLogManager::isReady()
{
    return g_log_file != nullptr && g_log_file->isOpen();
}

bool MyLogManager::init()
{
    // 静态存储期：生命周期覆盖整个进程，避免"文件对象在栈上、处理器却还活着"的悬垂指针
    static QFile log_file;

    log_file.setFileName(logFilePath());
    if (!log_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return false;
    }

    log_file.write(QStringLiteral("\n===== qtds start =====\n").toUtf8());
    log_file.flush();

    g_log_file = &log_file;
    qInstallMessageHandler(&MyLogManager::messageHandler);
    return true;
}

void MyLogManager::shutdown()
{
    qInstallMessageHandler(nullptr);

    QMutexLocker locker(&g_log_mutex);
    if (g_log_file != nullptr) {
        g_log_file->flush();
        g_log_file->close();
        g_log_file = nullptr;
    }
}

void MyLogManager::messageHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    const char *level = "INFO";
    switch (type) {
        case QtDebugMsg:    level = "DEBUG"; break;
        case QtInfoMsg:     level = "INFO";  break;
        case QtWarningMsg:  level = "WARN";  break;
        case QtCriticalMsg: level = "ERROR"; break;
        case QtFatalMsg:    level = "FATAL"; break;
    }

    // 加锁：可能有其它线程同时写日志；locker 出作用域自动解锁
    QMutexLocker locker(&g_log_mutex);
    if (g_log_file != nullptr && g_log_file->isOpen()) {
        const QString line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                             + QStringLiteral(" [")
                             + QLatin1String(level)
                             + QStringLiteral("] ")
                             + message
                             + QLatin1Char('\n');

        g_log_file->write(line.toUtf8());
        g_log_file->flush();
    }

    if (type == QtFatalMsg) {
        abort();  // 致命错误必须终止，与 Qt 默认行为一致
    }
}
