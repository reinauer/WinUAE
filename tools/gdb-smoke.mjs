// Exercises guest state; launches and cleans up its own isolated emulator.
import {spawn} from 'node:child_process';
import fs from 'node:fs';
import assert from 'node:assert/strict';
import path from 'node:path';
import os from 'node:os';
import {pathToFileURL} from 'node:url';
import net from 'node:net';
const {WINUAE_PATH, WINUAE_ROM, WINUAE_MCP_PATH} = process.env;
assert(WINUAE_PATH && WINUAE_ROM && WINUAE_MCP_PATH, 'Set WINUAE_PATH, WINUAE_ROM and WINUAE_MCP_PATH');
const {GdbProtocol} = await import(pathToFileURL(path.resolve(WINUAE_MCP_PATH, 'dist/gdb-protocol.js')));
const reserve = net.createServer();
await new Promise((resolve, reject) => { reserve.once('error', reject); reserve.listen(0, '127.0.0.1', resolve); });
const port = reserve.address().port;
await new Promise(resolve => reserve.close(resolve));
const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'winuae-gdb-smoke-'));
console.log('Test files:', temp);
const log=fs.openSync(path.join(temp, 'winuae.log'),'w');
const exe=spawn(WINUAE_PATH,[
 '-s','use_gui=no','-s',`kickstart_rom_file=${WINUAE_ROM}`,
 '-s',`cpu_model=${process.env.WINUAE_CPU_MODEL || '68000'}`,'-s',`cpu_compatible=${process.env.WINUAE_CPU_COMPATIBLE || 'true'}`,'-s','chipmem_size=2','-s',`cachesize=${process.env.WINUAE_CACHE_SIZE || '0'}`,
 '-s','cpu_cycle_exact=false','-s','cpu_memory_cycle_exact=false','-s','blitter_cycle_exact=false',
 '-s','debugging_features=gdbserver','-s',`gdb_port=${port}`
],{cwd:temp,env:{...process.env,SDL_VIDEODRIVER:'dummy',SDL_AUDIODRIVER:'dummy',QT_QPA_PLATFORM:'offscreen'},stdio:['ignore',log,log]});
let g;
const delay=ms=>new Promise(r=>setTimeout(r,ms));
try {
 for(let i=0;i<60;i++) {
  g=new GdbProtocol();
  try {await g.connect('127.0.0.1',port);break;} catch(e) {g.disconnect();g=null;await delay(100);}
 }
 assert(g,'connection failed');
 console.log('connected', await g.readRegisters());
 await g.continue();await delay(1200);await g.pause();
 console.log('booted',await g.readRegisters());
 await g.writeMemory(0x10000,Buffer.from('700152804e7160fe','hex'));
 let r=await g.readRegisters();r.SR=0x2700;r.A7=0x30000;r.PC=0x10000;await g.writeRegisters(r);
 await g.step();r=await g.readRegisters();assert.equal(r.PC,0x10002);assert.equal(r.D0,1);
 await g.step();r=await g.readRegisters();assert.equal(r.PC,0x10004);assert.equal(r.D0,2);
 await g.setBreakpoint(0x10006);await g.continue();await delay(100);await g.pause();
 assert.equal((await g.readRegisters()).PC,0x10006);await g.clearBreakpoint(0x10006);
 await g.writeMemory(0x10000,Buffer.from('13fc0042000200004e7160fe','hex'));
 await g.writeRegister(17,0x10000);await g.setWatchpoint(0x20000,1,'write');
 await g.continue();await delay(100);const stop=await g.pause();assert.match(stop,/watch:/);
 assert.equal((await g.readMemory(0x20000,1))[0],0x42);await g.clearWatchpoint(0x20000,1,'write');
 r=await g.readRegisters();r.SR=0;r.A7=0x22220;await g.writeRegisters(r);assert.equal((await g.readRegisters()).A7,0x22220);
 r.SR=0x2700;r.A7=0x30000;await g.writeRegisters(r);
 const custom=await g.readMemory(0xdff000,512);assert.equal(custom.length,512);
 await g.writeMemory(0xdff180,Buffer.from('0f00','hex'));assert.equal((await g.readMemory(0xdff180,2)).toString('hex'),'0f00');
 assert.equal(await g.sendCommand('M10000,100:00'),'E01');
 assert.equal(await g.sendCommand('P0=zzzzzzzz'),'E01');
 assert.equal(await g.sendCommand('Mffffffff,2:0000'),'E01');
 assert.equal(await g.sendCommand('Mf80000,1:00'),'E01');
 await g.sendCommand('D');g.disconnect();g=new GdbProtocol();await g.connect('127.0.0.1',port);
 console.log('MCP client live tests passed: registers, SR stack switch, RAM, custom snapshot/write, step, breakpoint, watchpoint, malformed writes, ROM rejection, reconnect');
} finally {
 if (g) g.disconnect();
 if (exe.exitCode === null && exe.signalCode === null) {
  await new Promise(resolve => {
   const timer = setTimeout(() => exe.kill('SIGKILL'), 2000);
   exe.once('exit', () => { clearTimeout(timer); resolve(); });
   exe.kill('SIGINT');
  });
 }
 fs.closeSync(log);
}
