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
    const QString value = settings.value(key).trimmed().toLower();
    if (value == QStringLiteral("true") || value == QStringLiteral("yes") || value == QStringLiteral("1")) {
        if (out) {
            *out = true;
        }
        return true;
    }
    if (value == QStringLiteral("false") || value == QStringLiteral("no") || value == QStringLiteral("0")) {
        if (out) {
            *out = false;
        }
        return true;
    }
    return false;
}

static void parseSettingLine(struct uae_prefs *prefs, const QString &key, const QString &value)
{
    if (value.isEmpty()) {
        return;
    }
    QByteArray line = QStringLiteral("%1=%2").arg(key, value).toLocal8Bit();
    cfgfile_parse_line(prefs, line.data(), 0);
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
    applyWindowSize(settings, prefs);
}

bool applyWinUaeQtConfigToPrefs(const WinUaeQtConfig &config, struct uae_prefs *prefs)
{
    if (!prefs) {
        return false;
    }

    const WinUaeQtConfig::Settings &settings = config.settings();
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        parseSettingLine(prefs, it.key(), it.value());
    }
    applyDirectSettings(settings, prefs);
    return true;
}
