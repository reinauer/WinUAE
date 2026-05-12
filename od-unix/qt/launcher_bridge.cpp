#include "launcher_bridge.h"

#include "launcher.h"
#include "prefs_adapter.h"

#include <QByteArray>
#include <QString>

#include <stdio.h>
#include <string.h>

int runWinUaeQtLauncherForPrefs(int argc, char **argv, struct uae_prefs *prefs, int *exitCode)
{
    return runWinUaeQtLauncherForPrefsWithConfig(argc, argv, prefs, nullptr, exitCode);
}

int runWinUaeQtLauncherForPrefsWithConfig(int argc, char **argv, struct uae_prefs *prefs, const char *initialConfigPath, int *exitCode)
{
    const QString initialPath = initialConfigPath && initialConfigPath[0]
        ? QString::fromLocal8Bit(initialConfigPath)
        : QString();
    WinUaeQtLauncherResult result = runWinUaeQtLauncherForConfig(argc, argv, initialPath);
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

int runWinUaeQtRuntimeFileDialog(int argc, char **argv, int shortcut, const char *initialPath, char *selectedPath, size_t selectedPathLen, int *exitCode)
{
    if (!selectedPath || selectedPathLen == 0) {
        if (exitCode) {
            *exitCode = 1;
        }
        return WINUAE_QT_LAUNCHER_ERROR;
    }
    selectedPath[0] = 0;

    const QString initial = initialPath && initialPath[0]
        ? QString::fromLocal8Bit(initialPath)
        : QString();
    const WinUaeQtRuntimeFileDialogResult result = runWinUaeQtRuntimeFileDialog(argc, argv, shortcut, initial);
    if (!result.accepted) {
        if (exitCode) {
            *exitCode = 0;
        }
        return WINUAE_QT_LAUNCHER_EXIT;
    }

    const QByteArray path = result.path.toLocal8Bit();
    if (size_t(path.size()) >= selectedPathLen) {
        fprintf(stderr, "Unix Qt UI failed: selected path is too long\n");
        if (exitCode) {
            *exitCode = 1;
        }
        return WINUAE_QT_LAUNCHER_ERROR;
    }
    memcpy(selectedPath, path.constData(), size_t(path.size()) + 1);
    if (exitCode) {
        *exitCode = 0;
    }
    return WINUAE_QT_LAUNCHER_START;
}
