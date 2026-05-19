#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "memory.h"
#include "traps.h"
#include "autoconf.h"
#include "picasso96.h"
#include "gfxboard.h"
#include "xwin.h"

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

enum {
    UNIX_PICASSO_STATE_SETDISPLAY = 1,
    UNIX_PICASSO_STATE_SETPANNING = 2,
    UNIX_PICASSO_STATE_SETGC = 4,
    UNIX_PICASSO_STATE_SETDAC = 8,
    UNIX_PICASSO_STATE_SETSWITCH = 16
};

#define UNIX_RTG_DEFAULT_MODEFLAGS (RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_B8G8R8A8)
#define UNIX_UAEGFX_VERSION 3
#define UNIX_UAEGFX_REVISION 4

#define UNIX_LIB_SIZE 34
#define UNIX_CARD_FLAGS UNIX_LIB_SIZE
#define UNIX_CARD_EXECBASE (UNIX_CARD_FLAGS + 2)
#define UNIX_CARD_EXPANSIONBASE (UNIX_CARD_EXECBASE + 4)
#define UNIX_CARD_SEGMENTLIST (UNIX_CARD_EXPANSIONBASE + 4)
#define UNIX_CARD_NAME (UNIX_CARD_SEGMENTLIST + 4)
#define UNIX_CARD_RESLIST (UNIX_CARD_NAME + 4)
#define UNIX_CARD_RESLISTSIZE (UNIX_CARD_RESLIST + 4)
#define UNIX_CARD_BOARDINFO (UNIX_CARD_RESLISTSIZE + 4)
#define UNIX_CARD_SIZEOF (UNIX_CARD_BOARDINFO + 4)

#define UNIX_BIB_GRANTDIRECTACCESS 26
#define UNIX_BIF_GRANTDIRECTACCESS (1 << UNIX_BIB_GRANTDIRECTACCESS)
#define UNIX_BIB_DACSWITCH 28
#define UNIX_BIF_DACSWITCH (1 << UNIX_BIB_DACSWITCH)

#define UNIX_PSSO_BoardInfo_FreeCardMem (PSSO_BoardInfo_AllocCardMem + 4)
#define UNIX_PSSO_BoardInfo_SetSwitch (UNIX_PSSO_BoardInfo_FreeCardMem + 4)
#define UNIX_PSSO_BoardInfo_SetColorArray (UNIX_PSSO_BoardInfo_SetSwitch + 4)
#define UNIX_PSSO_BoardInfo_SetDAC (UNIX_PSSO_BoardInfo_SetColorArray + 4)
#define UNIX_PSSO_BoardInfo_SetGC (UNIX_PSSO_BoardInfo_SetDAC + 4)
#define UNIX_PSSO_BoardInfo_SetPanning (UNIX_PSSO_BoardInfo_SetGC + 4)
#define UNIX_PSSO_BoardInfo_CalculateBytesPerRow (UNIX_PSSO_BoardInfo_SetPanning + 4)
#define UNIX_PSSO_BoardInfo_CalculateMemory (UNIX_PSSO_BoardInfo_CalculateBytesPerRow + 4)
#define UNIX_PSSO_BoardInfo_GetCompatibleFormats (UNIX_PSSO_BoardInfo_CalculateMemory + 4)
#define UNIX_PSSO_BoardInfo_SetDisplay (UNIX_PSSO_BoardInfo_GetCompatibleFormats + 4)
#define UNIX_PSSO_BoardInfo_ResolvePixelClock (UNIX_PSSO_BoardInfo_SetDisplay + 4)
#define UNIX_PSSO_BoardInfo_GetPixelClock (UNIX_PSSO_BoardInfo_ResolvePixelClock + 4)
#define UNIX_PSSO_BoardInfo_SetClock (UNIX_PSSO_BoardInfo_GetPixelClock + 4)
#define UNIX_PSSO_BoardInfo_SetMemoryMode (UNIX_PSSO_BoardInfo_SetClock + 4)
#define UNIX_PSSO_BoardInfo_SetWriteMask (UNIX_PSSO_BoardInfo_SetMemoryMode + 4)
#define UNIX_PSSO_BoardInfo_SetClearMask (UNIX_PSSO_BoardInfo_SetWriteMask + 4)
#define UNIX_PSSO_BoardInfo_SetReadPlane (UNIX_PSSO_BoardInfo_SetClearMask + 4)
#define UNIX_PSSO_BoardInfo_WaitVerticalSync (UNIX_PSSO_BoardInfo_SetReadPlane + 4)
#define UNIX_PSSO_BoardInfo_SetInterrupt (UNIX_PSSO_BoardInfo_WaitVerticalSync + 4)
#define UNIX_PSSO_BoardInfo_WaitBlitter (UNIX_PSSO_BoardInfo_SetInterrupt + 4)
#define UNIX_PSSO_BoardInfo_ScrollPlanar (UNIX_PSSO_BoardInfo_WaitBlitter + 4)
#define UNIX_PSSO_BoardInfo_ScrollPlanarDefault (UNIX_PSSO_BoardInfo_ScrollPlanar + 4)
#define UNIX_PSSO_BoardInfo_UpdatePlanar (UNIX_PSSO_BoardInfo_ScrollPlanarDefault + 4)
#define UNIX_PSSO_BoardInfo_UpdatePlanarDefault (UNIX_PSSO_BoardInfo_UpdatePlanar + 4)
#define UNIX_PSSO_BoardInfo_BlitPlanar2Chunky (UNIX_PSSO_BoardInfo_UpdatePlanarDefault + 4)
#define UNIX_PSSO_BoardInfo_BlitPlanar2ChunkyDefault (UNIX_PSSO_BoardInfo_BlitPlanar2Chunky + 4)
#define UNIX_PSSO_BoardInfo_FillRect (UNIX_PSSO_BoardInfo_BlitPlanar2ChunkyDefault + 4)
#define UNIX_PSSO_BoardInfo_FillRectDefault (UNIX_PSSO_BoardInfo_FillRect + 4)
#define UNIX_PSSO_BoardInfo_InvertRect (UNIX_PSSO_BoardInfo_FillRectDefault + 4)
#define UNIX_PSSO_BoardInfo_InvertRectDefault (UNIX_PSSO_BoardInfo_InvertRect + 4)
#define UNIX_PSSO_BoardInfo_BlitRect (UNIX_PSSO_BoardInfo_InvertRectDefault + 4)
#define UNIX_PSSO_BoardInfo_BlitRectDefault (UNIX_PSSO_BoardInfo_BlitRect + 4)
#define UNIX_PSSO_BoardInfo_BlitTemplate (UNIX_PSSO_BoardInfo_BlitRectDefault + 4)
#define UNIX_PSSO_BoardInfo_BlitTemplateDefault (UNIX_PSSO_BoardInfo_BlitTemplate + 4)
#define UNIX_PSSO_BoardInfo_BlitPattern (UNIX_PSSO_BoardInfo_BlitTemplateDefault + 4)
#define UNIX_PSSO_BoardInfo_BlitPatternDefault (UNIX_PSSO_BoardInfo_BlitPattern + 4)
#define UNIX_PSSO_BoardInfo_DrawLine (UNIX_PSSO_BoardInfo_BlitPatternDefault + 4)
#define UNIX_PSSO_BoardInfo_DrawLineDefault (UNIX_PSSO_BoardInfo_DrawLine + 4)
#define UNIX_PSSO_BoardInfo_BlitRectNoMaskComplete (UNIX_PSSO_BoardInfo_DrawLineDefault + 4)
#define UNIX_PSSO_BoardInfo_BlitRectNoMaskCompleteDefault (UNIX_PSSO_BoardInfo_BlitRectNoMaskComplete + 4)
#define UNIX_PSSO_BoardInfo_BlitPlanar2Direct (UNIX_PSSO_BoardInfo_BlitRectNoMaskCompleteDefault + 4)
#define UNIX_PSSO_BoardInfo_BlitPlanar2DirectDefault (UNIX_PSSO_BoardInfo_BlitPlanar2Direct + 4)
#define UNIX_PSSO_BoardInfo_Reserved0 (UNIX_PSSO_BoardInfo_BlitPlanar2DirectDefault + 4)
#define UNIX_PSSO_BoardInfo_Reserved0Default (UNIX_PSSO_BoardInfo_Reserved0 + 4)
#define UNIX_PSSO_BoardInfo_Reserved1 (UNIX_PSSO_BoardInfo_Reserved0Default + 4)
#define UNIX_PSSO_SetSplitPosition (UNIX_PSSO_BoardInfo_Reserved1 + 4)
#define UNIX_PSSO_ReInitMemory (UNIX_PSSO_SetSplitPosition + 4)
#define UNIX_PSSO_BoardInfo_GetCompatibleDACFormats (UNIX_PSSO_ReInitMemory + 4)
#define UNIX_PSSO_BoardInfo_CoerceMode (UNIX_PSSO_BoardInfo_GetCompatibleDACFormats + 4)

