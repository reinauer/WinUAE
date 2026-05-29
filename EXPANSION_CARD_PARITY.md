# WinUAE Unix Expansion Card Parity Audit

This audit uses the shared Windows catalog in `expansion.cpp`
(`expansionroms[]`, `cpuboards[]`) and the shared RTG hardware board
catalog in `gfxboard.cpp`. It lists cards that are not yet fully at
Windows parity in the Unix port.

Status meanings:

- Missing/deferred: Windows builds the feature, but the Unix build does
  not currently compile or expose the required backend.
- Partial: the shared emulation is compiled or the card is visible, but
  Unix-specific host integration or board/driver validation is not yet
  at Windows parity.
- Validation: no known Unix-only code stub remains, but we still need a
  real guest/driver/ROM test before calling it complete.

## Missing or Deferred

These are Windows-supported expansion-card paths gated by feature macros
that are still not enabled by the Unix build.

- `catweasel` - Catweasel, Individual Computers. Windows defines
  `CATWEASEL`; Unix has no native Catweasel hardware backend.

## Built or Visible, but Still Partial

### PCI Bridges

The Unix build now enables `WITH_PCI` through `WINUAE_UNIX_WITH_GFXBOARD`,
so these are no longer simply absent. They still need guest/driver
validation and PCI-device coverage before they can be called complete.

- `grex` - G-REX, DCE.
- `mediator` - Mediator, Elbox.
- `prometheus` - Prometheus, Matay.
- `prometheusfirestorm` - Prometheus FireStorm, E3B.

### RTG and Hardware Graphics Boards

Unix now builds `GFXBOARD`, PCem video adapters, QEMU VGA glue, and the
RTG UI list. The remaining work is full Windows-equivalent behavior:
board ROM execution, monitor switching, backbuffers, hardware callbacks,
render-thread interactions, and real driver validation for every board.

- UAE Zorro II RTG.
- UAE Zorro III RTG.
- A2410, Commodore.
- Spectrum 28/24 Zorro II and Zorro III.
- Piccolo Zorro II and Zorro III.
- Piccolo SD64 Zorro II and Zorro III.
- CyberVision 64 Zorro III.
- CyberVision 64/3D Zorro II and Zorro III.
- BlizzardVision/CyberVision PPC, Permedia2 PCI.
- Picasso II.
- Picasso II+.
- Picasso IV Zorro II and Zorro III.
- Retina Zorro II and Zorro III.
- Altais DracoBus.
- Merlin Zorro II and Zorro III.
- Graffity Zorro II and Zorro III.
- EGS 110/24, GVP local bus.
- Visiona Zorro II.
- Rainbow III Zorro III.
- Domino Zorro II.
- Pixel64 AteoBus.
- oMniBus ET4000AX and ET4000W32.
- Harlequin Zorro II.
- Rainbow II Zorro II.
- Voodoo 3 3000 PCI.
- Voodoo 5 5500 PCI ROM selector.
- Virge PCI.
- Trio64 PCI.
- Matrox Millennium PCI.
- Matrox Millennium II PCI.
- Matrox Mystique PCI.
- Matrox Mystique 220 PCI.
- x86 Bridgeboard VGA ISA.

### DSP3210

Unix now defines `WITH_DSP` by default and links the shared DSP3210
emulation sources used by Windows. The board is no longer a missing
catalog entry, but it still needs a real DSP3210 guest/ROM validation
pass before it can be marked complete.

- `dsp3210` - DSP3210, AT&T.

### x86 Bridgeboards and ISA Expansions

Unix now defines `WITH_X86` by default, links `x86.cpp`, and builds the
shared PCem x86 bridgeboard interpreter, storage, input, sound, serial,
timer, DMA, NVRAM, and VGA source set. The old PCem dynamic recompiler
source is compiled only far enough to provide the interpreter opcode
tables when `UAE` is defined; Unix does not enable the old PCem dynarec
backend. These boards are visible through the shared expansion catalog,
but still need ROM and guest-driver validation.

- `a1060` - A1060 Sidecar.
- `a2088` - A2088.
- `a2088t` - A2088T.
- `a2286` - A2286.
- `a2386` - A2386SX.
- `x86athdprimary` - AT IDE Primary.
- `x86athdxt` - XTIDE Universal BIOS HD.
- `x86rt1000` - Rancho RT1000.
- `sb_isa` - SoundBlaster ISA.
- `ne2000_isa` - RTL8019 ISA, NE2000 compatible.
- `x86_mouse` - x86 Bridgeboard mouse.
- `x86vga` / `GFXBOARD_ID_VGA` - x86 Bridgeboard VGA.

