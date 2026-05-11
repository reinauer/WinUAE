#pragma once

struct uae_prefs;

enum {
    WINUAE_QT_LAUNCHER_EXIT = 1,
    WINUAE_QT_LAUNCHER_START = 2,
    WINUAE_QT_LAUNCHER_ERROR = 3
};

int runWinUaeQtLauncherForPrefs(int argc, char **argv, struct uae_prefs *prefs, int *exit_code);
