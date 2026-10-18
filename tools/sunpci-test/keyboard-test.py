#!/usr/bin/env python3
"""Check SunPCi key stations against the keyboard core's actual scan tables."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
keyboard = (root / 'pcem/keyboard.cpp').read_text()
adapter = (root / 'sunpci.cpp').read_text()
tables = keyboard[keyboard.index('typedef struct'):keyboard.index('static scancode scancode_tandy')]
sender = keyboard[keyboard.index('void keyboard_send_scancode(int code, int is_break)'):]
stations = adapter[adapter.index('static const uae_u16 sunpci_station_to_scancode'):adapter.index("// Sun's first-generation mouse driver")]
code = '''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <initializer_list>
using uae_u8 = uint8_t;
using uae_u16 = uint16_t;
#define write_log(...) ((void)0)
#define SUNPCI_DEBUG_CSR 0
int AT = 1, romset = 0, keyboard_scan = 1;
enum { ROM_XI8088 = 1 };
std::vector<int> output;
void keyboard_send(uint8_t value) { output.push_back(value); }
''' + tables + '\nstatic scancode *at_scancodes = scancode_set1;\n' + sender + stations + '''
static void check(int station, bool release, std::initializer_list<int> expected)
{
 output.clear();
 sunpci_pic_keyboard(station | (release ? 0x80 : 0));
 assert(output == std::vector<int>(expected));
}
int main()
{
 for (auto index : sunpci_station_to_scancode) assert(index < 272);
 check(83, false, {0xe0, 0x48}); check(83, true, {0xe0, 0xc8});
 check(84, false, {0xe0, 0x50}); check(84, true, {0xe0, 0xd0});
 check(79, false, {0xe0, 0x4b}); check(89, false, {0xe0, 0x4d});
 check(76, false, {0xe0, 0x53}); check(64, false, {0xe0, 0x1d});
 check(62, false, {0xe0, 0x38}); check(108, false, {0xe0, 0x1c});
 check(43, false, {0x1c}); check(112, false, {0x3b});
 at_scancodes = scancode_set2;
 check(83, false, {0xe0, 0x75}); check(83, true, {0xe0, 0xf0, 0x75});
 check(84, false, {0xe0, 0x72}); check(84, true, {0xe0, 0xf0, 0x72});
 for (auto table : {scancode_set1, scancode_set2, scancode_set3}) {
  at_scancodes = table;
  for (int station = 0; station < 128; station++) {
   output.clear(); sunpci_pic_keyboard(station); sunpci_pic_keyboard(station | 0x80);
  }
 }
 puts("SunPCi keyboard: extended keys, make/break and all station bounds passed");
}
'''
with tempfile.TemporaryDirectory() as temp:
    source = Path(temp) / 'test.cpp'
    binary = Path(temp) / 'test'
    source.write_text(code)
    subprocess.run(['c++', '-std=c++11', '-fsanitize=address,undefined',
                    str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
