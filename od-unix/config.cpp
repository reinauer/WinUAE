#include "sysconfig.h"
#include "sysdeps.h"

#include <ctype.h>
#include <cstdlib>
#include <limits.h>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

#include "options.h"
#include "sound_unix.h"
#include "uae/string.h"
#include "uae.h"
#include "zfile.h"

TCHAR start_path_data[MAX_DPATH];
TCHAR start_path_data_exe[MAX_DPATH];
TCHAR start_path_plugins[MAX_DPATH];
int saveimageoriginalpath;

static std::string trim_copy(const std::string &s)
{
    size_t first = 0;
    while (first < s.size() && isspace((unsigned char)s[first])) {
        first++;
    }
    size_t last = s.size();
    while (last > first && isspace((unsigned char)s[last - 1])) {
        last--;
    }
    return s.substr(first, last - first);
}

static std::string lowercase_copy(std::string s)
{
    for (char &c : s) {
        c = (char)tolower((unsigned char)c);
    }
    return s;
}

static bool is_absolute_path(const std::string &path)
{
    return !path.empty() && path[0] == '/';
}

static bool file_exists(const std::string &path)
{
    return !path.empty() && access(path.c_str(), F_OK) == 0;
}

static std::string dirname_copy(const char *path)
{
    std::string s = path ? path : "";
    size_t slash = s.find_last_of('/');
    if (slash == std::string::npos) {
        return ".";
    }
    if (slash == 0) {
        return "/";
    }
    return s.substr(0, slash);
}

static std::string join_path(const std::string &dir, const std::string &name)
{
    if (dir.empty() || dir == ".") {
        return name;
    }
    if (!dir.empty() && dir[dir.size() - 1] == '/') {
        return dir + name;
    }
    return dir + "/" + name;
}

static std::string expand_unix_path(const std::string &path)
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
        out.replace(i, end - i, value);
        i += strlen(value);
    }

    return out;
}

static std::string resolve_legacy_path(const std::string &value, const std::string &config_dir, const char *subdir)
{
    std::string path = expand_unix_path(trim_copy(value));
    if (path.size() >= 2 &&
        ((path[0] == '"' && path[path.size() - 1] == '"') || (path[0] == '\'' && path[path.size() - 1] == '\''))) {
        path = path.substr(1, path.size() - 2);
    }
    if (path.empty() || is_absolute_path(path)) {
        return path;
    }

    std::string direct = join_path(config_dir, path);
    if (file_exists(direct)) {
        return direct;
    }
    if (subdir) {
        std::string parent = dirname_copy(config_dir.c_str());
        std::string sibling = join_path(join_path(parent, subdir), path);
        if (file_exists(sibling)) {
            return sibling;
        }
    }
    return direct;
}

static std::string translate_filesystem2_path(const std::string &value, const std::string &config_dir)
{
    std::string line = trim_copy(value);
    size_t comma = line.find(',');
    if (comma == std::string::npos) {
        return line;
    }

    size_t devsep = line.find(':', comma + 1);
    if (devsep == std::string::npos) {
        return line;
    }
    size_t volsep = line.find(':', devsep + 1);
    if (volsep == std::string::npos) {
        return line;
    }

    size_t path_start = volsep + 1;
    size_t path_end = std::string::npos;
    std::string path;
    if (path_start < line.size() && line[path_start] == '"') {
        for (size_t i = path_start + 1; i < line.size(); i++) {
            if (line[i] == '"' && line[i - 1] != '\\') {
                path_end = i + 1;
                path = line.substr(path_start + 1, i - path_start - 1);
                break;
            }
        }
        if (path_end == std::string::npos) {
            return line;
        }
    } else {
        path_end = line.find(',', path_start);
        if (path_end == std::string::npos) {
            path_end = line.size();
        }
        path = line.substr(path_start, path_end - path_start);
    }

    std::string resolved = resolve_legacy_path(path, config_dir, NULL);
    return line.substr(0, path_start) + resolved + line.substr(path_end);
}

static void strip_inline_comment(std::string &line)
{
    bool quoted = false;
    char quote = 0;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if ((c == '"' || c == '\'') && (i == 0 || line[i - 1] != '\\')) {
            if (!quoted) {
                quoted = true;
                quote = c;
            } else if (quote == c) {
                quoted = false;
            }
            continue;
        }
        if (!quoted && (c == ';' || c == '#')) {
            line.erase(i);
            return;
        }
    }
}

