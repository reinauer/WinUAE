/*
* UAE - The Un*x Amiga Emulator
*
* Sun SunPCi I coprocessor card
*
* The card is a PC on a PCI card: an AMD K6-2 with a SiS 5597/5598 chipset
* behind an Intel (DEC) 21554 non-transparent PCI-to-PCI bridge. The host
* sees the bridge's primary interface; the card's PC sees its secondary
* interface at device 11 of its own PCI bus. Both sides share the bridge's
* device registers and CSRs (doorbells, scratchpads, lookup table).
*
* The card's PC runs Sun's Award BIOS (SUNWspci bios/sunpci.bin) on the
* 86Box CPU and PC devices. The chipset is emulated only as far as the BIOS
* needs it. The BIOS itself programs the bridge's window setup registers, so
* the BAR sizes the host sees come from the card.
*
* 21554 register layout: illumos usr/src/uts/sun4u/sys/pci/db21554_*.h.
*
* Uses the 86Box core shared with the x86 bridgeboards, the ATonce and the
* DraCo; the core is single instance, so only one of them can be active.
*/

#include "sysconfig.h"
#include "sysdeps.h"

#ifdef WITH_X86

#include "options.h"
#include "uae.h"
#include "custom.h"
#include "memory.h"
#include "rommgr.h"
#include "zfile.h"
#include "devices.h"
#include "autoconf.h"
#include "pci_hw.h"
#include "gfxboard.h"
#include "sunpci.h"

#include "86box_glue.h"
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/timer.h>
#include <86box/mem.h>
#include <86box/smram.h>
#include <86box/rom.h>
#include <86box/pic.h>
#include <86box/pit.h>
#include <86box/dma.h>
#include <86box/nvr.h>
#include <86box/keyboard.h>
#include <86box/serial.h>
#include <86box/fifo.h>
#include <86box/port_6x.h>
#include <86box/video.h>
#include <86box/vid_svga.h>
#include "86box/cpu/cpu.h"
#include "vid_sis5598.h"

// 0: startup/errors only; 1: register tracing; 2: also CSR reads.
#ifndef SUNPCI_DEBUG_CSR
#define SUNPCI_DEBUG_CSR 0
#endif

// 0: first access per port/width/direction; 1: trace repeats as well.
// Both modes retain the per-board diagnostic budget.
#ifndef SUNPCI_DEBUG_IO
#define SUNPCI_DEBUG_IO 0
#endif

#define SUNPCI_BIOS_SIZE 0x40000 // the Am29F002T flash
#define SUNPCI_RAM_KB (64 * 1024)
#define SUNPCI_MAX_DEVS 8
#define SUNPCI_BRIDGE_DEV 11
// Sun BIOS SMI VGA initialization reads BAR0 at CF8 address 8000a010.
#define SUNPCI_VGA_DEV 20
#define SUNPCI_VGA_LFB_SIZE 0x400000

// 21554 CSRs, shared by both interfaces
#define DB_CSR_SIZE 0x1000
#define DB_CSR_P_CLR_IRQ 0x98
#define DB_CSR_S_CLR_IRQ 0x9a
#define DB_CSR_P_SET_IRQ 0x9c
#define DB_CSR_S_SET_IRQ 0x9e
#define DB_CSR_P_CLR_MASK 0xa0
#define DB_CSR_S_CLR_MASK 0xa2
#define DB_CSR_P_SET_MASK 0xa4
#define DB_CSR_S_SET_MASK 0xa6
#define DB_CSR_SCRATCH 0xa8
// Sun's on-card microcontroller ("PIC") sits on the 21554 ROM interface
#define DB_CSR_PIC_DATA 0xca
#define DB_CSR_PIC_COMMAND 0xcc
#define DB_CSR_PIC_CONTROL 0xcf
#define PIC_CONTROL_BUSY 0x02
#define PIC_CONTROL_WRITE 0x04
#define PIC_CMD_STATUS 0
#define PIC_CMD_KEYBOARD 1
#define PIC_CMD_MOUSE 2
#define PIC_CMD_IRQ_SET 4
#define PIC_CMD_IRQ_CLEAR 5
#define PIC_CMD_LED 6
#define PIC_CMD_SMI_RATE 7
#define DB_CSR_LUT_OFFSET 0x28
#define DB_CSR_LUT_DATA 0x2c
#define DB_CSR_LUT 0x100
#define DB_LUT_ENTRIES 64

// 21554 device registers (configuration 80-FF, shared by both interfaces)
#define DB_CONF_DS_MEM0_TR 0x94
#define DB_CONF_DS_IO_MEM1_TR 0x98
#define DB_CONF_DS_MEM2_TR 0x9c
#define DB_CONF_DS_MEM3_TR 0xa0
#define DB_CONF_US_IO_MEM0_TR 0xa4
#define DB_CONF_US_MEM1_TR 0xa8
#define DB_CONF_DS_MEM0_SETUP 0xac
#define DB_CONF_DS_IO_MEM1_SETUP 0xb0
#define DB_CONF_DS_MEM2_SETUP 0xb4
#define DB_CONF_DS_MEM3_SETUP 0xb8
#define DB_CONF_ROM_SETUP 0xc0
#define DB_CONF_US_IO_MEM0_SETUP 0xc4
#define DB_CONF_US_MEM1_SETUP 0xc8
#define DB_CONF_CHIP_CTRL0 0xcc
#define DB_CONF_CHIP_CTRL1 0xce
#define DB_CONF_RESET_CTRL 0xd8
#define DB_CONF_PM_CAP 0xde
#define DB_CONF_PM_CSR 0xe0
#define DB_CONF_VPD_ADDR 0xe6
#define DB_CONF_VPD_DATA 0xe8
#define DB_CONF_HS_CTRL 0xee

#define DB_PLOCKOUT 0x0400
#define DB_RESET_SEC 0x01
#define DB_RESET_CHIP 0x02
// Reset control bit 2 shows the s_pme_l pin; a SunPCi I card reads 04
#define DB_RESET_SUBSYS_STAT 0x04
#define DB_HS_INS_STAT 0x80

// windows: host BARs (primary) and card BARs (secondary)
enum {
	DB_WIN_NONE,
	DB_WIN_CSR,
	DB_WIN_DS0,
	DB_WIN_DS1,
	DB_WIN_DS2,
	DB_WIN_DS3,
	DB_WIN_US0,
	DB_WIN_US1,
	DB_WIN_US2,
	DB_WIN_ROM
};

extern bool x86_io_handler_access(uint16_t port, int dir, uint32_t *val, int size);
extern void x86_io_handlers_free(void);

struct sunpci_dev
{
	const TCHAR *name;
	int devfn;
	uae_u8 cfg[256];
	uae_u8 wmask[256];
	void (*config_written)(struct sunpci_dev *dev, int reg, uae_u8 old);
};

struct db21554
{
	uae_u8 pri[0x40];
	uae_u8 sec[0x40];
	uae_u8 regs[0x80];
	uae_u32 pri_bar_size[6];
	uae_u32 sec_bar_size[6];
	uae_u8 csr[DB_CSR_SIZE];
	uae_u16 p_irq, s_irq, p_mask, s_mask;
	uae_u8 pic_led, pic_smi_rate;
	bool host_irq, card_irq;
	int card_pic_irq;
	uae_u8 card_irq_state;
	bool master_abort_logged;
	bool rom_read_logged, vpd_logged;
	uae_u8 pic_host_irqs;
};

struct sunpci_state
{
	uae_u8 *bios;
	uae_u32 cf8;
	struct sunpci_dev devs[SUNPCI_MAX_DEVS];
	int ndevs;
	struct sunpci_dev *sis_host, *sis_isa, *sis_vga, *sis_usb;
	uae_u32 ohci[0x100];
	void *vga;
	mem_mapping_t vga_legacy_map;
	mem_mapping_t pci_hole;
	mem_mapping_t isa_hole;
	mem_mapping_t flash_map, flash_high_map;
	int flash_state;
	bool flash_autoselect;
	smram_t *smram;
	uae_u8 smram_layout;
	int smi_lines;
	void *serial;
	uae_u8 mouse_queue[256];
	int mouse_head, mouse_tail;
	nvr_t *nvr;
	struct db21554 db;
	uae_u8 post;
	uae_u8 port92;
	uae_u8 reset_control;
	bool in_reset;
	bool init_done;
	float cycle_acc;
	int unknown_logs;
	uae_u8 unknown_io_seen[65536];
	bool acpi_probe_logged;
	int trace_lines;
	struct pci_board_state *host_pcibs;
	struct zfile *cmosfile;
};

static struct sunpci_state *sunpci;

bool sunpci_x86_active(void)
{
	return sunpci != NULL;
}

static uae_u32 get_le32(const uae_u8 *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((uae_u32)p[3] << 24);
}

static uae_u16 get_le16(const uae_u8 *p)
{
	return p[0] | (p[1] << 8);
}

static void put_le32(uae_u8 *p, uae_u32 v)
{
	p[0] = (uae_u8)v;
	p[1] = (uae_u8)(v >> 8);
	p[2] = (uae_u8)(v >> 16);
	p[3] = (uae_u8)(v >> 24);
}

/* 21554 configuration */

static uae_u32 db_reg32(struct db21554 *b, int reg)
{
	return get_le32(&b->regs[reg - 0x80]);
}

static uae_u16 db_reg16(struct db21554 *b, int reg)
{
	return get_le16(&b->regs[reg - 0x80]);
}

// Window size from a setup register: bit 31 enables it, bit 0 selects I/O
// space and the remaining high bits are the size mask.
static uae_u32 db_setup_size(uae_u32 setup)
{
	if (!(setup & 0x80000000)) {
		return 0;
	}
	if (setup & 1) {
		return (~(setup & 0xfffffffc)) + 1;
	}
	return (~(setup & 0xfffff000)) + 1;
}

// A setup register's low bits are returned in its BAR.
static uae_u32 db_setup_flags(uae_u32 setup)
{
	if (!(setup & 0x80000000)) {
		return 0;
	}
	return setup & ((setup & 1) ? 3 : 15);
}

static uae_u32 db_us2_page_size(struct db21554 *b)
{
	int code = (db_reg16(b, DB_CONF_CHIP_CTRL1) >> 8) & 15;
	return code ? 0x80 << code : 0;
}

// flags are the BAR's read-only low bits: I/O, or memory type and prefetch
static void db_set_bar_size(uae_u8 *hdr, uae_u32 *sizes, int bar, uae_u32 size, uae_u32 flags)
{
	int off = 0x10 + bar * 4;
	uae_u32 low = (flags & 1) ? 3 : 15;
	sizes[bar] = size | (flags & 1);
	uae_u32 v = get_le32(&hdr[off]);
	if (size) {
		v &= ~(size - 1) & ~low;
		v |= flags & low;
	} else {
		v = 0;
	}
	put_le32(&hdr[off], v);
}

// The primary expansion ROM BAR follows its setup register: bit 24 enables
// it, bits 23:12 size it, and bits 31:24 and the decode enable (bit 0) are
// writable while it is enabled.
static uae_u32 db_rom_bar_mask(struct db21554 *b)
{
	uae_u32 setup = db_reg32(b, DB_CONF_ROM_SETUP);
	if (!(setup & 0x01000000)) {
		return 0;
	}
	return 0xff000000 | (setup & 0x00fff000) | 1;
}

