#!/usr/bin/env python3
"""Exercise the Sun PIC's COM2 receive path and status bounds."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'sunpci.cpp').read_text()
functions=s[s.index('static void sunpci_mouse_identify'):s.index('// The control write starts a command')]
code=r'''
#include <cassert>
#include <cstdint>
using uae_u8=uint8_t;
struct serial_t { uint8_t mctrl,lcr,lsr,fifo_enabled; uint16_t out_new; void *rcvr_fifo; uint8_t rx[64]; int n; };
struct sunpci_state { void *serial; uae_u8 mouse_queue[256]; int mouse_head,mouse_tail; };
static int fifo_count;
int fifo_get_full(void *){return fifo_count>=16;}
// 86Box: the byte waits in the shift register until the receive timer runs
void serial_write_fifo(serial_t *s,uint8_t b){s->out_new=b;s->rx[s->n++]=b;}
static void receive_timer(serial_t *s){if(s->out_new==0xffff)return;s->out_new=0xffff;s->lsr|=1;if(s->fifo_enabled)fifo_count++;}
'''+functions+r'''
int main(){
 serial_t u={};u.out_new=0xffff;sunpci_state st={};st.serial=&u;
 u.lcr=2; // seven data bits, as selected by Microsoft mouse drivers
 sunpci_pic_mouse(&st,0x40);assert(st.mouse_head==0); // RTS low
 u.mctrl=2;sunpci_mouse_identify(&u,&st);
 assert(u.n==1&&u.rx[0]=='M');receive_timer(&u);u.lsr=0;
 sunpci_pic_mouse(&st,0x6f);sunpci_pic_mouse(&st,0x3f);sunpci_pic_mouse(&st,0xbf);
 assert(st.mouse_head==3&&st.mouse_queue[2]==0x3f);
 // one byte per character time, into the shift register
 sunpci_mouse_feed(&st);assert(u.n==2&&u.rx[1]==0x6f);
 sunpci_mouse_feed(&st);assert(u.n==2);
 // without its FIFO the UART then holds it until the driver reads it
 receive_timer(&u);sunpci_mouse_feed(&st);assert(u.n==2);
 u.lsr=0;sunpci_mouse_feed(&st);assert(u.n==3&&u.rx[2]==0x3f);
 receive_timer(&u);u.lsr=0;u.fifo_enabled=1;sunpci_mouse_feed(&st);assert(u.n==4&&st.mouse_tail==st.mouse_head);
 receive_timer(&u);assert(fifo_count==1);
 u.mctrl=0x12;sunpci_pic_mouse(&st,0x80);assert(st.mouse_head==3); // loopback
 u.mctrl=2;u.lcr=3;sunpci_pic_mouse(&st,0x80);assert(st.mouse_queue[3]==0x80);
 st.mouse_tail=0;st.mouse_head=252;assert(!sunpci_mouse_status(&st));
 sunpci_pic_mouse(&st,1);assert(sunpci_mouse_status(&st)==0x10);
 sunpci_pic_mouse(&st,2);sunpci_pic_mouse(&st,3);sunpci_pic_mouse(&st,4);
 assert(st.mouse_head==255&&st.mouse_queue[254]==3);
 st.mouse_tail=4;assert(!sunpci_mouse_status(&st));
 sunpci_mouse_identify(&u,&st);assert(st.mouse_head==0&&st.mouse_tail==0);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('SunPCi mouse: COM2 bytes, data width, loopback, UART pacing and backpressure passed')