static const int unix_rtg_mode_sizes[][2] = {
    { 320, 240 },
    { 320, 256 },
    { 640, 480 },
    { 640, 512 },
    { 800, 600 },
    { 1024, 768 },
    { 1280, 720 },
    { 1280, 1024 },
    { 0, 0 }
};

static uaecptr unix_picasso_amem;
static uaecptr unix_picasso_amemend;
static uaecptr unix_uaegfx_resname;
static uaecptr unix_uaegfx_prefix;
static uaecptr unix_uaegfx_resid;
static uaecptr unix_uaegfx_base;
static uaecptr unix_uaegfx_rom;
static uaecptr unix_picasso_boardinfo;
static int unix_uaegfx_old;
static int unix_uaegfx_active;
static uae_u32 unix_reserved_gfxmem;

static uae_u32 unix_rtg_modeflags(void)
{
    return currprefs.picasso96_modeflags ? currprefs.picasso96_modeflags : UNIX_RTG_DEFAULT_MODEFLAGS;
}

static int unix_picasso_bytes_per_pixel(uae_u32 rgbfmt)
{
    switch (rgbfmt) {
    case RGBFB_CLUT:
    case RGBFB_Y4U1V1:
        return 1;
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_B5G6R5PC:
    case RGBFB_B5G5R5PC:
    case RGBFB_Y4U2V2:
        return 2;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        return 3;
    case RGBFB_A8R8G8B8:
    case RGBFB_A8B8G8R8:
    case RGBFB_R8G8B8A8:
    case RGBFB_B8G8R8A8:
        return 4;
    }
    return 0;
}

static uae_u32 unix_picasso_rgbmask_for_format(uae_u32 rgbfmt)
{
    switch (rgbfmt) {
    case RGBFB_CLUT:
        return RGBMASK_8BIT;
    case RGBFB_R5G5B5PC:
    case RGBFB_R5G5B5:
    case RGBFB_B5G5R5PC:
        return RGBMASK_15BIT;
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G6B5:
    case RGBFB_B5G6R5PC:
        return RGBMASK_16BIT;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        return RGBMASK_24BIT;
    case RGBFB_A8R8G8B8:
    case RGBFB_A8B8G8R8:
    case RGBFB_R8G8B8A8:
    case RGBFB_B8G8R8A8:
        return RGBMASK_32BIT;
    }
    return 0;
}

static bool unix_picasso_renderinfo(TrapContext *ctx, uaecptr renderinfo, RenderInfo *ri)
{
    if (!ri || !trap_valid_address(ctx, renderinfo, PSSO_RenderInfo_sizeof)) {
        write_log(_T("Unix RTG invalid RenderInfo: %08X\n"), renderinfo);
        return false;
    }

    uaecptr mem = trap_get_long(ctx, renderinfo + PSSO_RenderInfo_Memory);
    int bytes_per_row = (uae_s16)trap_get_word(ctx, renderinfo + PSSO_RenderInfo_BytesPerRow);
    RGBFTYPE rgbfmt = (RGBFTYPE)trap_get_long(ctx, renderinfo + PSSO_RenderInfo_RGBFormat);

    if (bytes_per_row < 0 || !trap_valid_address(ctx, mem, bytes_per_row > 0 ? bytes_per_row : 1)) {
        write_log(_T("Unix RTG invalid RenderInfo memory: %08X bpr=%d fmt=%d\n"),
            mem, bytes_per_row, rgbfmt);
        return false;
    }

    ri->AMemory = mem;
    ri->Memory = get_real_address(mem);
    ri->BytesPerRow = bytes_per_row;
    ri->RGBFormat = rgbfmt;
    return ri->Memory != NULL;
}

static bool unix_picasso_validate_rect(RenderInfo *ri, uae_u32 rgbfmt,
    uae_u32 *x, uae_u32 *y, uae_u32 *width, uae_u32 *height)
{
    if (!ri || !x || !y || !width || !height ||
        *x > 32767 || *y > 32767 || *width > 32767 || *height > 32767) {
        return false;
    }
    if (!*width || !*height) {
        return true;
    }

    int bytes_per_pixel = unix_picasso_bytes_per_pixel(rgbfmt);
    int bytes_per_row = ri->BytesPerRow;
    if (!bytes_per_pixel || bytes_per_row < 0) {
        return false;
    }
    if (!bytes_per_row) {
        if (*x) {
            return false;
        }
        bytes_per_row = *width * bytes_per_pixel;
    }
    if (*x * bytes_per_pixel >= (uae_u32)bytes_per_row) {
        return false;
    }

    uae_u32 x2 = *x + *width;
    if (x2 * bytes_per_pixel > (uae_u32)bytes_per_row) {
        x2 = bytes_per_row / bytes_per_pixel;
        *width = x2 - *x;
    }

    addrbank *bank = gfxmem_banks[0];
    if (!bank || !bank->baseaddr || ri->AMemory < bank->start || ri->AMemory >= bank->start + bank->allocated_size) {
        return false;
    }
    uaecptr end = ri->AMemory + (*y + *height - 1) * ri->BytesPerRow + (*x + *width - 1) * bytes_per_pixel;
    return end >= bank->start && end < bank->start + bank->allocated_size;
}

static void unix_picasso_store_pen(uae_u8 *dst, uae_u32 pen, int bytes_per_pixel)
{
    switch (bytes_per_pixel) {
    case 1:
        dst[0] = (uae_u8)pen;
        break;
    case 2:
        do_put_mem_word((uae_u16 *)dst, (uae_u16)pen);
        break;
    case 3:
        dst[0] = (uae_u8)(pen >> 16);
        dst[1] = (uae_u8)(pen >> 8);
        dst[2] = (uae_u8)pen;
        break;
    case 4:
        do_put_mem_long((uae_u32 *)dst, pen);
        break;
    }
}

