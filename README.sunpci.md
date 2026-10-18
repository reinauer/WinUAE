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

Run `python3 tools/sunpci-test/sis-state-test.py` for latch/attribute
readback and packed-to-planar word-write regression coverage under
ASan/UBSan. This is an isolated source harness, not physical-card proof.

An isolated Windows 95 Setup run still showed intermittent eight-scanline
black bands in the external PCem framebuffer after these corrections.
The observed host-capture mode had SR01=0x21 (Screen Off set), while the
renderer still emitted pixels. The BIOS temporarily switches to packed
access during capture; its relationship to those bands remains under
investigation. No SMM frame-freezing or rendering workaround is applied.

The [SunPCi user's guide](https://docs.oracle.com/cd/E19085-01/pci1.card/805-6058-11/6j5nc882f/index.html)
describes host-window and external-VGA routing as separate modes. Correct
routing and output blanking need validation before changing monitor 2's
current visibility. The state corrections alone do not fix its flicker.
