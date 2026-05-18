#include "launcher_bridge.h"

#include "launcher.h"
#include "prefs_adapter.h"

#include <QByteArray>
#include <QVector>
#include <QString>
#include <QStringList>

#include <stdio.h>
#include <string.h>

// Qt defines Qt::HANDLE, and the Unix core compatibility header defines HANDLE.
#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "memory.h"
#include "autoconf.h"
#include "xwin.h"
#include "audio.h"
#include "inputrecord.h"
#include "savestate.h"
#include "newcpu.h"
#include "zfile.h"

static QString bridgeText(const TCHAR *text)
{
    return text && text[0] ? QString::fromLocal8Bit(text) : QString();
}

static QString bridgeHex(uae_u32 value)
{
    return QStringLiteral("0x%1").arg(value, 8, 16, QLatin1Char('0'));
}

static QVector<WinUaeQtHardwareBoard> bridgeHardwareBoards(void *context)
{
    struct uae_prefs *prefs = static_cast<struct uae_prefs *>(context);
    QVector<WinUaeQtHardwareBoard> boards;
    if (!prefs) {
        return boards;
    }

    expansion_generate_autoconfig_info(prefs);
    for (int i = 0;; i++) {
        struct autoconfig_info *aci = expansion_get_autoconfig_data(prefs, i);
        if (!aci) {
            break;
        }
        WinUaeQtHardwareBoard board;
        board.index = i;
        board.type = aci->zorro >= 1 && aci->zorro <= 3 ? QStringLiteral("Z%1").arg(aci->zorro) : QStringLiteral("-");
        if (aci->parent_of_previous) {
            board.name += QStringLiteral(" - ");
        } else if (aci->parent_address_space || aci->parent_romtype) {
            board.name += QStringLiteral("? ");
        }
        board.name += bridgeText(aci->name);
        if (board.name.isEmpty()) {
            board.name = QStringLiteral("<no name>");
        }
        board.start = aci->start != 0xffffffff ? bridgeHex(aci->start) : QStringLiteral("-");
        board.end = aci->size != 0 ? bridgeHex(aci->start + aci->size - 1) : QStringLiteral("-");
        board.size = aci->size != 0 ? bridgeHex(aci->size) : QStringLiteral("-");
        if (aci->autoconfig_bytes[0] != 0xff) {
            board.id = QStringLiteral("0x%1/0x%2")
                .arg((aci->autoconfig_bytes[4] << 8) | aci->autoconfig_bytes[5], 4, 16, QLatin1Char('0'))
                .arg(aci->autoconfig_bytes[1], 2, 16, QLatin1Char('0'));
        } else {
            board.id = QStringLiteral("-");
        }
        board.movable = expansion_can_move(prefs, i);
        boards.append(board);
    }
    return boards;
}

static bool bridgeHardwareCustomOrder(void *context)
{
    struct uae_prefs *prefs = static_cast<struct uae_prefs *>(context);
    return prefs && prefs->autoconfig_custom_sort;
}

static void bridgeSetHardwareCustomOrder(void *context, bool enabled)
{
    struct uae_prefs *prefs = static_cast<struct uae_prefs *>(context);
    if (!prefs) {
        return;
    }
    prefs->autoconfig_custom_sort = enabled;
    expansion_set_autoconfig_sort(prefs);
}

static bool bridgeHardwareCanMove(void *context, int index, int direction)
{
    struct uae_prefs *prefs = static_cast<struct uae_prefs *>(context);
    return prefs
        && expansion_can_move(prefs, index)
        && expansion_autoconfig_move(prefs, index, direction, true) >= 0;
}

static int bridgeMoveHardwareBoard(void *context, int index, int direction)
{
    struct uae_prefs *prefs = static_cast<struct uae_prefs *>(context);
    if (!prefs) {
        return -1;
    }
    return expansion_autoconfig_move(prefs, index, direction, false);
}

static QString bridgeOrderOnlyValue(const QString &value)
{
    for (QString token : value.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        token = token.trimmed();
        if (token.startsWith(QStringLiteral("order="), Qt::CaseInsensitive)) {
            return token;
        }
    }
    return QString();
}