static void db_update_bar_sizes(struct db21554 *b)
{
	uae_u32 s;

	// primary: CSR memory and downstream memory 0 share BAR0
	s = db_reg32(b, DB_CONF_DS_MEM0_SETUP);
	db_set_bar_size(b->pri, b->pri_bar_size, 0, db_setup_size(s) > DB_CSR_SIZE ? db_setup_size(s) : DB_CSR_SIZE, db_setup_flags(s) & ~1);
	db_set_bar_size(b->pri, b->pri_bar_size, 1, 256, 1);
	s = db_reg32(b, DB_CONF_DS_IO_MEM1_SETUP);
	db_set_bar_size(b->pri, b->pri_bar_size, 2, db_setup_size(s), db_setup_flags(s));
	s = db_reg32(b, DB_CONF_DS_MEM2_SETUP);
	db_set_bar_size(b->pri, b->pri_bar_size, 3, db_setup_size(s), db_setup_flags(s));
	s = db_reg32(b, DB_CONF_DS_MEM3_SETUP);
	db_set_bar_size(b->pri, b->pri_bar_size, 4, db_setup_size(s), db_setup_flags(s));
	db_set_bar_size(b->pri, b->pri_bar_size, 5, 0, 0);
	put_le32(&b->pri[0x30], get_le32(&b->pri[0x30]) & db_rom_bar_mask(b));

	// secondary
	db_set_bar_size(b->sec, b->sec_bar_size, 0, DB_CSR_SIZE, 0);
	db_set_bar_size(b->sec, b->sec_bar_size, 1, 256, 1);
	s = db_reg32(b, DB_CONF_US_IO_MEM0_SETUP);
	db_set_bar_size(b->sec, b->sec_bar_size, 2, db_setup_size(s), db_setup_flags(s));
	s = db_reg32(b, DB_CONF_US_MEM1_SETUP);
	db_set_bar_size(b->sec, b->sec_bar_size, 3, db_setup_size(s), db_setup_flags(s));
	// upstream memory 2 is prefetchable (a SunPCi I card reads 00000008)
	db_set_bar_size(b->sec, b->sec_bar_size, 4, db_us2_page_size(b) * DB_LUT_ENTRIES, 8);
	db_set_bar_size(b->sec, b->sec_bar_size, 5, 0, 0);
}

static uae_u32 db_bar(const uae_u8 *hdr, int bar)
{
	uae_u32 v = get_le32(&hdr[0x10 + bar * 4]);
	return v & ((v & 1) ? ~3 : ~15);
}

static void sunpci_card_irq_update(struct sunpci_state *s);

static void db_irq_update(struct sunpci_state *s)
{
	struct db21554 *b = &s->db;
	bool host = (b->p_irq & ~b->p_mask) != 0;
	if (host != b->host_irq) {
		b->host_irq = host;
		if (s->host_pcibs) {
			s->host_pcibs->irq_callback(s->host_pcibs, host);
		}
	}
	sunpci_card_irq_update(s);
}

static void sunpci_card_remap(struct sunpci_state *s);
static void sunpci_card_reset(struct sunpci_state *s);

// Configuration access from either interface. Each interface sees its own
// header at 00 and the other interface's header at 40.
static uae_u8 *db_config_ptr(struct db21554 *b, int reg, bool secondary, bool *header, bool *primary_hdr)
{
	*header = reg < 0x80;
	if (reg >= 0x80) {
		return &b->regs[reg - 0x80];
	}
	bool pri = secondary ? reg >= 0x40 : reg < 0x40;
	*primary_hdr = pri;
	return pri ? &b->pri[reg & 0x3f] : &b->sec[reg & 0x3f];
}

static uae_u8 db_config_read(struct sunpci_state *s, int reg, bool secondary)
{
	struct db21554 *b = &s->db;
	bool header, pri;
	if (!secondary && s->init_done && (db_reg16(b, DB_CONF_CHIP_CTRL0) & DB_PLOCKOUT)) {
		return 0xff;
	}
	return *db_config_ptr(b, reg, secondary, &header, &pri);
}

static bool db_header_writable(int off)
{
	if (off == 0x04 || off == 0x05 || off == 0x0c || off == 0x0d || off == 0x3c) {
		return true;
	}
	if (off >= 0x10 && off < 0x28) {
		return true;
	}
	return false;
}

// Reset control and the enhanced capabilities: power management at DC,
// VPD at E4 and CompactPCI hot-swap at EC, then reserved space to FF.
static void db_ecp_write(struct sunpci_state *s, int reg, uae_u8 *p, uae_u8 old, uae_u8 v, bool secondary)
{
	struct db21554 *b = &s->db;

	switch (reg)
	{
	case DB_CONF_RESET_CTRL:
	{
		bool hold = (v & DB_RESET_SEC) != 0;
		if (hold && !s->in_reset) {
			write_log(_T("SUNPCI: card reset asserted by %s\n"), secondary ? _T("card") : _T("host"));
			s->in_reset = true;
		} else if (!hold && s->in_reset) {
			write_log(_T("SUNPCI: card reset released\n"));
			s->in_reset = false;
			sunpci_card_reset(s);
		}
		// chip reset completes at once; the status bits follow pins
		*p = (old & ~(DB_RESET_SEC | DB_RESET_CHIP)) | (v & DB_RESET_SEC);
		break;
	}
	case DB_CONF_PM_CAP:
	case DB_CONF_PM_CAP + 1:
		// the local processor may set the version, DSI, D1/D2 and PME support
		if (secondary) {
			uae_u8 m = reg == DB_CONF_PM_CAP ? 0x27 : 0x7e;
			*p = (old & ~m) | (v & m);
		}
		// PME clock reads 1 when any PME support bit is set
		b->regs[DB_CONF_PM_CAP - 0x80] = (b->regs[DB_CONF_PM_CAP - 0x80] & ~0x08) | ((b->regs[DB_CONF_PM_CAP + 1 - 0x80] & 0xf8) ? 0x08 : 0);
		break;
	case DB_CONF_PM_CSR:
	{
		// D0 and D3 always, D1 and D2 when the capabilities say so
		int state = v & 3;
		if ((state == 1 && !(b->regs[DB_CONF_PM_CAP + 1 - 0x80] & 0x02)) || (state == 2 && !(b->regs[DB_CONF_PM_CAP + 1 - 0x80] & 0x04))) {
			state = old & 3;
		}
		*p = state;
		break;
	}
	case DB_CONF_PM_CSR + 1:
	{
		// PME enable needs PME support, the data select PM data from the
		// serial ROM; PME status is never set without PME support
		uae_u8 m = (b->regs[DB_CONF_PM_CAP + 1 - 0x80] & 0xf8) ? 0x01 : 0;
		if (secondary) {
			m |= 0x60;
		}
		*p = (old & ~m) | (v & m);
		break;
	}
	case DB_CONF_VPD_ADDR:
		*p = v;
		break;
	case DB_CONF_VPD_ADDR + 1:
		// The VPD lives in the serial ROM. Nothing is known about one on
		// the card, so reads return ones and writes are dropped; the flag
		// completes the access either way.
		if (!b->vpd_logged) {
			b->vpd_logged = true;
			write_log(_T("SUNPCI: VPD %s at %03x by %s; no serial ROM contents\n"), (v & 0x80) ? _T("write") : _T("read"),
				((v & 1) << 8) | b->regs[DB_CONF_VPD_ADDR - 0x80], secondary ? _T("card") : _T("host"));
		}
		if (!(v & 0x80)) {
			put_le32(&b->regs[DB_CONF_VPD_DATA - 0x80], 0xffffffff);
		}
		*p = (v & 1) | (~v & 0x80);
		break;
	case DB_CONF_VPD_DATA:
	case DB_CONF_VPD_DATA + 1:
	case DB_CONF_VPD_DATA + 2:
	case DB_CONF_VPD_DATA + 3:
		*p = v;
		break;
	case DB_CONF_HS_CTRL:
		// ENUM# mask and LED control; the status bits are write 1 to clear
		*p = (old & 0xc0 & ~v) | (v & 0x0a);
		break;
	}
}

static void db_config_write(struct sunpci_state *s, int reg, uae_u8 v, bool secondary)
{
	struct db21554 *b = &s->db;
	bool header, pri;
	uae_u8 *p = db_config_ptr(b, reg, secondary, &header, &pri);
	uae_u8 old = *p;

	if (header) {
		int off = reg & 0x3f;
		if (off >= 0x10 && off < 0x28) {
			// BAR: keep the bits within the window's size
			int bar = (off - 0x10) / 4;
			uae_u32 size = (pri ? b->pri_bar_size : b->sec_bar_size)[bar];
			uae_u32 mask = (size & ~1) ? ~((size & ~1) - 1) : 0;
			uae_u8 m = (uae_u8)(mask >> ((off & 3) * 8));
			if ((off & 3) == 0) {
				m &= (size & 1) ? ~3 : ~15;
			}
			*p = (old & ~m) | (v & m);
		} else if (pri && off >= 0x30 && off < 0x34) {
			uae_u8 m = (uae_u8)(db_rom_bar_mask(b) >> ((off & 3) * 8));
			*p = (old & ~m) | (v & m);
		} else if (off == 0x2c || off == 0x2d || off == 0x2e || off == 0x2f) {
			// subsystem IDs are written from the secondary side
			if (secondary) {
				*p = v;
			}
		} else if (db_header_writable(off)) {
			*p = v;
		}
	} else if (reg >= DB_CONF_RESET_CTRL) {
		db_ecp_write(s, reg, p, old, v, secondary);
	} else {
		*p = v;
		if ((reg >= DB_CONF_DS_MEM0_SETUP && reg < DB_CONF_CHIP_CTRL0) || reg == DB_CONF_CHIP_CTRL1 + 1) {
			db_update_bar_sizes(b);
		}
		if (reg == DB_CONF_CHIP_CTRL0 + 1 && (old & (DB_PLOCKOUT >> 8)) && !(v & (DB_PLOCKOUT >> 8))) {
			// l_stat is low on the card, so clearing the primary lockout
			// signals the insertion in the hot-swap control register
			b->regs[DB_CONF_HS_CTRL - 0x80] |= DB_HS_INS_STAT;
		}
	}
#if SUNPCI_DEBUG_CSR
	if (*p != old || reg >= 0x80) {
		write_log(_T("SUNPCI: %s cfg 21554 [%02x] %02x -> %02x\n"), secondary ? _T("card") : _T("host"), reg, old, *p);
	}
#endif
	if (header && ((reg & 0x3f) == 0x04 || ((reg & 0x3f) >= 0x10 && (reg & 0x3f) < 0x28))) {
		sunpci_card_remap(s);
	}
	if (header && (reg & 0x3f) == 0x04) {
		db_irq_update(s);
	}
}

static void db_init(struct db21554 *b)
{
	memset(b->pri, 0, sizeof b->pri);
	memset(b->sec, 0, sizeof b->sec);
	memset(b->regs, 0, sizeof b->regs);
	for (int i = 0; i < 2; i++) {
		uae_u8 *h = i ? b->sec : b->pri;
		put_le32(&h[0x00], 0x00461011);
		h[0x06] = 0x90;
		h[0x07] = 0x02;
		h[0x08] = 0x01;
		h[0x09] = 0x00;
		h[0x0a] = 0x80;
		h[0x0b] = 0x06;
		h[0x34] = 0xdc;
		h[0x3d] = 1;
	}
	b->regs[DB_CONF_RESET_CTRL - 0x80] = DB_RESET_SUBSYS_STAT;
	// power management 1.0 without PME#, then VPD and hot-swap
	b->regs[0xdc - 0x80] = 0x01;
	b->regs[0xdd - 0x80] = 0xe4;
	b->regs[DB_CONF_PM_CAP - 0x80] = 0x01;
	b->regs[0xe4 - 0x80] = 0x03;
	b->regs[0xe5 - 0x80] = 0xec;
	b->regs[0xec - 0x80] = 0x06;
	// primary lockout until the card's BIOS has set the bridge up
	b->regs[DB_CONF_CHIP_CTRL0 + 1 - 0x80] = DB_PLOCKOUT >> 8;
	b->p_mask = 0xffff;
	b->s_mask = 0xffff;
	db_update_bar_sizes(b);
}

/* 21554 CSRs */

static uae_u16 db_doorbell_read(struct db21554 *b, int reg)
{
	switch (reg & ~1)
	{
	case DB_CSR_P_CLR_IRQ:
	case DB_CSR_P_SET_IRQ:
		return b->p_irq;
	case DB_CSR_S_CLR_IRQ:
	case DB_CSR_S_SET_IRQ:
		return b->s_irq;
	case DB_CSR_P_CLR_MASK:
	case DB_CSR_P_SET_MASK:
		return b->p_mask;
	case DB_CSR_S_CLR_MASK:
	case DB_CSR_S_SET_MASK:
		return b->s_mask;
	}
	return 0;
}

static void db_doorbell_write(struct db21554 *b, int reg, uae_u16 v)
{
	switch (reg)
	{
	case DB_CSR_P_CLR_IRQ:
		b->p_irq &= ~v;
		break;
	case DB_CSR_S_CLR_IRQ:
		b->s_irq &= ~v;
		break;
	case DB_CSR_P_SET_IRQ:
		b->p_irq |= v;
		break;
	case DB_CSR_S_SET_IRQ:
		b->s_irq |= v;
		break;
	case DB_CSR_P_CLR_MASK:
		b->p_mask &= ~v;
		break;
	case DB_CSR_S_CLR_MASK:
		b->s_mask &= ~v;
		break;
	case DB_CSR_P_SET_MASK:
		b->p_mask |= v;
		break;
	case DB_CSR_S_SET_MASK:
		b->s_mask |= v;
		break;
	}
}