static bool read_legacy_config(const TCHAR *filename, std::map<std::string, std::string> &kv)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        return false;
    }

    char line[4096];
    bool recognized = false;
    while (fgets(line, sizeof line, f)) {
        std::string s(line);
        strip_inline_comment(s);
        s = trim_copy(s);
        if (s.empty() || s[0] == '[') {
            continue;
        }
        size_t eq = s.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = lowercase_copy(trim_copy(s.substr(0, eq)));
        std::string value = trim_copy(s.substr(eq + 1));
        kv[key] = value;
        if (key == "amiga_model" || key == "kickstart_file" || key == "floppy_drive_0" || key == "hard_drive_0") {
            recognized = true;
        }
    }
    fclose(f);
    return recognized;
}

static int parse_bool_value(const std::string &value)
{
    std::string v = lowercase_copy(trim_copy(value));
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

static bool parse_int_value(const TCHAR *value, int *out)
{
    if (!value || !out) {
        return false;
    }
    std::string text = trim_copy(value);
    if (text.empty()) {
        return false;
    }
    char *end = NULL;
    long parsed = strtol(text.c_str(), &end, 0);
    if (!end || *end != 0) {
        return false;
    }
    *out = (int)parsed;
    return true;
}

static int activity_priority_index_from_value(int value, int defpri)
{
    switch (value) {
    case 1:
        return 0;
    case 0:
        return 1;
    case -1:
        return 2;
    case -2:
        return 3;
    default:
        return defpri;
    }
}

static void add_line(std::vector<std::string> &lines, const std::string &option, const std::string &value)
{
    if (!value.empty()) {
        lines.push_back(option + "=" + value);
    }
}

static void add_int_line(std::vector<std::string> &lines, const std::string &option, int value)
{
    char tmp[64];
    snprintf(tmp, sizeof tmp, "%d", value);
    add_line(lines, option, tmp);
}

static int load_legacy_config(struct uae_prefs *p, const TCHAR *filename)
{
    std::map<std::string, std::string> kv;
    if (!read_legacy_config(filename, kv)) {
        return 0;
    }

    std::string config_dir = dirname_copy(filename);
    std::vector<std::string> lines;

    auto get = [&kv](const char *key) -> std::string {
        auto it = kv.find(key);
        return it == kv.end() ? std::string() : it->second;
    };

    std::string model = lowercase_copy(get("amiga_model"));
    if (!model.empty()) {
        if (model == "a4000" || model == "a1200" || model == "cd32") {
            add_line(lines, "chipset", "aga");
        }
        if (model == "a4000") {
            add_line(lines, "chipset_compatible", "A4000");
            add_line(lines, "ide", "a4000");
        } else if (model == "a3000") {
            add_line(lines, "chipset_compatible", "A3000");
        } else if (model == "cd32") {
            add_line(lines, "chipset_compatible", "CD32");
        }
    }

    std::string cpu = get("cpu");
    if (!cpu.empty()) {
        add_line(lines, "cpu_model", cpu);
        if (cpu == "68030" || cpu == "68040" || cpu == "68060") {
            add_line(lines, "cpu_type", "68020/68881");
        }
    }
    if (!get("jit_compiler").empty()) {
        add_int_line(lines, "cachesize", parse_bool_value(get("jit_compiler")) ? 8192 : 0);
    }
    if (!get("chip_memory").empty()) {
        add_int_line(lines, "chipmem_size", atoi(get("chip_memory").c_str()) / 512);
    }
    if (!get("motherboard_ram").empty()) {
        add_int_line(lines, "a3000mem_size", atoi(get("motherboard_ram").c_str()) / 1024);
    }

    add_line(lines, "kickstart_rom_file", resolve_legacy_path(get("kickstart_file"), config_dir, "Kickstarts"));
#ifdef NCR
    add_line(lines, "a4091_rom_file", resolve_legacy_path(get("uae_a4091_rom_file"), config_dir, "Kickstarts"));
    add_line(lines, "a4091_rom_options", get("uae_a4091_rom_options"));
#endif

    int floppies = 0;
    for (int i = 0; i < 4; i++) {
        char key[32], option[32];
        snprintf(key, sizeof key, "floppy_drive_%d", i);
        std::string value = get(key);
        if (value.empty()) {
            continue;
        }
        snprintf(option, sizeof option, "floppy%d", i);
        add_line(lines, option, resolve_legacy_path(value, config_dir, "Floppies"));
        floppies = i + 1;
    }
    if (floppies) {
        add_int_line(lines, "nr_floppies", floppies);
    }

    std::string cd0 = resolve_legacy_path(get("cdrom_drive_0"), config_dir, "CD-ROMs");
    if (!cd0.empty()) {
        add_line(lines, "cdimage0", cd0 + ",image");
    }

    for (int i = 0; i < 8; i++) {
        char key[32];
        snprintf(key, sizeof key, "hard_drive_%d", i);
        std::string value = get(key);
        if (value.empty()) {
            continue;
        }
        std::string path = resolve_legacy_path(value, config_dir, "Hard Drives");
        snprintf(key, sizeof key, "hard_drive_%d_controller", i);
        std::string controller = get(key);
        if (controller.empty()) {
            controller = "uae";
        }
        char line[2048];
        snprintf(line, sizeof line, "hardfile2=rw,DH%d:%s,0,0,0,512,0,,%s", i, path.c_str(), controller.c_str());
        lines.push_back(line);
        snprintf(line, sizeof line, "uaehf%d=hdf,rw,DH%d:%s,0,0,0,512,0,,%s", i, i, path.c_str(), controller.c_str());
        lines.push_back(line);
    }

    add_line(lines, "filesystem2", translate_filesystem2_path(get("filesystem2"), config_dir));
    add_line(lines, "unix.serial_port", get("serial_port"));
    if (!get("console_debugger").empty()) {
        add_line(lines, "use_debugger", parse_bool_value(get("console_debugger")) ? "true" : "false");
    }

    for (const std::string &line : lines) {
        char tmp[4096];
        uae_tcslcpy(tmp, line.c_str(), sizeof tmp);
        cfgfile_parse_line(p, tmp, 0);
    }

    write_log(_T("Imported Unix legacy config '%s' (%d translated options)\n"), filename, (int)lines.size());
    return 1;
}

int target_cfgfile_load(struct uae_prefs *p, const TCHAR *filename, int type, int isdefault)
{
    if (isdefault && type == CONFIG_TYPE_DEFAULT && !file_exists(filename)) {
        return 1;
    }
    if (type == CONFIG_TYPE_ALL && load_legacy_config(p, filename)) {
        return 1;
    }
    int loaded_type = type;
    return cfgfile_load(p, filename, &loaded_type, 0, !isdefault);
}

int target_parse_option(struct uae_prefs *p, const TCHAR *option, const TCHAR *value, int)
{
    if (!_tcsicmp(option, _T("middle_mouse"))) {
        if (parse_bool_value(value ? value : "")) {
            p->input_mouse_untrap |= MOUSEUNTRAP_MIDDLEBUTTON;
        } else {
            p->input_mouse_untrap &= ~MOUSEUNTRAP_MIDDLEBUTTON;
        }
        return 1;
    }
    if (!_tcsicmp(option, _T("active_not_captured_pause"))) {
        p->win32_active_nocapture_pause = parse_bool_value(value ? value : "");
        return 1;
    }
    if (!_tcsicmp(option, _T("active_not_captured_nosound"))) {
        p->win32_active_nocapture_nosound = parse_bool_value(value ? value : "");
        return 1;
    }
    if (!_tcsicmp(option, _T("inactive_pause"))) {
        p->win32_inactive_pause = parse_bool_value(value ? value : "");
        return 1;
    }
    if (!_tcsicmp(option, _T("inactive_nosound"))) {
        p->win32_inactive_nosound = parse_bool_value(value ? value : "");
        return 1;
    }
    if (!_tcsicmp(option, _T("iconified_pause"))) {
        p->win32_iconified_pause = parse_bool_value(value ? value : "");
        return 1;
    }
    if (!_tcsicmp(option, _T("iconified_nosound"))) {
        p->win32_iconified_nosound = parse_bool_value(value ? value : "");
        return 1;
    }
    if (!_tcsicmp(option, _T("active_input"))
        || !_tcsicmp(option, _T("inactive_input"))
        || !_tcsicmp(option, _T("iconified_input"))) {
        int parsed = 0;
        if (!parse_int_value(value, &parsed)) {
            return 0;
        }
        if (!_tcsicmp(option, _T("active_input"))) {
            p->win32_active_input = parsed;
        } else if (!_tcsicmp(option, _T("inactive_input"))) {
            p->win32_inactive_input = parsed;
        } else {
            p->win32_iconified_input = parsed;
        }
        return 1;
    }
    if (!_tcsicmp(option, _T("active_priority"))
        || !_tcsicmp(option, _T("activepriority"))
        || !_tcsicmp(option, _T("inactive_priority"))
        || !_tcsicmp(option, _T("iconified_priority"))) {
        int parsed = 0;
        if (!parse_int_value(value, &parsed)) {
            return 0;
        }
        if (!_tcsicmp(option, _T("active_priority")) || !_tcsicmp(option, _T("activepriority"))) {
            p->win32_active_capture_priority = activity_priority_index_from_value(parsed, 1);
        } else if (!_tcsicmp(option, _T("inactive_priority"))) {
            p->win32_inactive_priority = activity_priority_index_from_value(parsed, 1);
        } else {
            p->win32_iconified_priority = activity_priority_index_from_value(parsed, 2);
        }
        return 1;
    }
    if (!_tcsicmp(option, _T("serial_port"))) {
        std::string port = trim_copy(value ? value : "");
        if (port.size() >= 2 &&
            ((port[0] == '"' && port[port.size() - 1] == '"') || (port[0] == '\'' && port[port.size() - 1] == '\''))) {
            port = port.substr(1, port.size() - 2);
        }
        if (lowercase_copy(port) == "none") {
            port.clear();
        }
        uae_tcslcpy(p->sername, port.c_str(), sizeof p->sername / sizeof(TCHAR));
        p->use_serial = p->sername[0] != 0;
        return 1;
    }
    if (!_tcsicmp(option, _T("soundcard"))) {
        int parsed = 0;
        if (!parse_int_value(value, &parsed)) {
            return 0;
        }
        if (parsed < 0 || parsed >= unix_sound_device_count()) {
            parsed = 0;
        }
        p->win32_soundcard = parsed;
        return 1;
    }
    if (!_tcsicmp(option, _T("soundcardname"))) {
        int index = unix_sound_device_index_from_config_name(value);
        if (index >= 0) {
            p->win32_soundcard = index;
        }
        return 1;
    }
    return 0;
}

void target_save_options(struct zfile *f, struct uae_prefs *p)
{
    int index = p->win32_soundcard;
    if (index < 0 || index >= unix_sound_device_count()) {
        index = 0;
    }
    cfgfile_target_write(f, _T("soundcard"), _T("%d"), index);
    const TCHAR *name = unix_sound_device_config_name(index);
    if (name && name[0]) {
        cfgfile_target_write_str(f, _T("soundcardname"), name);
    }
}

void target_default_options(struct uae_prefs*, int)
{
}

void target_fixup_options(struct uae_prefs*)
{
}

void target_multipath_modified(struct uae_prefs*)
{
}

bool target_isrelativemode(void)
{
    return false;
}

TCHAR *target_expand_environment(const TCHAR *path, TCHAR *out, int maxlen)
{
    if (!path) {
        return NULL;
    }

    std::string expanded = expand_unix_path(path);
    if (out) {
        uae_tcslcpy(out, expanded.c_str(), maxlen);
        return out;
    }
    return my_strdup(expanded.c_str());
}

bool get_plugin_path(TCHAR *out, int size, const TCHAR *path)
{
    uae_tcslcpy(out, path, size);
    return true;
}

void stripslashes(TCHAR *p)
{
    while (*p) {
        if (*p == '\\') {
            *p = '/';
        }
        p++;
    }
}

void fixtrailing(TCHAR *p)
{
    int len = _tcslen(p);
    if (len > 0 && p[len - 1] != '/') {
        _tcscat(p, "/");
    }
}

void fullpath(TCHAR *path, int size)
{
    fullpath(path, size, false);
}

void fullpath(TCHAR *path, int size, bool)
{
    char tmp[PATH_MAX];
    if (realpath(path, tmp)) {
        uae_tcslcpy(path, tmp, size);
    }
}

void getpathpart(TCHAR *outpath, int size, const TCHAR *inpath)
{
    uae_tcslcpy(outpath, inpath, size);
    TCHAR *slash = _tcsrchr(outpath, '/');
    if (slash) {
        slash[1] = 0;
    } else {
        outpath[0] = 0;
    }
}

void getfilepart(TCHAR *out, int size, const TCHAR *path)
{
    const TCHAR *slash = _tcsrchr(path, '/');
    uae_tcslcpy(out, slash ? slash + 1 : path, size);
}

bool samepath(const TCHAR *p1, const TCHAR *p2)
{
    return _tcscmp(p1, p2) == 0;
}

static void fetch_home_path(TCHAR *out, int size)
{
    const char *home = getenv("HOME");
    uae_tcslcpy(out, home ? home : ".", size);
    fixtrailing(out);
}

void fetch_saveimagepath(TCHAR *out, int size, int) { fetch_home_path(out, size); }
void fetch_configurationpath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_nvrampath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_luapath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_screenshotpath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_ripperpath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_statefilepath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_inputfilepath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_datapath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_rompath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_videopath(TCHAR *out, int size) { fetch_home_path(out, size); }

void target_getdate(int *y, int *m, int *d)
{
    *y = 2026;
    *m = 5;
    *d = 10;
}
