from pathlib import Path
import gzip, json, os, platform, re, shutil, subprocess, sys

binary = Path(sys.argv[1]).resolve()
source = Path(sys.argv[2]).resolve()
root = Path(os.environ.get('C14_RESULTS', 'build/c14-native')).resolve()
root.mkdir(parents=True, exist_ok=True)
data = (source/'aros.rom.cpp').read_text().split('{', 1)[1].split('}', 1)[0]
rom = gzip.decompress(bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', data)))
assert len(rom) == 1024*1024
(root/'kick.rom').write_bytes(rom[524288:])
(root/'ext.rom').write_bytes(rom[:524288])

cases = {
    'high': ('bogomem_size=0\nz3_autoconfig=true\nz3mapping=real\nz3mem_size=1024\nz3mem2_size=64', ' high'),
    'direct': ('', ''),
    'indirect': ('comp_trustbyte=indirect\ncomp_trustword=indirect\ncomp_trustlong=indirect\ncomp_trustnaddr=indirect', ''),
    'writable': ('ks_write_enabled=true', ' writable'),
    'maprom': ('maprom=0x0f000000', ' maprom'),
    'real': ('bogomem_size=0\nz3_autoconfig=true\nz3mapping=real', ''),
    '24bit': ('cpu_model=68020\nfpu_model=0\ncpu_24bit_addressing=true\nz3mem_size=0\nfastmem_size=4', ''),
}
selected = sys.argv[3:] or [name for name in cases if name != 'high']
for name in selected:
    extra, arg = cases[name]
    p = root/name
    guest = p/'guest'
    (guest/'S').mkdir(parents=True, exist_ok=True)
    shutil.copy2(Path(__file__).parent/'memory', guest/'memory')
    (guest/'S/Startup-Sequence').write_text('FailAt 21\nSYS:memory'+arg+' >SYS:memory.txt\nEcho >SYS:done.txt "done"\n')
    for file in ['memory.txt', 'done.txt']: (guest/file).unlink(missing_ok=True)
    config = f'''use_gui=no
kickstart_rom_file={root}/kick.rom
kickstart_ext_rom_file={root}/ext.rom
cpu_model=68040
fpu_model=68040
cpu_24bit_addressing=false
cpu_speed=max
cachesize=8192
comp_flushmode=hard
comp_trustbyte=direct
comp_trustword=direct
comp_trustlong=direct
comp_trustnaddr=direct
chipset=aga
chipset_compatible=A4000
chipmem_size=4
z3mem_size=64
fastmem_size=0
uaeboard=disabled
sound_output=none
floppy0type=-1
floppy1type=-1
nr_floppies=0
cpu_compatible=false
cpu_cycle_exact=false
cpu_memory_cycle_exact=false
blitter_cycle_exact=false
unix.inactive_pause=false
unix.iconified_pause=false
filesystem2=rw,DH0:Test:{guest},0
{extra}
'''
    (p/'guest.uae').write_text(config)
    (p/'trace.py').write_text('''import lldb,json
from pathlib import Path
faults=[]
def fault(frame,bp,unused):
 info=frame.FindVariable('info')
 address=info.GetChildMemberWithName('si_addr').GetValueAsUnsigned()
 faults.append(address)
 return False
def save():
 Path('''+repr(str(p/'faults.json'))+''').write_text(json.dumps(faults))
''')
    tracing = platform.machine() == 'x86_64'
    trace_commands = f'command script import {p}/trace.py\n'
    if tracing:
        trace_commands += 'breakpoint set -n sigsegv_handler\nbreakpoint command add -F trace.fault 2\n'
    script = f'''settings set platform.plugin.darwin.ignored-exceptions EXC_BAD_ACCESS
settings set target.env-vars QT_QPA_PLATFORM=offscreen SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_RENDER_DRIVER=software WINUAE_INI={p}/test.ini
process handle SIGSEGV -s false -n false -p true
process handle SIGBUS -s false -n false -p true
breakpoint set -n do_start_program
run
expression -- canbang
expression -- currprefs.cachesize
expression -- currprefs.comptrustbyte
expression -- z3fastmem_bank[0].start
expression -- z3fastmem_bank[1].start
script assert lldb.debugger.GetSelectedTarget().GetProcess().GetSelectedThread().GetFrameAtIndex(0).EvaluateExpression('canbang').GetValueAsUnsigned() == 1
breakpoint disable 1
{trace_commands}script lldb.debugger.SetAsync(True)
script lldb.debugger.GetSelectedTarget().GetProcess().Continue()
script exec("import time,pathlib; result=pathlib.Path('{guest}/done.txt'); deadline=time.monotonic()+120\\nwhile not result.exists() and time.monotonic()<deadline: time.sleep(0.2)\\ntime.sleep(0.5)\\n")
script lldb.debugger.GetSelectedTarget().GetProcess().Stop()
script lldb.debugger.SetAsync(False)
script trace.save()
expression -- uae_quit()
continue
'''
    (p/'test.lldb').write_text(script)
    with (p/'runtime.log').open('w') as log:
        result = subprocess.run(['lldb','-b','-s',str(p/'test.lldb'),'--',str(binary),'-f',str(p/'guest.uae')], stdout=log, stderr=subprocess.STDOUT, timeout=180)
    assert result.returncode == 0, (name, result.returncode)
    assert (guest/'done.txt').exists(), name
    output = (guest/'memory.txt').read_text()
    assert 'PASS: 20000 ADD/ADDX carry chains' in output, output
    assert 'PASS: memory widths' in output, output
    assert 'FAIL:' not in output, output
    faults = json.loads((p/'faults.json').read_text())
    if tracing and name == 'direct': assert faults, 'direct JIT fault handler was not exercised'
    print(name, 'PASS', 'native faults:', len(faults), flush=True)