/* Sun PIC */

// The Solaris 504311 ioctl writes command 4 then 5 with the same mask.
// Keep the pin state separate from the 8259's latched edge: picintc() would
// erase the pending interrupt before the x86 core has a chance to accept it.
static void sunpci_pic_host_irq(struct db21554 *b, bool assert_pin, uae_u8 mask)
{
	static const unsigned irqs[4] = { 9, 10, 11, 15 };
	mask &= 15;
	if (assert_pin) {
		uae_u8 rising = mask & ~b->pic_host_irqs;
		b->pic_host_irqs |= mask;
		for (unsigned i = 0; i < 4; i++) {
			if (rising & (1U << i)) {
				picint(1U << irqs[i]);
			}
		}
	} else {
		b->pic_host_irqs &= ~mask;
	}
}

// IBM enhanced keyboard key station numbers (what PIC command 1 carries) to
// set 1 scancodes, bit 7 = E0 prefix, FF = Pause.
static const uae_u16 sunpci_station_to_scancode[128] =
{
	/* 0*/ 0, 0x29, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0, 0x0e,
	/*16*/ 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x2b, 0x3a, 0x1e,
	/*32*/ 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2b, 0x1c, 0x2a, 0x56, 0x2c, 0x2d,
	/*48*/ 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0, 0x36, 0x1d, 0, 0x38, 0x39, 0xb8, 0,
	/*64*/ 0x9d, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xd2, 0xd3, 0, 0, 0xcb,
	/*80*/ 0xc7, 0xcf, 0, 0xc8, 0xd0, 0xc9, 0xd1, 0, 0, 0xcd, 0x45, 0x47, 0x4b, 0x4f, 0, 0xb5,
	/*96*/ 0x48, 0x4c, 0x50, 0x52, 0x37, 0x49, 0x4d, 0x51, 0x53, 0x4a, 0x4e, 0, 0x9c, 0, 0x01, 0,
	/*112*/ 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58, 0xb7, 0x46, 0xff, 0
};

static void sunpci_pic_keyboard(uae_u8 v)
{
	int station = v & 0x7f;
	uae_u16 code = sunpci_station_to_scancode[station];
	if (!code) {
		write_log(_T("SUNPCI: PIC keyboard: unknown station %02x\n"), station);
		return;
	}
	if (code == 0xff && (v & 0x80)) {
		return; // Pause has no break code
	}
#if SUNPCI_DEBUG_CSR
	write_log(_T("SUNPCI: PIC keyboard station %02x %s -> scancode %03x\n"), station, (v & 0x80) ? _T("break") : _T("make"), code);
#endif
	// 86Box numbers the E0 keys 1xx and takes Pause as E1 1D
	uae_u16 scan = code == 0xff ? 0xe11d : (code & 0x80) ? 0x100 | (code & 0x7f) : code;
	keyboard_input(!(v & 0x80), scan);
}

// Sun's first-generation mouse driver defaults to COM2. The PIC supplies
// the serial byte stream; bit 7 on its final packet byte is outside the
// seven data bits selected by a Microsoft mouse driver.
static void sunpci_mouse_identify(serial_t *serial, void *p)
{
	struct sunpci_state *s = (struct sunpci_state *)p;
	// a new identification replaces the packets still queued
	s->mouse_head = s->mouse_tail = 0;
	serial_write_fifo(serial, 'M');
}

static uae_u8 sunpci_mouse_status(struct sunpci_state *s)
{
	unsigned used = (s->mouse_head - s->mouse_tail) & 255;
	// Leave room for a complete three-byte PIC packet.
	return used > 252 ? 0x10 : 0;
}

static void sunpci_pic_mouse(struct sunpci_state *s, uae_u8 data)
{
	serial_t *serial = (serial_t *)s->serial;
	if (!serial || !(serial->mctrl & 2) || (serial->mctrl & 0x10)) {
		return;
	}
	if (((s->mouse_head + 1) & 255) == s->mouse_tail) {
		return;
	}
	unsigned bits = 5 + (serial->lcr & 3);
	s->mouse_queue[s->mouse_head] = data & ((1U << bits) - 1);
	s->mouse_head = (s->mouse_head + 1) & 255;
}

// serial_write_fifo() fills the UART's receive shift register, which its
// receive timer moves on once per character time; a second byte before
// then replaces the first. Pass the queued bytes on one at a time, as the
// shift register and the holding register or FIFO behind it have room.
static void sunpci_mouse_feed(struct sunpci_state *s)
{
	serial_t *serial = (serial_t *)s->serial;
	if (!serial || s->mouse_tail == s->mouse_head || serial->out_new != 0xffff) {
		return;
	}
	if (serial->fifo_enabled ? fifo_get_full(serial->rcvr_fifo) : (serial->lsr & 1)) {
		return;
	}
	serial_write_fifo(serial, s->mouse_queue[s->mouse_tail]);
	s->mouse_tail = (s->mouse_tail + 1) & 255;
}

// The control write starts a command (6 = write, 2 = read). It completes at
// once, so the busy bit is clear again by the time the host polls it.
static void sunpci_pic_command(struct sunpci_state *s, uae_u8 control, bool secondary)
{
	struct db21554 *b = &s->db;
	uae_u16 cmd = get_le16(&b->csr[DB_CSR_PIC_COMMAND]);
	uae_u8 data = b->csr[DB_CSR_PIC_DATA];
	bool write = (control & PIC_CONTROL_WRITE) != 0;

	if (control & PIC_CONTROL_BUSY) {
		if (write) {
			switch (cmd)
			{
			case PIC_CMD_MOUSE:
				sunpci_pic_mouse(s, data);
				break;
			case PIC_CMD_KEYBOARD:
				sunpci_pic_keyboard(data);
				break;
			case PIC_CMD_IRQ_SET:
				sunpci_pic_host_irq(b, true, data);
				break;
			case PIC_CMD_IRQ_CLEAR:
				sunpci_pic_host_irq(b, false, data);
				break;
			case PIC_CMD_LED:
				b->pic_led = data;
				break;
			case PIC_CMD_SMI_RATE:
#if SUNPCI_DEBUG_CSR
				write_log(_T("SUNPCI: PIC SMI rate %d\n"), data);
#endif
				b->pic_smi_rate = data;
				s->smi_lines = 0;
				break;
			default:
				write_log(_T("SUNPCI: PIC write command %04x data %02x from %s\n"), cmd, data, secondary ? _T("card") : _T("host"));
				break;
			}
		} else {
			switch (cmd)
			{
			case PIC_CMD_STATUS:
				b->csr[DB_CSR_PIC_DATA] = sunpci_mouse_status(s);
				break;
			case PIC_CMD_LED:
				b->csr[DB_CSR_PIC_DATA] = b->pic_led << 4;
				break;
			case PIC_CMD_SMI_RATE:
				b->csr[DB_CSR_PIC_DATA] = b->pic_smi_rate;
				break;
			default:
				write_log(_T("SUNPCI: PIC read command %04x from %s\n"), cmd, secondary ? _T("card") : _T("host"));
				b->csr[DB_CSR_PIC_DATA] = 0;
				break;
			}
		}
	}
	b->csr[DB_CSR_PIC_CONTROL] = control & ~PIC_CONTROL_BUSY;
}

// The I/O window ends before the lookup table; LUT data reaches the entry
// selected by the LUT offset register.
static int db_csr_index(struct db21554 *b, uae_u32 a)
{
	if (a >= DB_CSR_LUT_DATA && a < DB_CSR_LUT_DATA + 4) {
		return DB_CSR_LUT + (b->csr[DB_CSR_LUT_OFFSET] & 0xfc) + (a & 3);
	}
	return a;
}

static uae_u32 db_csr_read(struct sunpci_state *s, uae_u32 addr, int size, bool secondary)
{
	struct db21554 *b = &s->db;
	addr &= DB_CSR_SIZE - 1;
	uae_u32 v = 0;
	for (int i = 0; i < size; i++) {
		uae_u32 a = (addr + i) & (DB_CSR_SIZE - 1);
		uae_u8 c;
		if (a >= DB_CSR_P_CLR_IRQ && a < DB_CSR_SCRATCH) {
			c = (uae_u8)(db_doorbell_read(b, a) >> ((a & 1) * 8));
		} else {
			c = b->csr[db_csr_index(b, a)];
		}
		v |= c << (i * 8);
	}
#if SUNPCI_DEBUG_CSR > 1
	write_log(_T("SUNPCI: %s CSR read %03x/%d = %08x\n"), secondary ? _T("card") : _T("host"), addr, size, v);
#endif
	return v;
}

static void db_csr_write(struct sunpci_state *s, uae_u32 addr, uae_u32 v, int size, bool secondary)
{
	struct db21554 *b = &s->db;
	addr &= DB_CSR_SIZE - 1;
#if SUNPCI_DEBUG_CSR
	if (secondary) {
		write_log(_T("SUNPCI: card CSR write %03x/%d = %08x PC=%04x:%08x\n"), addr, size, v, CS, cpu_state.pc);
	} else {
		write_log(_T("SUNPCI: host CSR write %03x/%d = %08x\n"), addr, size, v);
	}
#endif
	if (addr >= DB_CSR_P_CLR_IRQ && addr < DB_CSR_SCRATCH) {
		if (size == 1) {
			uae_u16 w = (addr & 1) ? (uae_u16)((v & 0xff) << 8) : (uae_u16)(v & 0xff);
			db_doorbell_write(b, addr & ~1, w);
		} else {
			for (int i = 0; i < size; i += 2) {
				db_doorbell_write(b, (addr + i) & ~1, (uae_u16)(v >> (i * 8)));
			}
		}
		db_irq_update(s);
		return;
	}
	for (int i = 0; i < size; i++) {
		b->csr[db_csr_index(b, (addr + i) & (DB_CSR_SIZE - 1))] = (uae_u8)(v >> (i * 8));
	}
	if (addr <= DB_CSR_PIC_CONTROL && addr + size > DB_CSR_PIC_CONTROL) {
		sunpci_pic_command(s, b->csr[DB_CSR_PIC_CONTROL], secondary);
	}
}

/* Upstream windows: card accesses to host memory */

// A translated base register has only the bits its window's BAR has, so a
// base that is not aligned to the window is rounded down to it, as on the
// chip. Sun's driver aligns its 128 KB RM region to 128 KB for this reason.
static uae_u32 db_us_translate(struct db21554 *b, int bar, int treg, uae_u32 off)
{
	uae_u32 size = b->sec_bar_size[bar] & ~1;
	uae_u32 tr = db_reg32(b, treg);
	if (size) {
		tr &= ~(size - 1);
	}
	return tr + off;
}

static bool db_us_address(struct sunpci_state *s, int win, uae_u32 off, uae_u32 *haddr)
{
	struct db21554 *b = &s->db;
	switch (win)
	{
	case DB_WIN_US0:
		*haddr = db_us_translate(b, 2, DB_CONF_US_IO_MEM0_TR, off);
		return true;
	case DB_WIN_US1:
		*haddr = db_us_translate(b, 3, DB_CONF_US_MEM1_TR, off);
		return true;
	case DB_WIN_US2:
	{
		uae_u32 page = db_us2_page_size(b);
		if (!page) {
			return false;
		}
		uae_u32 idx = off / page;
		if (idx >= DB_LUT_ENTRIES) {
			return false;
		}
		uae_u32 e = get_le32(&b->csr[DB_CSR_LUT + idx * 4]);
		if (!(e & 1)) {
			return false;
		}
		*haddr = (e & ~(page - 1)) + (off & (page - 1));
		return true;
	}
	}
	return false;
}

// The bridge masters on the host's bus only with the primary command
// register's bus master bit set; without it the access ends in a master
// abort: reads return all ones and writes are dropped.
static bool db_us_master(struct sunpci_state *s)
{
	if (s->db.pri[0x04] & 4) {
		return true;
	}
	if (!s->db.master_abort_logged) {
		s->db.master_abort_logged = true;
		write_log(_T("SUNPCI: upstream access with primary bus mastering off, PC=%04x:%08x\n"), CS, cpu_state.pc);
	}
	return false;
}