static uae_u8 unix_picasso_blit_op(uae_u8 src, uae_u8 dst, BLIT_OPCODE op)
{
    switch (op) {
    case BLIT_FALSE:
        return 0;
    case BLIT_NOR:
        return (uae_u8)~(src | dst);
    case BLIT_ONLYDST:
        return (uae_u8)(dst & ~src);
    case BLIT_NOTSRC:
        return (uae_u8)~src;
    case BLIT_ONLYSRC:
        return (uae_u8)(src & ~dst);
    case BLIT_NOTDST:
        return (uae_u8)~dst;
    case BLIT_EOR:
        return src ^ dst;
    case BLIT_NAND:
        return (uae_u8)~(src & dst);
    case BLIT_AND:
        return src & dst;
    case BLIT_NEOR:
        return (uae_u8)~(src ^ dst);
    case BLIT_DST:
        return dst;
    case BLIT_NOTONLYSRC:
        return (uae_u8)(~src | dst);
    case BLIT_SRC:
        return src;
    case BLIT_NOTONLYDST:
        return (uae_u8)(~dst | src);
    case BLIT_OR:
        return src | dst;
    case BLIT_TRUE:
        return 0xff;
    default:
        return dst;
    }
}

static int unix_picasso_depth_supported(int depth)
{
    uae_u32 flags = unix_rtg_modeflags();

    if (depth == 8 && (flags & RGBFF_CLUT)) {
        return 1;
    }
    if (depth == 15 && (flags & (RGBFF_R5G5B5PC | RGBFF_R5G5B5 | RGBFF_B5G5R5PC))) {
        return 1;
    }
    if (depth == 16 && (flags & (RGBFF_R5G6B5PC | RGBFF_R5G6B5 | RGBFF_B5G6R5PC))) {
        return 1;
    }
    if (depth == 24 && (flags & (RGBFF_R8G8B8 | RGBFF_B8G8R8))) {
        return 1;
    }
    if (depth == 32 && (flags & (RGBFF_A8R8G8B8 | RGBFF_A8B8G8R8 | RGBFF_R8G8B8A8 | RGBFF_B8G8R8A8))) {
        return 1;
    }
    return 0;
}

static int unix_picasso_mode_depth_count(void)
{
    int depths = 0;
    depths += unix_picasso_depth_supported(8);
    depths += unix_picasso_depth_supported(15);
    depths += unix_picasso_depth_supported(16);
    depths += unix_picasso_depth_supported(24);
    depths += unix_picasso_depth_supported(32);
    return depths;
}

static int unix_picasso_resolution_count(void)
{
    int count = 0;
    for (int i = 0; unix_rtg_mode_sizes[i][0]; i++) {
        int width = unix_rtg_mode_sizes[i][0];
        int height = unix_rtg_mode_sizes[i][1];
        if ((uae_u32)width * (uae_u32)height <= gfxmem_bank.allocated_size - 256) {
            count++;
        }
    }
    return count;
}

static int unix_picasso_resolution_memory_size(void)
{
    if (currprefs.picasso96_noautomodes) {
        return 0;
    }
    return unix_picasso_resolution_count() *
        (PSSO_LibResolution_sizeof + unix_picasso_mode_depth_count() * PSSO_ModeInfo_sizeof);
}

static int unix_picasso_mode_id(int width, int height, int index)
{
    static const struct {
        int width;
        int height;
        int id;
    } mode_ids[] = {
        { 320, 240, 1 },
        { 640, 480, 3 },
        { 800, 600, 4 },
        { 1024, 768, 5 },
        { 1280, 1024, 7 },
        { 320, 256, 9 },
        { 640, 512, 10 },
        { 1280, 720, 142 },
        { 0, 0, 0 }
    };

    for (int i = 0; mode_ids[i].width; i++) {
        if (mode_ids[i].width == width && mode_ids[i].height == height) {
            return 0x50001000 | (mode_ids[i].id * 0x10000);
        }
    }
    return 0x51001000 - index * 0x10000;
}

static void unix_amiga_list_add_tail(TrapContext *ctx, uaecptr list, uaecptr node)
{
    trap_put_long(ctx, node + 0, list + 4);
    trap_put_long(ctx, node + 4, trap_get_long(ctx, list + 8));
    trap_put_long(ctx, trap_get_long(ctx, list + 8), node);
    trap_put_long(ctx, list + 8, node);
}

static void unix_copy_lib_resolution(TrapContext *ctx, const struct LibResolution *res, uaecptr ptr)
{
    trap_set_bytes(ctx, ptr, 0, PSSO_LibResolution_sizeof);
    for (int i = 0; i < 6 && res->P96ID[i]; i++) {
        trap_put_byte(ctx, ptr + PSSO_LibResolution_P96ID + i, res->P96ID[i]);
    }
    trap_put_long(ctx, ptr + PSSO_LibResolution_DisplayID, res->DisplayID);
    trap_put_word(ctx, ptr + PSSO_LibResolution_Width, res->Width);
    trap_put_word(ctx, ptr + PSSO_LibResolution_Height, res->Height);
    trap_put_word(ctx, ptr + PSSO_LibResolution_Flags, res->Flags);
    for (int i = 0; i < MAXMODES; i++) {
        trap_put_long(ctx, ptr + PSSO_LibResolution_Modes + i * 4, res->Modes[i]);
    }
    trap_put_long(ctx, ptr + 10, ptr + PSSO_LibResolution_P96ID);
    trap_put_long(ctx, ptr + PSSO_LibResolution_BoardInfo, res->BoardInfo);
}

static void unix_fill_mode_info(TrapContext *ctx, uaecptr ptr, struct LibResolution *res, int width, int height, int depth)
{
    switch (depth) {
    case 8:
        res->Modes[CHUNKY] = ptr;
        break;
    case 15:
    case 16:
        res->Modes[HICOLOR] = ptr;
        break;
    case 24:
        res->Modes[TRUECOLOR] = ptr;
        break;
    default:
        res->Modes[TRUEALPHA] = ptr;
        break;
    }

    trap_set_bytes(ctx, ptr, 0, PSSO_ModeInfo_sizeof);
    trap_put_word(ctx, ptr + PSSO_ModeInfo_Active, 1);
    trap_put_word(ctx, ptr + PSSO_ModeInfo_Width, width);
    trap_put_word(ctx, ptr + PSSO_ModeInfo_Height, height);
    trap_put_byte(ctx, ptr + PSSO_ModeInfo_Depth, depth);
    trap_put_word(ctx, ptr + PSSO_ModeInfo_HorTotal, width + 8);
    trap_put_word(ctx, ptr + PSSO_ModeInfo_HorBlankSize, 8);
    trap_put_word(ctx, ptr + PSSO_ModeInfo_HorSyncStart, 2);
    trap_put_word(ctx, ptr + PSSO_ModeInfo_HorSyncSize, 2);
    trap_put_word(ctx, ptr + PSSO_ModeInfo_VerTotal, height + 8);
    trap_put_word(ctx, ptr + PSSO_ModeInfo_VerBlankSize, 8);
    trap_put_word(ctx, ptr + PSSO_ModeInfo_VerSyncStart, 2);
    trap_put_word(ctx, ptr + PSSO_ModeInfo_VerSyncSize, 2);
    trap_put_byte(ctx, ptr + PSSO_ModeInfo_first_union, 98);
    trap_put_byte(ctx, ptr + PSSO_ModeInfo_second_union, 14);
    trap_put_long(ctx, ptr + PSSO_ModeInfo_PixelClock, width * height * 60);
}