static WinUaeQtConfig::Settings bridgeHardwareOrderSettings(void *context)
{
    struct uae_prefs *prefs = static_cast<struct uae_prefs *>(context);
    WinUaeQtConfig::Settings settings;
    if (!prefs) {
        return settings;
    }

    struct zfile *file = zfile_fopen_empty(nullptr, _T("unix-qt-prefs.uae"), 0);
    if (!file) {
        return settings;
    }
    cfgfile_save_options(file, prefs, CONFIG_TYPE_ALL);
    size_t len = 0;
    const uae_u8 *data = zfile_get_data_pointer(file, &len);
    const QString text = data && len ? QString::fromLocal8Bit((const char *)data, int(len)) : QString();
    zfile_fclose(file);

    for (QString line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        if (key == QStringLiteral("board_custom_order")) {
            settings.insert(key, value);
            continue;
        }
        const QString order = bridgeOrderOnlyValue(value);
        if (!order.isEmpty()) {
            settings.insert(key, order);
        }
    }
    return settings;
}

static void bridgeSaveScreenshot(void *)
{
    screenshot(-1, 1, 0);
}

static bool bridgeSampleRipperEnabled(void *)
{
    return sampleripper_enabled != 0;
}

static void bridgeSetSampleRipperEnabled(void *, bool enabled)
{
    if ((sampleripper_enabled != 0) == enabled) {
        return;
    }
    sampleripper_enabled = enabled ? 1 : 0;
    audio_sampleripper(-1);
}

static void bridgeCopyPath(TCHAR *dst, size_t dstSize, const char *path)
{
    if (!dst || !dstSize) {
        return;
    }
    if (!path) {
        path = "";
    }
    _tcsncpy(dst, path, dstSize - 1);
    dst[dstSize - 1] = 0;
}

static bool bridgeStatePlaybackEnabled(void *)
{
    return input_play != 0;
}

static bool bridgeStateRecordingEnabled(void *)
{
    return input_record != 0;
}

static bool bridgeCanSaveStateRecording(void *)
{
    return input_record > INPREC_RECORD_NORMAL;
}

static bool bridgeSetStatePlayback(void *, bool enabled, const char *path)
{
    if (input_record) {
        inprec_close(true);
    }
    if (!enabled) {
        if (input_play) {
            inprec_close(true);
        }
        return true;
    }
    if (!path || !path[0]) {
        return false;
    }
    inprec_close(true);
    input_play = INPREC_PLAY_NORMAL;
    bridgeCopyPath(currprefs.inprecfile, sizeof currprefs.inprecfile / sizeof(TCHAR), path);
    set_special(SPCFLAG_MODE_CHANGE);
    return true;
}

static void bridgeToggleStateRecording(void *)
{
    if (input_play) {
        inprec_playtorecord();
    } else if (input_record) {
        inprec_close(true);
    } else {
        input_record = INPREC_RECORD_START;
        set_special(SPCFLAG_MODE_CHANGE);
    }
}

static bool bridgeSaveStateRecording(void *, const char *path)
{
    if (input_record <= INPREC_RECORD_NORMAL || !path || !path[0]) {
        return false;
    }

    TCHAR inputPath[MAX_DPATH];
    TCHAR statePath[MAX_DPATH];
    bridgeCopyPath(inputPath, sizeof inputPath / sizeof(TCHAR), path);
    _sntprintf(statePath, sizeof statePath / sizeof(TCHAR), _T("%s.uss"), inputPath);
    statePath[(sizeof statePath / sizeof(TCHAR)) - 1] = 0;

    inprec_save(inputPath, statePath);
    statefile_save_recording(statePath);
    return true;
}