static uae_u32 db_us_read(struct sunpci_state *s, int win, uae_u32 off, int size)
{
	uae_u32 haddr;
	uae_u8 buf[4];
	if (!s->host_pcibs || !db_us_address(s, win, off, &haddr) || !db_us_master(s)) {
		return 0xffffffff;
	}
	pci_read_dma(s->host_pcibs, haddr, buf, size);
	uae_u32 v = 0;
	for (int i = 0; i < size; i++) {
		v |= buf[i] << (i * 8);
	}
	return v;
}

static void db_us_write(struct sunpci_state *s, int win, uae_u32 off, uae_u32 v, int size)
{
	uae_u32 haddr;
	uae_u8 buf[4];
	if (!s->host_pcibs || !db_us_address(s, win, off, &haddr) || !db_us_master(s)) {
		return;
	}
	for (int i = 0; i < size; i++) {
		buf[i] = (uae_u8)(v >> (i * 8));
	}
	pci_write_dma(s->host_pcibs, haddr, buf, size);
}

// BAR changes need no remapping: sunpci_hole_* decodes the BARs on access.
static void sunpci_card_remap(struct sunpci_state *s)
{
}

/* BIOS flash: Am29F002T (Sun's sunpciflash also knows the MXIC, PMC and
 * EON equivalents), 256 KB with the boot block on top. The card decodes the
 * upper 128 KB at E0000-FFFFF and the whole part below 4 GB. Award's ESCD
 * update identifies it by its autoselect ID and programs it in place.
 * Program and erase complete at once, so status polls see the final data.
 */

#define FLASH_MFG_AMD 0x01
#define FLASH_DEV_29F002T 0xb0

enum {
	FLASH_READ,
	FLASH_UNLOCK1,
	FLASH_UNLOCK2,
	FLASH_PROGRAM,
	FLASH_ERASE,
	FLASH_ERASE_UNLOCK1,
	FLASH_ERASE_UNLOCK2
};

static uae_u32 sunpci_flash_offset(uint32_t addr)
{
	if (addr >= 0xfffc0000) {
		return addr - 0xfffc0000;
	}
	return addr - 0xe0000 + 0x20000;
}

// Top boot sectors: 3 x 64 KB, 32 KB, 2 x 8 KB, 16 KB
static void sunpci_flash_sector(uae_u32 off, uae_u32 *start, uae_u32 *size)
{
	if (off < 0x30000) {
		*start = off & ~0xffff;
		*size = 0x10000;
	} else if (off < 0x38000) {
		*start = 0x30000;
		*size = 0x8000;
	} else if (off < 0x3c000) {
		*start = off & ~0x1fff;
		*size = 0x2000;
	} else {
		*start = 0x3c000;
		*size = 0x4000;
	}
}

// Code runs straight from the array, except while the part answers with
// IDs or status.
static void sunpci_flash_update_exec(struct sunpci_state *s)
{
	bool array = !s->flash_autoselect;
	mem_mapping_set_exec(&s->flash_map, array ? s->bios + 0x20000 : NULL);
	mem_mapping_set_exec(&s->flash_high_map, array ? s->bios : NULL);
	flushmmucache();
}

static void sunpci_flash_reset(struct sunpci_state *s)
{
	s->flash_state = FLASH_READ;
	if (s->flash_autoselect) {
		s->flash_autoselect = false;
		sunpci_flash_update_exec(s);
	}
}

static uint8_t sunpci_flash_readb(uint32_t addr, void *priv)
{
	struct sunpci_state *s = (struct sunpci_state *)priv;
	uae_u32 off = sunpci_flash_offset(addr);
	if (s->flash_autoselect) {
		switch (off & 3)
		{
		case 0:
			return FLASH_MFG_AMD;
		case 1:
			return FLASH_DEV_29F002T;
		default:
			return 0x00; // sector not protected
		}
	}
	return s->bios[off];
}
static uint16_t sunpci_flash_readw(uint32_t addr, void *priv)
{
	return sunpci_flash_readb(addr, priv) | (sunpci_flash_readb(addr + 1, priv) << 8);
}
static uint32_t sunpci_flash_readl(uint32_t addr, void *priv)
{
	return sunpci_flash_readw(addr, priv) | (sunpci_flash_readw(addr + 2, priv) << 16);
}

static void sunpci_flash_writeb(uint32_t addr, uint8_t val, void *priv)
{
	struct sunpci_state *s = (struct sunpci_state *)priv;
	uae_u32 off = sunpci_flash_offset(addr);
	uae_u32 cmd = off & 0x7ff;
	uae_u32 start, size;

	if (s->flash_state == FLASH_PROGRAM) {
		// programming only clears bits
		s->bios[off] &= val;
		s->flash_state = FLASH_READ;
		return;
	}
	if (val == 0xf0) {
		sunpci_flash_reset(s);
		return;
	}
	switch (s->flash_state)
	{
	case FLASH_READ:
		s->flash_state = (cmd == 0x555 && val == 0xaa) ? FLASH_UNLOCK1 : FLASH_READ;
		break;
	case FLASH_UNLOCK1:
		s->flash_state = (cmd == 0x2aa && val == 0x55) ? FLASH_UNLOCK2 : FLASH_READ;
		break;
	case FLASH_UNLOCK2:
		s->flash_state = FLASH_READ;
		if (cmd != 0x555) {
			break;
		}
		if (val == 0x90) {
#if SUNPCI_DEBUG_CSR
			write_log(_T("SUNPCI: flash autoselect at %05x PC=%04x:%08x\n"), off, CS, cpu_state.pc);
#endif
			if (!s->flash_autoselect) {
				s->flash_autoselect = true;
				sunpci_flash_update_exec(s);
			}
		} else if (val == 0xa0) {
			s->flash_state = FLASH_PROGRAM;
		} else if (val == 0x80) {
			s->flash_state = FLASH_ERASE;
		}
		break;
	case FLASH_ERASE:
		s->flash_state = (cmd == 0x555 && val == 0xaa) ? FLASH_ERASE_UNLOCK1 : FLASH_READ;
		break;
	case FLASH_ERASE_UNLOCK1:
		s->flash_state = (cmd == 0x2aa && val == 0x55) ? FLASH_ERASE_UNLOCK2 : FLASH_READ;
		break;
	case FLASH_ERASE_UNLOCK2:
		s->flash_state = FLASH_READ;
		if (cmd == 0x555 && val == 0x10) {
			memset(s->bios, 0xff, SUNPCI_BIOS_SIZE);
			write_log(_T("SUNPCI: flash chip erase\n"));
		} else if (val == 0x30) {
			sunpci_flash_sector(off, &start, &size);
			memset(s->bios + start, 0xff, size);
			write_log(_T("SUNPCI: flash sector erase %05x-%05x\n"), start, start + size - 1);
		}
		break;
	}
}
static void sunpci_flash_writew(uint32_t addr, uint16_t val, void *priv)
{
	sunpci_flash_writeb(addr, (uint8_t)val, priv);
	sunpci_flash_writeb(addr + 1, val >> 8, priv);
}
static void sunpci_flash_writel(uint32_t addr, uint32_t val, void *priv)
{
	sunpci_flash_writew(addr, (uint16_t)val, priv);
	sunpci_flash_writew(addr + 2, val >> 16, priv);
}

/* SiS 5597/5598 */

static int sunpci_shadow_state(uae_u8 v, int rbit, int wbit)
{
	int state = (v & rbit) ? MEM_READ_INTERNAL : MEM_READ_EXTERNAL;
	state |= (v & wbit) ? MEM_WRITE_INTERNAL : MEM_WRITE_EXTERNAL;
	return state;
}

// Shadow RAM: 70-75 cover C0000-EFFFF in 32 KB steps (bits 7/5 read/write
// for the low 16 KB, bits 3/1 for the high 16 KB), 76 covers F0000-FFFFF.
static void sunpci_sis_shadow(struct sunpci_dev *d)
{
	for (int i = 0x70; i <= 0x75; i++) {
		uae_u32 base = 0xc0000 + (i - 0x70) * 0x8000;
		mem_set_mem_state_both(base, 0x4000, sunpci_shadow_state(d->cfg[i], 0x80, 0x20));
		mem_set_mem_state_both(base + 0x4000, 0x4000, sunpci_shadow_state(d->cfg[i], 0x08, 0x02));
	}
	mem_set_mem_state_both(0xf0000, 0x10000, sunpci_shadow_state(d->cfg[0x76], 0x80, 0x20));
	flushmmucache();
}

// SMRAM (A3): bits 7-6 select the window, bit 4 opens it outside SMM.
// As 86Box's SiS 5571: E0000 from RAM E0000, A0000 or B0000 (32 KB), or
// A0000 from RAM A0000 (64 KB).
static void sunpci_smram_update(struct sunpci_state *s)
{
	static const uae_u32 host[4] = { 0xe0000, 0xe0000, 0xe0000, 0xa0000 };
	static const uae_u32 rambase[4] = { 0xe0000, 0xa0000, 0xb0000, 0xa0000 };
	static const uae_u32 size[4] = { 0x8000, 0x8000, 0x8000, 0x10000 };
	uae_u8 a3 = s->sis_host ? s->sis_host->cfg[0xa3] : 0;
	int mode = a3 >> 6;
	// Translated code is kept apart by SMM state, but not by which memory
	// the window and open bit select. Drop it when they change.
	if ((a3 & 0xd0) != s->smram_layout) {
		s->smram_layout = a3 & 0xd0;
		mem_invalidate_range(0xa0000, 0xfffff);
	}
	smram_disable(s->smram);
	smram_enable(s->smram, host[mode], rambase[mode], size[mode], (a3 & 0x10) != 0, 1);
	flushmmucache();
}

// The PIC drives the SiS EXTSMI# input. Record the enabled source before
// entering SMM so the firmware can distinguish it from software APM SMI.
// SiS5597/5598 datasheet: 93[2] pin select, 9B[1] enable, 9D[1] status.
static void sunpci_external_smi(struct sunpci_state *s)
{
	if (!s->sis_host || !(s->sis_host->cfg[0x93] & 4) ||
		!(s->sis_host->cfg[0x9b] & 2)) {
		return;
	}
	s->sis_host->cfg[0x9d] |= 2;
	// The CPU latches an SMI raised in SMM and takes it after RSM
	smi_raise();
}

static void sunpci_sis_host_config_written(struct sunpci_dev *d, int reg, uae_u8 old)
{
	if (reg >= 0x70 && reg <= 0x76 && d->cfg[reg] != old) {
		sunpci_sis_shadow(d);
		// SMRAM decoding takes priority over the shadow attributes
		sunpci_smram_update(sunpci);
	}
	if (reg == 0xa3) {
#if SUNPCI_DEBUG_CSR
		write_log(_T("SUNPCI: SMRAM control %02x\n"), d->cfg[0xa3]);
#endif
		sunpci_smram_update(sunpci);
	}
	if (reg == 0x93 && (d->cfg[0x9b] & 0x01) && (d->cfg[0x93] & 0x02) && !(old & 0x02)) {
		// software SMI (APM). The handler rewrites 93 with the bit still
		// set before clearing it, so only a 0 to 1 write triggers.
		d->cfg[0x9d] |= 0x01;
		smi_raise();
	}
	if (reg == 0x9d) {
		// status: write 0 to clear
		d->cfg[0x9d] = old & d->cfg[0x9d];
	}
}

static void sunpci_sis_isa_config_written(struct sunpci_dev *d, int reg, uae_u8 old)
{
	if (reg >= 0x41 && reg <= 0x44) {
		sunpci_card_irq_update(sunpci);
	}
}

// The 21554's secondary INTA# is wired to PIRQA (SiS PCI-to-ISA 41).
static void sunpci_card_irq_update(struct sunpci_state *s)
{
	struct db21554 *b = &s->db;
	bool active = (b->s_irq & ~b->s_mask) != 0 && !(b->sec[0x05] & 0x04);
	int irq = -1;
	if (s->sis_isa) {
		uae_u8 r = s->sis_isa->cfg[0x41];
		if (!(r & 0x80) && (r & 15)) {
			irq = r & 15;
		}
	}
	if (b->card_irq && (!active || irq != b->card_pic_irq)) {
		if (b->card_pic_irq >= 0) {
			picintclevel(1 << b->card_pic_irq, &b->card_irq_state);
		}
		b->card_irq = false;
	}
	if (active && irq >= 0 && !b->card_irq) {
		picintlevel(1 << irq, &b->card_irq_state);
		b->card_irq = true;
		b->card_pic_irq = irq;
	}
}

/* SiS 5597/5598 VGA: the SunPCi VGA board in gfxboard.cpp */

