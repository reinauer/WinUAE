#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "traps.h"
#include "memory.h"
#include "autoconf.h"
#include "audio.h"
#include "clipboard.h"
#include "cpuboard.h"
#include "debug.h"
#include "filesys.h"
#include "fsdb.h"
#include "gfxboard.h"
#include "inputdevice.h"
#include "keyboard.h"
#include "keybuf.h"
#include "rommgr.h"
#include "savestate.h"
#include "sampler.h"
#include "scsidev.h"
#include "statusline.h"
#include "ethernet.h"
#include "uae/string.h"
#include "videograb.h"
#include "zfile.h"
#include "zarchive.h"

int consoleopen;
int log_scsi;
int log_net;
int log_vsync;
int debug_vsync_min_delay;
int debug_vsync_forced_delay;
int uaelib_debug;
int pissoff_value = 15000 * CYCLE_UNIT;
int multithread_enabled = 1;
int seriallog;
int p96syncrate = 312;
int p96refresh_active;
int max_uae_width = 8192;
int max_uae_height = 8192;
int pissoff_nojit_value = 160 * CYCLE_UNIT;
volatile int bsd_int_requested;

static addrbank unix_gfxmem_banks[MAX_RTG_BOARDS];
addrbank *gfxmem_banks[MAX_RTG_BOARDS] = {
    &unix_gfxmem_banks[0],
    &unix_gfxmem_banks[1],
    &unix_gfxmem_banks[2],
    &unix_gfxmem_banks[3]
};

void machdep_free(void) {}
void protect_roms(bool) {}
void debugger_change(int) {}
void clipboard_vsync(void) {}
void clipboard_unsafeperiod(void) {}
void pausevideograb(int) {}
bool getpausevideograb(void) { return false; }
uae_s64 getsetpositionvideograb(uae_s64) { return -1; }
int sampler_init(void) { return 0; }
void sampler_free(void) {}
void sampler_vsync(void) {}
uae_u8 sampler_getsample(int) { return 0; }
float sampler_evtime;
int audio_is_pull(void) { return 0; }
bool audio_is_pull_event(void) { return false; }
int audio_pull_buffer(void) { return 0; }
bool audio_finish_pull(void) { return false; }
void save_log_open(void) {}
void update_debug_info(void) {}
void statusline_updated(int) {}
void bsdsock_fake_int_handler(void) { bsd_int_requested = 0; }

uae_u8 *save_log(int, size_t *len)
{
    if (len) {
        *len = 0;
    }
    return NULL;
}

uae_u8 *save_screenshot(int, size_t *len)
{
    if (len) {
        *len = 0;
    }
    return NULL;
}

uae_u8 *save_p96(size_t *len, uae_u8 *)
{
    if (len) {
        *len = 0;
    }
    return NULL;
}

uae_u8 *restore_p96(uae_u8 *src) { return src; }
void restore_p96_finish(void) {}
void picasso96_alloc(TrapContext *) {}
uae_u32 picasso_demux(uae_u32, TrapContext *) { return 0; }
void picasso_handle_vsync(void) {}
void uaegfx_install_code(uaecptr) {}
bool gfxboard_set(int, bool) { return false; }
void gfxboard_refresh(int) {}
void gfxboard_reset_init(void) {}
int gfxboard_get_configtype(struct rtgboardconfig *) { return 0; }
int gfxboard_get_vram_min(struct rtgboardconfig *) { return 0; }
int gfxboard_get_vram_max(struct rtgboardconfig *) { return 0; }
uae_u32 gfxboard_get_romtype(struct rtgboardconfig *) { return 0; }
const TCHAR *gfxboard_get_configname(int) { return _T("none"); }
int gfxboard_get_devnum(struct uae_prefs *, int index) { return index; }

void ldp_render(const char *, int, uae_u8 *, struct vidbuffer *, int, int, int, int)
{
}

bool cpuboard_autoconfig_init(struct autoconfig_info *) { return false; }
bool cpuboard_maprom(void) { return false; }
void cpuboard_map(void) {}
void cpuboard_reset(int) {}
void cpuboard_rethink(void) {}
void cpuboard_cleanup(void) {}
void cpuboard_init(void) {}
void cpuboard_clear(void) {}
int cpuboard_memorytype(struct uae_prefs *) { return 0; }
int cpuboard_maxmemory(struct uae_prefs *) { return 0; }
bool cpuboard_32bit(struct uae_prefs *) { return false; }
bool cpuboard_io_special(int, uae_u32 *, int, bool) { return false; }
void cpuboard_overlay_override(void) {}
uaecptr cpuboard_get_reset_pc(uaecptr *) { return 0; }
void cpuboard_set_flash_unlocked(bool) {}
bool cpuboard_forced_hardreset(void) { return false; }
bool cpuboard_fc_check(uaecptr, uae_u32 *, int, bool) { return false; }
void cpuboard_gvpmaprom(int) {}
void unprotect_maprom(void) {}

