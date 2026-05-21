#include "config.h"
#include "prefs_adapter.h"

#include <QDebug>
#include <QStringList>

#include <cstdlib>
#include <cstring>

#include "sysconfig.h"
#include "sysdeps.h"
#include "disk.h"
#include "options.h"

static QStringList parsedLines;

static bool configBoolValue(const char *value)
{
    return !strcmp(value, "true") || !strcmp(value, "yes") || !strcmp(value, "1");
}

static void copyText(TCHAR *dst, size_t dstSize, const char *src)
{
    if (!dst || !dstSize) {
        return;
    }
    snprintf(dst, dstSize, "%s", src ? src : "");
}

int cfgfile_parse_option(struct uae_prefs *prefs, const TCHAR *key, TCHAR *value, int)
{
    parsedLines.append(QStringLiteral("%1=%2")
        .arg(QString::fromLocal8Bit(key), QString::fromLocal8Bit(value)));

    if (!strcmp(key, "kickstart_rom_file")) {
        copyText(prefs->romfile, sizeof prefs->romfile, value);
    } else if (!strcmp(key, "kickstart_ext_rom_file")) {
        copyText(prefs->romextfile, sizeof prefs->romextfile, value);
    } else if (!strcmp(key, "floppy0")) {
        copyText(prefs->floppyslots[0].df, sizeof prefs->floppyslots[0].df, value);
    } else if (!strcmp(key, "floppy1")) {
        copyText(prefs->floppyslots[1].df, sizeof prefs->floppyslots[1].df, value);
    } else if (!strcmp(key, "floppy0type")) {
        prefs->floppyslots[0].dfxtype = atoi(value);
    } else if (!strcmp(key, "floppy1type")) {
        prefs->floppyslots[1].dfxtype = atoi(value);
    } else if (!strcmp(key, "floppy2type")) {
        prefs->floppyslots[2].dfxtype = atoi(value);
    } else if (!strcmp(key, "floppy0wp")) {
        prefs->floppyslots[0].forcedwriteprotect = configBoolValue(value);
    } else if (!strcmp(key, "floppy1wp")) {
        prefs->floppyslots[1].forcedwriteprotect = configBoolValue(value);
    } else if (!strcmp(key, "floppy2wp")) {
        prefs->floppyslots[2].forcedwriteprotect = configBoolValue(value);
    } else if (!strcmp(key, "chipset")) {
        prefs->chipset_mask = !strcmp(value, "aga") ? 0x0707 : 0x0101;
    } else if (!strcmp(key, "chipset_compatible")) {
        prefs->cs_compatible = !strcmp(value, "A1200") ? CP_A1200 : CP_GENERIC;
    } else if (!strcmp(key, "cpu_model")) {
        prefs->cpu_model = atoi(value);
        prefs->fpu_model = 0;
    } else if (!strcmp(key, "fpu_model")) {
        prefs->fpu_model = atoi(value);
    } else if (!strcmp(key, "sound_output")) {
        prefs->produce_sound = !strcmp(value, "normal") ? 2 : 0;
    } else if (!strcmp(key, "gfxcard_type")) {
        prefs->rtgboards[0].rtg_index = 0;
        prefs->rtgboards[0].rtgmem_type = !strcmp(value, "ZorroIII") ? 1 : 0;
    }
    return 1;
}

static bool require(bool condition, const char *message)
{
    if (!condition) {
        qWarning().noquote() << message;
    }
    return condition;
}

static bool requireInt(int actual, int expected, const char *field)
{
    if (actual == expected) {
        return true;
    }
    qWarning().noquote() << field << "expected" << expected << "got" << actual;
    return false;
}

static bool requireUnsigned(uae_u32 actual, uae_u32 expected, const char *field)
{
    if (actual == expected) {
        return true;
    }
    qWarning().noquote() << field << "expected" << (qulonglong)expected << "got" << (qulonglong)actual;
    return false;
}

static bool requireText(const TCHAR *actual, const char *expected, const char *field)
{
    if (!strcmp(actual, expected)) {
        return true;
    }
    qWarning().noquote() << field << "expected" << expected << "got" << actual;
    return false;
}

static struct uae_prefs *allocPrefs()
{
    return static_cast<struct uae_prefs *>(calloc(1, sizeof(struct uae_prefs)));
}

