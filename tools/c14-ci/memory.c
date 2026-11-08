#include <proto/exec.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <stdlib.h>
#include <string.h>
static volatile unsigned long sink;
static void emit_line(const char *s) { PutStr((STRPTR)s); PutStr((STRPTR)"\n"); }
static void emit_address(const char *s, unsigned long value) {
 char out[9]; static const char digits[]="0123456789abcdef";
 for(int i=0;i<8;i++)out[i]=digits[(value>>((7-i)*4))&15];
 out[8]=0; PutStr((STRPTR)s); emit_line(out);
}
static __attribute__((noinline)) unsigned char r8(volatile unsigned char *p) { return *p; }
static __attribute__((noinline)) unsigned short r16(volatile unsigned short *p) { return *p; }
static __attribute__((noinline)) unsigned long r32(volatile unsigned long *p) { return *p; }
static __attribute__((noinline)) void w8(volatile unsigned char *p, unsigned char v) { *p=v; }
static __attribute__((noinline)) void w16(volatile unsigned short *p, unsigned short v) { *p=v; }
static __attribute__((noinline)) void w32(volatile unsigned long *p, unsigned long v) { *p=v; }
static void fail(const char *s) { PutStr((STRPTR)"FAIL: "); emit_line(s); exit(20); }
static void ram(unsigned long flags) {
 unsigned char *p=AllocMem(65536,flags|MEMF_CLEAR);
 if(!p)fail("AllocMem");
 for(unsigned long i=0;i<20000;i++) {
  unsigned long off=(i*12)&65520;
  w8(p+off,(unsigned char)i); if(r8(p+off)!=(unsigned char)i)fail("byte RAM");
  w16((void*)(p+off),(unsigned short)(i^0xa55a)); if(r16((void*)(p+off))!=(unsigned short)(i^0xa55a))fail("word RAM");
  w32((void*)(p+off),i^0x12345678); if(r32((void*)(p+off))!=(i^0x12345678))fail("long RAM");
 }
 emit_address(flags&MEMF_CHIP?"RAM chip PASS at ":"RAM fast PASS at ",(unsigned long)p);
 FreeMem(p,65536);
}
static void carry(void) {
 for(unsigned long i=0;i<20000;i++) {
  register unsigned long a __asm__("d0")=i&1?0xffff:0;
  register unsigned long b __asm__("d1")=1;
  register unsigned long c __asm__("d2")=0x12345678;
  register unsigned long z __asm__("d3")=0;
  __asm__ volatile("add.w %0,%1\n\tclr.w %0\n\tswap %0\n\taddx.l %3,%2"
   : "+d"(a),"+d"(b),"+d"(c) : "d"(z) : "cc");
  if(c!=0x12345678+(i&1))fail("ADD/ADDX carry chain");
 }
 emit_line("PASS: 20000 ADD/ADDX carry chains");
}
int main(int argc,char **argv) {
 carry();
 ram(MEMF_CHIP);ram(MEMF_FAST);
 volatile unsigned char *rom=(void*)0xfc0100;
 unsigned long before=r32((void*)rom);
 if(argc>1 && !strcmp(argv[1],"maprom")) {
  typedef unsigned long (*uaelib_fn)(unsigned long,unsigned long,unsigned long,unsigned long,unsigned long,unsigned long);
  unsigned long base=((uaelib_fn)0xf0ff60)(80,0,0,0,0,0);
  if(base==0xffffffff || !base)fail("MapROM unavailable");
  volatile unsigned char *alias=(void*)(base+0x40100);
  w8(alias,(unsigned char)(before>>24)^0xff);
  if(r8(rom)!=((unsigned char)(before>>24)^0xff))fail("MapROM byte");
  w16((void*)alias,(unsigned short)(before>>16)^0xa55a);
  if(r16((void*)rom)!=((unsigned short)(before>>16)^0xa55a))fail("MapROM word");
  w32((void*)alias,before^0xabcdef12);
  if(r32((void*)rom)!=(before^0xabcdef12))fail("MapROM long");
  w32((void*)alias,before);
  emit_address("PASS: MapROM byte/word/long writes at ",base);
 } else if(argc>1 && !strcmp(argv[1],"writable")) {
  w32((void*)rom,before^0xabcdef12);
  if(r32((void*)rom)!=(before^0xabcdef12))fail("writable ROM");
  w32((void*)rom,before);
 } else {
  w8(rom,(unsigned char)(r8(rom)^0xff));
  w16((void*)rom,(unsigned short)(r16((void*)rom)^0xffff));
  w32((void*)rom,before^0xffffffff);
  if(r32((void*)rom)!=before)fail("protected ROM changed");
 }
 for(int i=0;i<20000;i++)sink += r8((void*)0xdff006)+r16((void*)0xdff004)+r32((void*)0xdff004);
 emit_line("PASS: memory widths, ROM protection and custom-register reads");
 return 0;
}
