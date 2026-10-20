#!/usr/bin/env python3
"""Exercise VGA blanking and secondary-monitor mode changes in the real core."""
from pathlib import Path
import re,subprocess,tempfile,sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
core=(root/'pcem/vid_svga.cpp').read_text()
render=(root/'pcem/vid_svga_render.cpp').read_text()
gfx=(root/'gfxboard.cpp').read_text(encoding='latin1')
poll=core[core.index('int svga_poll('):core.index('void svga_setvram(')]
blank=render[render.index('void svga_render_blank('):render.index('void svga_render_text_40(')]
blit=core[core.index('void svga_doblit(',core.index('void svga_write(')):core.index('void svga_writew(')]
# Compile the actual mode-change loop up to its rendering section.
a=gfx.index('\n\tfor (int i = 0;',gfx.index('void gfxboard_vsync_handler('))
a=gfx.index('\n\tfor (int i = 0;',a+1)
b=gfx.index('\n\t\tif (!redraw_required)',a)
sizing='void check_modes() {\n'+gfx[a:b]+'\n\t}\n}\n'
code=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define UAE 1
#define MAX_RTG_BOARDS 2
typedef int mem_mapping_t,pc_timer_t,device_t;
typedef uint8_t PALETTE[768];
#include "vid_svga.h"
struct Buffer { int w; uint8_t *line[512]; } buffer,*buffer32=&buffer;
static uint32_t pixels[512][2048];
static int renders,overlays,changeframecount=2,readflash,xsize,ysize,vid_resize;
static int resize_w,resize_h,blit_h;
void video_wait_for_buffer(){}
void updatewindowsize(int w,int,int h,int){resize_w=w;resize_h=h;}
void video_blit_memtoscreen(int,int,int,int,int,int h){blit_h=h;}
void svga_doblit(int,int,int,int,svga_t*);
'''
for name in sorted(set(re.findall(r'svga_render_\w+',poll))-{'svga_render_blank'}):
 code+='void '+name+'(svga_t*){}\n'
code+=blank+blit+poll+r'''
struct rtggfxboard {
 int monitor_id,configured_mem,configured_regs,resolutionchange;
 bool monswitch_current;
 int vga_width,vga_height;
};
struct amigadisplay {} adisplays[2];
struct picasso96_state_struct {int Width,Height;bool ModeChanged;} picasso96_state[2];
static rtggfxboard rtggfxboards[2];
static int rtg_visible[2]={0,1},mode_changes,disabled;
static bool fail_mode;
bool gfxboard_checkchanged(rtggfxboard *g){
 auto &s=picasso96_state[g->monitor_id];
 return s.Width!=g->vga_width||s.Height!=g->vga_height;
}
bool gfxboard_setmode_ext(rtggfxboard *g){
 if(fail_mode)return false;
 auto &s=picasso96_state[g->monitor_id];
 s.Width=g->vga_width;s.Height=g->vga_height;mode_changes++;return true;
}
void gfxboard_rtg_disable(int,int){disabled++;}
'''+sizing+r'''
void draw(svga_t *v){
 renders++;v->firstline_draw=0;v->lastline_draw=v->displine;
 for(int x=32;x<32+v->hdisp;x++)pixels[v->displine][x]=0x123456;
}
void overlay(svga_t*,int){overlays++;}
void init(svga_t &v){
 memset(&v,0,sizeof(v));memset(pixels,0xab,sizeof(pixels));
 buffer.w=2048;for(int y=0;y<512;y++)buffer.line[y]=(uint8_t*)pixels[y];
 static uint8_t dirty[2048];v.changedvram=dirty;v.vram_display_mask=65535;
 v.render=draw;v.hdisp=640;v.dispon=1;v.firstline=v.firstline_draw=4000;
 v.dispend=480;v.vsyncstart=490;v.vtotal=527;v.rowoffset=80;
 v.video_res_override=1;v.char_width=8;v.crtc[0x17]=0xe3;
 renders=overlays=0;
}
int main(){
 svga_t v;
 // Blanking cannot depend on character width or touch adjacent scanlines.
 for(int clock: {0,1,8,9}){
  init(v);v.seqregs[1]=clock;v.scrblank=32;
  v.overlay_on=v.hwcursor_on=v.dac_hwcursor_on=2;
  v.overlay_draw=v.hwcursor_draw=v.dac_hwcursor_draw=overlay;
  svga_poll(&v);assert(!renders&&!overlays);
  for(int x=0;x<2048;x++)assert(pixels[0][x]==(x>=32&&x<672?0:0xabababab));
  assert(pixels[1][0]==0xabababab);
  assert(v.overlay_on==1&&v.hwcursor_on==1&&v.dac_hwcursor_on==1);
 }
 // A blanked 480-line frame still reports its timing-derived geometry.
 init(v);v.scrblank=32;
 for(int i=0;i<980;i++)svga_poll(&v);
 assert(!renders&&resize_w==640&&resize_h==480&&blit_h==480);
 for(int y=0;y<480;y++)for(int x=32;x<672;x++)assert(!pixels[y][x]);
 // Unblanking restores ordinary drawing without changing the selected renderer.
 v.scrblank=0;v.linepos=0;v.dispon=1;v.displine=0;svga_poll(&v);
 assert(renders==1&&pixels[0][32]==0x123456);
 init(v);v.dpms=1;svga_poll(&v);assert(!renders&&!pixels[0][32]);
 init(v);v.scrblank=32;v.hdisp=4096;svga_poll(&v);
 assert(!pixels[0][2047]&&pixels[1][0]==0xabababab);
 // The secondary surface must resize even when the output switch is off.
 rtggfxboards[0]={0,1,1,5,false,640,480};
 rtggfxboards[1]={1,1,1,5,false,640,480};
 picasso96_state[0]={640,350,false};picasso96_state[1]={640,350,false};
 for(int i=0;i<4;i++)check_modes();assert(!mode_changes);
 check_modes();assert(mode_changes==1&&picasso96_state[1].Height==480);
 assert(picasso96_state[1].ModeChanged&&picasso96_state[0].Height==350);
 rtggfxboards[0].monswitch_current=true;
 for(int i=0;i<5;i++)check_modes();assert(mode_changes==2);
 rtggfxboards[1].resolutionchange=1;check_modes();assert(mode_changes==2);
 rtggfxboards[1].vga_height=600;rtggfxboards[1].resolutionchange=1;
 fail_mode=true;check_modes();assert(disabled==1);
 puts("VGA blanking, scanline bounds, re-enable and secondary resize passed");
}
'''
code=code.replace('#include <cassert>','#include <cassert>\n#include <initializer_list>')
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++11','-fsanitize=address,undefined','-I'+str(root/'pcem'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
