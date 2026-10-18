#!/usr/bin/env python3
"""Check VGA state saved by Sun BIOS SMM and restored before RSM."""
from pathlib import Path
import subprocess, tempfile, sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
sis=(root/'pcem/vid_sis5598.cpp').read_text()
core=(root/'pcem/vid_svga.cpp').read_text()
state=sis[sis.index('typedef struct sis5598_t'):sis.index('static void sis5598_update_banks')]
packed=sis[sis.index('static int sis5598_packed'):sis.index('static void sis5598_recalctimings')]
mode=sis[sis.index('static void sis5598_mode_changed'):sis.index('void sis5598_out')]
read=sis[sis.index('uint8_t sis5598_in'):sis.index('svga_t *sis5598_svga')]
access=core[core.index('void svga_write('):core.index('void svga_doblit(',core.index('void svga_write('))]
word=core[core.index('void svga_writew('):core.index('void svga_writel(')]
code=r"""
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
typedef int mem_mapping_t, pc_timer_t, device_t;
typedef uint8_t PALETTE[768];
#include "vid_svga.h"
uint8_t svga_rotate[8][256];
int changeframecount=2;
void svga_recalctimings(svga_t *){}
uint8_t svga_in(uint16_t,void *){return 0xee;}
"""+state+packed+mode+read+access+word+r"""
int main(){
 sis5598_t sis={};svga_t *v=&sis.svga;v->miscout=1;
 v->la=0x12;v->lb=0x34;v->lc=0x56;v->ld=0x78;
 v->crtcreg=0x22;
 for(unsigned i=0;i<4;i++){
  v->readplane=i;assert(sis5598_in(0x3d5,&sis)==0x12+0x22*i);
 }
 v->crtcreg=0x24;v->attrff=0;assert(!sis5598_in(0x3d5,&sis));
 v->attrff=1;assert(sis5598_in(0x3d5,&sis)==0x80);
 v->crtcreg=0x26;v->attraddr=0x13;v->attr_palette_enable=0x20;
 assert(sis5598_in(0x3d5,&sis)==0x33);
 v->attr_palette_enable=0;assert(sis5598_in(0x3d5,&sis)==0x13);
 v->miscout=0;assert(sis5598_in(0x3b5,&sis)==0x13);
 v->crtcreg=0x12;v->crtc[0x12]=0xdf;v->miscout=1;
 assert(sis5598_in(0x3d5,&sis)==0xdf);
 /* SMM selects packed access, restores the GDC while still packed, then
  * restores SR06 last. A following word write must honour VGA planes. */
 static uint8_t ram[65536],dirty[16];
 v->vram=ram;v->changedvram=dirty;v->vram_max=65536;
 v->vram_mask=v->decode_mask=v->banked_mask=65535;
 v->adv_flags=FLAG_ADDR_BY8;v->writemask=1;v->gdcreg[8]=255;
 sis.ext_seq[6]=0x82;sis5598_mode_changed(&sis);assert(v->fast);
 svga_writew(32,0x3412,v);assert(ram[32]==0x12&&ram[33]==0x34);
 sis.ext_seq[6]=0;sis5598_mode_changed(&sis);assert(!v->fast);
 svga_writew(32,0x7856,v);
 assert(ram[32*8]==0x56&&ram[33*8]==0x78&&ram[32*8+1]==0);
 assert(ram[32]==0x12&&ram[33]==0x34);
 sis.ext_seq[6]=0x82;v->gdcreg[8]=0x0f;
 sis5598_mode_changed(&sis);assert(!v->fast);
 puts("SiS latch/attribute readback and packed-to-planar word access passed");
}
"""
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++11','-fsanitize=address,undefined','-I'+str(root/'pcem'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
