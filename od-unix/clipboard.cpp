#include "sysconfig.h"
#include "sysdeps.h"

#include <stdio.h>
#include <string>
#include <vector>

#ifdef UAE_UNIX_WITH_SDL3
#include <SDL3/SDL.h>
#endif

#include "traps.h"
#include "clipboard.h"
#include "keybuf.h"
#include "native2amiga_api.h"
#include "uae.h"

extern bool filesys_heartbeat(void);

static constexpr size_t MAX_CLIPBOARD_PASTE = 32 * 1024;
static constexpr size_t CLIP_SIZE_LIMIT = 10000000;
static constexpr size_t CLIP_SIZE_LIMIT_INIT = 30000;

static uaecptr clipboard_data;
static int vdelay, vdelay2;
static int signaling, initialized;
static std::vector<uae_u8> to_amiga;
static int to_amiga_phase;
static bool clip_disabled;
static int host_poll_delay;
static std::string last_host_clipboard;
static bool last_host_clipboard_valid;

static bool read_command_output(const char *command, std::string *out, size_t max_bytes)
{
	FILE *pipe = popen(command, "r");
	if (!pipe) {
		return false;
	}

	char buffer[4096];
	while (out->size() < max_bytes) {
		const size_t remaining = max_bytes - out->size();
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

	const bool truncated = out->size() >= max_bytes;
	const int status = pclose(pipe);
	return status == 0 || truncated;
}

static bool write_command_input(const char *command, const std::string &text)
{
	FILE *pipe = popen(command, "w");
	if (!pipe) {
		return false;
	}
	if (!text.empty() && fwrite(text.data(), 1, text.size(), pipe) != text.size()) {
		pclose(pipe);
		return false;
	}
	return pclose(pipe) == 0;
}

static bool read_host_clipboard_text(std::string *text, size_t max_bytes)
{
#ifdef UAE_UNIX_WITH_SDL3
	if (SDL_HasClipboardText()) {
		char *clipboard = SDL_GetClipboardText();
		if (clipboard) {
			text->assign(clipboard);
			SDL_free(clipboard);
			if (text->size() > max_bytes) {
				text->resize(max_bytes);
			}
			return true;
		}
	}
#endif

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
		if (read_command_output(commands[i], text, max_bytes)) {
			return true;
		}
	}
	text->clear();
	return false;
}

static bool write_host_clipboard_text(const std::string &text)
{
#ifdef UAE_UNIX_WITH_SDL3
	if (SDL_SetClipboardText(text.c_str())) {
		last_host_clipboard = text;
		last_host_clipboard_valid = true;
		return true;
	}
#endif

	const char *commands[] = {
#ifdef __APPLE__
		"/usr/bin/pbcopy",
#else
		"wl-copy 2>/dev/null",
		"xclip -selection clipboard 2>/dev/null",
		"xsel --clipboard --input 2>/dev/null",
#endif
		NULL
	};

	for (int i = 0; commands[i]; i++) {
		if (write_command_input(commands[i], text)) {
			last_host_clipboard = text;
			last_host_clipboard_valid = true;
			return true;
		}
	}
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

static std::string host_text_to_amiga_text(const std::string &text)
{
	std::string converted;
	converted.reserve(text.size());
	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] != '\r') {
			converted.push_back(text[i]);
		}
	}
	return converted;
}

static void append_be32(std::vector<uae_u8> *out, uae_u32 value)
{
	out->push_back(value >> 24);
	out->push_back(value >> 16);
	out->push_back(value >> 8);
	out->push_back(value);
}

static void append_literal(std::vector<uae_u8> *out, const char *literal)
{
	for (int i = 0; i < 4; i++) {
		out->push_back((uae_u8)literal[i]);
	}
}