static svga_t *sunpci_vga_svga(struct sunpci_state *s)
{
	return s->vga ? sis5598_svga(s->vga) : NULL;
}

// The legacy window follows the graphics controller memory map (GR06).
static bool sunpci_vga_legacy_hit(svga_t *svga, uint32_t addr)
{
	switch ((svga->gdcreg[6] >> 2) & 3)
	{
	case 0:
		return true;
	case 1:
		return addr < 0xb0000;
	case 2:
		return addr >= 0xb0000 && addr < 0xb8000;
	default:
		return addr >= 0xb8000;
	}
}

static uae_u32 sunpci_hole_read(uint32_t addr, int size);
static void sunpci_hole_write(uint32_t addr, uae_u32 v, int size);

// A0000-BFFFF: the VGA's legacy window when its decode is on and GR06
// selects the address, otherwise the 21554 windows (Sun's bootblock puts the
// upstream window at B0000).
static bool sunpci_vga_claims(struct sunpci_state *s, svga_t *svga, uint32_t addr)
{
	return svga && s->sis_vga && (s->sis_vga->cfg[0x04] & 2) && sunpci_vga_legacy_hit(svga, addr);
}

static uint8_t sunpci_vga_readb(uint32_t addr, void *priv)
{
	svga_t *svga = sunpci_vga_svga(sunpci);
	if (!sunpci_vga_claims(sunpci, svga, addr)) {
		return (uint8_t)sunpci_hole_read(addr, 1);
	}
	return svga_read(addr, svga);
}
static uint16_t sunpci_vga_readw(uint32_t addr, void *priv)
{
	return sunpci_vga_readb(addr, priv) | (sunpci_vga_readb(addr + 1, priv) << 8);
}
static uint32_t sunpci_vga_readl(uint32_t addr, void *priv)
{
	return sunpci_vga_readw(addr, priv) | (sunpci_vga_readw(addr + 2, priv) << 16);
}
static void sunpci_vga_writeb(uint32_t addr, uint8_t val, void *priv)
{
	svga_t *svga = sunpci_vga_svga(sunpci);
	if (sunpci_vga_claims(sunpci, svga, addr)) {
		svga_write(addr, val, svga);
	} else {
		sunpci_hole_write(addr, val, 1);
	}
}
static void sunpci_vga_writew(uint32_t addr, uint16_t val, void *priv)
{
	sunpci_vga_writeb(addr, (uint8_t)val, priv);
	sunpci_vga_writeb(addr + 1, val >> 8, priv);
}
static void sunpci_vga_writel(uint32_t addr, uint32_t val, void *priv)
{
	sunpci_vga_writew(addr, (uint16_t)val, priv);
	sunpci_vga_writew(addr + 2, val >> 16, priv);
}

static uae_u32 sunpci_dev_bar(struct sunpci_dev *d, int bar)
{
	uae_u32 v = get_le32(&d->cfg[0x10 + bar * 4]);
	return v & ((v & 1) ? ~3 : ~15);
}

// The legacy window is decoded per access (sunpci_vga_claims), so changes of
// the command register or GR06 need nothing here.
static void sunpci_vga_update_legacy(struct sunpci_state *s)
{
}

static void sunpci_vga_config_written(struct sunpci_dev *d, int reg, uae_u8 old)
{
	struct sunpci_state *s = sunpci;
	if (reg == 0x04) {
		sunpci_vga_update_legacy(s);
	}
}

// The BIOS's SMM display capture reprograms the sequencer, graphics and
// attribute registers to read the planes and restores them before RSM. A
// frame drawn while it ran is not presented: monitor 2 keeps the previous
// frame and redraws the next one in full. Holding whole frames keeps the
// renderers, which also advance the display address, running on every line.
static bool sunpci_vga_smm_frame;

static void sunpci_vga_vblank_start(svga_t *svga)
{
	if (sunpci_vga_smm_frame) {
		svga->override = 1;
	}
}

static void sunpci_vga_vsync(svga_t *svga)
{
	if (svga->override && sunpci_vga_smm_frame) {
		svga->override = 0;
		svga->fullchange = svga->monitor->mon_changeframecount;
	}
	sunpci_vga_smm_frame = in_smm != 0;
}

// Legacy VGA ports, and the relocated copy of 380-3FF behind BAR2.
static bool sunpci_vga_io(struct sunpci_state *s, uint16_t port, int dir, uint32_t *val, int size)
{
	if (!s->vga || !s->sis_vga) {
		return false;
	}
	uint16_t vport = port;
	if (!(s->sis_vga->cfg[0x04] & 1)) {
		return false;
	}
	if (port < 0x3b0 || port >= 0x3e0) {
		uae_u32 base = sunpci_dev_bar(s->sis_vga, 2);
		if (!(s->sis_vga->cfg[0x04] & 1) || !base || port < base || port >= base + 0x80) {
			return false;
		}
		vport = 0x380 + (port - base);
	}
	if (dir) {
		uint32_t v = 0;
		for (int i = 0; i < size; i++) {
			v |= sis5598_in(vport + i, s->vga) << (i * 8);
		}
		*val = v;
	} else {
		svga_t *svga = sunpci_vga_svga(s);
		if (in_smm) {
			sunpci_vga_smm_frame = true;
		}
		for (int i = 0; i < size; i++) {
			sis5598_out(vport + i, (uint8_t)(*val >> (i * 8)), s->vga);
		}
		// In host-window mode the capture also sets Screen Off and never
		// clears it, which blanks an external monitor from POST until
		// Windows loads Sun's display driver. Monitor 2 keeps showing the
		// picture; SR01 still reads back as written.
		if (in_smm && vport <= 0x3c5 && vport + size > 0x3c5 && (svga->seqaddr & 0xf) == 1 && (svga->scrblank & 0x20)) {
			svga->scrblank &= ~0x20;
			// the renderer follows the blanking at recalculation
			svga_recalctimings(svga);
		}
		if (vport <= 0x3cf && vport + size > 0x3ce) {
			sunpci_vga_update_legacy(s);
		}
	}
	return true;
}

/* SiS 5597/5598 USB OHCI (device 1 function 2)
 *
 * Only a register file: no root hub ports. The BIOS's USB legacy SMM code
 * programs it through BAR0; without the function its BAR reads as all ones
 * and those writes wrap around into the real-mode interrupt vectors. */

#define OHCI_REVISION 0x00
#define OHCI_COMMAND_STATUS 0x08
#define OHCI_FM_INTERVAL 0x34
#define OHCI_RH_DESCRIPTOR_A 0x48

static void sunpci_ohci_reset(struct sunpci_state *s)
{
	memset(s->ohci, 0, sizeof s->ohci);
	s->ohci[OHCI_REVISION / 4] = 0x10;
	s->ohci[OHCI_FM_INTERVAL / 4] = 0x2edf;
	s->ohci[OHCI_RH_DESCRIPTOR_A / 4] = 0x02000000;
}

static int sunpci_ohci_logs;

static uae_u32 sunpci_ohci_read(uint32_t addr)
{
	struct sunpci_state *s = sunpci;
	uae_u32 off = (addr - sunpci_dev_bar(s->sis_usb, 0)) & 0xfff;
	uae_u32 v = off < sizeof s->ohci ? s->ohci[off / 4] : 0;
#if SUNPCI_DEBUG_CSR
	if (sunpci_ohci_logs < 100) {
		sunpci_ohci_logs++;
		write_log(_T("SUNPCI: OHCI read %03x = %08x\n"), off, v);
	}
#endif
	return v;
}

static void sunpci_ohci_write(uint32_t addr, uae_u32 v)
{
	struct sunpci_state *s = sunpci;
	uae_u32 off = (addr - sunpci_dev_bar(s->sis_usb, 0)) & 0xfff;
#if SUNPCI_DEBUG_CSR
	if (sunpci_ohci_logs < 100) {
		sunpci_ohci_logs++;
		write_log(_T("SUNPCI: OHCI write %03x = %08x\n"), off, v);
	}
#endif
	if (off == OHCI_REVISION || off >= sizeof s->ohci) {
		return;
	}
	if (off == OHCI_COMMAND_STATUS && (v & 1)) {
		// host controller reset completes at once
		sunpci_ohci_reset(s);
		return;
	}
	s->ohci[off / 4] = v;
}

static uint8_t sunpci_ohci_readb(uint32_t addr, void *priv)
{
	return (uint8_t)(sunpci_ohci_read(addr & ~3) >> ((addr & 3) * 8));
}
static uint16_t sunpci_ohci_readw(uint32_t addr, void *priv)
{
	return (uint16_t)(sunpci_ohci_read(addr & ~3) >> ((addr & 2) * 8));
}
static uint32_t sunpci_ohci_readl(uint32_t addr, void *priv)
{
	return sunpci_ohci_read(addr);
}
static void sunpci_ohci_writeb(uint32_t addr, uint8_t val, void *priv)
{
	int sh = (addr & 3) * 8;
	uae_u32 v = sunpci_ohci_read(addr & ~3);
	sunpci_ohci_write(addr & ~3, (v & ~(0xff << sh)) | (val << sh));
}
static void sunpci_ohci_writew(uint32_t addr, uint16_t val, void *priv)
{
	int sh = (addr & 2) * 8;
	uae_u32 v = sunpci_ohci_read(addr & ~3);
	sunpci_ohci_write(addr & ~3, (v & ~(0xffff << sh)) | (val << sh));
}
static void sunpci_ohci_writel(uint32_t addr, uint32_t val, void *priv)
{
	sunpci_ohci_write(addr, val);
}

/* Card PCI memory space
 *
 * One handler covers the card's PCI memory above its RAM and decodes each
 * access against the current BARs, so BIOS and driver BAR changes need no
 * remapping. The BIOS packs 4 KB BARs (21554 CSRs, USB) next to each other. */

enum {
	HOLE_NONE,
	HOLE_CSR,
	HOLE_US0,
	HOLE_US1,
	HOLE_US2,
	HOLE_USB,
	HOLE_LFB
};

static bool sunpci_hole_hit(uae_u32 addr, uae_u32 base, uae_u32 size, uae_u32 *off)
{
	if (!base || addr < base || addr - base >= size) {
		return false;
	}
	*off = addr - base;
	return true;
}

static int sunpci_hole_decode(struct sunpci_state *s, uae_u32 addr, uae_u32 *off)
{
	struct db21554 *b = &s->db;
	if (b->sec[0x04] & 2) {
		static const int kind[5] = { HOLE_CSR, HOLE_NONE, HOLE_US0, HOLE_US1, HOLE_US2 };
		for (int bar = 0; bar < 5; bar++) {
			uae_u32 size = b->sec_bar_size[bar];
			if (kind[bar] != HOLE_NONE && size && !(size & 1) && sunpci_hole_hit(addr, db_bar(b->sec, bar), size, off)) {
				return kind[bar];
			}
		}
	}
	if (s->sis_usb && (s->sis_usb->cfg[0x04] & 2) && sunpci_hole_hit(addr, sunpci_dev_bar(s->sis_usb, 0), 0x1000, off)) {
		return HOLE_USB;
	}
	if (s->sis_vga && s->vga && (s->sis_vga->cfg[0x04] & 2) && sunpci_hole_hit(addr, sunpci_dev_bar(s->sis_vga, 0), SUNPCI_VGA_LFB_SIZE, off)) {
		return HOLE_LFB;
	}
	return HOLE_NONE;
}

// Sun's drivers wait for the host by polling bridge registers and ring
// pointers in host memory. Only the host changes those, and it runs only
// between time slices, so once the card repeats the same reads it cannot
// see a change before the slice ends: let the rest of it pass as time.
#define SUNPCI_SPIN_READS 8
static struct {
	uae_u32 key, value;
} sunpci_spin_recent[16];
static int sunpci_spin_next, sunpci_spin_count;

static void sunpci_spin_reset(void)
{
	sunpci_spin_count = 0;
}

static uae_u32 sunpci_spin_read(uae_u32 key, uae_u32 value)
{
	// The VxD reads the head and tail of every PM ring per round.
	for (int i = 0; i < 16; i++) {
		if (sunpci_spin_recent[i].key == key && sunpci_spin_recent[i].value == value) {
			if (++sunpci_spin_count >= SUNPCI_SPIN_READS) {
				sunpci_spin_count = 0;
				cycles = 0;
				cpu_end_timeslice = 1;
			}
			return value;
		}
	}
	sunpci_spin_recent[sunpci_spin_next].key = key;
	sunpci_spin_recent[sunpci_spin_next].value = value;
	sunpci_spin_next = (sunpci_spin_next + 1) & 15;
	sunpci_spin_count = 0;
	return value;
}

