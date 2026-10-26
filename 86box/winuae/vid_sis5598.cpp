/*
* UAE - The Un*x Amiga Emulator
*
* SiS 5597/5598 integrated VGA (PCI 1039:0200)
*
* Standard VGA on the 86Box SVGA core plus the SiS extended sequencer
* registers (SR05 password, SR06 graphics modes, SR0A/SR12 timing overflow,
* SR0B segment mode) and the 3CB/3CD segment selects.
*
* Register semantics follow the related SiS 530/620 model in 86Box
* (src/video/vid_sis6306.c).
*
* Used as the SunPCi I card's display. The card side (sunpci.cpp) decodes
* the legacy window, the VGA ports and the PCI BARs and calls the entry
* points below; the board's RTG output comes from gfxboard.cpp.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
/* The device declarations, first: 86Box's video.h has a video_blit_memtoscreen() macro */
#include "86box_video.h"
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/timer.h>
#include <86box/video.h>
#include <86box/vid_svga.h>
#include <86box/vid_svga_render.h>

typedef struct sis5598_t
{
	svga_t svga;
	uint8_t ext_seq[0x40];
	uint8_t segment0, segment1;
	uint32_t vram_size;
} sis5598_t;

static void sis5598_update_banks(sis5598_t *sis)
{
	svga_t *svga = &sis->svga;

	// SR0B bit 3: dual segment (3CD write, 3CB read), else 3CD holds both
	if (sis->ext_seq[0x0b] & 0x08) {
		svga->write_bank = (uint32_t)(sis->segment0 & 0x3f) << 16;
		svga->read_bank = (uint32_t)(sis->segment1 & 0x3f) << 16;
	} else {
		svga->write_bank = (uint32_t)((sis->segment0 >> 4) & 0x0f) << 16;
		svga->read_bank = (uint32_t)(sis->segment0 & 0x0f) << 16;
	}
}

static int sis5598_packed(sis5598_t *sis)
{
	return (sis->ext_seq[0x06] & 0x1e) != 0;
}

// The 64-bit UMA interface stores VGA planes 0..3 in the low dword of each
// qword. Packed modes address all bytes directly. Sun's BIOS captures mode
// 12h as dwords eight bytes apart and text as character/attribute words
// sixteen bytes apart. The display side is svga_t.qword_planes.
static uint32_t sis5598_translate_address(uint32_t addr, void *priv)
{
	svga_t *svga = (svga_t *)priv;
	if ((svga->chain4 && svga->packed_chain4) || svga->fb_only) {
		return addr;
	}
	return ((addr & ~3U) << 1) | (addr & 3);
}

static void sis5598_recalctimings(svga_t *svga)
{
	sis5598_t *sis = (sis5598_t *)svga->priv;
	uint8_t mode = sis->ext_seq[0x06];
	uint8_t hov = sis->ext_seq[0x12];
	uint8_t vov = sis->ext_seq[0x0a];

	// SR12: horizontal overflow
	if (hov & 0x01) {
		svga->htotal += 0x100;
	}
	if (hov & 0x02) {
		svga->hdisp += 0x100 * ((svga->seqregs[1] & 1) ? 8 : 9);
	}
	// SR0A: vertical overflow and offset bits 11:8
	if (vov & 0x01) {
		svga->vtotal += 0x400;
	}
	if (vov & 0x02) {
		svga->dispend += 0x400;
	}
	if (vov & 0x04) {
		svga->vblankstart += 0x400;
	}
	if (vov & 0x08) {
		svga->vsyncstart += 0x400;
	}

	if (!sis5598_packed(sis)) {
		svga->plane_mask = svga->attrregs[0x12] & 0x0f;
		return;
	}
	svga->rowoffset = svga->crtc[0x13] | ((vov & 0xf0) << 4);
	svga->interlace = (mode & 0x20) != 0;
	svga->rowcount = 0;
	// Screen off: keep the blank renderer svga_recalctimings() selected
	if (svga->scrblank || !(svga->crtc[0x17] & 0x80)) {
		return;
	}
	if (mode & 0x10) {
		svga->bpp = 24;
		svga->render = svga_render_24bpp_highres;
	} else if (mode & 0x08) {
		svga->bpp = 16;
		svga->render = svga_render_16bpp_highres;
	} else if (mode & 0x04) {
		svga->bpp = 15;
		svga->render = svga_render_15bpp_highres;
	} else {
		svga->bpp = 8;
		svga->map8 = svga->pallook;
		svga->render = svga_render_8bpp_highres;
		svga->plane_mask = 0x0f;
	}
}

static void sis5598_mode_changed(sis5598_t *sis)
{
	int packed = sis5598_packed(sis);
	sis->svga.fb_only = packed;
	sis->svga.packed_chain4 = packed;
	// SR06 changes the same CPU access path as the VGA sequencer/GDC.
	// Do not leave packed word/dword accesses enabled after restoring VGA.
	sis->svga.fast = packed && sis->svga.gdcreg[8] == 0xff &&
		!(sis->svga.gdcreg[3] & 0x18) && !sis->svga.gdcreg[1];
	if (!packed) {
		sis->svga.bpp = 8;
	}
	svga_recalctimings(&sis->svga);
}