static std::vector<uae_u8> make_iff_text(const std::string &host_text)
{
	const std::string amiga_text = host_text_to_amiga_text(host_text);
	std::vector<uae_u8> iff;
	const uae_u32 text_size = (uae_u32)amiga_text.size();
	const uae_u32 form_size = 4 + 8 + text_size + (text_size & 1);

	iff.reserve(form_size + 8);
	append_literal(&iff, "FORM");
	append_be32(&iff, form_size);
	append_literal(&iff, "FTXT");
	append_literal(&iff, "CHRS");
	append_be32(&iff, text_size);
	iff.insert(iff.end(), amiga_text.begin(), amiga_text.end());
	if (text_size & 1) {
		iff.push_back(0);
	}
	return iff;
}

static void to_amiga_start(void)
{
	to_amiga_phase = 0;
	if (!initialized || !clipboard_data || to_amiga.empty()) {
		return;
	}
	to_amiga_phase = 1;
}

static void queue_host_text_to_amiga(const std::string &text)
{
	to_amiga = make_iff_text(text);
	to_amiga_start();
}

static int parse_csi(const std::string &text, size_t offset)
{
	while (offset < text.size()) {
		if ((uae_u8)text[offset] >= 0x40) {
			break;
		}
		offset++;
	}
	return (int)offset;
}

static std::string amiga_text_to_host_text(const std::string &text)
{
	std::string converted;
	converted.reserve(text.size());
	for (size_t i = 0; i < text.size(); i++) {
		uae_u8 c = (uae_u8)text[i];
		if (c == 0 && i + 1 < text.size()) {
			continue;
		}
		if (c == 0x9b) {
			i = parse_csi(text, i + 1);
			continue;
		}
		if (c == 0x1b && i + 1 < text.size() && text[i + 1] == '[') {
			i = parse_csi(text, i + 2);
			continue;
		}
		if (c == '\r') {
			converted.push_back('\n');
			if (i + 1 < text.size() && text[i + 1] == '\n') {
				i++;
			}
			continue;
		}
		converted.push_back((char)c);
	}
	return converted;
}

static void clipboard_put_text(const std::string &text)
{
	if (!write_host_clipboard_text(text)) {
		write_log(_T("clipboard: failed to write host clipboard text\n"));
	}
}

static void from_iff_text(const uae_u8 *addr, uae_u32 len)
{
	if (len < 12 || memcmp(addr, "FORM", 4) || memcmp(addr + 8, "FTXT", 4)) {
		return;
	}

	std::string text;
	uae_u32 offset = 12;
	while (offset + 8 <= len) {
		const uae_u8 *chunk = addr + offset;
		uae_u32 csize = ((uae_u32)chunk[4] << 24) | ((uae_u32)chunk[5] << 16) | ((uae_u32)chunk[6] << 8) | chunk[7];
		offset += 8;
		if (csize > len - offset) {
			break;
		}
		if (!memcmp(chunk, "CHRS", 4)) {
			text.append((const char *)(addr + offset), csize);
		}
		offset += csize + (csize & 1);
	}

	clipboard_put_text(amiga_text_to_host_text(text));
}

static void from_iff(TrapContext *ctx, uaecptr data, uae_u32 len)
{
	if (len < 12 || !trap_valid_address(ctx, data, len)) {
		return;
	}

	std::vector<uae_u8> buffer((len + 3) & ~3);
	trap_get_bytes(ctx, buffer.data(), data, (len + 3) & ~3);
	if (!memcmp(buffer.data(), "FORM", 4) && !memcmp(buffer.data() + 8, "FTXT", 4)) {
		from_iff_text(buffer.data(), len);
	}
}

static void clipboard_read_host(TrapContext *, bool keyboardinject, bool initial)
{
	if (clip_disabled || (!keyboardinject && to_amiga_phase)) {
		return;
	}

	std::string text;
	if (!read_host_clipboard_text(&text, initial ? CLIP_SIZE_LIMIT_INIT : CLIP_SIZE_LIMIT)) {
		return;
	}

	if (keyboardinject) {
		normalize_text_for_keybuf(&text);
		if (!text.empty()) {
			keybuf_inject(text.c_str());
		}
		return;
	}

	if (last_host_clipboard_valid && text == last_host_clipboard) {
		return;
	}
	last_host_clipboard = text;
	last_host_clipboard_valid = true;
	queue_host_text_to_amiga(text);
}

