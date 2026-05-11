#pragma once

#include "config.h"

class QApplication;

enum class WinUaeQtLauncherStatus {
    Canceled,
    StartRequested,
    Error
};

struct WinUaeQtLauncherResult {
    WinUaeQtLauncherStatus status = WinUaeQtLauncherStatus::Canceled;
    int exitCode = 0;
    QString error;
    WinUaeQtConfig config;
};

int runWinUaeQtLauncher(QApplication &app);
int runWinUaeQtLauncher(int argc, char **argv);
WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(QApplication &app);
WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(QApplication &app, const QString &initialConfigPath);
WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(int argc, char **argv);
WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(int argc, char **argv, const QString &initialConfigPath);
