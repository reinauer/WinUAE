#include "prefs_adapter.h"

// Qt defines Qt::HANDLE, and the Unix core compatibility header defines HANDLE.
#include "config.h"

#include <QByteArray>
#include <QString>

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"

static bool settingToInt(const WinUaeQtConfig::Settings &settings, const QString &key, int *out)
{
    if (!settings.contains(key)) {
        return false;
    }
    bool ok = false;
    const int value = settings.value(key).toInt(&ok);
    if (ok && out) {
        *out = value;
    }
    return ok;
}

static bool settingToBool(const WinUaeQtConfig::Settings &settings, const QString &key, bool *out)
{
    if (!settings.contains(key)) {
        return false;
    }
    const QString value = settings.value(key);
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("true") || normalized == QStringLiteral("t") ||
        normalized == QStringLiteral("yes") || normalized == QStringLiteral("y") ||
        normalized == QStringLiteral("1")) {
        if (out) {
            *out = true;
        }
        return true;
    }
    if (normalized == QStringLiteral("false") || normalized == QStringLiteral("f") ||
        normalized == QStringLiteral("no") || normalized == QStringLiteral("n") ||
        normalized == QStringLiteral("0")) {
        if (out) {
            *out = false;
        }
        return true;
    }
    return false;
}

static bool rtgVBlankValueToInt(const QString &value, int *out)
{
    const QString normalized = value.trimmed().toLower();
    int parsed = 0;
    bool ok = false;
    if (normalized == QStringLiteral("real") || normalized == QStringLiteral("default")) {
        parsed = -1;
        ok = true;
    } else if (normalized == QStringLiteral("disabled")) {
        parsed = -2;
        ok = true;
    } else if (normalized == QStringLiteral("chipset")) {
        parsed = 0;
        ok = true;
    } else {
        parsed = normalized.toInt(&ok);
    }
    if (ok && out) {
        *out = parsed;
    }
    return ok;
}

static void parseSettingOption(struct uae_prefs *prefs, const QString &key, const QString &value)
{
    if (value.isEmpty() || key.startsWith(QStringLiteral("unix.ui."))) {
        return;
    }
    if (key == QStringLiteral("uaescsimode") || key == QStringLiteral("unix.uaescsimode")) {
        return;
    }
    if (key == QStringLiteral("rtg_vblank") || key == QStringLiteral("unix.rtg_vblank")) {
        return;
    }
    QByteArray option = key.toLocal8Bit();
    QByteArray optionValue = value.toLocal8Bit();
    cfgfile_parse_option(prefs, option.constData(), optionValue.data(), 0);
}

static bool applyTypedSetting(const WinUaeQtConfig::Settings &settings,
    const WinUaeQtConfig::Setting &setting, struct uae_prefs *prefs)
{
    for (int drive = 0; drive < 4; drive++) {
        const QString key = QStringLiteral("floppy%1wp").arg(drive);
        if (setting.key != key) {
            continue;
        }
        bool value = false;
        if (settingToBool(settings, key, &value)) {
            prefs->floppyslots[drive].forcedwriteprotect = value;
            return true;
        }
        return false;
    }
    return false;
}

static void applyWindowSize(const WinUaeQtConfig::Settings &settings, struct uae_prefs *prefs)
{
    const QString width = settings.value(QStringLiteral("gfx_width_windowed"));
    const QString height = settings.value(QStringLiteral("gfx_height_windowed"));
    if (width.compare(QStringLiteral("native"), Qt::CaseInsensitive) == 0 ||
        height.compare(QStringLiteral("native"), Qt::CaseInsensitive) == 0) {
        prefs->gfx_monitor[0].gfx_size_win.width = 0;
        prefs->gfx_monitor[0].gfx_size_win.height = 0;
        return;
    }

    int value = 0;
    if (settingToInt(settings, QStringLiteral("gfx_width_windowed"), &value)) {
        prefs->gfx_monitor[0].gfx_size_win.width = value;
    }
    if (settingToInt(settings, QStringLiteral("gfx_height_windowed"), &value)) {
        prefs->gfx_monitor[0].gfx_size_win.height = value;
    }
}

static void applyDirectSettings(const WinUaeQtConfig::Settings &settings, struct uae_prefs *prefs)
{
    int value = 0;
    if (settingToInt(settings, QStringLiteral("nr_floppies"), &value)) {
        prefs->nr_floppies = value;
    }
    bool boolValue = false;
    if (settingToBool(settings, QStringLiteral("cpu_24bit_addressing"), &boolValue)) {
        prefs->address_space_24 = boolValue;
    }
    if (settingToInt(settings, QStringLiteral("chipmem_size"), &value)) {
        if (value < 0) {
            prefs->chipmem.size = 0x20000;
        } else if (value == 0) {
            prefs->chipmem.size = 0x40000;
        } else {
            prefs->chipmem.size = value * 0x80000;
        }
    }
    if (settingToInt(settings, QStringLiteral("fastmem_size"), &value)) {
        prefs->fastmem[0].size = value * 0x100000;
    }
    if (settingToInt(settings, QStringLiteral("bogomem_size"), &value)) {
        prefs->bogomem.size = value * 0x40000;
    }
    if (settingToInt(settings, QStringLiteral("z3mem_size"), &value)) {
        prefs->z3fastmem[0].size = value * 0x100000;
    }
    if (settingToInt(settings, QStringLiteral("cachesize"), &value)) {
        prefs->cachesize = value;
    }
    if (settingToInt(settings, QStringLiteral("gfxcard_size"), &value)) {
        prefs->rtgboards[0].rtgmem_size = value * 0x100000;
    }
    QString scsiMode = settings.value(QStringLiteral("uaescsimode"));
    if (scsiMode.isEmpty()) {
        scsiMode = settings.value(QStringLiteral("unix.uaescsimode"));
    }
    if (!scsiMode.isEmpty()) {
        if (scsiMode.compare(QStringLiteral("SPTI+SCSISCAN"), Qt::CaseInsensitive) == 0) {
            prefs->win32_uaescsimode = UAESCSI_SPTISCAN;
        } else if (scsiMode.compare(QStringLiteral("SPTI"), Qt::CaseInsensitive) == 0) {
            prefs->win32_uaescsimode = UAESCSI_SPTI;
        } else {
            prefs->win32_uaescsimode = UAESCSI_CDEMU;
        }
    }
    QString rtgVBlank = settings.value(QStringLiteral("unix.rtg_vblank"));
    if (rtgVBlank.isEmpty()) {
        rtgVBlank = settings.value(QStringLiteral("rtg_vblank"));
    }
    if (rtgVBlankValueToInt(rtgVBlank, &value)) {
        prefs->win32_rtgvblankrate = value;
    }
    applyWindowSize(settings, prefs);
}

bool applyWinUaeQtConfigToPrefs(const WinUaeQtConfig &config, struct uae_prefs *prefs)
{
    if (!prefs) {
        return false;
    }

    const WinUaeQtConfig::Settings &settings = config.settings();
    for (const WinUaeQtConfig::Setting &setting : config.orderedSettings()) {
        if (applyTypedSetting(settings, setting, prefs)) {
            continue;
        }
        parseSettingOption(prefs, setting.key, setting.value);
    }
    applyDirectSettings(settings, prefs);
    return true;
}
