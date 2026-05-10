#pragma once

enum {
    WINUAE_QT_LAUNCHER_EXIT = 1,
    WINUAE_QT_LAUNCHER_START = 2
};

int runWinUaeQtLauncherForEmulatorArgs(int argc, char **argv, int *exit_code, int *emulator_argc, char ***emulator_argv);
