#!/usr/bin/env python3
"""Verify Sun PIC host IRQ pulse latching, masks and repeated edges."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
s = (root / 'sunpci.cpp').read_text()
helper = s[s.index('static void sunpci_pic_host_irq'):s.index('// IBM enhanced keyboard')]
code = r'''
#include <stdint.h>
#include <assert.h>
typedef uint8_t uae_u8;
struct db21554 { uae_u8 pic_host_irqs; };
static unsigned pending, calls;
static void picint(unsigned mask) { pending |= mask; calls++; }
''' + helper + r'''
int main() {
 db21554 b = {0};
 sunpci_pic_host_irq(&b, true, 0xff);
 assert(pending == 0x8e00 && calls == 4 && b.pic_host_irqs == 15);
 sunpci_pic_host_irq(&b, true, 15);
 assert(calls == 4);
 sunpci_pic_host_irq(&b, false, 15);
 assert(pending == 0x8e00 && !b.pic_host_irqs);
 sunpci_pic_host_irq(&b, true, 4);
 assert(calls == 5 && b.pic_host_irqs == 4);
 sunpci_pic_host_irq(&b, false, 0xf0);
 assert(b.pic_host_irqs == 4);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p = Path(tmp)
 (p / 'test.cpp').write_text(code)
 subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                 '-fsanitize=address,undefined', str(p / 'test.cpp'),
                 '-o', str(p / 'test')], check=True)
 subprocess.run([str(p / 'test')], check=True)
print('SunPCi PIC IRQ: masks, edge latching and repeated pulses passed')
