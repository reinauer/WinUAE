# SunPCi diagnostic device fixtures

Select an existing PCI bridge and one of these expansion devices:

- SunPCi I diagnostic fixture (`sunpci1_test`)
- SunPCi II diagnostic fixture (`sunpci2_test`)
- SunPCi III diagnostic fixture (`sunpci3_test`)

These devices expose Sun PCI IDs 108e:5043, 108e:7063 and 108e:676a.
They are emulator test fixtures, not complete SunPCi machines. They do not
identify pro variants or claim their CPU speeds. The subsystem IDs, generic
bridge class 068000, revision zero and BAR geometry are fixture choices,
not captured card configuration. BAR0 is a 4 KiB memory aperture and BAR1
a 256-byte I/O alias. Eight scratch dwords occupy offsets a8 through c4.
Other register reads return all ones; writes are ignored. No interrupts
or DMA are generated. Unknown BARs are absent.

The intended first guest is SUNPCi-info through openpci.library. A register
test can exercise scratch writes and readback through either aperture.
Do not use this fixture as a source of hardware BAR sizes or capability
values. A complete driver must not treat discovery as firmware readiness.

GOGO, FLSH and MFGM written to scratch registers remain there: the fixture
never acknowledges a boot or claims POST completion. No BIOS, CMOS or disk
images are consumed. WinUAE machine reset releases/recreates card state.
A board reset callback is also provided for direct device tests.

## Implementation and validation

`sunpci_pci.inc` is included by `pci.cpp` so existing Windows and Unix build
lists automatically compile the model. The existing PCI layer handles
configuration-space access, BAR sizing/mapping and host-bridge swapping.
The new expansion entries are enabled only in WITH_PCI builds. Devices are
added after existing cards and respect the remaining logical-slot capacity.
Each instance owns its state; there is no global active-card pointer.

Run isolated callback tests from the checkout:

```
sh tools/sunpci-test/run.sh
```

The test compiles against the real `include/pci_hw.h` and `uae/types.h`,
with allocation/logging/calling-convention shims. It covers IDs, all32
walking bits in all eight scratch registers, mixed-width little-endian
access, memory/I/O aliases, aperture boundaries, unsupported accesses,
independent instances, reset and freeing. ASan/UBSan are enabled. It does
not exercise PCI bridge routing or execute an Amiga guest.

Next gates: build WinUAE; verify selection/config persistence; run the
Amiga probe with an OpenPCI-supported emulated bridge; verify BAR routing
and byte lanes; add source-backed board configuration and register
semantics before firmware transport. Scripted mailbox/DMA/interrupt
scenarios and fault injection follow. Actual Sun firmware boot requires
an x86 machine/chipset model and both sides of the Sun bridge.

BAR1 is limited to 256 bytes: OpenPCI 40.16 rejects larger I/O regions.
The memory BAR stays 4 KiB; the scratch registers fit in both apertures.

## SunPCi I machine logging

The full SunPCi I machine in `sunpci.cpp` defaults to quiet logging.
Startup, POST transitions, resets and error diagnostics remain enabled.
Routine SiS/21554 configuration writes, host/card CSR traffic, SMM entry,
PIC input and periodic text-screen dumps require `SUNPCI_DEBUG_CSR=1`
in the compiler definitions. Set it to 2 to include CSR reads; polling
makes that mode especially noisy. Generic host PCI configuration tracing
is separately controlled by `PCI_DEBUG_CONFIG=1` (also off by default).
These are build-time switches; restart WinUAE after rebuilding.

Unhandled card I/O reports each port, access width and direction once per
board lifetime by default. This keeps repeated BIOS probes from consuming
the 2,000-message diagnostic budget before later devices are probed.
`SUNPCI_DEBUG_IO=1` restores repeated-access tracing up to that same limit.
The filter does not change read values, writes, timing or device decoding.
A first probe of the configured SiS ACPI window also reports its base and
ISA configuration register 40 decode-enable bit; that register block is
not implemented yet.

