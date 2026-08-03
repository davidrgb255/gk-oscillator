#include "audio.h"
#include "main_window.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QStringList>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

static QIcon loadAppIcon()
{
    const char *env = getenv("GK_OSCILLATOR_ICON");
    if (env && env[0]) {
        QIcon ic(QString::fromUtf8(env));
        if (!ic.isNull()) {
            return ic;
        }
    }

    QStringList bases;
    char self[4096];
    const ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
        self[n] = '\0';
        QString exe = QString::fromUtf8(self);
        const int slash = exe.lastIndexOf(QLatin1Char('/'));
        const QString dir = (slash >= 0) ? exe.left(slash) : QStringLiteral(".");
        bases << dir + QStringLiteral("/assets/icons")
              << dir + QStringLiteral("/../assets/icons")
              << dir + QStringLiteral("/share/icons/hicolor");
    }
    bases << QStringLiteral("assets/icons")
          << QStringLiteral("/usr/local/share/icons/hicolor")
          << QStringLiteral("/usr/share/icons/hicolor")
          << QString(QDir::homePath() + QStringLiteral("/.local/share/icons/hicolor"));

    static const int kSizes[] = {16, 22, 24, 32, 48, 64, 128, 256};
    for (const QString &base : bases) {
        QIcon icon;
        bool any = false;
        /* Repo layout: assets/icons/gk-oscillator-NN.png */
        for (int sz : kSizes) {
            const QString p = base + QStringLiteral("/gk-oscillator-%1.png").arg(sz);
            if (QFile::exists(p)) {
                icon.addFile(p, QSize(sz, sz));
                any = true;
            }
        }
        const QString canonical = base + QStringLiteral("/gk-oscillator.png");
        if (QFile::exists(canonical)) {
            icon.addFile(canonical);
            any = true;
        }
        /* Freedesktop hicolor layout */
        for (int sz : kSizes) {
            const QString p = base
                + QStringLiteral("/%1x%1/apps/gk-oscillator.png").arg(sz);
            if (QFile::exists(p)) {
                icon.addFile(p, QSize(sz, sz));
                any = true;
            }
        }
        if (any && !icon.isNull()) {
            return icon;
        }
    }

    QIcon theme = QIcon::fromTheme(QStringLiteral("gk-oscillator"));
    if (!theme.isNull()) {
        return theme;
    }
    return QIcon::fromTheme(QStringLiteral("audio-x-generic"));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("gk-oscillator"));
    QApplication::setApplicationDisplayName(QStringLiteral("GK Oscillator"));
    QApplication::setOrganizationName(QStringLiteral("gk"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setDesktopFileName(QStringLiteral("gk-oscillator"));

    const QIcon icon = loadAppIcon();
    if (!icon.isNull()) {
        app.setWindowIcon(icon);
    }

    gk_audio *audio = gk_audio_create();
    if (audio == NULL) {
        std::fprintf(stderr, "gk-oscillator: failed to allocate audio engine\n");
        return 1;
    }
    if (gk_audio_init(audio) != 0) {
        const char *err = gk_audio_last_error(audio);
        std::fprintf(stderr, "gk-oscillator: PortAudio init failed: %s\n",
                     err ? err : "unknown");
        gk_audio_destroy(audio);
        return 1;
    }

    MainWindow win(audio);
    if (!icon.isNull()) {
        win.setWindowIcon(icon);
    }
    win.show();

    const int rc = app.exec();

    gk_audio_destroy(audio);
    return rc;
}