static bool unix_add_mode(TrapContext *ctx, uaecptr board_info, uaecptr *amem, int width, int height, int index)
{
    static const int depths[] = { 8, 15, 16, 24, 32 };
    struct LibResolution res = { 0 };
    bool added = false;

    memcpy(res.P96ID, "P96-0:", 6);
    snprintf(res.Name, sizeof res.Name, "UAE:%4dx%4d", width, height);
    res.DisplayID = unix_picasso_mode_id(width, height, index);
    res.BoardInfo = board_info;
    res.Width = width;
    res.Height = height;
    res.Flags = P96F_PUBLIC;

    for (int i = 0; i < (int)(sizeof depths / sizeof depths[0]); i++) {
        int depth = depths[i];
        int bytes_per_pixel = (depth + 7) / 8;
        if (!unix_picasso_depth_supported(depth)) {
            continue;
        }
        if (gfxmem_bank.allocated_size < (uae_u32)width * (uae_u32)height * (uae_u32)bytes_per_pixel) {
            continue;
        }
        unix_fill_mode_info(ctx, *amem, &res, width, height, depth);
        *amem += PSSO_ModeInfo_sizeof;
        added = true;
    }

    if (!added) {
        return false;
    }

    unix_copy_lib_resolution(ctx, &res, *amem);
    unix_amiga_list_add_tail(ctx, board_info + PSSO_BoardInfo_ResolutionsList, *amem);
    *amem += PSSO_LibResolution_sizeof;
    write_log(_T("Unix RTG mode: %08X %dx%d\n"), res.DisplayID, width, height);
    return true;
}

static void unix_picasso_init_alloc(TrapContext *ctx, int size)
{
    unix_picasso_amem = 0;
    unix_picasso_amemend = 0;
    if (unix_uaegfx_base) {
        size = trap_get_long(ctx, unix_uaegfx_base + UNIX_CARD_RESLISTSIZE);
        unix_picasso_amem = trap_get_long(ctx, unix_uaegfx_base + UNIX_CARD_RESLIST);
    } else if (unix_uaegfx_active) {
        unix_reserved_gfxmem = size;
        unix_picasso_amem = gfxmem_bank.start + gfxmem_bank.allocated_size - size;
    }
    unix_picasso_amemend = unix_picasso_amem + size;
    write_log(_T("Unix RTG P96 RESINFO: %08X-%08X (%d bytes)\n"),
        unix_picasso_amem, unix_picasso_amemend, size);
    picasso_allocatewritewatch(0, gfxmem_bank.allocated_size);
}

static uae_u32 REGPARAM2 unix_picasso_find_card(TrapContext *ctx)
{
    uaecptr board_info = trap_get_areg(ctx, 0);
    struct picasso96_state_struct *state = &picasso96_state[currprefs.rtgboards[0].monitor_id];

    if (!unix_uaegfx_active || !(gfxmem_bank.flags & ABFLAG_MAPPED)) {
        return 0;
    }
    if (unix_uaegfx_base) {
        trap_put_long(ctx, unix_uaegfx_base + UNIX_CARD_BOARDINFO, board_info);
    }
    unix_picasso_boardinfo = board_info;

    if (!gfxmem_bank.allocated_size || state->CardFound) {
        return 0;
    }
    trap_put_long(ctx, board_info + PSSO_BoardInfo_MemoryBase, gfxmem_bank.start);
    trap_put_long(ctx, board_info + PSSO_BoardInfo_MemorySize, gfxmem_bank.allocated_size - unix_reserved_gfxmem);
    state->CardFound = 1;
    write_log(_T("Unix RTG FindCard: boardinfo=%08X mem=%08X size=%u\n"),
        board_info, gfxmem_bank.start, gfxmem_bank.allocated_size - unix_reserved_gfxmem);
    return (uae_u32)-1;
}

static uae_u32 REGPARAM2 unix_picasso_set_switch(TrapContext *ctx)
{
    int monid = currprefs.rtgboards[0].monitor_id;
    struct amigadisplay *ad = &adisplays[monid];
    struct picasso96_state_struct *state = &picasso96_state[monid];
    struct picasso_vidbuf_description *vidinfo = &picasso_vidinfo[monid];
    bool oldstate = ad->picasso_on || ad->picasso_requested_on;
    bool requested = (trap_get_dreg(ctx, 0) & 0xffff) != 0;

    if (state->SwitchState != (requested ? 1 : 0)) {
        state->SwitchState = requested ? 1 : 0;
        atomic_or(&vidinfo->picasso_state_change, UNIX_PICASSO_STATE_SETSWITCH);
    }
    ad->picasso_requested_on = requested;
    write_log(_T("Unix RTG SetSwitch: %d old=%d\n"), requested ? 1 : 0, oldstate ? 1 : 0);
    return oldstate ? 1 : 0;
}

static uae_u32 REGPARAM2 unix_picasso_set_color_array(TrapContext *ctx)
{
    int monid = currprefs.rtgboards[0].monitor_id;
    struct picasso96_state_struct *state = &picasso96_state[monid];
    struct picasso_vidbuf_description *vidinfo = &picasso_vidinfo[monid];
    uaecptr board_info = trap_get_areg(ctx, 0);
    uae_u16 start = trap_get_dreg(ctx, 0);
    uae_u16 count = trap_get_dreg(ctx, 1);

    if (start >= 256 || start + count > 256) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        uaecptr src = board_info + PSSO_BoardInfo_CLUT + (start + i) * 3;
        state->CLUT[start + i].Red = trap_get_byte(ctx, src + 0);
        state->CLUT[start + i].Green = trap_get_byte(ctx, src + 1);
        state->CLUT[start + i].Blue = trap_get_byte(ctx, src + 2);
        state->CLUT[start + i].Pad = 0;
        vidinfo->clut[start + i] =
            0xff000000 |
            ((uae_u32)state->CLUT[start + i].Red << 16) |
            ((uae_u32)state->CLUT[start + i].Green << 8) |
            state->CLUT[start + i].Blue;
    }
    vidinfo->full_refresh = 1;
    return 1;
}

static uae_u32 REGPARAM2 unix_picasso_set_dac(TrapContext *ctx)
{
    int monid = currprefs.rtgboards[0].monitor_id;
    struct picasso96_state_struct *state = &picasso96_state[monid];
    struct picasso_vidbuf_description *vidinfo = &picasso_vidinfo[monid];
    uae_u16 index = trap_get_dreg(ctx, 0);
    RGBFTYPE rgbfmt = (RGBFTYPE)trap_get_dreg(ctx, 7);

    state->RGBFormat = rgbfmt;
    if (state->advDragging) {
        vidinfo->dacrgbformat[index ? 1 : 0] = rgbfmt;
    } else {
        vidinfo->dacrgbformat[0] = rgbfmt;
        vidinfo->dacrgbformat[1] = rgbfmt;
    }
    atomic_or(&vidinfo->picasso_state_change, UNIX_PICASSO_STATE_SETDAC);
    return 1;
}

static uae_u32 REGPARAM2 unix_picasso_set_gc(TrapContext *ctx)
{
    int monid = currprefs.rtgboards[0].monitor_id;
    struct picasso96_state_struct *state = &picasso96_state[monid];
    struct picasso_vidbuf_description *vidinfo = &picasso_vidinfo[monid];
    uaecptr board_info = trap_get_areg(ctx, 0);
    uaecptr mode_info = trap_get_areg(ctx, 1);

    trap_put_long(ctx, board_info + PSSO_BoardInfo_ModeInfo, mode_info);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_Border, trap_get_dreg(ctx, 0));

    uae_u16 width = trap_get_word(ctx, mode_info + PSSO_ModeInfo_Width);
    if (width != state->Width) {
        state->ModeChanged = true;
    }
    state->Width = width;
    state->VirtualWidth = state->Width;
    uae_u16 height = trap_get_word(ctx, mode_info + PSSO_ModeInfo_Height);
    if (height != state->Height) {
        state->ModeChanged = true;
    }
    state->Height = height;
    state->VirtualHeight = state->Height;
    state->GC_Depth = trap_get_byte(ctx, mode_info + PSSO_ModeInfo_Depth);
    state->GC_Flags = trap_get_byte(ctx, mode_info + PSSO_ModeInfo_Flags);
    state->HLineDBL = 1;
    state->VLineDBL = 1;
    state->HostAddress = NULL;
    atomic_or(&vidinfo->picasso_state_change, UNIX_PICASSO_STATE_SETGC);
    write_log(_T("Unix RTG SetGC: %dx%dx%d\n"), state->Width, state->Height, state->GC_Depth);
    return 1;
}