void keymcu_reset(void) {}
void keymcu_init(void) {}
void keymcu_free(void) {}
bool keymcu_run(bool) { return false; }
void keymcu2_reset(void) {}
void keymcu2_init(void) {}
void keymcu2_free(void) {}
bool keymcu2_run(bool) { return false; }
void keymcu3_reset(void) {}
void keymcu3_init(void) {}
void keymcu3_free(void) {}
bool keymcu3_run(bool) { return false; }
uae_u8 *save_kbmcu(size_t *len, uae_u8 *dst) { if (len) *len = 0; return dst; }
uae_u8 *restore_kbmcu(uae_u8 *src) { return src; }
uae_u8 *save_kbmcu2(size_t *len, uae_u8 *dst) { if (len) *len = 0; return dst; }
uae_u8 *restore_kbmcu2(uae_u8 *src) { return src; }
uae_u8 *save_kbmcu3(size_t *len, uae_u8 *dst) { if (len) *len = 0; return dst; }
uae_u8 *restore_kbmcu3(uae_u8 *src) { return src; }

struct netdriverdata **target_ethernet_enumerate(void)
{
    static netdriverdata *none[1] = { NULL };
    return none;
}

void ethernet_pause(int) {}
void ethernet_reset(void) {}

bool ariadne2_init(struct autoconfig_info *) { return false; }
bool hydra_init(struct autoconfig_info *) { return false; }
bool lanrover_init(struct autoconfig_info *) { return false; }
bool xsurf_init(struct autoconfig_info *) { return false; }
bool xsurf100_init(struct autoconfig_info *) { return false; }

uaecptr amiga_clipboard_proc_start(TrapContext *) { return 0; }
void amiga_clipboard_task_start(TrapContext *, uaecptr) {}
int amiga_clipboard_want_data(TrapContext *) { return 0; }
void amiga_clipboard_got_data(TrapContext *, uaecptr, uae_u32, uae_u32) {}
void amiga_clipboard_die(TrapContext *) {}
void amiga_clipboard_init(TrapContext *) {}
void target_paste_to_keyboard(void) {}

struct zvolume *archive_directory_plain(struct zfile *) { return NULL; }
struct zvolume *archive_directory_lha(struct zfile *) { return NULL; }
struct zvolume *archive_directory_zip(struct zfile *) { return NULL; }
struct zvolume *archive_directory_7z(struct zfile *) { return NULL; }
struct zfile *archive_access_7z(struct znode *) { return NULL; }
struct zvolume *archive_directory_rar(struct zfile *) { return NULL; }
struct zfile *archive_access_rar(struct znode *) { return NULL; }
struct zvolume *archive_directory_lzx(struct zfile *) { return NULL; }
struct zfile *archive_access_lzx(struct znode *) { return NULL; }
struct zvolume *archive_directory_arcacc(struct zfile *, unsigned int) { return NULL; }
struct zvolume *archive_directory_adf(struct znode *, struct zfile *) { return NULL; }
struct zvolume *archive_directory_rdb(struct zfile *) { return NULL; }
struct zvolume *archive_directory_fat(struct zfile *) { return NULL; }
struct zvolume *archive_directory_tar(struct zfile *) { return NULL; }
struct zfile *archive_access_select(struct znode *, struct zfile *, unsigned int, int, int *retcode, int)
{
    if (retcode) {
        *retcode = 0;
    }
    return NULL;
}
void archive_access_scan(struct zfile *, zfile_callback, void *, unsigned int) {}
void archive_access_close(void *, unsigned int) {}
struct zfile *archive_unpackzfile(struct zfile *zf) { return zf; }
struct zfile *archive_access_lha(struct znode *) { return NULL; }
struct zfile *archive_getzfile(struct znode *, unsigned int, int) { return NULL; }
int isfat(uae_u8 *) { return 0; }

int scsi_do_disk_change(int, int, int *pollmode) { if (pollmode) *pollmode = 0; return 0; }
uae_u32 scsi_get_cd_drive_mask(void) { return 0; }
uae_u32 scsi_get_cd_drive_media_mask(void) { return 0; }
int scsi_add_tape(struct uaedev_config_info *) { return -1; }
uae_u8 *save_scsidev(int, size_t *len, uae_u8 *dst) { if (len) *len = 0; return dst; }
uae_u8 *restore_scsidev(uae_u8 *src) { return src; }

a_inode *custom_fsdb_lookup_aino_aname(a_inode *, const TCHAR *) { return NULL; }
a_inode *custom_fsdb_lookup_aino_nname(a_inode *, const TCHAR *) { return NULL; }
int custom_fsdb_used_as_nname(a_inode *, const TCHAR *) { return 0; }

bool gui_ask_disk(int, TCHAR *) { return false; }

void filesys_addexternals(void) {}
int target_get_volume_name(struct uaedev_mount_info *, struct uaedev_config_info *, bool, bool, int) { return 0; }
uae_u8 *target_load_keyfile(struct uae_prefs *, const TCHAR *, int *size, TCHAR *)
{
    if (size) {
        *size = 0;
    }
    return NULL;
}
int target_get_display(const TCHAR *) { return 0; }
const TCHAR *target_get_display_name(int, bool) { return _T("Unix display"); }
int check_prefs_changed_gfx(void) { return 0; }
uae_u32 emulib_target_getcpurate(uae_u32, uae_u32 *low)
{
    if (low) {
        *low = 0;
    }
    return 0;
}
void setcapslockstate(int) {}
int target_checkcapslock(int, int *) { return 0; }
int is_touch_lightpen(void) { return 0; }
