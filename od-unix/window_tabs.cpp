/*
 * Window tabs: see window_tabs.h.
 */

#include "window_tabs.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

struct entry {
	int pid;
	unsigned long long started;
	std::string label;
};

bool s_option;
bool s_active;
int s_sock = -1;
std::string s_dir;
unsigned long long s_started;
std::string s_label;
std::vector<window_tab> s_tabs;
unsigned long long s_next_scan;

std::string base_dir(void)
{
	const char *d = getenv("XDG_RUNTIME_DIR");
	if (!d || !d[0])
		d = getenv("TMPDIR");
	if (!d || !d[0])
		d = "/tmp";
	std::string s(d);
	while (s.size() > 1 && s.back() == '/')
		s.pop_back();
	char tail[64];
	snprintf(tail, sizeof tail, "/winuae-tabs-%u", (unsigned)getuid());
	return s + tail;
}

std::string path_for(int pid, const char *ext)
{
	char name[64];
	snprintf(name, sizeof name, "/%d.%s", pid, ext);
	return s_dir + name;
}

bool alive(int pid)
{
	return pid > 0 && (kill(pid, 0) == 0 || errno == EPERM);
}

unsigned long long now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec;
}

std::string label_for(const char *config_path)
{
	const char *env = getenv("WINUAE_TAB_NAME");
	std::string l;
	if (env && env[0]) {
		l = env;
	} else if (config_path && config_path[0]) {
		const char *b = strrchr(config_path, '/');
		l = b ? b + 1 : config_path;
		size_t dot = l.rfind('.');
		if (dot != std::string::npos && dot > 0)
			l.erase(dot);
	}
	if (l.empty())
		l = "WinUAE";
	for (char &c : l) {
		if ((unsigned char)c < 32 || c == '\n')
			c = ' ';
	}
	return l;
}

bool write_file(const std::string &path, const std::string &text)
{
	std::string tmp = path + ".tmp";
	FILE *f = fopen(tmp.c_str(), "w");
	if (!f)
		return false;
	fputs(text.c_str(), f);
	fclose(f);
	return rename(tmp.c_str(), path.c_str()) == 0;
}

bool read_file(const std::string &path, std::string *text)
{
	FILE *f = fopen(path.c_str(), "r");
	if (!f)
		return false;
	char buf[512];
	size_t n = fread(buf, 1, sizeof buf - 1, f);
	fclose(f);
	buf[n] = 0;
	*text = buf;
	return true;
}

void send_to(int pid, const char *msg)
{
	if (s_sock < 0)
		return;
	struct sockaddr_un a;
	memset(&a, 0, sizeof a);
	a.sun_family = AF_UNIX;
	std::string p = path_for(pid, "sock");
	if (p.size() >= sizeof a.sun_path)
		return;
	strcpy(a.sun_path, p.c_str());
	sendto(s_sock, msg, strlen(msg), 0, (struct sockaddr *)&a, sizeof a);
}

void geom_msg(char *buf, size_t n, const char *cmd, const window_tab_geom &g)
{
	snprintf(buf, n, "%s %d %d %d %d", cmd, g.x, g.y, g.w, g.h);
}

/* Rescans the directory: live instances, oldest first. Removes the files of
 * instances that are gone. */
bool scan(void)
{
	std::vector<entry> found;
	DIR *d = opendir(s_dir.c_str());
	if (d) {
		struct dirent *e;
		while ((e = readdir(d)) != NULL) {
			const char *n = e->d_name;
			size_t len = strlen(n);
			if (len < 5 || strcmp(n + len - 4, ".tab") != 0)
				continue;
			int pid = atoi(n);
			if (pid <= 0)
				continue;
			if (!alive(pid)) {
				unlink(path_for(pid, "tab").c_str());
				unlink(path_for(pid, "sock").c_str());
				continue;
			}
			std::string text;
			if (!read_file(s_dir + "/" + n, &text))
				continue;
			entry en;
			en.pid = pid;
			en.started = strtoull(text.c_str(), NULL, 10);
			size_t nl = text.find('\n');
			en.label = nl == std::string::npos ? std::string() : text.substr(nl + 1);
			while (!en.label.empty() && (en.label.back() == '\n' || en.label.back() == '\r'))
				en.label.pop_back();
			found.push_back(en);
		}
		closedir(d);
	}
	std::sort(found.begin(), found.end(), [](const entry &a, const entry &b) {
		return a.started != b.started ? a.started < b.started : a.pid < b.pid;
	});
	std::vector<window_tab> tabs;
	for (const entry &en : found) {
		window_tab t;
		t.pid = en.pid;
		t.label = en.label;
		t.self = en.pid == (int)getpid();
		tabs.push_back(t);
	}
	bool changed = tabs.size() != s_tabs.size();
	for (size_t i = 0; !changed && i < tabs.size(); i++)
		changed = tabs[i].pid != s_tabs[i].pid || tabs[i].label != s_tabs[i].label;
	s_tabs = tabs;
	return changed;
}

bool parse_geom(const char *s, window_tab_geom *g)
{
	return sscanf(s, "%d %d %d %d", &g->x, &g->y, &g->w, &g->h) == 4 && g->w > 0 && g->h > 0;
}

} // namespace

