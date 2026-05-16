#include "sysconfig.h"
#include "sysdeps.h"

#include <ctype.h>
#include <cstdlib>
#include <string>
#include <string.h>

#include "path_expand.h"

std::string unix_expand_path(const std::string &path)
{
    std::string out;
    const char *home = getenv("HOME");

    if (!path.empty() && path[0] == '~' && (path.size() == 1 || path[1] == '/')) {
        out = home ? home : "";
        out += path.substr(1);
    } else {
        out = path;
    }

    if (home) {
        std::string homestr(home);
        size_t slash = homestr.find_last_of('/');
        std::string user = slash == std::string::npos ? homestr : homestr.substr(slash + 1);
        std::string oldhome = "/home/" + user;
        if (!user.empty() && out.compare(0, oldhome.size(), oldhome) == 0 &&
            (out.size() == oldhome.size() || out[oldhome.size()] == '/')) {
            out = homestr + out.substr(oldhome.size());
        }
    }

    for (size_t i = 0; i < out.size(); i++) {
        if (out[i] != '$') {
            continue;
        }
        size_t start = i + 1;
        size_t end = start;
        std::string name;
        if (start < out.size() && out[start] == '{') {
            start++;
            end = out.find('}', start);
            if (end == std::string::npos) {
                continue;
            }
            name = out.substr(start, end - start);
            end++;
        } else {
            while (end < out.size() && (isalnum((unsigned char)out[end]) || out[end] == '_')) {
                end++;
            }
            name = out.substr(start, end - start);
        }
        if (name.empty()) {
            continue;
        }
        const char *value = getenv(name.c_str());
        if (!value) {
            continue;
        }
        const size_t value_len = strlen(value);
        out.replace(i, end - i, value);
        if (value_len > 0) {
            i += value_len - 1;
        }
    }

    return out;
}
