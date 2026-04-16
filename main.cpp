#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "KubectlRunner.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

// The reMarkable notebook UUID that triggers the dashboard.
// When xochitl opens this document, the dashboard takes over the display.
static constexpr const char *TARGET_UUID = "4614616c-273c-4704-b1ff-4cc04a9eda83";

static bool xochitlRunning() {
    return system("pgrep -f /usr/bin/xochitl > /dev/null 2>&1") == 0;
}

// Blocks until xochitl opens TARGET_UUID, then returns true.
// Returns false if xochitl stops running before the trigger is detected.
static bool waitForXochitlTrigger() {
    if (!xochitlRunning()) {
        return false;
    }

    fprintf(stdout, "xochitl is running. Waiting for notebook %s...\n", TARGET_UUID);
    fflush(stdout);

    // journalctl -f can exit unexpectedly; loop until we find the trigger or
    // xochitl stops running on its own.
    bool found = false;
    while (!found && xochitlRunning()) {
        // -n 0: only new lines from this point on (don't replay old logs)
        FILE *pipe = popen("journalctl -u xochitl -f -n 0 2>/dev/null", "r");
        if (!pipe) {
            usleep(1000000);
            continue;
        }
        char buf[1024];
        while (!found && fgets(buf, sizeof(buf), pipe)) {
            // Match the EntityOpen or documentlockmanager "Opening document" entries.
            // Both appear in xochitl's log when a notebook is opened.
            if (strstr(buf, TARGET_UUID) &&
                (strstr(buf, "EntityOpen") || strstr(buf, "Opening document"))) {
                found = true;
            }
        }
        pclose(pipe);
        if (!found && xochitlRunning())
            usleep(500000); // brief pause before retrying journalctl
    }

    return found;
}

// --monitor mode: lightweight, no Qt. Watches for the notebook trigger and
// then starts inkctl-display.service, which has Conflicts=xochitl.service
// so systemd handles stopping xochitl atomically before the display starts.
static int runMonitorMode() {
    // If the display service is already running, just idle until it exits.
    if (system("systemctl is-active inkctl-display.service > /dev/null 2>&1") == 0) {
        sleep(5);
        return 0;
    }

    // Ensure xochitl is running so we have something to monitor.
    // This handles the case where xochitl was stopped by a previous display
    // session (via Conflicts=) and nothing restarted it yet.
    if (!xochitlRunning()) {
        fprintf(stdout, "xochitl not running — starting it...\n");
        fflush(stdout);
        system("systemctl start xochitl");
        // Wait up to 30s for xochitl to appear
        for (int i = 0; i < 150 && !xochitlRunning(); ++i)
            usleep(200000);
        if (!xochitlRunning()) {
            // Xochitl failed to start (likely hit StartLimitBurst).
            // Sleep briefly and let systemd restart us to try again.
            sleep(5);
            return 0;
        }
    }

    if (waitForXochitlTrigger()) {
        fprintf(stdout, "Trigger detected. Handing off to display service.\n");
        fflush(stdout);
        system("systemctl start inkctl-display.service");
    }
    return 0;
}

int main(int argc, char *argv[])
{
    // --monitor: lightweight trigger-detection mode (no Qt, no framebuffer).
    if (argc > 1 && strcmp(argv[1], "--monitor") == 0) {
        return runMonitorMode();
    }

    // Display mode: systemd (via Conflicts=xochitl.service in
    // inkctl-display.service) guarantees xochitl is fully stopped
    // before we reach here, so the framebuffer is free.

    // Remove stale EPFramebuffer lock files left by a previous crashed instance.
    // EPFramebuffer uses a PID-based lock (not flock) so it doesn't self-clean.
    remove("/tmp/epframebuffer.lock");
    remove("/tmp/epd.lock");

    qputenv("QT_QPA_PLATFORM", "epaper");
    qputenv("QT_QUICK_BACKEND", "epaper");

    QGuiApplication app(argc, argv);

    KubectlRunner runner;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("kubectl", &runner);

    const QUrl url(QStringLiteral("qrc:/qt/qml/inkctl_ui/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                    &app, [url](QObject *obj, const QUrl &objUrl) {
        // If the object is null and it's the URL we tried to load, it failed.
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