static void unix_picasso_set_panning_init(struct picasso96_state_struct *state)
{
    state->XYOffset = state->Address + (state->XOffset * state->BytesPerPixel) + (state->YOffset * state->BytesPerRow);
    state->BigAssBitmap = state->VirtualWidth > state->Width || state->VirtualHeight > state->Height;
}

static uae_u32 REGPARAM2 unix_picasso_set_panning(TrapContext *ctx)
{
    int monid = currprefs.rtgboards[0].monitor_id;
    struct picasso96_state_struct *state = &picasso96_state[monid];
    struct picasso_vidbuf_description *vidinfo = &picasso_vidinfo[monid];
    uaecptr board_info = trap_get_areg(ctx, 0);
    uaecptr bitmap_extra = trap_get_long(ctx, board_info + PSSO_BoardInfo_BitMapExtra);

    state->Address = trap_get_areg(ctx, 1);
    state->XOffset = (uae_s16)(trap_get_dreg(ctx, 1) & 0xffff);
    state->YOffset = (uae_s16)(trap_get_dreg(ctx, 2) & 0xffff);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_XOffset, (uae_u16)state->XOffset);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_YOffset, (uae_u16)state->YOffset);

    state->VirtualWidth = bitmap_extra ? trap_get_word(ctx, bitmap_extra + PSSO_BitMapExtra_Width) : trap_get_dreg(ctx, 0);
    state->VirtualHeight = bitmap_extra ? trap_get_word(ctx, bitmap_extra + PSSO_BitMapExtra_Height) : state->Height;
    if (!state->VirtualWidth) {
        state->VirtualWidth = state->Width;
    }
    if (!state->VirtualHeight) {
        state->VirtualHeight = state->Height;
    }
    state->RGBFormat = (RGBFTYPE)trap_get_dreg(ctx, 7);
    state->BytesPerPixel = unix_picasso_bytes_per_pixel(state->RGBFormat);
    state->BytesPerRow = state->VirtualWidth * state->BytesPerPixel;
    unix_picasso_set_panning_init(state);
    state->Extent = state->Address + state->BytesPerRow * state->VirtualHeight;

    atomic_or(&vidinfo->picasso_state_change, UNIX_PICASSO_STATE_SETPANNING);
    write_log(_T("Unix RTG SetPanning: addr=%08X xy=%d,%d virt=%dx%d bpr=%d fmt=%d\n"),
        state->Address, state->XOffset, state->YOffset, state->VirtualWidth, state->VirtualHeight,
        state->BytesPerRow, state->RGBFormat);
    return 1;
}

static uae_u32 REGPARAM2 unix_picasso_calculate_bytes_per_row(TrapContext *ctx)
{
    return (trap_get_dreg(ctx, 0) & 0xffff) * unix_picasso_bytes_per_pixel(trap_get_dreg(ctx, 7));
}

static uae_u32 REGPARAM2 unix_picasso_coerce_mode(TrapContext *ctx)
{
    uae_u16 board_width = trap_get_dreg(ctx, 2);
    uae_u16 friend_width = trap_get_dreg(ctx, 3);
    return board_width > friend_width ? board_width : friend_width;
}

static uae_u32 REGPARAM2 unix_picasso_get_compatible_dac_formats(TrapContext *ctx)
{
    int monid = currprefs.rtgboards[0].monitor_id;
    struct picasso96_state_struct *state = &picasso96_state[monid];
    uae_u32 rgbfmt = trap_get_dreg(ctx, 7);

    if (unix_picasso_rgbmask_for_format(rgbfmt)) {
        state->advDragging = true;
        return RGBMASK_8BIT | RGBMASK_15BIT | RGBMASK_16BIT | RGBMASK_24BIT | RGBMASK_32BIT;
    }
    return 0;
}

static uae_u32 REGPARAM2 unix_picasso_fill_rect(TrapContext *ctx)
{
    RenderInfo ri;
    uaecptr renderinfo = trap_get_areg(ctx, 1);
    uae_u32 x = (uae_u16)trap_get_dreg(ctx, 0);
    uae_u32 y = (uae_u16)trap_get_dreg(ctx, 1);
    uae_u32 width = (uae_u16)trap_get_dreg(ctx, 2);
    uae_u32 height = (uae_u16)trap_get_dreg(ctx, 3);
    uae_u32 pen = trap_get_dreg(ctx, 4);
    uae_u8 mask = (uae_u8)trap_get_dreg(ctx, 5);
    uae_u32 rgbfmt = trap_get_dreg(ctx, 7);
    int bytes_per_pixel = unix_picasso_bytes_per_pixel(rgbfmt);

    if (!bytes_per_pixel || !unix_picasso_renderinfo(ctx, renderinfo, &ri)) {
        return 0;
    }
    if (!unix_picasso_validate_rect(&ri, rgbfmt, &x, &y, &width, &height)) {
        write_log(_T("Unix RTG FillRect invalid region: %08X:%d:%d (%dx%d)-(%dx%d)\n"),
            ri.AMemory, ri.BytesPerRow, ri.RGBFormat, x, y, width, height);
        return 1;
    }
    if (!width || !height) {
        return 1;
    }

    uae_u8 *dst = ri.Memory + y * ri.BytesPerRow + x * bytes_per_pixel;
    for (uae_u32 row = 0; row < height; row++, dst += ri.BytesPerRow) {
        if (bytes_per_pixel == 1 && mask != 0xff) {
            for (uae_u32 col = 0; col < width; col++) {
                dst[col] = (uae_u8)((pen & mask) | (dst[col] & ~mask));
            }
        } else {
            for (uae_u32 col = 0; col < width; col++) {
                unix_picasso_store_pen(dst + col * bytes_per_pixel, pen, bytes_per_pixel);
            }
        }
    }
    return 1;
}

static uae_u32 REGPARAM2 unix_picasso_invert_rect(TrapContext *ctx)
{
    RenderInfo ri;
    uaecptr renderinfo = trap_get_areg(ctx, 1);
    uae_u32 x = (uae_u16)trap_get_dreg(ctx, 0);
    uae_u32 y = (uae_u16)trap_get_dreg(ctx, 1);
    uae_u32 width = (uae_u16)trap_get_dreg(ctx, 2);
    uae_u32 height = (uae_u16)trap_get_dreg(ctx, 3);
    uae_u8 mask = (uae_u8)trap_get_dreg(ctx, 4);
    uae_u32 rgbfmt = trap_get_dreg(ctx, 7);
    int bytes_per_pixel = unix_picasso_bytes_per_pixel(rgbfmt);

    if (!bytes_per_pixel || !unix_picasso_renderinfo(ctx, renderinfo, &ri)) {
        return 0;
    }
    if (!unix_picasso_validate_rect(&ri, rgbfmt, &x, &y, &width, &height)) {
        write_log(_T("Unix RTG InvertRect invalid region: %08X:%d:%d (%dx%d)-(%dx%d)\n"),
            ri.AMemory, ri.BytesPerRow, ri.RGBFormat, x, y, width, height);
        return 1;
    }
    if (!width || !height) {
        return 1;
    }

    if (bytes_per_pixel > 1) {
        mask = 0xff;
    }
    if (!mask) {
        return 1;
    }

    uae_u32 width_in_bytes = width * bytes_per_pixel;
    uae_u8 *dst = ri.Memory + y * ri.BytesPerRow + x * bytes_per_pixel;
    for (uae_u32 row = 0; row < height; row++, dst += ri.BytesPerRow) {
        for (uae_u32 col = 0; col < width_in_bytes; col++) {
            dst[col] ^= mask;
        }
    }
    return 1;
}

