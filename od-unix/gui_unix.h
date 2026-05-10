#pragma once

#include "sysdeps.h"

enum {
    UNIX_GUI_EARLY_NONE = 0,
    UNIX_GUI_EARLY_EXIT = 1,
    UNIX_GUI_EARLY_START = 2
};

int unix_gui_handle_early_options(int argc, TCHAR **argv, int *exit_code);
