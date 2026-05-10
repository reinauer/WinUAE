#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "memory.h"
#include "traps.h"
#include "picasso96.h"
#include "gfxboard.h"

static uae_u32 REGPARAM3 gfxmem_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM3 gfxmem_wget(uaecptr) REGPARAM;
static uae_u32 REGPARAM3 gfxmem_bget(uaecptr) REGPARAM;
static void REGPARAM3 gfxmem_lput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 gfxmem_wput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 gfxmem_bput(uaecptr, uae_u32) REGPARAM;
static int REGPARAM3 gfxmem_check(uaecptr, uae_u32) REGPARAM;
static uae_u8 *REGPARAM3 gfxmem_xlate(uaecptr) REGPARAM;

static uae_u32 REGPARAM3 gfxmem2_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM3 gfxmem2_wget(uaecptr) REGPARAM;
static uae_u32 REGPARAM3 gfxmem2_bget(uaecptr) REGPARAM;
static void REGPARAM3 gfxmem2_lput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 gfxmem2_wput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 gfxmem2_bput(uaecptr, uae_u32) REGPARAM;
static int REGPARAM3 gfxmem2_check(uaecptr, uae_u32) REGPARAM;
static uae_u8 *REGPARAM3 gfxmem2_xlate(uaecptr) REGPARAM;

static uae_u32 REGPARAM3 gfxmem3_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM3 gfxmem3_wget(uaecptr) REGPARAM;
static uae_u32 REGPARAM3 gfxmem3_bget(uaecptr) REGPARAM;
static void REGPARAM3 gfxmem3_lput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 gfxmem3_wput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 gfxmem3_bput(uaecptr, uae_u32) REGPARAM;
static int REGPARAM3 gfxmem3_check(uaecptr, uae_u32) REGPARAM;
static uae_u8 *REGPARAM3 gfxmem3_xlate(uaecptr) REGPARAM;

static uae_u32 REGPARAM3 gfxmem4_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM3 gfxmem4_wget(uaecptr) REGPARAM;
static uae_u32 REGPARAM3 gfxmem4_bget(uaecptr) REGPARAM;
static void REGPARAM3 gfxmem4_lput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 gfxmem4_wput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 gfxmem4_bput(uaecptr, uae_u32) REGPARAM;
static int REGPARAM3 gfxmem4_check(uaecptr, uae_u32) REGPARAM;
static uae_u8 *REGPARAM3 gfxmem4_xlate(uaecptr) REGPARAM;

#define UNIX_RTG_MEMORY_FUNCTIONS(prefix, bank) \
static uae_u32 REGPARAM2 prefix ## _lget(uaecptr addr) \
{ \
    addr &= bank.mask; \
    if (!bank.baseaddr || addr + 4 > bank.allocated_size) { \
        return 0; \
    } \
    return do_get_mem_long((uae_u32 *)(bank.baseaddr + addr)); \
} \
static uae_u32 REGPARAM2 prefix ## _wget(uaecptr addr) \
{ \
    addr &= bank.mask; \
    if (!bank.baseaddr || addr + 2 > bank.allocated_size) { \
        return 0; \
    } \
    return do_get_mem_word((uae_u16 *)(bank.baseaddr + addr)); \
} \
static uae_u32 REGPARAM2 prefix ## _bget(uaecptr addr) \
{ \
    addr &= bank.mask; \
    if (!bank.baseaddr || addr >= bank.allocated_size) { \
        return 0; \
    } \
    return bank.baseaddr[addr]; \
} \
static void REGPARAM2 prefix ## _lput(uaecptr addr, uae_u32 value) \
{ \
    addr &= bank.mask; \
    if (!bank.baseaddr || addr + 4 > bank.allocated_size) { \
        return; \
    } \
    do_put_mem_long((uae_u32 *)(bank.baseaddr + addr), value); \
} \
static void REGPARAM2 prefix ## _wput(uaecptr addr, uae_u32 value) \
{ \
    addr &= bank.mask; \
    if (!bank.baseaddr || addr + 2 > bank.allocated_size) { \
        return; \
    } \
    do_put_mem_word((uae_u16 *)(bank.baseaddr + addr), value); \
} \
static void REGPARAM2 prefix ## _bput(uaecptr addr, uae_u32 value) \
{ \
    addr &= bank.mask; \
    if (!bank.baseaddr || addr >= bank.allocated_size) { \
        return; \
    } \
    bank.baseaddr[addr] = (uae_u8)value; \
} \
static int REGPARAM2 prefix ## _check(uaecptr addr, uae_u32 size) \
{ \
    addr &= bank.mask; \
    return bank.baseaddr && addr + size <= bank.allocated_size; \
} \
static uae_u8 *REGPARAM2 prefix ## _xlate(uaecptr addr) \
{ \
    addr &= bank.mask; \
    if (!bank.baseaddr || addr >= bank.allocated_size) { \
        return NULL; \
    } \
    return bank.baseaddr + addr; \
}

// RTG RAM is allocated before autoconfig chooses the real Zorro address.
// mapped_malloc_dynamic() requires a non-zero start for its temporary label;
// the address is overwritten when expansion autoconfig maps the board.
addrbank gfxmem_bank = {
    gfxmem_lget, gfxmem_wget, gfxmem_bget,
    gfxmem_lput, gfxmem_wput, gfxmem_bput,
    gfxmem_xlate, gfxmem_check, NULL, NULL, _T("RTG RAM"),
    gfxmem_lget, gfxmem_wget,
    ABFLAG_RAM | ABFLAG_RTG, S_READ, S_WRITE,
    NULL, 0, 0, 1
};

