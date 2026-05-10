#pragma once

#include <QString>

class WinUaeQtConfig;

class WinUaeQtLauncherBackend {
public:
    bool start(const QString &program, const WinUaeQtConfig &config, QString *error = nullptr) const;
};
