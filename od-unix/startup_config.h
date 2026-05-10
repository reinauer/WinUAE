#pragma once

struct uae_prefs;

void unix_startup_config_clear(void);
void unix_startup_config_add_line(const char *line);
int unix_startup_config_count(void);
void unix_startup_config_apply(struct uae_prefs *prefs);
