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
 await g.rangeStep(0x10000,0x10004); await delay(100); await g.pause();
 assert.equal((await g.readRegisters()).PC,0x10004);
 await g.writeRegister(17,0x10000);
 await g.rangeStep(0x10000,0x10000); await delay(100); await g.pause();
 r=await g.readRegisters();assert.equal(r.PC,0x10002);assert.equal(r.D0,1);
 await g.step();r=await g.readRegisters();assert.equal(r.PC,0x10004);assert.equal(r.D0,2);
 await g.setBreakpoint(0x10006);await g.continue();await delay(100);await g.pause();
 assert.equal((await g.readRegisters()).PC,0x10006);await g.clearBreakpoint(0x10006);
 await g.rangeStep(0x10006,0x10008);await delay(100);await g.pause();
 assert.equal((await g.readRegisters()).PC,0x10006);
 await g.setBreakpoint(0x10002);await g.writeRegister(17,0x10000);
 await g.rangeStep(0x10000,0x10008);await delay(100);await g.pause();
 assert.equal((await g.readRegisters()).PC,0x10002);await g.clearBreakpoint(0x10002);
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
 // Copper DMA watchpoint: source filtering must ignore a CPU write.
 await g.writeMemory(0xdff096,Buffer.from('7fff','hex'));
 await g.writeMemory(0x25000,Buffer.from('01800f00fffffffe','hex'));
 const dmaId=Number(await g.sendMonitorCommand('dma-watch add 25000 8 3 200'));
 assert.equal(Number(await g.sendMonitorCommand('dma-watch add 25000 8 3 200')),dmaId);
 await g.writeMemory(0x10000,Buffer.from('33fc01800002500060fe','hex'));
 await g.writeRegister(17,0x10000); await g.setBreakpoint(0x10008);
 await g.continue(); await delay(100); assert.doesNotMatch(await g.pause(), /watch:/);
 assert.equal((await g.readRegisters()).PC,0x10008); await g.clearBreakpoint(0x10008);
 await g.writeMemory(0xdff080,Buffer.from('00025000','hex'));
 await g.writeMemory(0xdff088,Buffer.from('0000','hex'));
 await g.writeMemory(0xdff096,Buffer.from('8280','hex'));
 await g.continue(); await delay(100);
 assert.match(await g.pause(), /awatch:0002500[0246];winuae-source:00000200;/);
 await g.sendMonitorCommand(`dma-watch remove ${dmaId.toString(16)}`);
 assert.deepEqual(JSON.parse(await g.sendMonitorCommand('dma-watch list')),[]);
 await g.writeMemory(0xdff096,Buffer.from('7fff','hex'));
 // A synthetic Exec process exercises entry detection without a guest disk.
 const oldExec = await g.readMemory(4, 4);
 const long = value => { const b = Buffer.alloc(4); b.writeUInt32BE(value); return b; };
 const putLong = async (address, value) => g.writeMemory(address, long(value));
 await g.writeMemory(0x10000, Buffer.from('700152804e7160fe', 'hex'));
 await g.writeMemory(0x10100, Buffer.from('4ef900010000', 'hex'));
 await putLong(0xfff8, 16); await putLong(0xfffc, 0);
 await putLong(4, 0x21000); await putLong(0x21000 + 276, 0x22000);
 await g.writeMemory(0x22008, Buffer.from([13])); await putLong(0x2200a, 0x24000);
 await g.writeMemory(0x24000, Buffer.from('GDB fixture\0'));
 await putLong(0x22000 + 172, 0x23000 / 4); await putLong(0x23000 + 16, 0x24100 / 4);
 await g.writeMemory(0x24100, Buffer.from([7, ...Buffer.from('Fixture')]));
 await putLong(0x23000 + 60, 0xfffc / 4);
 await g.sendMonitorCommand('process-break name fixture');
 assert.equal(JSON.parse(await g.sendMonitorCommand('process-break status')).armed, true);
 await g.writeRegister(17, 0x10100); await g.continue(); await delay(100);
 assert.match(await g.pause(), /winuae-entry:00022000/);
 assert.equal((await g.readRegisters()).PC, 0x10000);
 assert.equal(JSON.parse(await g.sendMonitorCommand('process-break status')).armed, false);
 const segments = JSON.parse(await g.sendMonitorCommand('segments'));
 assert.equal(segments.process, 0x22000);
 assert.deepEqual(segments.segments, [{index:0,address:0x10000,size:8}]);
 assert.deepEqual(JSON.parse(await g.sendMonitorCommand('segments 22000')),segments);
 await putLong(0xfffc, 0xfffc / 4);
 await assert.rejects(g.sendMonitorCommand('segments'));
 await putLong(0xfffc, 0);
 await g.sendMonitorCommand('process-break clear');
 await g.writeMemory(4, oldExec);
 // Illegal instruction: snapshot the faulting state, stop after frame entry.
 const oldVector = await g.readMemory(16,4);
 await putLong(16,0x10100);
 await g.writeMemory(0x10100,Buffer.from('60fe','hex'));
 await g.writeMemory(0x10000,Buffer.from('4afc','hex'));
 await g.sendMonitorCommand('exception-mask 10');
 await g.writeRegister(17,0x10000); await g.continue(); await delay(100);
 assert.match(await g.pause(),/^T04winuae-exception:04;winuae-faultpc:00010000;/);
 const exception=JSON.parse(await g.sendMonitorCommand('exception'));
 assert.equal(exception.last.vector,4); assert.equal(exception.last.instruction_pc,0x10000);
 assert.equal(exception.last.registers[15],0x30000);
 assert.equal((await g.readRegisters()).PC,0x10100);
 await g.sendMonitorCommand('exception-mask 0');
 await g.writeMemory(16,oldVector);
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
