#include "launcher_bridge.h"

#include "launcher.h"
#include "../startup_config.h"

#include <QByteArray>

#include <stdio.h>

static void storeStartupConfig(const WinUaeQtConfig &config)
{
    unix_startup_config_clear();
    const WinUaeQtConfig::Settings &settings = config.settings();
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        if (it.value().isEmpty()) {
            continue;
        }
        const QByteArray line = QStringLiteral("%1=%2").arg(it.key(), it.value()).toLocal8Bit();
        unix_startup_config_add_line(line.constData());
    }
}

int runWinUaeQtLauncherForStartupConfig(int argc, char **argv, int *exitCode)
{
    WinUaeQtLauncherResult result = runWinUaeQtLauncherForConfig(argc, argv);
    if (result.status == WinUaeQtLauncherStatus::StartRequested) {
        storeStartupConfig(result.config);
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