static WinUaeQtHardwareInfoProvider bridgeHardwareProvider(struct uae_prefs *prefs, bool runtimeActions)
{
    WinUaeQtHardwareInfoProvider provider;
    provider.context = prefs;
    provider.boards = bridgeHardwareBoards;
    provider.customOrder = bridgeHardwareCustomOrder;
    provider.setCustomOrder = bridgeSetHardwareCustomOrder;
    provider.canMove = bridgeHardwareCanMove;
    provider.move = bridgeMoveHardwareBoard;
    provider.orderSettings = bridgeHardwareOrderSettings;
    provider.sampleRipperEnabled = bridgeSampleRipperEnabled;
    provider.setSampleRipperEnabled = bridgeSetSampleRipperEnabled;
    if (runtimeActions) {
        provider.saveScreenshot = bridgeSaveScreenshot;
        provider.statePlaybackEnabled = bridgeStatePlaybackEnabled;
        provider.stateRecordingEnabled = bridgeStateRecordingEnabled;
        provider.canSaveStateRecording = bridgeCanSaveStateRecording;
        provider.setStatePlayback = bridgeSetStatePlayback;
        provider.toggleStateRecording = bridgeToggleStateRecording;
        provider.saveStateRecording = bridgeSaveStateRecording;
    }
    return provider;
}

int runWinUaeQtLauncherForPrefs(int argc, char **argv, struct uae_prefs *prefs, int *exitCode)
{
    return runWinUaeQtLauncherForPrefsWithConfig(argc, argv, prefs, nullptr, exitCode);
}

int runWinUaeQtLauncherForPrefsWithConfig(int argc, char **argv, struct uae_prefs *prefs, const char *initialConfigPath, int *exitCode)
{
    const QString initialPath = initialConfigPath && initialConfigPath[0]
        ? QString::fromLocal8Bit(initialConfigPath)
        : QString();
    WinUaeQtLauncherResult result = runWinUaeQtLauncherForConfig(argc, argv, initialPath, bridgeHardwareProvider(prefs, !initialPath.isEmpty()));
    if (result.status == WinUaeQtLauncherStatus::StartRequested) {
        if (!applyWinUaeQtConfigToPrefs(result.config, prefs)) {
            fprintf(stderr, "Unix Qt UI failed: no preferences target available\n");
            if (exitCode) {
                *exitCode = 1;
            }
            return WINUAE_QT_LAUNCHER_ERROR;
        }
        return WINUAE_QT_LAUNCHER_START;
    }
    if (result.status == WinUaeQtLauncherStatus::Error) {
        QByteArray error = result.error.toLocal8Bit();
        fprintf(stderr, "Unix Qt UI failed: %s\n", error.constData());
        if (exitCode) {
            *exitCode = result.exitCode ? result.exitCode : 1;
        }
        return WINUAE_QT_LAUNCHER_ERROR;
    }
    if (exitCode) {
        *exitCode = 0;
    }
    return WINUAE_QT_LAUNCHER_EXIT;
}

int runWinUaeQtRuntimeFileDialog(int argc, char **argv, int shortcut, const char *initialPath, char *selectedPath, size_t selectedPathLen, int *exitCode)
{
    if (!selectedPath || selectedPathLen == 0) {
        if (exitCode) {
            *exitCode = 1;
        }
        return WINUAE_QT_LAUNCHER_ERROR;
    }
    selectedPath[0] = 0;

    const QString initial = initialPath && initialPath[0]
        ? QString::fromLocal8Bit(initialPath)
        : QString();
    const WinUaeQtRuntimeFileDialogResult result = runWinUaeQtRuntimeFileDialog(argc, argv, shortcut, initial);
    if (!result.accepted) {
        if (exitCode) {
            *exitCode = 0;
        }
        return WINUAE_QT_LAUNCHER_EXIT;
    }

    const QByteArray path = result.path.toLocal8Bit();
    if (size_t(path.size()) >= selectedPathLen) {
        fprintf(stderr, "Unix Qt UI failed: selected path is too long\n");
        if (exitCode) {
            *exitCode = 1;
        }
        return WINUAE_QT_LAUNCHER_ERROR;
    }
    memcpy(selectedPath, path.constData(), size_t(path.size()) + 1);
    if (exitCode) {
        *exitCode = 0;
    }
    return WINUAE_QT_LAUNCHER_START;
}