static bool testRepresentativeConfig()
{
    WinUaeQtConfig::Settings settings;
    settings.insert(QStringLiteral("kickstart_rom_file"), QStringLiteral("/roms/A1200.rom"));
    settings.insert(QStringLiteral("kickstart_ext_rom_file"), QStringLiteral("/roms/ext.rom"));
    settings.insert(QStringLiteral("floppy0"), QStringLiteral("/disks/install.adf"));
    settings.insert(QStringLiteral("floppy1"), QStringLiteral("/disks/extras.adf"));
    settings.insert(QStringLiteral("floppy0type"), QStringLiteral("0"));
    settings.insert(QStringLiteral("floppy1type"), QStringLiteral("1"));
    settings.insert(QStringLiteral("floppy2type"), QStringLiteral("-1"));
    settings.insert(QStringLiteral("floppy0wp"), QStringLiteral("true"));
    settings.insert(QStringLiteral("floppy1wp"), QStringLiteral("false"));
    settings.insert(QStringLiteral("floppy2wp"), QStringLiteral("yes"));
    settings.insert(QStringLiteral("uaehf0"), QStringLiteral("dir,rw,DH0:System:/tmp/System,0"));
    settings.insert(QStringLiteral("nr_floppies"), QStringLiteral("2"));
    settings.insert(QStringLiteral("chipset"), QStringLiteral("aga"));
    settings.insert(QStringLiteral("chipset_compatible"), QStringLiteral("A1200"));
    settings.insert(QStringLiteral("cpu_model"), QStringLiteral("68020"));
    settings.insert(QStringLiteral("fpu_model"), QStringLiteral("68882"));
    settings.insert(QStringLiteral("cpu_24bit_addressing"), QStringLiteral("false"));
    settings.insert(QStringLiteral("chipmem_size"), QStringLiteral("4"));
    settings.insert(QStringLiteral("fastmem_size"), QStringLiteral("8"));
    settings.insert(QStringLiteral("bogomem_size"), QStringLiteral("2"));
    settings.insert(QStringLiteral("z3mem_size"), QStringLiteral("64"));
    settings.insert(QStringLiteral("cachesize"), QStringLiteral("8"));
    settings.insert(QStringLiteral("sound_output"), QStringLiteral("normal"));
    settings.insert(QStringLiteral("gfxcard_size"), QStringLiteral("16"));
    settings.insert(QStringLiteral("gfxcard_type"), QStringLiteral("ZorroIII"));
    settings.insert(QStringLiteral("gfx_width_windowed"), QStringLiteral("800"));
    settings.insert(QStringLiteral("gfx_height_windowed"), QStringLiteral("600"));
    settings.insert(QStringLiteral("unix.ui.config_path"), QStringLiteral("/configs"));
    settings.insert(QStringLiteral("unix.screenshot_path"), QStringLiteral("/screenshots"));

    struct uae_prefs *prefs = allocPrefs();
    if (!prefs) {
        qWarning().noquote() << "failed to allocate preferences";
        return false;
    }
    prefs->fpu_model = 68881;
    prefs->address_space_24 = true;

    parsedLines.clear();
    bool ok = applyWinUaeQtConfigToPrefs(WinUaeQtConfig(settings), prefs);
    ok = require(ok, "adapter rejected representative config") && ok;
    ok = requireInt(parsedLines.size(), settings.size() - 1, "parsed line count") && ok;
    ok = require(!parsedLines.contains(QStringLiteral("unix.ui.config_path=/configs")), "UI-only path was delegated") && ok;
    ok = require(parsedLines.contains(QStringLiteral("unix.screenshot_path=/screenshots")), "runtime path was not delegated") && ok;
    ok = require(parsedLines.contains(QStringLiteral("chipset_compatible=A1200")), "chipset compatibility was not delegated") && ok;
    ok = require(parsedLines.contains(QStringLiteral("floppy1type=1")), "floppy drive type was not delegated") && ok;
    ok = require(parsedLines.contains(QStringLiteral("floppy0wp=true")), "floppy write protect was not delegated") && ok;
    ok = require(parsedLines.contains(QStringLiteral("uaehf0=dir,rw,DH0:System:/tmp/System,0")), "hard drive mount was not delegated") && ok;
    ok = require(parsedLines.contains(QStringLiteral("fpu_model=68882")), "fpu model was not delegated") && ok;
    ok = require(parsedLines.contains(QStringLiteral("sound_output=normal")), "sound output was not delegated") && ok;

    ok = requireText(prefs->romfile, "/roms/A1200.rom", "romfile") && ok;
    ok = requireText(prefs->romextfile, "/roms/ext.rom", "romextfile") && ok;
    ok = requireText(prefs->floppyslots[0].df, "/disks/install.adf", "floppy0") && ok;
    ok = requireText(prefs->floppyslots[1].df, "/disks/extras.adf", "floppy1") && ok;
    ok = requireInt(prefs->floppyslots[0].dfxtype, DRV_35_DD, "floppy0type") && ok;
    ok = requireInt(prefs->floppyslots[1].dfxtype, DRV_35_HD, "floppy1type") && ok;
    ok = requireInt(prefs->floppyslots[2].dfxtype, DRV_NONE, "floppy2type") && ok;
    ok = require(prefs->floppyslots[0].forcedwriteprotect, "floppy0wp") && ok;
    ok = require(!prefs->floppyslots[1].forcedwriteprotect, "floppy1wp") && ok;
    ok = require(prefs->floppyslots[2].forcedwriteprotect, "floppy2wp") && ok;
    ok = requireInt(prefs->nr_floppies, 2, "nr_floppies") && ok;
    ok = requireInt(prefs->cs_compatible, CP_A1200, "cs_compatible") && ok;
    ok = requireInt(prefs->cpu_model, 68020, "cpu_model") && ok;
    ok = requireInt(prefs->fpu_model, 68882, "fpu_model") && ok;
    ok = require(!prefs->address_space_24, "cpu_24bit_addressing") && ok;
    ok = requireUnsigned(prefs->chipmem.size, 4 * 0x80000, "chipmem.size") && ok;
    ok = requireUnsigned(prefs->fastmem[0].size, 8 * 0x100000, "fastmem[0].size") && ok;
    ok = requireUnsigned(prefs->bogomem.size, 2 * 0x40000, "bogomem.size") && ok;
    ok = requireUnsigned(prefs->z3fastmem[0].size, 64 * 0x100000, "z3fastmem[0].size") && ok;
    ok = requireInt(prefs->cachesize, 8, "cachesize") && ok;
    ok = requireInt(prefs->produce_sound, 2, "produce_sound") && ok;
    ok = requireUnsigned(prefs->rtgboards[0].rtgmem_size, 16 * 0x100000, "rtgmem_size") && ok;
    ok = requireInt(prefs->rtgboards[0].rtgmem_type, 1, "rtgmem_type") && ok;
    ok = requireInt(prefs->gfx_monitor[0].gfx_size_win.width, 800, "gfx width") && ok;
    ok = requireInt(prefs->gfx_monitor[0].gfx_size_win.height, 600, "gfx height") && ok;
    free(prefs);
    return ok;
}

