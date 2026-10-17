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
