// Runs the MCP stdio tools against one owned WinUAE at a time.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import path from 'node:path';
import os from 'node:os';
import net from 'node:net';
import {pathToFileURL} from 'node:url';
const {WINUAE_PATH, WINUAE_ROM, WINUAE_MCP_PATH} = process.env;
assert(WINUAE_PATH && WINUAE_ROM && WINUAE_MCP_PATH, 'Set WINUAE_PATH, WINUAE_ROM and WINUAE_MCP_PATH');
const sdk = path.resolve(WINUAE_MCP_PATH, 'node_modules/@modelcontextprotocol/sdk/dist/esm/client');
const {Client} = await import(pathToFileURL(path.join(sdk, 'index.js')));
const {StdioClientTransport} = await import(pathToFileURL(path.join(sdk, 'stdio.js')));
const reserve = net.createServer();
await new Promise((resolve, reject) => { reserve.once('error', reject); reserve.listen(0, '127.0.0.1', resolve); });
const port = reserve.address().port;
await new Promise(resolve => reserve.close(resolve));
const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'winuae-mcp-smoke-'));
console.log('Test files:', temp);
const config = path.join(temp, 'machine.uae');
const screenshot = path.join(temp, 'screen shot.png');
fs.writeFileSync(config,`use_gui=no\ncpu_model=68000\ncpu_compatible=true\nchipmem_size=2\ncachesize=0\nkickstart_rom_file=${WINUAE_ROM}\n`);
const original=fs.readFileSync(config);
const transport=new StdioClientTransport({command:process.execPath,args:[path.resolve(WINUAE_MCP_PATH, 'dist/index.js')],env:{...process.env,WINUAE_PATH,WINUAE_CONFIG:config,WINUAE_GDB_PORT:String(port),SDL_VIDEODRIVER:'dummy',SDL_AUDIODRIVER:'dummy',QT_QPA_PLATFORM:'offscreen'},stderr:'pipe'});
const client=new Client({name:'winuae-integration-test',version:'1'},{capabilities:{}});
const delay=ms=>new Promise(r=>setTimeout(r,ms));
async function call(name,args={}){const r=await client.callTool({name,arguments:args});const text=r.content.map(c=>c.text||'').join('\n');assert(!text.startsWith('Error:') && !/VERIFY (FAILED|MISMATCH)/.test(text),name+': '+text);console.log(name+': '+text.slice(0,120));return text;}
try {
 await client.connect(transport);
 transport.stderr?.on('data',d=>fs.appendFileSync(path.join(temp, 'mcp.log'),d));
 await call('winuae_connect');await call('winuae_continue');await delay(1500);await call('winuae_pause');
 await call('winuae_memory_write',{address:'$10000',data:'700152804e7160fe'});
 await call('winuae_registers_set',{SR:'$2700',A7:'$30000',PC:'$10000'});
 await call('winuae_step',{count:2});
 await call('winuae_memory_read',{address:'$10000',length:8});
 await call('winuae_memory_dump',{address:'$10000',length:8});
 await call('winuae_custom_registers');
 assert.match(await call('winuae_disassemble',{address:'$10000',count:3}), /moveq/i);
 await call('winuae_screenshot',{filename:screenshot});
 const png=fs.readFileSync(screenshot);assert.equal(png.subarray(0,8).toString('hex'),'89504e470d0a1a0a');
 await call('winuae_status');
 await call('winuae_reset');
 await call('winuae_eject_disk',{drive:0});
 await call('winuae_disconnect');
 await call('winuae_connect');await call('winuae_disconnect');
 assert.deepEqual(fs.readFileSync(config),original);
 console.log('MCP stdio end-to-end passed; user config unchanged; child shutdown awaited');
}finally{try{await call('winuae_disconnect');}catch{}await client.close();}
