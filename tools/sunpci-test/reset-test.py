#!/usr/bin/env python3
"""Exercise the production CF9 decoder and reset dispatch under sanitizers."""
from pathlib import Path
import subprocess
import tempfile
import sys
root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
source = (root / 'sunpci.cpp').read_text()
functions = source[source.index('static void sunpci_reset_control_write('):source.index('/* Machine */')]
code = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
using uae_u8 = uint8_t;
#define write_log(...) ((void)0)
struct sunpci_state { uint32_t cf8; uint8_t reset_control,post,port92; bool smm,smi_latched; } state;
sunpci_state *sunpci = &state;
struct { int smi_pending; } cpu_state;
int cpu_resets,board_resets,smram_updates,mem_a20_alt;
void resetx86() { cpu_resets++; }
void softresetx86() { cpu_resets++; }
void cpu_set_edx() {}
void mem_a20_recalc() {}
void sunpci_card_reset(sunpci_state *) { board_resets++; }
void sunpci_smram_update(sunpci_state *) { smram_updates++; }
uint8_t sunpci_config_readb(sunpci_state *,int) { return 0; }
void sunpci_config_writeb(sunpci_state *,int,uint8_t) {}
bool sunpci_bridge_io(sunpci_state *,uint16_t,int,uint32_t *,int) { return false; }
bool sunpci_vga_io(sunpci_state *,uint16_t,int,uint32_t *,int) { return false; }
bool x86_io_handler_access(uint16_t,int,uint32_t *,int) { return false; }
void sunpci_unknown_io(sunpci_state *,uint16_t,int,uint32_t,int) {}
''' + functions + r'''
static void write(uint8_t value) { sunpci_x86_io(0xcf9,0,value,1); }
int main()
{
 assert(sunpci_x86_io(0xcf9,1,0,1)==0);
 // BIOS enables control bit 3 before requesting the reset with 0x0c.
 write(8);assert(cpu_resets==0 && board_resets==0);
 assert(sunpci_x86_io(0xcf9,1,0,1)==8);
 // CF9 overlaps a byte of CF8, but PCI-address dword writes must not reset.
 sunpci_x86_io(0xcf8,0,0x80000cfc,4);
 assert(cpu_resets==0 && board_resets==0);
 state.smm=state.smi_latched=true;cpu_state.smi_pending=1;
 write(12);
 assert(cpu_resets==1 && board_resets==0 && smram_updates==1);
 assert(!state.smm && !state.smi_latched && !cpu_state.smi_pending);
 assert(state.cf8==0x80000cfc && sunpci_x86_io(0xcf9,1,0,1)==8);
 write(12);assert(cpu_resets==1); // no new rising edge
 write(8);write(12);assert(cpu_resets==2);
 write(0);cpu_state.smi_pending=1;write(6);
 assert(board_resets==1 && cpu_resets==2 && !cpu_state.smi_pending);
 assert(sunpci_x86_io(0xcf9,1,0,1)==0);
 write(6);assert(board_resets==2);
 puts("SunPCi reset: CF9 decoding, edge triggering and reset scope passed");
}
'''
with tempfile.TemporaryDirectory() as temp:
    src = Path(temp) / 'test.cpp'
    binary = Path(temp) / 'test'
    src.write_text(code)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(src), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