### DraCo and Casablanca

Unix now defines `WITH_DRACO` by default when NCR SCSI is enabled and
links the shared DraCo/Casablanca source plus the PCem serial, mouse,
and keyboard helpers used by that code. These accelerator subtypes are
no longer missing, but still need ROM/SCSI/video validation on real
configs.

- `draco` - MacroSystem DraCo accelerator subtype.
- `casablanca` - MacroSystem Casablanca accelerator subtype.

### Network Cards

A2065 and Ariadne are backed by the shared 7990 path and can expose SLIRP
on Unix. The wider Ethernet board set is compiled through the QEMU/PCI
network glue, but still needs driver-level validation, adapter selection
polish, and TAP/TUN/pcap permission checks on real systems.

- `a2065` - A2065, Commodore.
- `ariadne` - Ariadne, Village Tronic.
- `ariadne2` - Ariadne II, Village Tronic.
- `hydra` - AmigaNet, Hydra Systems.
- `eb920` - LAN Rover/EB920, ASDG.
- `xsurf` - X-Surf, including its IDE side.
- `xsurf100z2` - X-Surf-100 Zorro II.
- `xsurf100z3` - X-Surf-100 Zorro III.
- `ne2000pcmcia` - RTL8019 PCMCIA, NE2000 compatible.
- `ne2000_pci` - RTL8029 PCI, NE2000 compatible.

### Sound Cards

The Unix build compiles the Toccata/Prelude/UAESND path and PCI sound
devices, but audio routing, mixer behavior, guest drivers, and PCI card
validation are still not at Windows parity.

- `prelude` - Prelude, ACT.
- `prelude1200` - Prelude 1200, ACT.
- `toccata` - Toccata, MacroSystem.
- `es1370` - ES1370 PCI, Ensoniq.
- `fm801` - FM801 PCI, Fortemedia.
- `uaesnd_z2` - UAESND Zorro II.
- `uaesnd_z3` - UAESND Zorro III.
- `uaeboard_z2` - UAEBOARD Zorro II.
- `uaeboard_z3` - UAEBOARD Zorro III.

### Accelerator and CPU Boards

Unix now builds the shared CPU-board catalog and PPC/QEMU plugin support,
so these are visible and partly functional. They remain partial until
their ROM/SCSI/PPC side effects are validated board by board.

- ACT Apollo 1240/1260 and Apollo 630.
- Commodore A2620/A2630.
- DCE SX32 Pro and Typhoon MK2.
- DKB 1230/1240/Cobra and Wildfire.
- Great Valley Products A3001 Series I, A3001 Series II, A530,
  G-Force 030, G-Force 040, Tek Magic 2040/2060, A1230 Turbo+,
  A1230 Turbo+ Series II, QuikPak, QuikPak XP, and T-Rex II.
- Kupke Golem 030.
- MacroSystem Warp Engine A4000 and Falcon 040.
- M-Tec E-Matrix 530.
- Phase 5 Blizzard 1230 I/II, 1230 III, 1230 IV, 1260, 2060, and
  Blizzard PPC.
- Phase 5 CyberStorm MK I, MK II, MK III, and CyberStorm PPC.
- RCS Fusion Forty.
- IVS Vector.
- PPS Zeus 040.
- CSA Magnum 40/4 and Twelve Gauge.
- Hardital TQM.
- Harms Professional 3000.

### Storage Controllers and Disk-Carrying Expansions

Most of these use shared core code and are compiled on Unix, including
NCR/NCR9x where applicable. They are not listed as missing. They are
still not fully complete from a Unix parity perspective until their
board ROMs, disk attachment UI, DMA paths, and guest boot behavior have
been validated against Windows.

