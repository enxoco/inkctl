#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "KubectlRunner.h" // The header from the previous step

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Create the backend instance
    KubectlRunner runner;

    QQmlApplicationEngine engine;

    // This makes the 'runner' object accessible in QML as 'kubectl'
    engine.rootContext()->setContextProperty("kubectl", &runner);

    const QUrl url(u"qrc:/qt/qml/paper/dashboard/Main.qml"_qs);
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    
    engine.load(url);

    return app.exec();
}
