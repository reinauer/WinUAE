#!/usr/bin/env python3
"""Exercise the VGA core's legacy/linear paths and SiS plane layout."""
from pathlib import Path
import subprocess, tempfile, sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
source=(root/'pcem/vid_svga.cpp').read_text()
access=source[source.index('void svga_write('):source.index('void svga_doblit(',source.index('void svga_write('))]
render=(root/'pcem/vid_svga_render.cpp').read_text()
text=render[render.index('void svga_render_text_80('):render.index('void svga_render_text_80_ksc5601(')]
code=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
typedef int mem_mapping_t, pc_timer_t, device_t;
typedef uint8_t PALETTE[768];
#include "vid_svga.h"
#include "vid_svga_render_remap.h"
uint8_t svga_rotate[8][256];
int changeframecount=2;
struct Buffer { uint32_t line[1][32]; } buffer,*buffer32=&buffer;
'''+access+text+r'''
static svga_t v;
static uint8_t ram[1<<20],dirty[256];
static void init(unsigned flag){
 memset(&v,0,sizeof(v));memset(ram,0,sizeof(ram));
 v.vram=ram;v.changedvram=dirty;v.vram_mask=v.decode_mask=(1<<20)-1;
 v.vram_max=1<<20;v.banked_mask=0xffff;v.gdcreg[8]=255;
 v.adv_flags=flag;v.writemask=15;v.crtc[0x17]=3;
 v.vram_display_mask=v.vram_mask;
}
static void check(unsigned flag){
 init(flag);unsigned stride=flag?8:4;
 for(unsigned plane=0;plane<4;plane++){
  v.writemask=1<<plane;v.readplane=plane;
  svga_write(9,0x80>>plane,&v);
  assert(ram[9*stride+plane]==(0x80>>plane));
  assert(svga_read(9,&v)==(0x80>>plane));
  svga_write_linear(10,0xf0>>plane,&v);
  assert(svga_read_linear(10,&v)==(0xf0>>plane));
 }
 /* Read latches, then copy all four planes in write mode 1. */
 svga_read(9,&v);v.writemode=1;v.writemask=15;svga_write(11,0,&v);
 assert(!memcmp(ram+9*stride,ram+11*stride,4));
 svga_read_linear(10,&v);svga_write_linear(12,0,&v);
 assert(!memcmp(ram+10*stride,ram+12*stride,4));
 v.writemode=0;v.chain2_read=v.chain2_write=1;v.writemask=3;v.readplane=0;
 svga_write(0,'A',&v);svga_write(1,7,&v);
 svga_write(2,'B',&v);svga_write(3,7,&v);
 assert(ram[2*stride]=='B'&&ram[2*stride+1]==7);
 assert(svga_read(2,&v)=='B'&&svga_read(3,&v)==7);
 /* Packed access must expose physical bytes, including unused high lanes. */
 v.fb_only=1;
 assert(svga_read_linear(2*stride,&v)=='B');
 svga_write_linear(7,0xab,&v);assert(ram[7]==0xab);
 v.fb_only=0;svga_recalc_remap_func(&v);
 assert(v.remap_func(&v,4)==2*stride);
 /* Original font plane and cursor addressing still render on VGA output. */
 v.fullchange=1;v.seqregs[1]=1;v.hdisp=16;v.charseta=2;
 ram[65*32*stride+2]=0x80;ram[66*32*stride+2]=0x40;
 v.egapal[7]=7;v.pallook[7]=0xffffff;
 svga_render_text_80(&v);
 assert(buffer.line[0][0]==0xffffff&&buffer.line[0][1]==0);
 assert(buffer.line[0][8]==0&&buffer.line[0][9]==0xffffff);
 v.fb_only=1;svga_recalc_remap_func(&v);
 assert(v.remap_func(&v,4)==4);
}
int main(){check(0);check(FLAG_ADDR_BY8);puts("VGA planar, odd/even, latches, packed access and text rendering passed");}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++11','-fsanitize=address,undefined','-I'+str(root/'pcem'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
