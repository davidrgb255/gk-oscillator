#include "audio.h"
#include "main_window.h"

#include <QApplication>
#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("gk-oscillator"));
    QApplication::setOrganizationName(QStringLiteral("gk"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

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
    win.show();

    const int rc = app.exec();

    gk_audio_destroy(audio);
    return rc;
}