- `cdtvscsi` - CDTV SCSI.
- `scsi_a3000` - A3000 SCSI.
- `scsi_a4000t` - A4000T SCSI.
- `ide_mb` - A600/A1200/A4000 IDE.
- `pcmciaide` - PCMCIA IDE.
- `apollo` - Apollo 500/2000.
- `add500` - ADD-500.
- `overdrivehd` - Overdrive HD.
- `addhard` - AddHard.
- `blizzardscsikitiii` - Blizzard SCSI Kit III.
- `blizzardscsikitiv` - Blizzard SCSI Kit IV.
- `csmk1cyberscsi` - CyberSCSI module.
- `accessx` - AccessX.
- `oktagon2008` - Oktagon 2008.
- `alfapower` - AlfaPower/AT-Bus 2008.
- `alfapowerplus` - AlfaPower Plus.
- `tandem` - Tandem.
- `malibu` - Malibu.
- `cltda1000scsi` - A1000/A2000 SCSI.
- `a2090a` - A2090a.
- `a2090b` - A2090 Combitec.
- `a2091` - A590/A2091.
- `a4091` - A4091.
- `comspec` - SA series.
- `rapidfire` - RapidFire/SpitFire.
- `fastata4000` - FastATA 4000.
- `elsathd` - Mega Ram HD.
- `eveshamref` - Reference 40/100.
- `dataflyerscsiplus` - DataFlyer SCSI+.
- `dataflyerplus` - DataFlyer Plus.
- `arriba` - Arriba.
- `gvp1` - GVP Series I.
- `gvp` - GVP Series II.
- `gvpa1208` - GVP A1208.
- `dotto` - Dotto.
- `synthesis` - Synthesis.
- `vector` - Vector Falcon 8000.
- `surfsquirrel` - Surf Squirrel.
- `adide` - AdIDE.
- `adscsi2000` - AdSCSI Advantage 2000/2080.
- `trifecta` - Trifecta.
- `buddha` - Buddha.
- `trumpcard` - Trumpcard.
- `trumpcardpro` - Grand Slam.
- `trumpcardat` - Trumpcard 500AT.
- `kommos` - Kommos A500/A2000 SCSI.
- `golemhd3000` - HD3000.
- `golem` - Golem SCSI II.
- `golemfast` - Golem Fast SCSI/IDE.
- `multievolution` - Multi Evolution 500/2000.
- `mastfb` - Fireball.
- `scram8490` - SCRAM DP8490V.
- `scram5394` - SCRAM NCR53C94.
- `paradox` - Paradox SCSI.
- `ateam` - A-Team.
- `mtecat` - AT 500.
- `mtecmastercard` - Mastercard, M-Tec.
- `masoboshi` - MasterCard, Masoboshi.
- `hardframe` - HardFrame.
- `stardrive` - StarDrive.
- `filecard2000` - Filecard 2000/OSSI 500.
- `pacificoverdrive` - Overdrive, Pacific Peripherals/IVS.
- `fastlane` - Fastlane.
- `phoenixboard` - Phoenix Board SCSI.
- `ptnexus` - Nexus.
- `profex` - HD 3300.
- `protar` - A500 HD.
- `rochard` - RocHard RH800C.
- `inmate` - InMate.
- `supradrive` - SupraDrive.
- `emplant` - Emplant, SCSI only.
- `omtiadapter` - OMTI-Adapter.
- `hd20a` - HD 20 A/HD 40 A.
- `alf1` - A.L.F.
- `alf2` - A.L.F.2.
- `alf3` - A.L.F.3.
- `promigos` - Promigos.
- `wedge` - Wedge.
- `tecmar` - T-Card/T-Disk.
- `system2000` - System 2000.
- `xebec` - 9720H.
- `kronos` - Kronos.
- `hda506` - HDA-506.
- `fasttrak` - FastTrak.
- `ripple` - RIPPLE.

### Other Expansion Entries

These are visible or shared, but still need targeted Unix validation if
we want to mark the full Windows expansion catalog complete.

- `cd32fmv` - CD32 FMV.
- `cdtvdmac` - CDTV DMAC.
- `cdtvsram` - CDTV SRAM.
- `cdtvcr` - CDTV-CR.
- `pcmcia_mb` - A600/A1200 PCMCIA.
- `a1000wom512k` - A1000 512k WOM.
- `amax` - AMAX ROM dongle.
- `dev_ide` - DEV IDE, debug/development IDE entry.
- `keyboard` - Keyboard MCU ROM/settings entry.
- `pcmciasram` - PCMCIA SRAM.
- `cubo` - Cubo CD32.

### Memory Boards

The memory-board catalog is shared and visible, but it also needs a
short Windows-vs-Unix autoconfig validation pass.

- UAE 0xf00000 RAM.
- DKB Insider I/II.
- GVP Impact A2000-RAM8.
- Kupke Golem RAM-Card.
- SupraRAM 500RX.
- SupraRAM 2000.
- E3B ZorRAM.
