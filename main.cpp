#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "KubectlRunner.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

// The reMarkable notebook UUID that triggers the dashboard.
// When xochitl opens this document, the dashboard takes over the display.
static constexpr const char *TARGET_UUID = "4614616c-273c-4704-b1ff-4cc04a9eda83";

static bool xochitlRunning() {
    return system("pgrep -x xochitl > /dev/null 2>&1") == 0;
}

// Blocks until it is safe to initialize the epaper display.
//
// If awaitStart is true (i.e. we relaunched after exit), we poll for up to
// 5 seconds for xochitl to appear before monitoring it. This handles the
// race where xochitl hasn't started yet when we relaunch ourselves.
//
// Once xochitl is seen running, we tail its journal and wait for TARGET_UUID
// to be opened. Then we stop xochitl and poll until its framebuffer lock is
// released before returning.
static void waitForXochitlTrigger() {
    if (!xochitlRunning()) {
        return; // Nothing to wait for — proceed directly to dashboard
    }

    fprintf(stdout, "xochitl is running. Waiting for notebook %s...\n", TARGET_UUID);

    // -n 0: only new lines from this point on (don't replay old logs)
    FILE *pipe = popen("journalctl -u xochitl -f -n 0 2>/dev/null", "r");
    if (!pipe) return;

    char buf[1024];
    bool found = false;
    while (!found && fgets(buf, sizeof(buf), pipe)) {
        if (strstr(buf, "Opening document") && strstr(buf, TARGET_UUID)) {
            found = true;
        }
    }
    pclose(pipe);

    if (!found) return;

    fprintf(stdout, "Target notebook opened. Stopping xochitl...\n");
    system("systemctl stop xochitl");

    // Poll until xochitl's process is gone (framebuffer released)
    while (xochitlRunning()) {
        usleep(200000);
    }

    fprintf(stdout, "xochitl stopped. Launching dashboard.\n");
}

int main(int argc, char *argv[])
{
    // Run before QGuiApplication — which is what initializes the epaper
    // platform plugin and acquires the framebuffer lock.
    waitForXochitlTrigger();

    qputenv("QT_QPA_PLATFORM", "epaper");
    qputenv("QT_QUICK_BACKEND", "epaper");

    QGuiApplication app(argc, argv);

    KubectlRunner runner;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("kubectl", &runner);

    const QUrl url(u"qrc:/qt/qml/paper/dashboard/Main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
