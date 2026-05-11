#include "launcher_bridge.h"

#include "launcher.h"
#include "prefs_adapter.h"

#include <QByteArray>

#include <stdio.h>

int runWinUaeQtLauncherForPrefs(int argc, char **argv, struct uae_prefs *prefs, int *exitCode)
{
    WinUaeQtLauncherResult result = runWinUaeQtLauncherForConfig(argc, argv);
    if (result.status == WinUaeQtLauncherStatus::StartRequested) {
        if (!applyWinUaeQtConfigToPrefs(result.config, prefs)) {
            fprintf(stderr, "Unix Qt UI failed: no preferences target available\n");
            if (exitCode) {
                *exitCode = 1;
            }
            return WINUAE_QT_LAUNCHER_ERROR;
        }
        return WINUAE_QT_LAUNCHER_START;
    }
    if (result.status == WinUaeQtLauncherStatus::Error) {
        QByteArray error = result.error.toLocal8Bit();
        fprintf(stderr, "Unix Qt UI failed: %s\n", error.constData());
        if (exitCode) {
            *exitCode = result.exitCode ? result.exitCode : 1;
        }
        return WINUAE_QT_LAUNCHER_ERROR;
    }
    if (exitCode) {
        *exitCode = 0;
    }
    return WINUAE_QT_LAUNCHER_EXIT;
}