static uae_u32 to_amiga_start_cb(TrapContext *ctx, void *)
{
	if (!clipboard_data || to_amiga.empty() || trap_get_long(ctx, clipboard_data) != 0) {
		return 0;
	}
	trap_put_long(ctx, clipboard_data, (uae_u32)to_amiga.size());
	uae_Signal(trap_get_long(ctx, clipboard_data + 8), 1 << 13);
	to_amiga_phase = 2;
	return 1;
}

static uae_u32 clipboard_vsync_cb(TrapContext *ctx, void *)
{
	if (clipboard_data) {
		uaecptr task = trap_get_long(ctx, clipboard_data + 8);
		if (task && native2amiga_isfree()) {
			uae_Signal(task, 1 << 13);
		}
	}
	return 0;
}

void clipboard_vsync(void)
{
	if (!filesys_heartbeat() || !clipboard_data || !initialized) {
		return;
	}

	if (signaling) {
		vdelay--;
		if (vdelay <= 0) {
			trap_callback(clipboard_vsync_cb, NULL);
			vdelay = 50;
		}
	}

	if (vdelay2 > 0) {
		vdelay2--;
	}

	if (to_amiga_phase == 1 && vdelay2 <= 0) {
		trap_callback(to_amiga_start_cb, NULL);
	}

	if (host_poll_delay > 0) {
		host_poll_delay--;
	} else {
		host_poll_delay = 100;
		clipboard_read_host(NULL, false, false);
	}
}

void clipboard_host_changed(void)
{
	host_poll_delay = 0;
}

void clipboard_unsafeperiod(void)
{
	vdelay2 = 100;
	if (vdelay < 60) {
		vdelay = 60;
	}
}

void clipboard_disable(bool disabled)
{
	clip_disabled = disabled;
}

uaecptr amiga_clipboard_proc_start(TrapContext *)
{
	signaling = 1;
	to_amiga_start();
	return clipboard_data;
}

void amiga_clipboard_task_start(TrapContext *, uaecptr data)
{
	clipboard_data = data;
	signaling = 1;
	write_log(_T("clipboard task init: %08x\n"), clipboard_data);
	to_amiga_start();
}

int amiga_clipboard_want_data(TrapContext *ctx)
{
	if (!clipboard_data) {
		return 0;
	}

	uae_u32 addr = trap_get_long(ctx, clipboard_data + 4);
	uae_u32 size = trap_get_long(ctx, clipboard_data);

	if (!initialized || to_amiga.empty()) {
		to_amiga.clear();
		to_amiga_phase = 0;
		return 0;
	}
	if (size != to_amiga.size()) {
		write_log(_T("clipboard: size %d <> %d mismatch\n"), size, (int)to_amiga.size());
		to_amiga.clear();
		to_amiga_phase = 0;
		return 0;
	}
	if (addr && size) {
		trap_put_bytes(ctx, to_amiga.data(), addr, size);
	}
	to_amiga.clear();
	to_amiga_phase = 0;
	return 1;
}

void amiga_clipboard_got_data(TrapContext *ctx, uaecptr data, uae_u32, uae_u32 actual)
{
	if (!initialized) {
		return;
	}
	from_iff(ctx, data, actual);
}

void amiga_clipboard_die(TrapContext *)
{
	signaling = 0;
	write_log(_T("clipboard not initialized\n"));
}

void amiga_clipboard_init(TrapContext *ctx)
{
	signaling = 0;
	initialized = 1;
	host_poll_delay = 50;
	write_log(_T("clipboard initialized\n"));
	clipboard_read_host(ctx, false, true);
}

void target_paste_to_keyboard(void)
{
	clipboard_read_host(NULL, true, false);
}
