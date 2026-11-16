// Azravibe Recorder — entry point.
//
// Debug env hooks (development / smoke tests only):
//   AZRAVIBE_SHOT=/path/to.png   render the window to a PNG and exit (works offscreen)
//   AZRAVIBE_SMOKE_SECONDS=N     start recording automatically, stop after N seconds, print result, exit
#include "MainWindow.h"
#include "Style.h"

#include <QApplication>
#include <QTimer>
#include <QImage>
#include <QStandardPaths>
#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("azravibe-recorder"));
    app.setApplicationDisplayName(QStringLiteral("Azravibe Recorder"));
    app.setOrganizationName(QStringLiteral("azravibe"));
    app.setDesktopFileName(QStringLiteral("azravibe-recorder"));

    QFont appFont = app.font();
    appFont.setFamily(QStringLiteral("Inter"));   // falls back to system sans when absent
    appFont.setPixelSize(12);
    app.setFont(appFont);

    MainWindow window;
    window.setStyleSheet(az::styleSheet());

    // ---- debug / smoke hooks ----
    const char* shotPath = std::getenv("AZRAVIBE_SHOT");
    const char* smokeEnv = std::getenv("AZRAVIBE_SMOKE_SECONDS");

    if (shotPath && *shotPath) {
        window.show();
        // Render once the layout has settled, then write the PNG and quit.
        QTimer::singleShot(400, [&window, shotPath] {
            QImage img(window.size(), QImage::Format_ARGB32);
            img.fill(Qt::transparent);
            window.render(&img);
            const bool ok = img.save(QString::fromLocal8Bit(shotPath), "PNG");
            std::fprintf(stderr, "azravibe: shot saved=%d -> %s\n", ok ? 1 : 0, shotPath);
            QApplication::quit();
        });
        return app.exec();
    }

    if (smokeEnv && *smokeEnv) {
        const int seconds = std::atoi(smokeEnv);
        window.show();
        QTimer::singleShot(800, &window, [&window, seconds] {
            window.debugStartDefaultRecording();
            QTimer::singleShot(seconds * 1000, &window, [&window] {
                window.debugStopAndReport();
                QApplication::quit();
            });
        });
        return app.exec();
    }

    window.show();
    return app.exec();
}