// SUNPCI.VXD waits for a PM reply by calling a helper that returns the
// bytes queued in the down ring, from its head and tail in card RAM.
// Those are plain memory reads the bridge never sees, so find the helper
// by its code when the recompiler first meets it, and pass the ring
// values it is about to read through the same spin check.
static const uae_u8 sunpci_vxd_ring_used[] = {
	0x55, 0x8b, 0x4c, 0x24, 0x08, 0x8b, 0xec, 0x8b, 0x01, 0x8b, 0x49, 0x04,
	0x8b, 0x55, 0x0c, 0x3b, 0xc8, 0x89, 0x02, 0x89, 0x4a, 0x04, 0x75, 0x04,
	0x33, 0xc0, 0xeb, 0x0d, 0x73, 0x04, 0x2b, 0xc1, 0xeb, 0x07, 0x2b, 0xc1,
	0x05, 0xf8, 0xff, 0x01, 0x00, 0x5d, 0xc3
};
static uae_u32 sunpci_vxd_ring_used_phys;

static void sunpci_block_hook(uint32_t pc, uint32_t phys)
{
	bool match = (phys & 0xfff) <= 0x1000 - sizeof sunpci_vxd_ring_used;
	for (int i = 0; match && i < (int)sizeof sunpci_vxd_ring_used; i++) {
		match = mem_readb_phys(phys + i) == sunpci_vxd_ring_used[i];
	}
	if (match) {
		if (pc != cpu_spin_pc) {
			write_log(_T("SUNPCI: VxD ring wait helper at %08x\n"), pc);
		}
		cpu_spin_pc = pc;
		sunpci_vxd_ring_used_phys = phys;
	} else if (pc == cpu_spin_pc) {
		cpu_spin_pc = 0xffffffff;
	}
}

static bool sunpci_card_readl(uae_u32 addr, uae_u32 *v)
{
	uae_u32 phys = (cr0 & 0x80000000) ? mmutranslate_noabrt(addr, 0) : addr;
	if (phys == 0xffffffff || (addr & 0xfff) > 0xffc) {
		return false;
	}
	*v = mem_readl_phys(phys);
	return true;
}

static void sunpci_spin_hook(uint32_t phys)
{
	uae_u32 ring, head, tail;
	if (phys != sunpci_vxd_ring_used_phys || !sunpci_card_readl(ss + ESP + 4, &ring)
		|| !sunpci_card_readl(ring, &head) || !sunpci_card_readl(ring + 4, &tail)) {
		return;
	}
	sunpci_spin_read(0x50000000 | (ring & 0x0fffffff), head);
	sunpci_spin_read(0x50000000 | ((ring + 4) & 0x0fffffff), tail);
}

static uae_u32 sunpci_hole_read(uint32_t addr, int size)
{
	struct sunpci_state *s = sunpci;
	uae_u32 off;
	switch (sunpci_hole_decode(s, addr, &off))
	{
	case HOLE_CSR:
		return sunpci_spin_read(0x10000000 | off, db_csr_read(s, off, size, true));
	case HOLE_US0:
		return sunpci_spin_read(0x20000000 | off, db_us_read(s, DB_WIN_US0, off, size));
	case HOLE_US1:
		return sunpci_spin_read(0x30000000 | off, db_us_read(s, DB_WIN_US1, off, size));
	case HOLE_US2:
		return sunpci_spin_read(0x40000000 | off, db_us_read(s, DB_WIN_US2, off, size));
	case HOLE_USB:
		return size == 4 ? sunpci_ohci_readl(addr, s) : size == 2 ? sunpci_ohci_readw(addr, s) : sunpci_ohci_readb(addr, s);
	case HOLE_LFB:
		return size == 4 ? svga_readl_linear(addr, sunpci_vga_svga(s)) : size == 2 ? svga_readw_linear(addr, sunpci_vga_svga(s)) : svga_read_linear(addr, sunpci_vga_svga(s));
	}
	return size == 4 ? 0xffffffff : size == 2 ? 0xffff : 0xff;
}

static void sunpci_hole_write(uint32_t addr, uae_u32 v, int size)
{
	struct sunpci_state *s = sunpci;
	uae_u32 off;
	sunpci_spin_reset();
	switch (sunpci_hole_decode(s, addr, &off))
	{
	case HOLE_CSR:
		db_csr_write(s, off, v, size, true);
		break;
	case HOLE_US0:
		db_us_write(s, DB_WIN_US0, off, v, size);
		break;
	case HOLE_US1:
		db_us_write(s, DB_WIN_US1, off, v, size);
		break;
	case HOLE_US2:
		db_us_write(s, DB_WIN_US2, off, v, size);
		break;
	case HOLE_USB:
		if (size == 4) {
			sunpci_ohci_writel(addr, v, s);
		} else if (size == 2) {
			sunpci_ohci_writew(addr, (uint16_t)v, s);
		} else {
			sunpci_ohci_writeb(addr, (uint8_t)v, s);
		}
		break;
	case HOLE_LFB:
		if (size == 4) {
			svga_writel_linear(addr, v, sunpci_vga_svga(s));
		} else if (size == 2) {
			svga_writew_linear(addr, (uint16_t)v, sunpci_vga_svga(s));
		} else {
			svga_write_linear(addr, (uint8_t)v, sunpci_vga_svga(s));
		}
		break;
	}
}

static uint8_t sunpci_hole_readb(uint32_t addr, void *priv)
{
	return (uint8_t)sunpci_hole_read(addr, 1);
}
static uint16_t sunpci_hole_readw(uint32_t addr, void *priv)
{
	return (uint16_t)sunpci_hole_read(addr, 2);
}
static uint32_t sunpci_hole_readl(uint32_t addr, void *priv)
{
	return sunpci_hole_read(addr, 4);
}
static void sunpci_hole_writeb(uint32_t addr, uint8_t val, void *priv)
{
	sunpci_hole_write(addr, val, 1);
}
static void sunpci_hole_writew(uint32_t addr, uint16_t val, void *priv)
{
	sunpci_hole_write(addr, val, 2);
}
static void sunpci_hole_writel(uint32_t addr, uint32_t val, void *priv)
{
	sunpci_hole_write(addr, val, 4);
}

/* Card PCI bus */

static struct sunpci_dev *sunpci_find_dev(struct sunpci_state *s, int devfn)
{
	for (int i = 0; i < s->ndevs; i++) {
		if (s->devs[i].devfn == devfn) {
			return &s->devs[i];
		}
	}
	return NULL;
}

static struct sunpci_dev *sunpci_add_dev(struct sunpci_state *s, const TCHAR *name, int dev, int fn,
	uae_u16 vendor, uae_u16 device, uae_u8 rev, uae_u32 cls, uae_u8 header)
{
	struct sunpci_dev *d = &s->devs[s->ndevs++];
	memset(d, 0, sizeof(struct sunpci_dev));
	d->name = name;
	d->devfn = (dev << 3) | fn;
	d->cfg[0x00] = (uae_u8)vendor;
	d->cfg[0x01] = vendor >> 8;
	d->cfg[0x02] = (uae_u8)device;
	d->cfg[0x03] = device >> 8;
	d->cfg[0x07] = 0x02;
	d->cfg[0x08] = rev;
	d->cfg[0x09] = (uae_u8)cls;
	d->cfg[0x0a] = (uae_u8)(cls >> 8);
	d->cfg[0x0b] = (uae_u8)(cls >> 16);
	d->cfg[0x0e] = header;
	d->wmask[0x04] = 0xff;
	d->wmask[0x05] = 0x03;
	d->wmask[0x0c] = 0xff;
	d->wmask[0x0d] = 0xff;
	d->wmask[0x3c] = 0xff;
	for (int i = 0x40; i < 0x100; i++) {
		d->wmask[i] = 0xff;
	}
	return d;
}

static void sunpci_set_bar(struct sunpci_dev *d, int bar, uae_u32 size, bool io)
{
	int off = 0x10 + bar * 4;
	uae_u32 mask = ~(size - 1);
	d->cfg[off] = io ? 1 : 0;
	for (int i = 0; i < 4; i++) {
		d->wmask[off + i] = (uae_u8)(mask >> (i * 8));
	}
	d->wmask[off] &= io ? ~3 : ~15;
}

static void sunpci_build_bus(struct sunpci_state *s)
{
	struct sunpci_dev *d;

	s->ndevs = 0;

	// SiS 5597/5598 host bridge
	d = sunpci_add_dev(s, _T("SiS5598 host"), 0, 0, 0x1039, 0x5597, 0x00, 0x060000, 0x00);
	d->cfg[0x04] = 0x07;
	// DRAM bank registers (undocumented; 86Box SiS 5571 reset values)
	d->cfg[0x60] = d->cfg[0x62] = d->cfg[0x64] = d->cfg[0x66] = d->cfg[0x68] = d->cfg[0x6a] = 0x04;
	d->cfg[0x63] = d->cfg[0x67] = d->cfg[0x6b] = 0x80;
	d->cfg[0x9e] = d->cfg[0x9f] = d->cfg[0xa0] = d->cfg[0xa2] = 0xff;
	d->config_written = sunpci_sis_host_config_written;
	s->sis_host = d;

	// SiS 5597/5598 PCI-to-ISA bridge
	d = sunpci_add_dev(s, _T("SiS5598 ISA"), 1, 0, 0x1039, 0x0008, 0x00, 0x060100, 0x80);
	d->cfg[0x04] = 0x0f;
	d->cfg[0x41] = d->cfg[0x42] = d->cfg[0x43] = d->cfg[0x44] = 0x80;
	d->config_written = sunpci_sis_isa_config_written;
	s->sis_isa = d;

	// SiS 5597/5598 USB OHCI
	d = sunpci_add_dev(s, _T("SiS5598 USB"), 1, 2, 0x1039, 0x7001, 0x00, 0x0c0310, 0x80);
	sunpci_set_bar(d, 0, 0x1000, false);
	d->cfg[0x3d] = 1;
	s->sis_usb = d;
	sunpci_ohci_reset(s);

	// SiS 5513 IDE
	d = sunpci_add_dev(s, _T("SiS5513 IDE"), 1, 1, 0x1039, 0x5513, 0xd0, 0x01018a, 0x80);
	sunpci_set_bar(d, 0, 8, true);
	sunpci_set_bar(d, 1, 4, true);
	sunpci_set_bar(d, 2, 8, true);
	sunpci_set_bar(d, 3, 4, true);
	sunpci_set_bar(d, 4, 16, true);

	// SiS 5597/5598 VGA, when the SunPCi VGA board is configured
	s->sis_vga = NULL;
	if (s->vga) {
		d = sunpci_add_dev(s, _T("SiS5598 VGA"), SUNPCI_VGA_DEV, 0, 0x1039, 0x0200, 0x00, 0x030000, 0x00);
		sunpci_set_bar(d, 0, SUNPCI_VGA_LFB_SIZE, false);
		d->cfg[0x10] |= 0x08;
		sunpci_set_bar(d, 1, 0x10000, false);
		// No relocatable I/O BAR: Sun's SMI code expects the 21554's CSR I/O
		// window at E000, the first I/O address the BIOS hands out.
		d->config_written = sunpci_vga_config_written;
		s->sis_vga = d;
	}
}

static uae_u8 sunpci_config_readb(struct sunpci_state *s, int reg)
{
	if (s->cf8 & 0x00ff0000) {
		return 0xff;
	}
	int devfn = (s->cf8 >> 8) & 0xff;
	if (devfn == SUNPCI_BRIDGE_DEV << 3) {
		return db_config_read(s, reg, true);
	}
	struct sunpci_dev *d = sunpci_find_dev(s, devfn);
	if (!d) {
		return 0xff;
	}
	return d->cfg[reg];
}

static void sunpci_config_writeb(struct sunpci_state *s, int reg, uae_u8 v)
{
	if (s->cf8 & 0x00ff0000) {
		return;
	}
	int devfn = (s->cf8 >> 8) & 0xff;
	if (devfn == SUNPCI_BRIDGE_DEV << 3) {
		db_config_write(s, reg, v, true);
		return;
	}
	struct sunpci_dev *d = sunpci_find_dev(s, devfn);
	if (!d) {
		return;
	}
	uae_u8 old = d->cfg[reg];
	d->cfg[reg] = (old & ~d->wmask[reg]) | (v & d->wmask[reg]);
#if SUNPCI_DEBUG_CSR
	if (reg >= 0x40 || old != d->cfg[reg]) {
		write_log(_T("SUNPCI: cfg %s [%02x] %02x -> %02x PC=%04x:%08x\n"), d->name, reg, old, d->cfg[reg], CS, cpu_state.pc);
	}
#endif
	if (d->config_written) {
		d->config_written(d, reg, old);
	}
}