static bool testNativeWindowSize()
{
    WinUaeQtConfig::Settings settings;
    settings.insert(QStringLiteral("gfx_width_windowed"), QStringLiteral("native"));
    settings.insert(QStringLiteral("gfx_height_windowed"), QStringLiteral("900"));

    struct uae_prefs *prefs = allocPrefs();
    if (!prefs) {
        qWarning().noquote() << "failed to allocate preferences";
        return false;
    }
    prefs->gfx_monitor[0].gfx_size_win.width = 123;
    prefs->gfx_monitor[0].gfx_size_win.height = 456;

    bool ok = applyWinUaeQtConfigToPrefs(WinUaeQtConfig(settings), prefs);
    ok = require(ok, "adapter rejected native window size config") && ok;
    ok = requireInt(prefs->gfx_monitor[0].gfx_size_win.width, 0, "native gfx width") && ok;
    ok = requireInt(prefs->gfx_monitor[0].gfx_size_win.height, 0, "native gfx height") && ok;
    free(prefs);
    return ok;
}

static bool testRepeatedMountDelegation()
{
    WinUaeQtConfig config;
    config.applyRepeatedSettings({
        { QStringLiteral("filesystem2"), QStringLiteral("rw,DH0:System:/tmp/System,0") },
        { QStringLiteral("filesystem2"), QStringLiteral("ro,DH1:Work:/tmp/Work,5") }
    }, {
        QStringLiteral("filesystem2")
    });

    struct uae_prefs *prefs = allocPrefs();
    if (!prefs) {
        qWarning().noquote() << "failed to allocate preferences";
        return false;
    }

    parsedLines.clear();
    bool ok = applyWinUaeQtConfigToPrefs(config, prefs);
    ok = require(ok, "adapter rejected repeated mount config") && ok;
    ok = requireInt(parsedLines.size(), 2, "repeated mount parsed line count") && ok;
    ok = require(parsedLines.contains(QStringLiteral("filesystem2=rw,DH0:System:/tmp/System,0")), "first repeated mount was not delegated") && ok;
    ok = require(parsedLines.contains(QStringLiteral("filesystem2=ro,DH1:Work:/tmp/Work,5")), "second repeated mount was not delegated") && ok;
    free(prefs);
    return ok;
}

int main()
{
    bool ok = true;
    ok = require(!applyWinUaeQtConfigToPrefs(WinUaeQtConfig(), nullptr), "null prefs should fail") && ok;
    ok = testRepresentativeConfig() && ok;
    ok = testNativeWindowSize() && ok;
    ok = testRepeatedMountDelegation() && ok;
    return ok ? 0 : 1;
}