addrbank gfxmem2_bank = {
    gfxmem2_lget, gfxmem2_wget, gfxmem2_bget,
    gfxmem2_lput, gfxmem2_wput, gfxmem2_bput,
    gfxmem2_xlate, gfxmem2_check, NULL, NULL, _T("RTG RAM #2"),
    gfxmem2_lget, gfxmem2_wget,
    ABFLAG_RAM | ABFLAG_RTG, S_READ, S_WRITE,
    NULL, 0, 0, 1
};

addrbank gfxmem3_bank = {
    gfxmem3_lget, gfxmem3_wget, gfxmem3_bget,
    gfxmem3_lput, gfxmem3_wput, gfxmem3_bput,
    gfxmem3_xlate, gfxmem3_check, NULL, NULL, _T("RTG RAM #3"),
    gfxmem3_lget, gfxmem3_wget,
    ABFLAG_RAM | ABFLAG_RTG, S_READ, S_WRITE,
    NULL, 0, 0, 1
};

addrbank gfxmem4_bank = {
    gfxmem4_lget, gfxmem4_wget, gfxmem4_bget,
    gfxmem4_lput, gfxmem4_wput, gfxmem4_bput,
    gfxmem4_xlate, gfxmem4_check, NULL, NULL, _T("RTG RAM #4"),
    gfxmem4_lget, gfxmem4_wget,
    ABFLAG_RAM | ABFLAG_RTG, S_READ, S_WRITE,
    NULL, 0, 0, 1
};

UNIX_RTG_MEMORY_FUNCTIONS(gfxmem, gfxmem_bank)
UNIX_RTG_MEMORY_FUNCTIONS(gfxmem2, gfxmem2_bank)
UNIX_RTG_MEMORY_FUNCTIONS(gfxmem3, gfxmem3_bank)
UNIX_RTG_MEMORY_FUNCTIONS(gfxmem4, gfxmem4_bank)

addrbank *gfxmem_banks[MAX_RTG_BOARDS] = {
    &gfxmem_bank,
    &gfxmem2_bank,
    &gfxmem3_bank,
    &gfxmem4_bank
};

struct unix_rtg_board {
    int id;
    const TCHAR *name;
    const TCHAR *manufacturer;
    const TCHAR *config_name;
    int config_type;
};

static const unix_rtg_board unix_rtg_boards[] = {
    { GFXBOARD_UAE_Z2, _T("UAE [Zorro II]"), NULL, _T("ZorroII"), 2 },
    { GFXBOARD_UAE_Z3, _T("UAE [Zorro III]"), NULL, _T("ZorroIII"), 3 },
    { -1, NULL, NULL, NULL, 0 }
};

static const unix_rtg_board *unix_rtg_find_board(int id)
{
    for (int i = 0; unix_rtg_boards[i].name; i++) {
        if (unix_rtg_boards[i].id == id) {
            return &unix_rtg_boards[i];
        }
    }
    return NULL;
}

void picasso96_alloc(TrapContext *) {}
uae_u32 picasso_demux(uae_u32, TrapContext *) { return 0; }
void picasso_handle_vsync(void) {}
void uaegfx_install_code(uaecptr) {}

bool gfxboard_set(int, bool)
{
    return false;
}

void gfxboard_refresh(int) {}
void gfxboard_reset_init(void) {}

int gfxboard_get_configtype(struct rtgboardconfig *rbc)
{
    const unix_rtg_board *board = rbc ? unix_rtg_find_board(rbc->rtgmem_type) : NULL;
    return board ? board->config_type : 0;
}

int gfxboard_get_vram_min(struct rtgboardconfig *)
{
    return -1;
}

int gfxboard_get_vram_max(struct rtgboardconfig *)
{
    return -1;
}

uae_u32 gfxboard_get_romtype(struct rtgboardconfig *)
{
    return 0;
}

const TCHAR *gfxboard_get_name(int id)
{
    const unix_rtg_board *board = unix_rtg_find_board(id);
    return board ? board->name : NULL;
}

const TCHAR *gfxboard_get_manufacturername(int id)
{
    const unix_rtg_board *board = unix_rtg_find_board(id);
    return board ? board->manufacturer : NULL;
}

const TCHAR *gfxboard_get_configname(int id)
{
    const unix_rtg_board *board = unix_rtg_find_board(id);
    return board ? board->config_name : NULL;
}

int gfxboard_get_index_from_id(int id)
{
    return unix_rtg_find_board(id) ? id : -1;
}

int gfxboard_get_id_from_index(int index)
{
    return unix_rtg_find_board(index) ? index : -1;
}

struct gfxboard_func *gfxboard_get_func(struct rtgboardconfig *)
{
    return NULL;
}

bool gfxboard_get_switcher(struct rtgboardconfig *rbc)
{
    return rbc && unix_rtg_find_board(rbc->rtgmem_type);
}

bool gfxboard_need_byteswap(struct rtgboardconfig *)
{
    return false;
}

int gfxboard_get_autoconfig_size(struct rtgboardconfig *)
{
    return -1;
}

int gfxboard_is_registers(struct rtgboardconfig *)
{
    return 0;
}

int gfxboard_num_boards(struct rtgboardconfig *)
{
    return 1;
}

int gfxboard_get_devnum(struct uae_prefs *, int index)
{
    return index;
}
