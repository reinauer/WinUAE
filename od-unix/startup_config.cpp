#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "startup_config.h"

#include <string>
#include <vector>

static std::vector<std::string> startup_config_lines;

void unix_startup_config_clear(void)
{
    startup_config_lines.clear();
}

void unix_startup_config_add_line(const char *line)
{
    if (line && line[0]) {
        startup_config_lines.emplace_back(line);
    }
}

int unix_startup_config_count(void)
{
    return (int)startup_config_lines.size();
}

void unix_startup_config_apply(struct uae_prefs *prefs)
{
    for (const std::string &line : startup_config_lines) {
        TCHAR *mutable_line = my_strdup(line.c_str());
        cfgfile_parse_line(prefs, mutable_line, 0);
        xfree(mutable_line);
    }
    unix_startup_config_clear();
}
