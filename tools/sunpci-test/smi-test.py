#!/usr/bin/env python3
"""Check the chipset source latch delivered with the PIC's external SMI."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
s=(root/'sunpci.cpp').read_text()
f=s[s.index('static void sunpci_external_smi('):s.index('static void sunpci_sis_host_config_written(')]
code='''
#include <cassert>
#include <cstdint>
#include <cstdio>
struct sunpci_dev { uint8_t cfg[256]; };
struct sunpci_state { sunpci_dev *sis_host; };
static unsigned entries;
static void sunpci_raise_smi(sunpci_state *s){assert(s->sis_host->cfg[0x9d]&2);entries++;}
'''+f+'''
int main(){
 sunpci_dev host={};sunpci_state s={};
 sunpci_external_smi(&s);assert(!entries);
 s.sis_host=&host;host.cfg[0x9d]=0x80;
 sunpci_external_smi(&s);assert(!entries&&host.cfg[0x9d]==0x80);
 host.cfg[0x93]=4;sunpci_external_smi(&s);assert(!entries);
 host.cfg[0x93]=0;host.cfg[0x9b]=2;sunpci_external_smi(&s);assert(!entries);
 host.cfg[0x93]=4;sunpci_external_smi(&s);
 assert(entries==1&&host.cfg[0x9d]==0x82);
 puts("SunPCi external SMI: pin selection, enable and source latch passed");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