static uae_u32 unix_picasso_blit_rect_common(TrapContext *ctx, uaecptr srcinfo, uaecptr dstinfo,
    uae_u32 srcx, uae_u32 srcy, uae_u32 dstx, uae_u32 dsty, uae_u32 width, uae_u32 height,
    uae_u8 mask, uae_u32 rgbfmt, BLIT_OPCODE opcode)
{
    RenderInfo src_ri;
    RenderInfo dst_ri;
    RenderInfo *dst = &src_ri;
    int bytes_per_pixel = unix_picasso_bytes_per_pixel(rgbfmt);

    if (!bytes_per_pixel || !unix_picasso_renderinfo(ctx, srcinfo, &src_ri)) {
        return 0;
    }
    if (dstinfo) {
        if (!unix_picasso_renderinfo(ctx, dstinfo, &dst_ri)) {
            return 0;
        }
        dst = &dst_ri;
    }
    if (bytes_per_pixel > 1 && mask != 0xff) {
        return 0;
    }
    if (!unix_picasso_validate_rect(&src_ri, rgbfmt, &srcx, &srcy, &width, &height) ||
        !unix_picasso_validate_rect(dst, rgbfmt, &dstx, &dsty, &width, &height)) {
        write_log(_T("Unix RTG BlitRect invalid region: %08X->%08X fmt=%d (%dx%d)\n"),
            src_ri.AMemory, dst->AMemory, rgbfmt, width, height);
        return 1;
    }
    if (!width || !height) {
        return 1;
    }

    uae_u32 width_in_bytes = width * bytes_per_pixel;
    uae_u8 *srcbase = src_ri.Memory + srcy * src_ri.BytesPerRow + srcx * bytes_per_pixel;
    uae_u8 *dstbase = dst->Memory + dsty * dst->BytesPerRow + dstx * bytes_per_pixel;

    if (opcode == BLIT_SRC && mask == 0xff) {
        if (dstbase > srcbase && dstbase < srcbase + height * src_ri.BytesPerRow) {
            for (uae_s32 row = height - 1; row >= 0; row--) {
                memmove(dstbase + row * dst->BytesPerRow, srcbase + row * src_ri.BytesPerRow, width_in_bytes);
            }
        } else {
            for (uae_u32 row = 0; row < height; row++) {
                memmove(dstbase + row * dst->BytesPerRow, srcbase + row * src_ri.BytesPerRow, width_in_bytes);
            }
        }
        return 1;
    }

    if (opcode < BLIT_FALSE || opcode >= BLIT_LAST) {
        return 0;
    }
    for (uae_u32 row = 0; row < height; row++) {
        uae_u8 *srcrow = srcbase + row * src_ri.BytesPerRow;
        uae_u8 *dstrow = dstbase + row * dst->BytesPerRow;
        for (uae_u32 col = 0; col < width_in_bytes; col++) {
            uae_u8 olddst = dstrow[col];
            uae_u8 value = unix_picasso_blit_op(srcrow[col], olddst, opcode);
            dstrow[col] = (uae_u8)((value & mask) | (olddst & ~mask));
        }
    }
    return 1;
}

static uae_u32 REGPARAM2 unix_picasso_blit_rect(TrapContext *ctx)
{
    uaecptr renderinfo = trap_get_areg(ctx, 1);
    return unix_picasso_blit_rect_common(ctx, renderinfo, 0,
        (uae_u16)trap_get_dreg(ctx, 0),
        (uae_u16)trap_get_dreg(ctx, 1),
        (uae_u16)trap_get_dreg(ctx, 2),
        (uae_u16)trap_get_dreg(ctx, 3),
        (uae_u16)trap_get_dreg(ctx, 4),
        (uae_u16)trap_get_dreg(ctx, 5),
        (uae_u8)trap_get_dreg(ctx, 6),
        trap_get_dreg(ctx, 7),
        BLIT_SRC);
}

static uae_u32 REGPARAM2 unix_picasso_blit_rect_no_mask_complete(TrapContext *ctx)
{
    return unix_picasso_blit_rect_common(ctx, trap_get_areg(ctx, 1), trap_get_areg(ctx, 2),
        (uae_u16)trap_get_dreg(ctx, 0),
        (uae_u16)trap_get_dreg(ctx, 1),
        (uae_u16)trap_get_dreg(ctx, 2),
        (uae_u16)trap_get_dreg(ctx, 3),
        (uae_u16)trap_get_dreg(ctx, 4),
        (uae_u16)trap_get_dreg(ctx, 5),
        0xff,
        trap_get_dreg(ctx, 7),
        (BLIT_OPCODE)(trap_get_dreg(ctx, 6) & 0xff));
}

static uae_u32 REGPARAM2 unix_picasso_set_display(TrapContext *ctx)
{
    struct picasso_vidbuf_description *vidinfo = &picasso_vidinfo[currprefs.rtgboards[0].monitor_id];
    atomic_or(&vidinfo->picasso_state_change, UNIX_PICASSO_STATE_SETDISPLAY);
    return !(trap_get_dreg(ctx, 0) != 0);
}

static uae_u32 REGPARAM2 unix_picasso_default_unsupported(TrapContext *)
{
    return 0;
}

static void unix_picasso_init_board(TrapContext *ctx, uaecptr board_info)
{
    int monid = currprefs.rtgboards[0].monitor_id;
    struct picasso96_state_struct *state = &picasso96_state[monid];
    uae_u32 flags = trap_get_long(ctx, board_info + PSSO_BoardInfo_Flags);

    write_log(_T("Unix RTG mode mask: %x BI=%08x\n"), unix_rtg_modeflags(), board_info);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_BitsPerCannon, 8);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_RGBFormats, unix_rtg_modeflags());
    trap_put_long(ctx, board_info + PSSO_BoardInfo_BoardType, BT_uaegfx);
    trap_put_long(ctx, board_info + PSSO_BoardInfo_GraphicsControllerType, 0);
    trap_put_long(ctx, board_info + PSSO_BoardInfo_PaletteChipType, 0);
    trap_put_long(ctx, board_info + PSSO_BoardInfo_BoardName, unix_uaegfx_prefix);
    trap_put_long(ctx, board_info + PSSO_BoardInfo_MemoryClock, 200000000);

    for (int i = 0; i < MAXMODES; i++) {
        trap_put_long(ctx, board_info + PSSO_BoardInfo_PixelClockCount + i * 4, 1);
        trap_put_word(ctx, board_info + PSSO_BoardInfo_MaxHorValue + i * 2, 0x4000);
        trap_put_word(ctx, board_info + PSSO_BoardInfo_MaxVerValue + i * 2, 0x4000);
    }

    flags &= 0xffff0000;
    flags |= BIF_BLITTER | BIF_NOMEMORYMODEMIX | BIF_INDISPLAYCHAIN | UNIX_BIF_GRANTDIRECTACCESS;
    if (currprefs.rtg_dacswitch) {
        flags |= UNIX_BIF_DACSWITCH;
    }
    trap_put_long(ctx, board_info + PSSO_BoardInfo_Flags, flags);

    trap_put_word(ctx, board_info + PSSO_BoardInfo_MaxHorResolution + CHUNKY * 2, 1280);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_MaxVerResolution + CHUNKY * 2, 1024);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_MaxHorResolution + HICOLOR * 2, 1280);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_MaxVerResolution + HICOLOR * 2, 1024);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_MaxHorResolution + TRUECOLOR * 2, 1280);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_MaxVerResolution + TRUECOLOR * 2, 1024);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_MaxHorResolution + TRUEALPHA * 2, 1280);
    trap_put_word(ctx, board_info + PSSO_BoardInfo_MaxVerResolution + TRUEALPHA * 2, 1024);

    state->CardFound = 1;
}

