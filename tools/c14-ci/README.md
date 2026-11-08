This fork-only fixture boots the AROS ROM already embedded in the checked-out
source. It contains no commercial ROMs or operating-system files.

The guest executable is built from `memory.c` with:

```
/opt/amiga-16.2/bin/m68k-amigaos-gcc -O2 -m68020 -o memory memory.c
```

`run.py` checks carry propagation, byte/word/long RAM accesses, protected and
writable ROM, MapROM, custom-register reads, UAE/REAL Z3 placement and 24-bit
addressing. On Intel hosts it records entry into the native JIT fault handler.
Only diagnostics are uploaded by the workflow.