/* Card side I/O */

static void sunpci_unknown_io(struct sunpci_state *s, uint16_t port, int dir, uint32_t val, int size)
{
	// The BIOS writes unused port EB for I/O delays, including the speaker
	// loop at F000:EE84. AL is incidental; these are not device commands.
	if (port == 0xeb && !dir && size == 1) {
		return;
	}
	if (s->unknown_logs >= 2000) {
		return;
	}
#if !SUNPCI_DEBUG_IO
	// Widths 1, 2 and 4 occupy three bits for each direction. Repeated
	// BIOS probes must not exhaust the budget before new ports appear.
	unsigned bit = 1U << ((dir ? 3 : 0) + (size == 4 ? 2 : size == 2 ? 1 : 0));
	if (s->unknown_io_seen[port] & bit) {
		return;
	}
	s->unknown_io_seen[port] |= bit;
	if (!s->unknown_logs) {
		write_log(_T("SUNPCI: unhandled I/O reports each port/width/direction once (SUNPCI_DEBUG_IO=1 traces repeats)\n"));
	}
#endif
	if (!s->acpi_probe_logged && s->sis_isa) {
		unsigned base = s->sis_isa->cfg[0x90] | (s->sis_isa->cfg[0x91] << 8);
		if (base && port >= base && unsigned(port) - base < 0x40) {
			s->acpi_probe_logged = true;
			write_log(_T("SUNPCI: ACPI probe base=%04x ISA40=%02x decode=%s; ACPI registers not implemented\n"),
				base, s->sis_isa->cfg[0x40], (s->sis_isa->cfg[0x40] & 0x80) ? _T("enabled") : _T("disabled"));
		}
	}
	s->unknown_logs++;
	if (dir) {
		write_log(_T("SUNPCI: unhandled IN%d %04x PC=%04x:%08x\n"), size * 8, port, CS, cpu_state.pc);
	} else {
		write_log(_T("SUNPCI: unhandled OUT%d %04x = %08x PC=%04x:%08x\n"), size * 8, port, val, CS, cpu_state.pc);
	}
	if (s->unknown_logs == 2000) {
		write_log(_T("SUNPCI: unhandled I/O diagnostic limit reached\n"));
	}
}

static bool sunpci_bridge_io(struct sunpci_state *s, uint16_t port, int dir, uint32_t *val, int size)
{
	struct db21554 *b = &s->db;
	if (!(b->sec[0x04] & 1)) {
		return false;
	}
	uae_u32 base = db_bar(b->sec, 1);
	if (!base || port < base || port >= base + 256) {
		return false;
	}
	if (dir) {
		*val = sunpci_spin_read(0x10000000 | (port - base), db_csr_read(s, port - base, size, true));
	} else {
		sunpci_spin_reset();
		db_csr_write(s, port - base, *val, size, true);
	}
	return true;
}

// CF9 is the chipset reset control, distinct from the CF8 PCI address.
// Bit 2 triggers on its rising edge; bit 1 requests a system reset.
static void sunpci_reset_control_write(struct sunpci_state *s, uae_u8 val)
{
	if (!(s->reset_control & 4) && (val & 4)) {
		write_log(_T("SUNPCI: CF9 reset %02x PC=%04x:%08x\n"), val, CS, cpu_state.pc);
		if (val & 2) {
			sunpci_card_reset(s);
		} else {
			// also leaves SMM and drops a pending SMI
			resetx86();
			mem_a20_recalc();
		}
	}
	s->reset_control = val & 0xfd;
	if (val & 2) {
		s->reset_control &= ~4;
	}
}

uint32_t sunpci_x86_io(uint16_t port, int dir, uint32_t val, int size)
{
	struct sunpci_state *s = sunpci;

	if (port == 0xcf9 && size == 1) {
		if (dir) {
			return s->reset_control & ~4;
		}
		sunpci_reset_control_write(s, (uae_u8)val);
		return 0;
	}
	if (port == 0xcf8 && size == 4) {
		if (dir) {
			return s->cf8;
		}
		s->cf8 = val & 0x80fffffc;
		return 0;
	}
	if (port >= 0xcfc && port < 0xd00) {
		if (!(s->cf8 & 0x80000000)) {
			return 0xffffffff;
		}
		int reg = (s->cf8 & 0xfc) | (port & 3);
		if (dir) {
			uint32_t v = 0;
			for (int i = 0; i < size; i++) {
				v |= sunpci_config_readb(s, reg + i) << (i * 8);
			}
			return v;
		}
		for (int i = 0; i < size; i++) {
			sunpci_config_writeb(s, reg + i, (uae_u8)(val >> (i * 8)));
		}
		return 0;
	}
	if (port == 0x80 && !dir) {
		if (s->post != (uae_u8)val) {
			write_log(_T("SUNPCI: POST %02x PC=%04x:%08x\n"), val & 0xff, CS, cpu_state.pc);
		}
		s->post = (uae_u8)val;
	}
	if (port == 0x92) {
		if (dir) {
			return s->port92;
		}
		s->port92 = val & 0x03;
		mem_a20_alt = (val & 2) ? 1 : 0;
		mem_a20_recalc();
		if (val & 1) {
			write_log(_T("SUNPCI: port 92 reset\n"));
			softresetx86();
			cpu_set_edx();
		}
		return 0;
	}
	if (sunpci_bridge_io(s, port, dir, &val, size)) {
		return dir ? val : 0;
	}
	if (sunpci_vga_io(s, port, dir, &val, size)) {
		return dir ? val : 0;
	}
	if (x86_io_handler_access(port, dir, &val, size)) {
		return dir ? val : 0;
	}
	// SiS 5513 IDE without drives: SunPCi disks are host services (INT 13h)
	if ((port >= 0x1f0 && port <= 0x1f7) || (port >= 0x170 && port <= 0x177) || port == 0x3f6 || port == 0x376) {
		return 0;
	}
	sunpci_unknown_io(s, port, dir, val, size);
	return 0xffffffff;
}

/* Machine */

static void sunpci_card_reset(struct sunpci_state *s)
{
	if (!s->vga) {
		s->vga = gfxboard_get_sunpci_vga();
		if (s->vga) {
			write_log(_T("SUNPCI: SiS 5598 VGA attached\n"));
			svga_t *svga = sunpci_vga_svga(s);
			svga->vblank_start = sunpci_vga_vblank_start;
			svga->vsync_callback = sunpci_vga_vsync;
		}
	}
	sunpci_build_bus(s);
	sunpci_flash_reset(s);
	s->db.pic_smi_rate = 0;
	s->smi_lines = 0;
	sunpci_vga_smm_frame = false;
	sunpci_smram_update(s);
	sunpci_sis_shadow(s->sis_host);
	s->port92 = 0;
	s->reset_control = 0;
	s->db.pic_host_irqs = 0;
	pic_reset();
	if (s->serial) {
		ns16550_device.reset(s->serial);
	}
	s->mouse_head = s->mouse_tail = 0;
	dma_reset();
	resetx86();
	mem_a20_alt = 0;
	mem_a20_recalc();
	sunpci_card_irq_update(s);
}

static void sunpci_cmos_save(struct sunpci_state *s)
{
	if (!s->cmosfile || !s->nvr) {
		return;
	}
	zfile_fseek(s->cmosfile, 0, SEEK_SET);
	if (zfile_fwrite(s->nvr->regs, 1, s->nvr->size, s->cmosfile) == s->nvr->size) {
		nvr_dosave = 0;
	}
}

static void sunpci_x86_free(void)
{
	struct sunpci_state *s = sunpci;
	if (!s) {
		return;
	}
	sunpci_cmos_save(s);
	zfile_fclose(s->cmosfile);
	cpu_spin_pc = 0xffffffff;
	cpu_spin_hook = NULL;
	cpu_block_hook = NULL;
	x86pc_close_devices();
	if (s->smram) {
		smram_del(s->smram);
	}
	x86pc_mem_free();
	x86_io_handlers_free();
	rom = NULL;
	xfree(s->bios);
	xfree(s);
	sunpci = NULL;
}

static void sunpci_cmos_load(struct sunpci_state *s)
{
	TCHAR cmospath[MAX_DPATH];
	fetch_nvrampath(cmospath, MAX_DPATH);
	if (_tcslen(cmospath) + _tcslen(_T("sunpci1-cmos.nvr")) >= MAX_DPATH) {
		return;
	}
	_tcscat(cmospath, _T("sunpci1-cmos.nvr"));
	memset(s->nvr->regs, 0, s->nvr->size);
	s->cmosfile = zfile_fopen(cmospath, _T("rb+"), ZFD_NORMAL);
	if (s->cmosfile) {
		if (zfile_fread(s->nvr->regs, 1, s->nvr->size, s->cmosfile) != s->nvr->size) {
			memset(s->nvr->regs, 0, s->nvr->size);
		}
	} else {
		s->cmosfile = zfile_fopen(cmospath, _T("wb+"), 0);
	}
	if (!s->cmosfile) {
		write_log(_T("SUNPCI: could not open CMOS file %s\n"), cmospath);
	}
	nvr_dosave = 0;
}

static bool sunpci_x86_init(struct romconfig *rc)
{
	struct sunpci_state *s = xcalloc(struct sunpci_state, 1);
	s->bios = xcalloc(uae_u8, SUNPCI_BIOS_SIZE);
	if (!load_rom_rc(rc, ROMTYPE_SUNPCI1, SUNPCI_BIOS_SIZE, 0, s->bios, SUNPCI_BIOS_SIZE, LOADROM_FILL)) {
		error_log(_T("SunPCi I: BIOS (bios/sunpci.bin) failed to load"));
		xfree(s->bios);
		xfree(s);
		return false;
	}
	sunpci = s;
	s->db.card_pic_irq = -1;
	db_init(&s->db);

	x86pc_close_devices();
	x86pc_set_serial_irq_hook(NULL);
	x86pc_set_machine(X86PC_SUNPCI, 0); // K6-2/300
	fpu_type = FPU_INTERNAL;
	x86pc_set_ps2(true);
	// also does cpu_set()
	x86pc_set_speed_throttle(0);
	x86pc_timer_init();

	mem_size = SUNPCI_RAM_KB;
	mem_init();
	mem_reset();
	x86pc_codegen_init();

	pic_init();
	pic2_init();
	pic_elcr_set_enabled(1);
	pic_elcr_io_handler(1);
	dma_init();
	dma16_init();
	pit_common_init(PIT_8254, pit_irq0_timer, NULL);
	refresh_at_enable = 1;
	pit_devs[0].set_out_func(pit_devs[0].data, 1, pit_refresh_timer_at);
	device_add(&port_6x_device);
	// The Award BIOS sets the translate bit and expects set 1 codes on port 60
	device_add_params(&kbc_at_device, (void *)(uintptr_t)KBC_VEN_GENERIC);
	device_add(&keyboard_at_device);
	// keyboard_input() maps each key through scancode_config_map[], which
	// 86Box's configuration loader fills with the identity mapping
	for (int i = 0; i < 768; i++) {
		scancode_config_map[i] = i;
	}
	s->nvr = (nvr_t *)device_add_params(&nvr_at_device, (void *)(uintptr_t)NVR_AT);
	if (s->nvr) {
		sunpci_cmos_load(s);
	}
	s->serial = x86pc_add_serial(1);
	serial_attach(1, sunpci_mouse_identify, NULL, s);
	x86pc_set_turbo(0);
	x86pc_set_turbo(1);
	cpu_spin_pc = 0xffffffff;
	cpu_spin_hook = sunpci_spin_hook;
	cpu_block_hook = sunpci_block_hook;

	rom = s->bios;
	biosmask = SUNPCI_BIOS_SIZE - 1;
	mem_mapping_add(&s->flash_map, 0xe0000, 0x20000,
		sunpci_flash_readb, sunpci_flash_readw, sunpci_flash_readl,
		sunpci_flash_writeb, sunpci_flash_writew, sunpci_flash_writel,
		s->bios + 0x20000, MEM_MAPPING_EXTERNAL | MEM_MAPPING_ROM, s);
	mem_mapping_add(&s->flash_high_map, 0xfffc0000, SUNPCI_BIOS_SIZE,
		sunpci_flash_readb, sunpci_flash_readw, sunpci_flash_readl,
		sunpci_flash_writeb, sunpci_flash_writew, sunpci_flash_writel,
		s->bios, MEM_MAPPING_ROM, s);

	// PCI memory space: above the card's RAM, below the BIOS flash
	mem_mapping_add(&s->pci_hole, SUNPCI_RAM_KB * 1024, 0xfffc0000 - SUNPCI_RAM_KB * 1024,
		sunpci_hole_readb, sunpci_hole_readw, sunpci_hole_readl,
		sunpci_hole_writeb, sunpci_hole_writew, sunpci_hole_writel,
		NULL, MEM_MAPPING_EXTERNAL, s);
	// C0000-DFFFF segments without shadow RAM go to PCI. RMIPC moves the
	// 21554's upstream window to D0000 before its disk requests.
	mem_mapping_add(&s->isa_hole, 0xc0000, 0x20000,
		sunpci_hole_readb, sunpci_hole_readw, sunpci_hole_readl,
		sunpci_hole_writeb, sunpci_hole_writew, sunpci_hole_writel,
		NULL, MEM_MAPPING_EXTERNAL, s);
	mem_mapping_add(&s->vga_legacy_map, 0xa0000, 0x20000,
		sunpci_vga_readb, sunpci_vga_readw, sunpci_vga_readl,
		sunpci_vga_writeb, sunpci_vga_writew, sunpci_vga_writel,
		NULL, MEM_MAPPING_EXTERNAL, s);

	// SMRAM wins over the shadow, VGA and ROM mappings it covers
	s->smram = smram_add();

	sunpci_card_reset(s);
	write_log(_T("SUNPCI: card PC started, %d KB RAM, %s\n"), SUNPCI_RAM_KB, cpu_use_dynarec ? _T("recompiler") : _T("interpreter"));
	return true;
}