void sis5598_out(uint16_t addr, uint8_t val, void *p)
{
	sis5598_t *sis = (sis5598_t *)p;
	svga_t *svga = &sis->svga;
	uint8_t old;

	if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) {
		addr ^= 0x60;
	}

	switch (addr)
	{
	case 0x3c4:
		svga->seqaddr = val & 0x3f;
		return;
	case 0x3c5:
		if (svga->seqaddr < 5) {
			break;
		}
		if (svga->seqaddr == 5) {
			// password: 86 unlocks the extended registers
			sis->ext_seq[5] = val == 0x86 ? 0xa1 : 0x21;
			return;
		}
		if (sis->ext_seq[5] != 0xa1) {
			return;
		}
		old = sis->ext_seq[svga->seqaddr];
		sis->ext_seq[svga->seqaddr] = val;
		if (old == val) {
			return;
		}
		switch (svga->seqaddr)
		{
		case 0x06:
			sis5598_mode_changed(sis);
			break;
		case 0x0a:
		case 0x12:
		case 0x07:
			svga->fullchange = svga->monitor->mon_changeframecount;
			svga_recalctimings(svga);
			break;
		case 0x0b:
			sis5598_update_banks(sis);
			break;
		}
		return;
	case 0x3cb:
		sis->segment1 = val;
		sis5598_update_banks(sis);
		return;
	case 0x3cd:
		sis->segment0 = val;
		sis5598_update_banks(sis);
		return;
	case 0x3d4:
		svga->crtcreg = val & 0x3f;
		return;
	case 0x3d5:
		if (svga->crtcreg < 7 && (svga->crtc[0x11] & 0x80)) {
			return;
		}
		if (svga->crtcreg == 7 && (svga->crtc[0x11] & 0x80)) {
			val = (svga->crtc[7] & ~0x10) | (val & 0x10);
		}
		old = svga->crtc[svga->crtcreg];
		svga->crtc[svga->crtcreg] = val;
		if (old != val) {
			if (svga->crtcreg < 0x0e || svga->crtcreg > 0x10) {
				if (svga->crtcreg == 0x0c || svga->crtcreg == 0x0d) {
					svga->fullchange = 3;
					svga->memaddr_latch = ((svga->crtc[0x0c] << 8) | svga->crtc[0x0d]) + ((svga->crtc[8] & 0x60) >> 5);
				} else {
					svga->fullchange = svga->monitor->mon_changeframecount;
					svga_recalctimings(svga);
				}
			}
		}
		return;
	}
	svga_out(addr, val, svga);
}

uint8_t sis5598_in(uint16_t addr, void *p)
{
	sis5598_t *sis = (sis5598_t *)p;
	svga_t *svga = &sis->svga;

	if (((addr & 0xfff0) == 0x3d0 || (addr & 0xfff0) == 0x3b0) && !(svga->miscout & 1)) {
		addr ^= 0x60;
	}

	switch (addr)
	{
	case 0x3c4:
		return svga->seqaddr;
	case 0x3c5:
		if (svga->seqaddr < 5) {
			break;
		}
		if (svga->seqaddr == 5) {
			return sis->ext_seq[5];
		}
		if (sis->ext_seq[5] != 0xa1) {
			return 0xff;
		}
		return sis->ext_seq[svga->seqaddr];
	case 0x3cb:
		return sis->segment1;
	case 0x3cd:
		return sis->segment0;
	case 0x3d4:
		return svga->crtcreg;
	case 0x3d5:
		// SiS readback registers let SMM preserve an interrupted VGA access.
		switch (svga->crtcreg)
		{
		case 0x22:
			return svga->latch.b[svga->readplane & 3];
		case 0x24:
			return svga->attrff ? 0x80 : 0;
		case 0x26:
			return svga->attraddr | svga->attr_palette_enable;
		}
		return svga->crtc[svga->crtcreg];
	}
	return svga_in(addr, svga);
}

svga_t *sis5598_svga(void *p)
{
	return &((sis5598_t *)p)->svga;
}

static void *sis5598_init(const device_t *info)
{
	sis5598_t *sis = (sis5598_t *)calloc(1, sizeof(sis5598_t));

	sis->vram_size = device_get_config_int("memory") << 20;
	if (!sis->vram_size) {
		sis->vram_size = 4 << 20;
	}
	svga_init(info, &sis->svga, sis, sis->vram_size, sis5598_recalctimings, sis5598_in, sis5598_out, NULL, NULL);
	sis->svga.decode_mask = sis->vram_size - 1;
	sis->svga.translate_address = sis5598_translate_address;
	sis->svga.qword_planes = 1;
	sis->ext_seq[5] = 0x21;
	return sis;
}

static void sis5598_close(void *p)
{
	sis5598_t *sis = (sis5598_t *)p;
	svga_close(&sis->svga);
	free(sis);
}

static void sis5598_speed_changed(void *p)
{
	sis5598_t *sis = (sis5598_t *)p;
	svga_recalctimings(&sis->svga);
}

static void sis5598_force_redraw(void *p)
{
	sis5598_t *sis = (sis5598_t *)p;
	sis->svga.fullchange = sis->svga.monitor->mon_changeframecount;
}

const device_t sis5598_device = {
	.name          = "SiS 5598",
	.internal_name = "sis5598",
	.flags         = 0,
	.local         = 0,
	.init          = sis5598_init,
	.close         = sis5598_close,
	.reset         = NULL,
	.available     = NULL,
	.speed_changed = sis5598_speed_changed,
	.force_redraw  = sis5598_force_redraw,
	.config        = NULL
};
