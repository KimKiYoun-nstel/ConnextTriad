#include "mainwindow.hpp"
#include <QApplication>
#include <QDebug>

static MainWindow *g_mainwin = nullptr;

static void qtLogToUi(QtMsgType type, const QMessageLogContext &,
                      const QString &msg) {
    const char *tag = "";
    switch (type) {
    case QtDebugMsg:
        tag = "DBG";
        break;
    case QtInfoMsg:
        tag = "INF";
        break;
    case QtWarningMsg:
        tag = "WRN";
        break;
    case QtCriticalMsg:
        tag = "ERR";
        break;
    case QtFatalMsg:
        tag = "FTL";
        break;
    }
    const QString line = QString("[%1] %2").arg(tag, msg);

    // 접근지정자 무시: 메타 호출(appendLog가 Q_INVOKABLE)
    if (g_mainwin) {
        QMetaObject::invokeMethod(g_mainwin, "appendLog", Qt::QueuedConnection,
                                  Q_ARG(QString, line), Q_ARG(bool, false));
    }

#if defined(_WIN32)
    OutputDebugStringW((line + '\n').toStdWString().c_str());
#endif
    fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
}

int main(int argc, char *argv[]) {
    qInstallMessageHandler(qtLogToUi);
    qSetMessagePattern("%{message}");

    QApplication a(argc, argv);
    MainWindow w;
    g_mainwin = &w;
    w.show();
    const int rc = a.exec();
    g_mainwin = nullptr;
    return rc;
}