#define UNIX_PUTABI(func) \
    do { \
        if (ABI) { \
            trap_put_long(ctx, ABI + (func), here()); \
        } \
        save_rom_absolute(ABI + (func)); \
    } while (0)

#define UNIX_RTGCALL(func, fallback, call) \
    do { \
        UNIX_PUTABI(func); \
        dl(0x48e78000); \
        calltrap(deftrap(call)); \
        dw(0x4a80); \
        dl(0x4cdf0001); \
        dw(0x6604); \
        dw(0x2f28); \
        dw(fallback); \
        dw(RTS); \
    } while (0)

#define UNIX_RTGCALL2(func, call) \
    do { \
        UNIX_PUTABI(func); \
        calltrap(deftrap(call)); \
        dw(RTS); \
    } while (0)

#define UNIX_RTGCALLDEFAULT(func, fallback) \
    do { \
        UNIX_PUTABI(func); \
        dw(0x2f28); \
        dw(fallback); \
        dw(RTS); \
    } while (0)

#define UNIX_RTGNONE(func) \
    do { \
        if (ABI) { \
            trap_put_long(ctx, ABI + (func), start); \
        } \
        save_rom_absolute(ABI + (func)); \
    } while (0)

static void unix_init_uaegfx_funcs(TrapContext *ctx, uaecptr start, uaecptr ABI)
{
    if (unix_uaegfx_old || !ABI) {
        return;
    }

    org(start);
    dw(RTS);

    UNIX_PUTABI(UNIX_PSSO_BoardInfo_ResolvePixelClock);
    dl(0x2340002c);
    dw(0x7000);
    dl(0x137c0062);
    dw(0x002a);
    dl(0x137c000e);
    dw(0x002b);
    dw(RTS);

    UNIX_PUTABI(UNIX_PSSO_BoardInfo_GetPixelClock);
    dw(0x203c);
    dl(100227260);
    dw(RTS);

    UNIX_PUTABI(UNIX_PSSO_BoardInfo_CalculateMemory);
    dw(0x2009);
    dw(RTS);

    UNIX_PUTABI(UNIX_PSSO_BoardInfo_GetCompatibleFormats);
    dw(0x203c);
    dl(RGBMASK_8BIT | RGBMASK_15BIT | RGBMASK_16BIT | RGBMASK_24BIT | RGBMASK_32BIT);
    dw(RTS);

    UNIX_RTGCALL2(UNIX_PSSO_BoardInfo_CalculateBytesPerRow, unix_picasso_calculate_bytes_per_row);
    UNIX_RTGNONE(UNIX_PSSO_BoardInfo_SetClock);
    UNIX_RTGNONE(UNIX_PSSO_BoardInfo_SetMemoryMode);
    UNIX_RTGNONE(UNIX_PSSO_BoardInfo_SetWriteMask);
    UNIX_RTGNONE(UNIX_PSSO_BoardInfo_SetClearMask);
    UNIX_RTGNONE(UNIX_PSSO_BoardInfo_SetReadPlane);
    UNIX_RTGNONE(UNIX_PSSO_BoardInfo_WaitVerticalSync);
    UNIX_RTGNONE(UNIX_PSSO_BoardInfo_WaitBlitter);

    UNIX_RTGCALL(UNIX_PSSO_BoardInfo_BlitPlanar2Direct, UNIX_PSSO_BoardInfo_BlitPlanar2DirectDefault, unix_picasso_default_unsupported);
    UNIX_RTGCALL(UNIX_PSSO_BoardInfo_FillRect, UNIX_PSSO_BoardInfo_FillRectDefault, unix_picasso_fill_rect);
    UNIX_RTGCALL(UNIX_PSSO_BoardInfo_BlitRect, UNIX_PSSO_BoardInfo_BlitRectDefault, unix_picasso_blit_rect);
    UNIX_RTGCALL(UNIX_PSSO_BoardInfo_BlitPlanar2Chunky, UNIX_PSSO_BoardInfo_BlitPlanar2ChunkyDefault, unix_picasso_default_unsupported);
    UNIX_RTGCALL(UNIX_PSSO_BoardInfo_BlitTemplate, UNIX_PSSO_BoardInfo_BlitTemplateDefault, unix_picasso_default_unsupported);
    UNIX_RTGCALL(UNIX_PSSO_BoardInfo_InvertRect, UNIX_PSSO_BoardInfo_InvertRectDefault, unix_picasso_invert_rect);
    UNIX_RTGCALL(UNIX_PSSO_BoardInfo_BlitRectNoMaskComplete, UNIX_PSSO_BoardInfo_BlitRectNoMaskCompleteDefault, unix_picasso_blit_rect_no_mask_complete);
    UNIX_RTGCALL(UNIX_PSSO_BoardInfo_BlitPattern, UNIX_PSSO_BoardInfo_BlitPatternDefault, unix_picasso_default_unsupported);

    UNIX_RTGCALL2(UNIX_PSSO_BoardInfo_SetSwitch, unix_picasso_set_switch);
    UNIX_RTGCALL2(UNIX_PSSO_BoardInfo_SetColorArray, unix_picasso_set_color_array);
    UNIX_RTGCALL2(UNIX_PSSO_BoardInfo_SetDAC, unix_picasso_set_dac);
    UNIX_RTGCALL2(UNIX_PSSO_BoardInfo_SetGC, unix_picasso_set_gc);
    UNIX_RTGCALL2(UNIX_PSSO_BoardInfo_SetPanning, unix_picasso_set_panning);
    UNIX_RTGCALL2(UNIX_PSSO_BoardInfo_SetDisplay, unix_picasso_set_display);

    UNIX_RTGCALLDEFAULT(UNIX_PSSO_BoardInfo_ScrollPlanar, UNIX_PSSO_BoardInfo_ScrollPlanarDefault);
    UNIX_RTGCALLDEFAULT(UNIX_PSSO_BoardInfo_UpdatePlanar, UNIX_PSSO_BoardInfo_UpdatePlanarDefault);
    UNIX_RTGCALLDEFAULT(UNIX_PSSO_BoardInfo_DrawLine, UNIX_PSSO_BoardInfo_DrawLineDefault);

    if (currprefs.rtg_dacswitch) {
        UNIX_RTGCALL2(UNIX_PSSO_BoardInfo_GetCompatibleDACFormats, unix_picasso_get_compatible_dac_formats);
        UNIX_RTGCALL2(UNIX_PSSO_BoardInfo_CoerceMode, unix_picasso_coerce_mode);
    }

    write_log(_T("Unix RTG uaegfx.card code: %08X-%08X BI=%08X\n"), start, here(), ABI);
}

