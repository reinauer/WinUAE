#include "sysconfig.h"
#include "sysdeps.h"

#include <stdio.h>
#include <string>

#include "traps.h"
#include "clipboard.h"
#include "keybuf.h"
#include "uae.h"

static constexpr size_t MAX_CLIPBOARD_PASTE = 32 * 1024;

static bool read_command_output(const char *command, std::string *out)
{
	FILE *pipe = popen(command, "r");
	if (!pipe) {
		return false;
	}

	char buffer[4096];
	while (out->size() < MAX_CLIPBOARD_PASTE) {
		const size_t remaining = MAX_CLIPBOARD_PASTE - out->size();
		const size_t wanted = remaining < sizeof buffer ? remaining : sizeof buffer;
		const size_t got = fread(buffer, 1, wanted, pipe);
		if (got > 0) {
			out->append(buffer, got);
		}
		if (got < wanted) {
			if (feof(pipe) || ferror(pipe)) {
				break;
			}
		}
	}

	const bool truncated = out->size() >= MAX_CLIPBOARD_PASTE;
	const int status = pclose(pipe);
	return !out->empty() && (status == 0 || truncated);
}

static bool read_host_clipboard_text(std::string *text)
{
	const char *commands[] = {
#ifdef __APPLE__
		"/usr/bin/pbpaste",
#else
		"wl-paste --no-newline 2>/dev/null",
		"xclip -selection clipboard -o 2>/dev/null",
		"xsel --clipboard --output 2>/dev/null",
#endif
		NULL
	};

	for (int i = 0; commands[i]; i++) {
		text->clear();
		if (read_command_output(commands[i], text)) {
			return true;
		}
	}
	text->clear();
	return false;
}

static void normalize_text_for_keybuf(std::string *text)
{
	std::string normalized;
	normalized.reserve(text->size());
	for (size_t i = 0; i < text->size(); i++) {
		const char ch = (*text)[i];
		if (ch == '\r') {
			if (i + 1 < text->size() && (*text)[i + 1] == '\n') {
				continue;
			}
			normalized.push_back('\n');
		} else {
			normalized.push_back(ch);
		}
	}
	*text = normalized;
}

void clipboard_vsync(void)
{
}

void clipboard_unsafeperiod(void)
{
}

void clipboard_disable(bool)
{
}

uaecptr amiga_clipboard_proc_start(TrapContext *)
{
	return 0;
}

void amiga_clipboard_task_start(TrapContext *, uaecptr)
{
}

int amiga_clipboard_want_data(TrapContext *)
{
	return 0;
}

void amiga_clipboard_got_data(TrapContext *, uaecptr, uae_u32, uae_u32)
{
}

void amiga_clipboard_die(TrapContext *)
{
}

void amiga_clipboard_init(TrapContext *)
{
}

void target_paste_to_keyboard(void)
{
	std::string text;
	if (!read_host_clipboard_text(&text)) {
		return;
	}
	normalize_text_for_keybuf(&text);
	if (!text.empty()) {
		keybuf_inject(text.c_str());
	}
}
