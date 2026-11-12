#pragma once

/*
 * Window tabs: several WinUAE processes shown as tabs of one window.
 *
 * Every instance keeps its own window. With tabs on, the windows are kept
 * stacked at the same position and size, and each draws the same tab strip
 * in its status bar; clicking a tab brings that instance's window to the
 * front. Instances find each other through a per-user directory
 * ($XDG_RUNTIME_DIR, else $TMPDIR, else /tmp)/winuae-tabs-<uid>/, holding
 * <pid>.tab (start time and label) and <pid>.sock (a datagram socket that
 * takes "show" and "geom" messages). A tab whose process is gone disappears.
 */

#include <string>
#include <vector>

struct window_tab {
	int pid;
	std::string label;
	bool self;
};

struct window_tab_geom {
	int x, y, w, h;
};

/* The "window_tabs" option (config file or WINUAE_WINDOW_TABS=1). */
void window_tabs_set_option(bool on);
bool window_tabs_option(void);

/* Registers this instance (label: WINUAE_TAB_NAME, else the config file's
 * name). Returns true when tabs are on. If another instance is in front,
 * *adopt is set to its window geometry and *have_adopt to true. */
bool window_tabs_start(const char *config_path, window_tab_geom *adopt, bool *have_adopt);
bool window_tabs_active(void);

enum window_tabs_cmd { WINDOW_TABS_NONE, WINDOW_TABS_SHOW, WINDOW_TABS_GEOM };

/* Call often (the event loop). Returns a command another instance sent
 * (geometry in *geom), one per call. *changed is set when the tab list
 * changed (rescanned at most every 500 ms). */
window_tabs_cmd window_tabs_poll(unsigned long long now_ms, window_tab_geom *geom, bool *changed);

const std::vector<window_tab> &window_tabs_list(void);

/* Asks instance pid to come to the front at geom. */
void window_tabs_show(int pid, const window_tab_geom &geom);
/* Tells every other instance to follow geom, and records it for new ones. */
void window_tabs_follow(const window_tab_geom &geom);
/* Records geom as the front window's (focus gained). */
void window_tabs_front(const window_tab_geom &geom);

/* macOS (window_tabs_macos.mm): hand activation to process pid (its tab
 * was clicked), and activate this process (asked to come to the front).
 * Nothing elsewhere. */
void window_tabs_platform_yield(int pid);
void window_tabs_platform_activate(void);

/* Unregisters; when this window was in front, the next tab comes to the
 * front at geom. */
void window_tabs_stop(bool front, const window_tab_geom &geom);
