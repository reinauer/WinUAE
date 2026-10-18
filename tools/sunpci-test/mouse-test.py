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
struct SERIAL { unsigned fifo_read,fifo_write,mctrl,lcr; uint8_t fifo[256]; } serial2;
void serial_write_fifo(SERIAL *s,uint8_t b){s->fifo[s->fifo_write]=b;s->fifo_write=(s->fifo_write+1)&255;}
'''+functions+r'''
int main(){
 serial2.lcr=2; // seven data bits, as selected by Microsoft mouse drivers
 sunpci_pic_mouse(0x40);assert(!serial2.fifo_write); // RTS low
 serial2.mctrl=2;sunpci_mouse_identify(&serial2,nullptr);
 assert(serial2.fifo_write==1&&serial2.fifo[0]=='M');serial2.fifo_read=1;
 sunpci_pic_mouse(0x6f);sunpci_pic_mouse(0x3f);sunpci_pic_mouse(0xbf);
 assert(serial2.fifo[1]==0x6f&&serial2.fifo[2]==0x3f&&serial2.fifo[3]==0x3f);
 serial2.mctrl=0x12;sunpci_pic_mouse(0x80);assert(serial2.fifo_write==4);
 serial2.mctrl=2;serial2.lcr=3;sunpci_pic_mouse(0x80);assert(serial2.fifo[4]==0x80);
 serial2.fifo_read=0;serial2.fifo_write=252;assert(!sunpci_mouse_status());
 sunpci_pic_mouse(1);assert(sunpci_mouse_status()==0x10);
 sunpci_pic_mouse(2);sunpci_pic_mouse(3);sunpci_pic_mouse(4);
 assert(serial2.fifo_write==255&&serial2.fifo[254]==3);
 serial2.fifo_read=4;assert(!sunpci_mouse_status());
 sunpci_mouse_identify(&serial2,nullptr);assert(serial2.fifo_write==1&&serial2.fifo_read==0);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('SunPCi mouse: COM2 bytes, data width, loopback and backpressure passed')