static uae_u32 REGPARAM2 unix_picasso_init_card(TrapContext *ctx)
{
    uaecptr board_info = trap_get_areg(ctx, 0);
    uaecptr amem = unix_picasso_amem;
    int count = 0;

    if (!amem) {
        write_log(_T("Unix RTG InitCard without resolution memory\n"));
        return 0;
    }

    unix_picasso_init_board(ctx, board_info);
    unix_init_uaegfx_funcs(ctx, unix_uaegfx_rom, board_info);

    for (int i = 0; unix_rtg_mode_sizes[i][0]; i++) {
        int width = unix_rtg_mode_sizes[i][0];
        int height = unix_rtg_mode_sizes[i][1];
        if ((uae_u32)width * (uae_u32)height > gfxmem_bank.allocated_size - 256) {
            continue;
        }
        if (unix_add_mode(ctx, board_info, &amem, width, height, ++count)) {
            continue;
        }
    }

    if (amem > unix_picasso_amemend) {
        write_log(_T("Unix RTG resolution list overflow %08X > %08X\n"), amem, unix_picasso_amemend);
    }
    write_log(_T("Unix RTG InitCard: %d modes\n"), count);
    return (uae_u32)-1;
}

static uae_u32 REGPARAM2 unix_gfx_open(TrapContext *ctx)
{
    trap_put_word(ctx, unix_uaegfx_base + 32, trap_get_word(ctx, unix_uaegfx_base + 32) + 1);
    return unix_uaegfx_base;
}

static uae_u32 REGPARAM2 unix_gfx_close(TrapContext *ctx)
{
    trap_put_word(ctx, unix_uaegfx_base + 32, trap_get_word(ctx, unix_uaegfx_base + 32) - 1);
    return 0;
}

static uae_u32 REGPARAM2 unix_gfx_expunge(TrapContext *)
{
    return 0;
}

static uaecptr unix_uaegfx_card_install(TrapContext *ctx, uae_u32 extrasize)
{
    uaecptr openfunc, closefunc, expungefunc, findcardfunc, initcardfunc;
    uaecptr functable, datatable, exec, olda2;

    if (unix_uaegfx_old || !(gfxmem_bank.flags & ABFLAG_MAPPED)) {
        return 0;
    }

    exec = trap_get_long(ctx, 4);
    unix_uaegfx_resid = ds(_T("UAE Graphics Card 4.0"));

    openfunc = here();
    calltrap(deftrap(unix_gfx_open));
    dw(RTS);
    closefunc = here();
    calltrap(deftrap(unix_gfx_close));
    dw(RTS);
    expungefunc = here();
    calltrap(deftrap(unix_gfx_expunge));
    dw(RTS);
    findcardfunc = here();
    calltrap(deftrap(unix_picasso_find_card));
    dw(RTS);
    initcardfunc = here();
    calltrap(deftrap(unix_picasso_init_card));
    dw(RTS);

    functable = here();
    dl(openfunc);
    dl(closefunc);
    dl(expungefunc);
    dl(EXPANSION_nullfunc);
    dl(findcardfunc);
    dl(initcardfunc);
    dl(0xffffffff);

    datatable = makedatatable(unix_uaegfx_resid, unix_uaegfx_resname, 0x09, -50, UNIX_UAEGFX_VERSION, UNIX_UAEGFX_REVISION);
    olda2 = trap_get_areg(ctx, 2);

    trap_call_add_areg(ctx, 0, functable);
    trap_call_add_areg(ctx, 1, datatable);
    trap_call_add_areg(ctx, 2, 0);
    trap_call_add_dreg(ctx, 0, UNIX_CARD_SIZEOF + extrasize);
    trap_call_add_dreg(ctx, 1, 0);
    unix_uaegfx_base = trap_call_lib(ctx, exec, -0x54);
    trap_set_areg(ctx, 2, olda2);
    if (!unix_uaegfx_base) {
        return 0;
    }

    trap_call_add_areg(ctx, 1, unix_uaegfx_base);
    trap_call_lib(ctx, exec, -0x18c);

    trap_call_add_areg(ctx, 1, EXPANSION_explibname);
    trap_call_add_dreg(ctx, 0, 0);
    trap_put_long(ctx, unix_uaegfx_base + UNIX_CARD_EXPANSIONBASE, trap_call_lib(ctx, exec, -0x228));
    trap_put_long(ctx, unix_uaegfx_base + UNIX_CARD_EXECBASE, exec);
    trap_put_long(ctx, unix_uaegfx_base + UNIX_CARD_NAME, unix_uaegfx_resname);
    trap_put_long(ctx, unix_uaegfx_base + UNIX_CARD_RESLIST, unix_uaegfx_base + UNIX_CARD_SIZEOF);
    trap_put_long(ctx, unix_uaegfx_base + UNIX_CARD_RESLISTSIZE, extrasize);

    unix_uaegfx_active = 1;
    write_log(_T("Unix uaegfx.card %d.%d init @%08X (%u bytes modes)\n"),
        UNIX_UAEGFX_VERSION, UNIX_UAEGFX_REVISION, unix_uaegfx_base, extrasize);
    return unix_uaegfx_base;
}

static void unix_picasso_alloc2(TrapContext *ctx)
{
    int size = 0;

    unix_picasso_amem = 0;
    unix_picasso_amemend = 0;
    if (!gfxmem_bank.allocated_size) {
        return;
    }
    if (!currprefs.picasso96_noautomodes) {
        size = unix_picasso_resolution_memory_size();
    }
    unix_uaegfx_card_install(ctx, size);
    unix_picasso_init_alloc(ctx, size);
}

void picasso96_alloc(TrapContext *ctx)
{
    if (currprefs.rtgboards[0].rtgmem_type >= GFXBOARD_HARDWARE) {
        return;
    }
    if (!currprefs.rtgboards[0].rtgmem_size) {
        return;
    }
    unix_uaegfx_resname = ds(_T("uaegfx.card"));
    unix_uaegfx_prefix = ds(_T("UAE"));
    if (unix_uaegfx_old) {
        return;
    }
    unix_picasso_alloc2(ctx);
}

uae_u32 picasso_demux(uae_u32, TrapContext *ctx)
{
    uae_u32 num = trap_get_long(ctx, trap_get_areg(ctx, 7) + 4);

    if (unix_uaegfx_base && num >= 16 && num <= 39) {
        write_log(_T("Unix RTG: obsolete Picasso96 uaelib hook ignored\n"));
        return 0;
    }
    if (!unix_uaegfx_old) {
        write_log(_T("Unix RTG: Picasso96 uaelib hook in use\n"));
        unix_uaegfx_old = 1;
        unix_uaegfx_active = 1;
    }

    switch (num) {
    case 16:
        return unix_picasso_find_card(ctx);
    case 18:
        return unix_picasso_set_switch(ctx);
    case 19:
        return unix_picasso_set_color_array(ctx);
    case 20:
        return unix_picasso_set_dac(ctx);
    case 21:
        return unix_picasso_set_gc(ctx);
    case 22:
        return unix_picasso_set_panning(ctx);
    case 23:
        return unix_picasso_calculate_bytes_per_row(ctx);
    case 26:
        return unix_picasso_set_display(ctx);
    case 29:
        return unix_picasso_init_card(ctx);
    case 35:
        return gfxmem_bank.allocated_size ? 1 : 0;
    default:
        return 0;
    }
}

void uaegfx_install_code(uaecptr start)
{
    unix_uaegfx_rom = start;
    org(start);
}

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