void window_tabs_set_option(bool on)
{
	s_option = on;
}

bool window_tabs_option(void)
{
	return s_option;
}

bool window_tabs_active(void)
{
	return s_active;
}

bool window_tabs_start(const char *config_path, window_tab_geom *adopt, bool *have_adopt)
{
	*have_adopt = false;
	const char *env = getenv("WINUAE_WINDOW_TABS");
	bool on = s_option || (env && env[0] && strcmp(env, "0") != 0);
	if (!on || s_active)
		return s_active;

	s_dir = base_dir();
	if (mkdir(s_dir.c_str(), 0700) != 0 && errno != EEXIST)
		return false;
	struct stat st;
	if (lstat(s_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != getuid())
		return false;

	int pid = (int)getpid();
	std::string sp = path_for(pid, "sock");
	struct sockaddr_un a;
	memset(&a, 0, sizeof a);
	a.sun_family = AF_UNIX;
	if (sp.size() >= sizeof a.sun_path)
		return false;
	strcpy(a.sun_path, sp.c_str());
	unlink(sp.c_str());
	s_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (s_sock < 0)
		return false;
	fcntl(s_sock, F_SETFL, fcntl(s_sock, F_GETFL) | O_NONBLOCK);
	fcntl(s_sock, F_SETFD, FD_CLOEXEC);
	if (bind(s_sock, (struct sockaddr *)&a, sizeof a) != 0) {
		close(s_sock);
		s_sock = -1;
		return false;
	}

	s_started = now_ns();
	s_label = label_for(config_path);
	char head[32];
	snprintf(head, sizeof head, "%llu\n", s_started);
	write_file(path_for(pid, "tab"), std::string(head) + s_label + "\n");
	s_active = true;
	scan();
	s_next_scan = 0;

	/* the window in front, if its instance is still running */
	std::string text;
	if (s_tabs.size() > 1 && read_file(s_dir + "/front", &text)) {
		int fpid = atoi(text.c_str());
		size_t sp1 = text.find(' ');
		if (fpid != pid && alive(fpid) && sp1 != std::string::npos &&
			parse_geom(text.c_str() + sp1 + 1, adopt))
			*have_adopt = true;
	}
	return true;
}

window_tabs_cmd window_tabs_poll(unsigned long long now_ms, window_tab_geom *geom, bool *changed)
{
	*changed = false;
	if (!s_active)
		return WINDOW_TABS_NONE;
	if (now_ms >= s_next_scan) {
		*changed = scan();
		s_next_scan = now_ms + 500;
	}
	char buf[128];
	ssize_t n = recv(s_sock, buf, sizeof buf - 1, 0);
	if (n <= 0)
		return WINDOW_TABS_NONE;
	buf[n] = 0;
	if (!strncmp(buf, "show ", 5) && parse_geom(buf + 5, geom))
		return WINDOW_TABS_SHOW;
	if (!strncmp(buf, "geom ", 5) && parse_geom(buf + 5, geom))
		return WINDOW_TABS_GEOM;
	return WINDOW_TABS_NONE;
}

const std::vector<window_tab> &window_tabs_list(void)
{
	return s_tabs;
}

void window_tabs_show(int pid, const window_tab_geom &geom)
{
	char msg[96];
	geom_msg(msg, sizeof msg, "show", geom);
	send_to(pid, msg);
}

void window_tabs_front(const window_tab_geom &geom)
{
	if (!s_active)
		return;
	char text[96];
	snprintf(text, sizeof text, "%d %d %d %d %d\n", (int)getpid(), geom.x, geom.y, geom.w, geom.h);
	write_file(s_dir + "/front", text);
}

void window_tabs_follow(const window_tab_geom &geom)
{
	if (!s_active)
		return;
	char msg[96];
	geom_msg(msg, sizeof msg, "geom", geom);
	for (const window_tab &t : s_tabs) {
		if (!t.self)
			send_to(t.pid, msg);
	}
	window_tabs_front(geom);
}

void window_tabs_stop(bool front, const window_tab_geom &geom)
{
	if (!s_active)
		return;
	int pid = (int)getpid();
	if (front) {
		/* the tab after this one comes to the front, else the one before */
		int next = 0;
		for (size_t i = 0; i < s_tabs.size(); i++) {
			if (s_tabs[i].pid != pid)
				continue;
			if (i + 1 < s_tabs.size())
				next = s_tabs[i + 1].pid;
			else if (i > 0)
				next = s_tabs[i - 1].pid;
		}
		if (next && alive(next))
			window_tabs_show(next, geom);
	}
	bool last = true;
	for (const window_tab &t : s_tabs) {
		if (t.pid != pid && alive(t.pid))
			last = false;
	}
	if (last)
		unlink((s_dir + "/front").c_str());
	close(s_sock);
	s_sock = -1;
	unlink(path_for(pid, "sock").c_str());
	unlink(path_for(pid, "tab").c_str());
	s_active = false;
	s_tabs.clear();
}

#ifndef __APPLE__
void window_tabs_platform_yield(int)
{
}

void window_tabs_platform_activate(void)
{
}
#endif