#if SUNPCI_DEBUG_CSR
// Log the VGA text screen when it changes, read the way the text renderer
// reads VRAM (no CPU-visible access, so the VGA latches are untouched).
static void sunpci_vga_dump_text(struct sunpci_state *s)
{
	static uae_u32 last_sum;
	svga_t *svga = sunpci_vga_svga(s);
	if (!svga || (svga->gdcreg[6] & 1) || !svga->vram || !svga->remap_func) {
		return;
	}
	uae_u32 start = (svga->crtc[0x0c] << 8) | svga->crtc[0x0d];
	char line[81];
	uae_u32 sum = 0;
	for (int i = 0; i < 80 * 25; i++) {
		uae_u32 a = svga->remap_func(svga, (start + i) << 2) & svga->vram_display_mask;
		sum = sum * 31 + svga->vram[a];
	}
	if (sum == last_sum) {
		return;
	}
	last_sum = sum;
	write_log(_T("SUNPCI: VGA text screen:\n"));
	for (int y = 0; y < 25; y++) {
		int len = 0;
		for (int x = 0; x < 80; x++) {
			uae_u32 a = svga->remap_func(svga, (start + y * 80 + x) << 2) & svga->vram_display_mask;
			uae_u8 c = svga->vram[a];
			line[x] = (c >= 0x20 && c < 0x7f) ? c : (c ? '.' : ' ');
			if (line[x] != ' ') {
				len = x + 1;
			}
		}
		line[len] = 0;
		if (len) {
			write_log(_T("SUNPCI: |%s\n"), line);
		}
	}
}
#endif

static void sunpci_x86_run(struct sunpci_state *s)
{
	if (s->in_reset) {
		return;
	}
	sunpci_spin_reset();
	cpu_end_timeslice = 0;
#if SUNPCI_DEBUG_CSR
	// where the card's PC is, about once a second
	if (++s->trace_lines >= maxvpos * 50) {
		s->trace_lines = 0;
		write_log(_T("SUNPCI: card at %04x:%08x POST %02x IF=%d\n"), CS, cpu_state.pc, s->post, (cpu_state.flags & 0x200) != 0);
		sunpci_vga_dump_text(s);
	}
#endif
	s->cycle_acc += (float)x86pc_get_cpu_speed() / (vblank_hz * maxvpos);
	// The PIC's periodic SMI. Rate unit is a guess: rate * 10 ms.
	if (s->db.pic_smi_rate) {
		int period = (int)(s->db.pic_smi_rate * 0.01f * vblank_hz * maxvpos);
		if (++s->smi_lines >= period) {
			s->smi_lines = 0;
			sunpci_external_smi(s);
		}
	}
	int cycs = (int)s->cycle_acc;
	if (cycs > 0) {
		cpu_exec(cycs);
		s->cycle_acc -= cycs;
	}
	sunpci_mouse_feed(s);

#if SUNPCI_DEBUG_CSR
	{
		static uae_u32 int10;
		uae_u32 v = mem_readb_phys(0x40) | (mem_readb_phys(0x41) << 8) | (mem_readb_phys(0x42) << 16) | (mem_readb_phys(0x43) << 24);
		if (v != int10) {
			write_log(_T("SUNPCI: INT 10h vector %04x:%04x -> %04x:%04x near %04x:%08x POST %02x\n"),
				int10 >> 16, int10 & 0xffff, v >> 16, v & 0xffff, CS, cpu_state.pc, s->post);
			int10 = v;
		}
	}
#endif
}

/* Host side: the bridge's primary interface */

static const struct pci_config sunpci1_pci_config =
{
	0x1011, 0x0046, 0, 0, 0x01, 0x068000, 0, 0, 0, 1, 0, 0, { 0, 1, 0, 0, 0, 0, 0 }
};

static uae_u8 sunpci1_get_config(uaecptr off)
{
	if (!sunpci) {
		return 0xff;
	}
	return db_config_read(sunpci, off & 0xff, false);
}

static void sunpci1_put_config(uaecptr off, uae_u8 v)
{
	if (sunpci) {
		db_config_write(sunpci, off & 0xff, v, false);
	}
}

static bool sunpci1_pci_init(struct pci_board_state *pcibs, struct autoconfig_info *aci)
{
	if (!sunpci) {
		return false;
	}
	sunpci->host_pcibs = pcibs;
	sunpci->init_done = true;
	return true;
}

static void sunpci1_pci_free(struct pci_board_state *pcibs)
{
	sunpci_x86_free();
}

static void sunpci1_pci_hsync(struct pci_board_state *pcibs)
{
	if (sunpci) {
		sunpci_x86_run(sunpci);
		if (nvr_dosave) {
			sunpci_cmos_save(sunpci);
		}
	}
}

// Host access to a primary BAR: CSRs or a downstream window into the card.
static int sunpci1_host_window(struct sunpci_state *s, int bar, uae_u32 off)
{
	switch (bar)
	{
	case 0:
		return off < DB_CSR_SIZE ? DB_WIN_CSR : DB_WIN_DS0;
	case 1:
		return DB_WIN_CSR;
	case 2:
		return DB_WIN_DS1;
	case 3:
		return DB_WIN_DS2;
	case 4:
		return DB_WIN_DS3;
	case 6:
		return (s->db.pri[0x30] & 1) ? DB_WIN_ROM : DB_WIN_NONE;
	}
	return DB_WIN_NONE;
}

static uae_u32 sunpci1_ds_base(struct sunpci_state *s, int win)
{
	switch (win)
	{
	case DB_WIN_DS0:
		return db_reg32(&s->db, DB_CONF_DS_MEM0_TR);
	case DB_WIN_DS1:
		return db_reg32(&s->db, DB_CONF_DS_IO_MEM1_TR);
	case DB_WIN_DS2:
		return db_reg32(&s->db, DB_CONF_DS_MEM2_TR);
	case DB_WIN_DS3:
		return db_reg32(&s->db, DB_CONF_DS_MEM3_TR);
	}
	return 0;
}

static uae_u32 sunpci1_host_access(struct pci_board_state *pcibs, uaecptr addr, uae_u32 v, int size, bool write)
{
	struct sunpci_state *s = sunpci;
	if (!s) {
		return 0xffffffff;
	}
	int bar = pcibs->selected_bar;
	uae_u32 off = bar == 1 ? addr : addr - pcibs->bar_start[bar];
	int win = sunpci1_host_window(s, bar, off);
	if (win == DB_WIN_CSR) {
		if (write) {
			db_csr_write(s, off, v, size, false);
			return 0;
		}
		return db_csr_read(s, off, size, false);
	}
	if (win == DB_WIN_NONE) {
		return 0xffffffff;
	}
	if (win == DB_WIN_ROM) {
		// The expansion ROM BAR reads the parallel ROM interface, where the
		// card has Sun's microcontroller rather than a known ROM image.
		if (!write && !s->db.rom_read_logged) {
			s->db.rom_read_logged = true;
			write_log(_T("SUNPCI: host read of the expansion ROM at +%x; no ROM contents\n"), off);
		}
		return 0xffffffff;
	}
	// downstream window into card memory
	uae_u32 caddr = sunpci1_ds_base(s, win) + off;
#if SUNPCI_DEBUG_CSR
	{
		static int ds_logs;
		if (ds_logs < 200) {
			ds_logs++;
			write_log(_T("SUNPCI: host %s BAR%d+%x -> card %08x/%d = %08x\n"), write ? _T("write") : _T("read"), bar, off, caddr, size, v);
		}
	}
#endif
	if (write) {
		for (int i = 0; i < size; i++) {
			mem_writeb_phys(caddr + i, (uint8_t)(v >> (i * 8)));
		}
#ifdef USE_DYNAREC
		// physical writes bypass the tracking of translated code
		if (cpu_use_dynarec && (caddr >> 12) < pages_sz && pages[caddr >> 12].code_present_mask) {
			mem_invalidate_range(caddr, caddr + size - 1);
		}
#endif
		return 0;
	}
	uae_u32 r = 0;
	for (int i = 0; i < size; i++) {
		r |= mem_readb_phys(caddr + i) << (i * 8);
	}
	return r;
}

static uae_u32 REGPARAM2 sunpci1_lget(struct pci_board_state *pcibs, uaecptr addr)
{
	return sunpci1_host_access(pcibs, addr, 0, 4, false);
}
static uae_u32 REGPARAM2 sunpci1_wget(struct pci_board_state *pcibs, uaecptr addr)
{
	return sunpci1_host_access(pcibs, addr, 0, 2, false);
}
static uae_u32 REGPARAM2 sunpci1_bget(struct pci_board_state *pcibs, uaecptr addr)
{
	return sunpci1_host_access(pcibs, addr, 0, 1, false);
}
static void REGPARAM2 sunpci1_lput(struct pci_board_state *pcibs, uaecptr addr, uae_u32 b)
{
	sunpci1_host_access(pcibs, addr, b, 4, true);
}
static void REGPARAM2 sunpci1_wput(struct pci_board_state *pcibs, uaecptr addr, uae_u32 b)
{
	sunpci1_host_access(pcibs, addr, b, 2, true);
}
static void REGPARAM2 sunpci1_bput(struct pci_board_state *pcibs, uaecptr addr, uae_u32 b)
{
	sunpci1_host_access(pcibs, addr, b, 1, true);
}

#define SUNPCI1_BANK { sunpci1_lget, sunpci1_wget, sunpci1_bget, sunpci1_lput, sunpci1_wput, sunpci1_bput }

const struct pci_board sunpci1_pci_board =
{
	_T("SunPCi I"),
	&sunpci1_pci_config, sunpci1_pci_init, sunpci1_pci_free, NULL, sunpci1_pci_hsync, NULL,
	{ SUNPCI1_BANK, SUNPCI1_BANK, SUNPCI1_BANK, SUNPCI1_BANK, SUNPCI1_BANK, SUNPCI1_BANK, SUNPCI1_BANK },
	false, sunpci1_get_config, sunpci1_put_config, NULL
};

// The card's PC must be running before the host enumerates the bridge, so
// that its BIOS has programmed the windows and cleared the primary lockout.
bool sunpci1_start(void)
{
	if (sunpci) {
		write_log(_T("SunPCi I: only one card is supported\n"));
		return false;
	}
	struct romconfig *rc = get_device_romconfig(&currprefs, ROMTYPE_SUNPCI1, 0);
	if (!rc) {
		return false;
	}
	return sunpci_x86_init(rc);
}

#endif /* WITH_X86 */