In the BIOS trace, writes to 0378 mirror POST codes. The 0279/0a79 sequence
and reads at 020b, 020f, 0213 and subsequent candidates are ISA PnP
isolation probes. Repeated status accesses at 6100, 6114 and 6125 match
ACPI base 6100 plus the PM, general-purpose and SMI status offsets in the
[SiS 5597/5598 datasheet](https://ftpmirror.your.org/pub/misc/bitsavers/components/sis/SiS-5597-5598_199704.pdf),
printed pages 184-194. Whether the block should respond also depends on
its programmed base and decode-enable bit; these traces alone do not
justify replacing open-bus reads with zero.

Run `python3 tools/sunpci-test/io-log-test.py` for production-function
coverage of repetition, independent widths/directions/boards, the final
port, diagnostic limits and ACPI decode reporting under ASan/UBSan.
Both default and verbose builds are exercised. A native build and isolated
Windows 95 Setup boot also passed; the boot retained later probe messages
without repeating each ISA PnP access. This does not resolve the separate
Windows installation hang or external-display flicker.

## SiS VGA state across native BIOS captures

The native BIOS saves VGA latches and attribute-controller state around
its SMM display capture. CR22 exposes the latch selected by GDC read plane,
CR24 bit 7 exposes the attribute index/data toggle, and CR26 exposes the
attribute index and video-enable bit. These readbacks follow the
[SiS 5597/5598 datasheet](https://ftpmirror.your.org/pub/misc/bitsavers/components/sis/SiS-5597-5598_199704.pdf),
printed page 248. Restoring SR06 also recomputes the CPU access fast path
and restores the attribute plane mask when returning to planar VGA.

Windows 95 sets Screen Off (SR01=0x21) at 640x480. 86Box's SVGA core
selects its blank renderer for Screen Off, and the SiS packed modes keep
it. Dedicated monitors apply pending size changes even with their output
switch off, so they cannot retain an earlier 640x350 surface.

The [SunPCi user's guide](https://docs.oracle.com/cd/E19085-01/pci1.card/805-6058-11/6j5nc882f/index.html)
describes host-window and external-VGA routing as separate modes. In
host-window mode the BIOS's SMM display capture (e000:248f in BIOS 0.59)
sets Screen Off in SR01 and its restore path never clears it, so an
external monitor stays blank from the first POST screen until Windows
loads Sun's display driver. Monitor 2 ignores a Screen Off written in SMM
and keeps showing POST, DOS and the Windows shutdown screen; SR01 still
reads back as written, and Screen Off from normal code still blanks it.
The capture also reprograms the sequencer, graphics and attribute
registers to read the planes, and it repeats at the PIC's SMI rate. A
frame during which SMM wrote VGA registers is not presented: monitor 2
keeps the previous frame and redraws the next one in full, instead of
showing lines drawn in the capture's register state, which flickered and
resized the window. Whole frames are held because 86Box's text renderers
advance the display address, so skipping lines would shift the rest.

## The card on 86Box

The card's PC is an 86Box machine of its own (`X86PC_SUNPCI` in
`86box/winuae/86box_machine.cpp`): an AMD K6-2/300 with 64 MB, 86Box's
8259s with ELCR, 8254, 8237s, AT keyboard controller with a PS/2 keyboard,
MC146818 NVR (saved as `sunpci1-cmos.nvr`) and a 16550 COM2 that the PIC's
mouse bytes reach. The PIC's keys go through 86Box's `keyboard_input()`,
which maps them through `scancode_config_map[]`; 86Box's configuration
loader fills that with the identity mapping, so the card does it at init. `sunpci.cpp` decodes the SiS chipset, the PCI bus at
CF8/CFC and the 21554 itself; SMRAM uses 86Box's `smram_t`. The 86Box PC
side is single instance, so the card can't run next to a bridgeboard,
ATonce or DraCo.

The SiS VGA (`86box/winuae/vid_sis5598.cpp`) runs on 86Box's SVGA core
like the RTG boards and shares their single-card video glue.

The upstream windows forward the card's accesses to the host's PCI bus
only while the primary command register enables bus mastering, as on the
21554; otherwise reads return all ones and writes are dropped. A translated
base has only the bits its window's BAR has, so a base that is not aligned
to the window is rounded down to it: a 128 KB RM region at a 64 KB boundary
lands 64 KB low, as it does on the chip. An upstream access that nothing
on the host's bus decodes sets Received Master Abort in the primary status
register, and Signaled Target Abort in the secondary status when Chip
Control 0's master abort mode asks for it; reads return all ones. The
status registers' error bits and the chip status are write 1 to clear.

Run the `sunpci.cpp` harnesses with

```
for t in io-log mouse pic-irq reset smi; do python3 tools/sunpci-test/$t-test.py; done
```

## Secondary-window presentation cost (21 October 2026)

Secondary SDL windows now retain the last successfully presented pixels.
Unchanged frames skip texture upload and presentation, including static
blank VGA output while Sun's host-window driver is active. Changed pixels
still present immediately; there is no frame-rate limit or dropped update.
Exposure, resize, restore and renderer reset invalidate the retained frame.
Hidden/minimized windows defer presentation and repaint when shown again.
The primary Amiga window and its shaders/status overlays are unaffected.

Disabling secondary vsync alone does not prevent Metal from waiting for a
drawable. During the live version checker, 25.9% of sampled main-thread
stacks included display presentation, with substantial drawable waits.
However, a later idle-desktop comparison advanced 297 emulator frames in
10.026 seconds both with monitor 2 shown and hidden. That comparison found
no idle throughput gain; presentation waits can occupy spare frame time.
Do not describe this change as a measured Windows or disk speedup.

A standalone SDL fixture compiled the production secondary-window path
before and after the change. After one initial 640x480 frame, 300 submissions
with two pixel changes took 4912.855 ms and 300 presents before, versus
16.306 ms and two presents after. Pixel readback checked the rendered colour
and exercised exposure, resize, hide/show and simulated renderer reset.
This is an isolated presentation replay, not a complete emulator workload.
Temporary fixture and live comparison records are under
`/private/tmp/sunpci-presentation/` on the development machine.

The persistent cache regression is reproducible with:

```
cmake --build build --target winuae_unix_frame_cache_test
ctest --test-dir build -R winuae_unix_frame_cache_test --output-on-failure
```

It covers pixel changes, pitch padding, 16/32-bit formats, invalidation,
failed-present retry semantics and repeated static frames. ASan/UBSan and
the Release CMake test passed.
