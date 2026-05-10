#include "launcher_backend.h"

#include "config.h"

#include <QProcess>

bool WinUaeQtLauncherBackend::start(const QString &program, const WinUaeQtConfig &config, QString *error) const
{
    if (QProcess::startDetached(program, config.commandArguments())) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("Failed to start emulator.");
    }
    return false;
}
