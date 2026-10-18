#!/usr/bin/env python3
"""Check production unknown-I/O diagnostics without changing bus behavior."""
from pathlib import Path
import subprocess
import tempfile
import sys
root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
source = (root / 'sunpci.cpp').read_text()
function = source[source.index('static void sunpci_unknown_io('):source.index('static bool sunpci_bridge_io(')]
code = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <string>
using uae_u8 = uint8_t;
#define _T(x) x
static unsigned CS;
static struct { unsigned pc; } cpu_state;
static std::string log_text;
static void write_log(const char *fmt, ...) {
 char line[512];va_list args;va_start(args,fmt);
 vsnprintf(line,sizeof(line),fmt,args);va_end(args);log_text+=line;
}
struct sunpci_dev { uint8_t cfg[256]; };
struct sunpci_state {
 int unknown_logs;
 uae_u8 unknown_io_seen[65536];
 bool acpi_probe_logged;
 sunpci_dev *sis_isa;
};
''' + function + r'''
int main() {
 sunpci_state s={},other={};sunpci_dev isa={};(void)other;
 sunpci_unknown_io(&s,0xeb,0,0x3f,1);
 assert(log_text.empty()&&!s.unknown_logs);
 // Ordinary first access, then 10,000 BIOS probes of the same port.
 sunpci_unknown_io(&s,0x6100,1,0,2);
 assert(log_text.find("IN16 6100")!=std::string::npos);
 for(int i=0;i<10000;i++)sunpci_unknown_io(&s,0x6100,1,0,2);
#if SUNPCI_DEBUG_IO
 assert(s.unknown_logs==2000);
 assert(log_text.find("diagnostic limit reached")!=std::string::npos);
#else
 assert(s.unknown_logs==1);
 assert(log_text.find("diagnostic limit reached")==std::string::npos);
 // Values/PC changes do not re-enable a port; width/direction do.
 for(int dir=0;dir<2;dir++)for(int size=1;size<=4;size*=2)
  sunpci_unknown_io(&s,0x6100,dir,0xffff,size);
 assert(s.unknown_logs==6);
 CS=0xe000;cpu_state.pc=0x2ba3;
 sunpci_unknown_io(&s,0x6100,0,0x1234,2);assert(s.unknown_logs==6);
 sunpci_unknown_io(&s,0xffff,1,0,4);assert(s.unknown_logs==7);
 sunpci_unknown_io(&s,0xeb,1,0,1);assert(s.unknown_logs==8);
 sunpci_unknown_io(&s,0xeb,0,0,2);assert(s.unknown_logs==9);
 // Suppression is per board, and new ports survive repeated probes.
 sunpci_unknown_io(&other,0x6100,1,0,2);assert(other.unknown_logs==1);
 for(int port=0x1000;port<0x2000;port++)sunpci_unknown_io(&s,port,1,0,1);
 assert(s.unknown_logs==2000);
 assert(log_text.find("diagnostic limit reached")!=std::string::npos);
#endif
 size_t end=log_text.size();sunpci_unknown_io(&s,0x4321,1,0,1);
 assert(log_text.size()==end);
 // ACPI decode is diagnostic only, with separate enabled/disabled cases.
 for(int enabled=0;enabled<2;enabled++) {
  sunpci_state probe={};probe.sis_isa=&isa;
  isa.cfg[0x90]=0;isa.cfg[0x91]=0x61;isa.cfg[0x40]=enabled?0x80:0;
  log_text.clear();sunpci_unknown_io(&probe,0x6100,1,0,2);
  assert(log_text.find(enabled?"decode=enabled":"decode=disabled")!=std::string::npos);
  assert(probe.acpi_probe_logged);
  log_text.clear();sunpci_unknown_io(&probe,0x6114,1,0,2);
  assert(log_text.find("ACPI probe")==std::string::npos);
  assert(isa.cfg[0x40]==(enabled?0x80:0));
 }
 puts("SunPCi I/O logging: repeats, widths, directions, bounds and ACPI diagnostic passed");
}
'''
with tempfile.TemporaryDirectory() as temp:
    p = Path(temp)
    (p/'test.cpp').write_text(code)
    for debug in (0,1):
        subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined','-DSUNPCI_DEBUG_IO='+str(debug),
                        str(p/'test.cpp'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)
