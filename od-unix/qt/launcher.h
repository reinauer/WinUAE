#pragma once

#include "config.h"

#include <QVector>

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

struct WinUaeQtRuntimeFileDialogResult {
    bool accepted = false;
    QString path;
};

struct WinUaeQtHardwareBoard {
    int index = -1;
    QString type;
    QString name;
    QString start;
    QString end;
    QString size;
    QString id;
    bool movable = false;
};

struct WinUaeQtHardwareInfoProvider {
    void *context = nullptr;
    QVector<WinUaeQtHardwareBoard> (*boards)(void *context) = nullptr;
    bool (*customOrder)(void *context) = nullptr;
    void (*setCustomOrder)(void *context, bool enabled) = nullptr;
    bool (*canMove)(void *context, int index, int direction) = nullptr;
    int (*move)(void *context, int index, int direction) = nullptr;
    WinUaeQtConfig::Settings (*orderSettings)(void *context) = nullptr;
    void (*saveScreenshot)(void *context) = nullptr;
};

int runWinUaeQtLauncher(QApplication &app);
int runWinUaeQtLauncher(int argc, char **argv);
WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(QApplication &app);
WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(QApplication &app, const QString &initialConfigPath);
WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(QApplication &app, const QString &initialConfigPath, const WinUaeQtHardwareInfoProvider &hardwareProvider);
WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(int argc, char **argv);
WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(int argc, char **argv, const QString &initialConfigPath);
WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(int argc, char **argv, const QString &initialConfigPath, const WinUaeQtHardwareInfoProvider &hardwareProvider);
WinUaeQtRuntimeFileDialogResult runWinUaeQtRuntimeFileDialog(QApplication &app, int shortcut, const QString &initialPath);
WinUaeQtRuntimeFileDialogResult runWinUaeQtRuntimeFileDialog(int argc, char **argv, int shortcut, const QString &initialPath);
