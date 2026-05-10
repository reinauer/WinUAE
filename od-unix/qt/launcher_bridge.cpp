#include "launcher_bridge.h"

#include "launcher.h"

#include <QByteArray>
#include <QStringList>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool isQtUiOption(const char *arg)
{
    return strcmp(arg, "--qt-ui") == 0 || strcmp(arg, "-qt-ui") == 0;
}

static char *duplicateArg(const QByteArray &arg)
{
    char *out = (char*)malloc((size_t)arg.size() + 1);
    if (!out) {
        return nullptr;
    }
    memcpy(out, arg.constData(), (size_t)arg.size() + 1);
    return out;
}

static void freeArgs(int argc, char **argv)
{
    if (!argv) {
        return;
    }
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}

static bool buildEmulatorArgs(int argc, char **argv, const WinUaeQtConfig &config, int *outArgc, char ***outArgv)
{
    if (!outArgc || !outArgv) {
        return false;
    }

    QStringList args;
    for (int i = 1; i < argc; i++) {
        if (!isQtUiOption(argv[i])) {
            args << QString::fromLocal8Bit(argv[i]);
        }
    }
    args << config.commandArguments();

    char **allocated = (char**)calloc((size_t)args.size() + 2, sizeof(char*));
    if (!allocated) {
        return false;
    }

    allocated[0] = duplicateArg(QByteArray(argv[0]));
    if (!allocated[0]) {
        free(allocated);
        return false;
    }

    for (int i = 0; i < args.size(); i++) {
        allocated[i + 1] = duplicateArg(args[i].toLocal8Bit());
        if (!allocated[i + 1]) {
            freeArgs(i + 1, allocated);
            return false;
        }
    }

    *outArgc = args.size() + 1;
    *outArgv = allocated;
    return true;
}

int runWinUaeQtLauncherForEmulatorArgs(int argc, char **argv, int *exitCode, int *emulatorArgc, char ***emulatorArgv)
{
    WinUaeQtLauncherResult result = runWinUaeQtLauncherForConfig(argc, argv);
    if (result.status == WinUaeQtLauncherStatus::StartRequested) {
        if (buildEmulatorArgs(argc, argv, result.config, emulatorArgc, emulatorArgv)) {
            return WINUAE_QT_LAUNCHER_START;
        }
        fputs("Failed to prepare Unix Qt UI configuration arguments.\n", stderr);
        if (exitCode) {
            *exitCode = 1;
        }
        return WINUAE_QT_LAUNCHER_EXIT;
    }
    if (result.status == WinUaeQtLauncherStatus::Error) {
        QByteArray error = result.error.toLocal8Bit();
        fprintf(stderr, "Unix Qt UI failed: %s\n", error.constData());
    }
    if (exitCode) {
        *exitCode = result.status == WinUaeQtLauncherStatus::Error
            ? (result.exitCode ? result.exitCode : 1)
            : 0;
    }
    return WINUAE_QT_LAUNCHER_EXIT;
}
