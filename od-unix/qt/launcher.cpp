#include <QtWidgets>

#include "config.h"
#include "launcher.h"
#include "launcher_backend.h"
#include "mount_config.h"

#ifndef WINUAE_UNIX_SOURCE_DIR
#define WINUAE_UNIX_SOURCE_DIR "."
#endif

#ifndef WINUAE_UNIX_BUILD_DIR
#define WINUAE_UNIX_BUILD_DIR "."
#endif

#ifndef WINUAE_UNIX_VERSION_MAJOR
#define WINUAE_UNIX_VERSION_MAJOR 0
#endif

#ifndef WINUAE_UNIX_VERSION_MINOR
#define WINUAE_UNIX_VERSION_MINOR 0
#endif

#ifndef WINUAE_UNIX_VERSION_REVISION
#define WINUAE_UNIX_VERSION_REVISION 0
#endif

static constexpr int FpuInternal = -1;
static constexpr int MaxMountEntries = 8;
static constexpr int MaxControllerUnits = 8;
static constexpr int MaxCdSlots = 8;
static constexpr int MaxRomBoards = 4;

struct WinUaeQtCdSlot {
    QString path;
    QString type;
    bool inUse = false;
};

struct WinUaeQtRomBoard {
    QString start;
    QString end;
    QString path;
};

enum MountDataRole {
    MountKindRole = Qt::UserRole,
    MountDeviceRole,
    MountVolumeRole,
    MountPathRole,
    MountReadOnlyRole,
    MountBootPriRole,
    MountEmuUnitRole,
    MountRawConfigRole,
    MountHardfileGeometryRole,
    MountHardfileTailRole
};

static QStringList mountControllerParts(QString tail)
{
    if (!tail.startsWith(QLatin1Char(','))) {
        tail.prepend(QLatin1Char(','));
    }
    QStringList parts = winUaeQtConfigFieldList(tail);
    while (parts.size() < 2) {
        parts.append(QString());
    }
    return parts;
}

static QString mountControllerValue(const WinUaeQtMountEntry &entry, const QString &fallback)
{
    const QStringList parts = mountControllerParts(entry.hardfileTail);
    return parts.value(1).isEmpty() ? fallback : parts.value(1);
}

static QString mountTailWithController(const WinUaeQtMountEntry &entry, const QString &controller)
{
    QStringList parts = mountControllerParts(entry.hardfileTail);
    parts[1] = controller;
    return winUaeQtConfigJoinFields(parts);
}

static QString mountControllerFamily(const WinUaeQtMountEntry &entry, const QString &fallback)
{
    const QString value = mountControllerValue(entry, fallback).toLower();
    if (value.startsWith(QStringLiteral("scsi"))) {
        return QStringLiteral("SCSI (Auto)");
    }
    if (value.startsWith(QStringLiteral("ide"))) {
        return QStringLiteral("IDE (Auto)");
    }
    return QStringLiteral("UAE (uaehf.device)");
}

static int mountControllerUnit(const WinUaeQtMountEntry &entry, const QString &fallback)
{
    const QString value = mountControllerValue(entry, fallback).toLower();
    int index = 0;
    while (index < value.size() && !value.at(index).isDigit()) {
        index++;
    }
    if (index >= value.size()) {
        return 0;
    }
    return qBound(0, value.mid(index).toInt(), MaxControllerUnits - 1);
}

static QString mountControllerConfigValue(const QString &family, int unit)
{
    const int clampedUnit = qBound(0, unit, MaxControllerUnits - 1);
    if (family == QStringLiteral("SCSI (Auto)")) {
        return QStringLiteral("scsi%1").arg(clampedUnit);
    }
    if (family == QStringLiteral("IDE (Auto)")) {
        return QStringLiteral("ide%1").arg(qBound(0, clampedUnit, 3));
    }
    return QStringLiteral("uae%1").arg(clampedUnit);
}

static QString mountControllerDisplay(const WinUaeQtMountEntry &entry)
{
    const QString fallback = entry.kind == QStringLiteral("cd") ? QStringLiteral("ide0") : QStringLiteral("uae0");
    const QString value = mountControllerValue(entry, fallback).toUpper();
    if (value.startsWith(QStringLiteral("IDE"))) {
        return QStringLiteral("IDE:%1").arg(mountControllerUnit(entry, fallback));
    }
    if (value.startsWith(QStringLiteral("SCSI"))) {
        return QStringLiteral("SCSI:%1").arg(mountControllerUnit(entry, fallback));
    }
    if (value.startsWith(QStringLiteral("UAE"))) {
        return QStringLiteral("UAE:%1").arg(mountControllerUnit(entry, fallback));
    }
    return value;
}

static bool isIntegerText(const QString &value)
{
    bool ok = false;
    value.toInt(&ok);
    return ok;
}

static QStringList hardfileGeometryParts(const WinUaeQtMountEntry &entry)
{
    QStringList parts = entry.hardfileGeometry.split(QLatin1Char(','));
    while (parts.size() < 4) {
        parts.append(QStringLiteral("0"));
    }
    return parts;
}

static bool hardfileIsRdb(const WinUaeQtMountEntry &entry)
{
    const QStringList geometry = hardfileGeometryParts(entry);
    return geometry.value(0).toInt() == 0
        && geometry.value(1).toInt() == 0
        && geometry.value(2).toInt() == 0;
}

static bool hardfileHasPhysicalGeometry(const WinUaeQtMountEntry &entry)
{
    const QStringList parts = mountControllerParts(entry.hardfileTail);
    return parts.size() > 3
        && isIntegerText(parts.value(2))
        && parts.value(3).contains(QLatin1Char('/'));
}

static QStringList hardfilePhysicalGeometryParts(const WinUaeQtMountEntry &entry)
{
    QStringList geometry;
    if (hardfileHasPhysicalGeometry(entry)) {
        geometry = mountControllerParts(entry.hardfileTail).value(3).split(QLatin1Char('/'));
    }
    while (geometry.size() < 3) {
        geometry.append(QStringLiteral("0"));
    }
    return geometry;
}

static QString hardfileTailGeometryFile(const WinUaeQtMountEntry &entry)
{
    const QStringList parts = mountControllerParts(entry.hardfileTail);
    return hardfileHasPhysicalGeometry(entry) ? parts.value(4) : QString();
}

static int hardfileTailExtraStart(const WinUaeQtMountEntry &entry)
{
    return hardfileHasPhysicalGeometry(entry) ? 5 : 2;
}

static bool isManagedHardfileTailToken(const QString &token)
{
    static const QStringList managed = {
        QStringLiteral("HD"),
        QStringLiteral("CF"),
        QStringLiteral("SCSI1"),
        QStringLiteral("SCSI2"),
        QStringLiteral("SASIE"),
        QStringLiteral("SASI"),
        QStringLiteral("SASI_CHS"),
        QStringLiteral("ATA1"),
        QStringLiteral("ATA2+"),
        QStringLiteral("ATA2+S")
    };
    for (const QString &item : managed) {
        if (token.compare(item, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

static QStringList hardfilePreservedTailExtras(const WinUaeQtMountEntry &entry)
{
    const QStringList parts = mountControllerParts(entry.hardfileTail);
    QStringList extras;
    for (int i = hardfileTailExtraStart(entry); i < parts.size(); i++) {
        if (!parts.value(i).isEmpty() && !isManagedHardfileTailToken(parts.value(i))) {
            extras.append(parts.value(i));
        }
    }
    return extras;
}

static bool hardfileTailHasToken(const WinUaeQtMountEntry &entry, const QString &token)
{
    const QStringList parts = mountControllerParts(entry.hardfileTail);
    for (int i = hardfileTailExtraStart(entry); i < parts.size(); i++) {
        if (parts.value(i).compare(token, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

static QString hardfileFeatureText(const WinUaeQtMountEntry &entry, const QString &controllerFamily)
{
    if (controllerFamily == QStringLiteral("IDE (Auto)")) {
        if (hardfileTailHasToken(entry, QStringLiteral("ATA2+S"))) {
            return QStringLiteral("ATA-2+ Strict");
        }
        if (hardfileTailHasToken(entry, QStringLiteral("ATA2+"))) {
            return QStringLiteral("ATA-2+");
        }
        if (hardfileTailHasToken(entry, QStringLiteral("ATA1"))) {
            return QStringLiteral("ATA-1");
        }
    } else if (controllerFamily == QStringLiteral("SCSI (Auto)")) {
        if (hardfileTailHasToken(entry, QStringLiteral("SASI_CHS"))) {
            return QStringLiteral("SASI CHS");
        }
        if (hardfileTailHasToken(entry, QStringLiteral("SASI"))) {
            return QStringLiteral("SASI");
        }
        if (hardfileTailHasToken(entry, QStringLiteral("SCSI1"))) {
            return QStringLiteral("SCSI-1");
        }
        if (hardfileTailHasToken(entry, QStringLiteral("SCSI2"))) {
            return QStringLiteral("SCSI-2");
        }
    }
    return QStringLiteral("Default");
}

static QString hardfileFeatureToken(const QString &featureText)
{
    if (featureText == QStringLiteral("ATA-1")) {
        return QStringLiteral("ATA1");
    }
    if (featureText == QStringLiteral("ATA-2+ Strict")) {
        return QStringLiteral("ATA2+S");
    }
    if (featureText == QStringLiteral("SCSI-1")) {
        return QStringLiteral("SCSI1");
    }
    if (featureText == QStringLiteral("SASI")) {
        return QStringLiteral("SASI");
    }
    if (featureText == QStringLiteral("SASI CHS")) {
        return QStringLiteral("SASI_CHS");
    }
    return QString();
}

static WinUaeQtCdSlot cdSlotFromConfigValue(const QString &value)
{
    WinUaeQtCdSlot slot;
    if (value.compare(QStringLiteral("autodetect"), Qt::CaseInsensitive) == 0) {
        slot.type = QStringLiteral("Autodetect");
        slot.inUse = true;
        return slot;
    }
    if (value.isEmpty()
        || value.compare(QStringLiteral("empty"), Qt::CaseInsensitive) == 0
        || value == QStringLiteral(".")) {
        slot.type = QStringLiteral("Image file");
        return slot;
    }

    const QStringList fields = winUaeQtConfigFieldList(value);
    slot.path = fields.value(0);
    slot.type = fields.value(1).compare(QStringLiteral("image"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("Image file")
        : QStringLiteral("Image file");
    slot.inUse = !slot.path.isEmpty();
    return slot;
}

static QString cdSlotConfigValue(const WinUaeQtCdSlot &slot)
{
    if (!slot.inUse && slot.path.isEmpty()) {
        return QString();
    }
    if (slot.type == QStringLiteral("Autodetect") && slot.path.isEmpty()) {
        return QStringLiteral("autodetect");
    }
    if (slot.path.isEmpty()) {
        return QString();
    }
    return winUaeQtConfigEscapeMin(slot.path);
}

static QString normalizedRomAddress(QString text, bool endAddress)
{
    text = text.trimmed();
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        text = text.mid(2);
    }
    bool ok = false;
    quint32 value = text.toUInt(&ok, 16);
    if (!ok) {
        return QString();
    }
    if (endAddress && value == 0) {
        return QString();
    }
    if (endAddress) {
        value = ((value - 1) & ~quint32(0xffff)) | quint32(0xffff);
    } else {
        value &= ~quint32(0xffff);
    }
    return QStringLiteral("%1").arg(value, 8, 16, QLatin1Char('0'));
}

static WinUaeQtRomBoard romBoardFromConfigValue(const QString &value)
{
    WinUaeQtRomBoard board;
    for (const QString &field : winUaeQtConfigFieldList(value)) {
        const int equals = field.indexOf(QLatin1Char('='));
        if (equals <= 0) {
            continue;
        }
        const QString key = field.left(equals).trimmed().toLower();
        const QString fieldValue = field.mid(equals + 1).trimmed();
        if (key == QStringLiteral("start")) {
            board.start = normalizedRomAddress(fieldValue, false);
        } else if (key == QStringLiteral("end")) {
            board.end = normalizedRomAddress(fieldValue, true);
        } else if (key == QStringLiteral("file")) {
            board.path = fieldValue;
        }
    }
    return board;
}

static QString romBoardConfigValue(const WinUaeQtRomBoard &board)
{
    const QString start = normalizedRomAddress(board.start, false);
    const QString end = normalizedRomAddress(board.end, true);
    if (start.isEmpty() || end.isEmpty()) {
        return QString();
    }
    QString value = QStringLiteral("start=%1,end=%2").arg(start, end);
    if (!board.path.trimmed().isEmpty()) {
        value += QStringLiteral(",file=%1").arg(winUaeQtConfigEscapeMin(board.path.trimmed()));
    }
    return value;
}

static int romBoardIndexFromKey(const QString &key)
{
    if (key == QStringLiteral("romboard_options")) {
        return 0;
    }
    if (!key.startsWith(QStringLiteral("romboard")) || !key.endsWith(QStringLiteral("_options"))) {
        return -1;
    }
    bool ok = false;
    const int board = key.mid(8, key.size() - 16).toInt(&ok);
    if (!ok) {
        return -1;
    }
    return board - 1;
}

static QString romBoardKey(int index)
{
    return index == 0
        ? QStringLiteral("romboard_options")
        : QStringLiteral("romboard%1_options").arg(index + 1);
}

static int floppyKeyDrive(const QString &key, const QString &suffix = QString())
{
    if (!key.startsWith(QStringLiteral("floppy"))) {
        return -1;
    }
    if (key.size() != 7 + suffix.size() || !key.endsWith(suffix)) {
        return -1;
    }
    const int drive = key.at(6).digitValue();
    return drive >= 0 && drive < 4 ? drive : -1;
}

static QString uaeBoardConfigValue(const QString &text)
{
    if (text == QStringLiteral("New UAE (64k + F0 ROM)")) {
        return QStringLiteral("min");
    }
    if (text == QStringLiteral("New UAE (128k, ROM, Direct)")) {
        return QStringLiteral("full");
    }
    if (text == QStringLiteral("New UAE (128k, ROM, Indirect)")) {
        return QStringLiteral("full+indirect");
    }
    return QStringLiteral("disabled");
}

static QString uaeBoardText(const QString &value)
{
    const QString lower = value.toLower();
    if (lower == QStringLiteral("min") || lower == QStringLiteral("min_off")) {
        return QStringLiteral("New UAE (64k + F0 ROM)");
    }
    if (lower == QStringLiteral("full") || lower == QStringLiteral("full_off")) {
        return QStringLiteral("New UAE (128k, ROM, Direct)");
    }
    if (lower == QStringLiteral("full+indirect") || lower == QStringLiteral("full+indirect_off")) {
        return QStringLiteral("New UAE (128k, ROM, Indirect)");
    }
    return QStringLiteral("Original UAE (FS + F0 ROM)");
}

static QString fullscreenModeConfigValue(const QString &text)
{
    if (text == QStringLiteral("Fullscreen")) {
        return QStringLiteral("true");
    }
    if (text == QStringLiteral("Full-window")) {
        return QStringLiteral("fullwindow");
    }
    return QStringLiteral("false");
}

static QString fullscreenModeText(const QString &value)
{
    if (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Fullscreen");
    }
    if (value.compare(QStringLiteral("fullwindow"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Full-window");
    }
    return QStringLiteral("Windowed");
}

static QString lineModeConfigValue(int id)
{
    switch (id) {
    case 0:
        return QStringLiteral("none");
    case 2:
        return QStringLiteral("scanlines");
    case 5:
        return QStringLiteral("double2");
    case 1:
    default:
        return QStringLiteral("double");
    }
}

static int lineModeId(const QString &value)
{
    if (value.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0) {
        return 0;
    }
    if (value.compare(QStringLiteral("scanlines"), Qt::CaseInsensitive) == 0) {
        return 2;
    }
    if (value.compare(QStringLiteral("double2"), Qt::CaseInsensitive) == 0) {
        return 5;
    }
    return 1;
}

static QStringList primaryPortDeviceItems()
{
    return {
        QStringLiteral("Mouse"),
        QStringLiteral("Keyboard Layout A"),
        QStringLiteral("Keyboard Layout B"),
        QStringLiteral("Keyboard Layout C"),
        QStringLiteral("Joystick 1"),
        QStringLiteral("Joystick 2"),
        QStringLiteral("Joystick 3"),
        QStringLiteral("Joystick 4"),
        QStringLiteral("<None>")
    };
}

static QStringList parallelPortDeviceItems()
{
    return {
        QStringLiteral("<None>"),
        QStringLiteral("Keyboard Layout A"),
        QStringLiteral("Keyboard Layout B"),
        QStringLiteral("Keyboard Layout C"),
        QStringLiteral("Joystick 1"),
        QStringLiteral("Joystick 2"),
        QStringLiteral("Joystick 3"),
        QStringLiteral("Joystick 4")
    };
}

static QString joyportDeviceConfigValue(const QString &text)
{
    if (text == QStringLiteral("Mouse")) {
        return QStringLiteral("mouse");
    }
    if (text == QStringLiteral("Keyboard Layout A")) {
        return QStringLiteral("kbd1");
    }
    if (text == QStringLiteral("Keyboard Layout B")) {
        return QStringLiteral("kbd2");
    }
    if (text == QStringLiteral("Keyboard Layout C")) {
        return QStringLiteral("kbd3");
    }
    if (text.startsWith(QStringLiteral("Joystick "))) {
        bool ok = false;
        const int index = text.mid(9).toInt(&ok);
        if (ok && index > 0) {
            return QStringLiteral("joy%1").arg(index - 1);
        }
    }
    return QStringLiteral("none");
}

static QString joyportDeviceText(const QString &value, bool allowMouse)
{
    const QString lower = value.toLower();
    if (allowMouse && (lower == QStringLiteral("mouse") || lower == QStringLiteral("mouse0"))) {
        return QStringLiteral("Mouse");
    }
    if (lower == QStringLiteral("kbd1")) {
        return QStringLiteral("Keyboard Layout A");
    }
    if (lower == QStringLiteral("kbd2")) {
        return QStringLiteral("Keyboard Layout B");
    }
    if (lower == QStringLiteral("kbd3")) {
        return QStringLiteral("Keyboard Layout C");
    }
    if (lower.startsWith(QStringLiteral("joy"))) {
        bool ok = false;
        const int index = lower.mid(3).toInt(&ok);
        if (ok && index >= 0 && index < 4) {
            return QStringLiteral("Joystick %1").arg(index + 1);
        }
    }
    return QStringLiteral("<None>");
}

static QString autofireConfigValue(const QString &text)
{
    if (text == QStringLiteral("Autofire")) {
        return QStringLiteral("normal");
    }
    if (text == QStringLiteral("Autofire (toggle)")) {
        return QStringLiteral("toggle");
    }
    if (text == QStringLiteral("Autofire (always)")) {
        return QStringLiteral("always");
    }
    if (text == QStringLiteral("No autofire (toggle)")) {
        return QStringLiteral("togglebutton");
    }
    return QStringLiteral("none");
}

static QString autofireText(const QString &value)
{
    if (value.compare(QStringLiteral("normal"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Autofire");
    }
    if (value.compare(QStringLiteral("toggle"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Autofire (toggle)");
    }
    if (value.compare(QStringLiteral("always"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Autofire (always)");
    }
    if (value.compare(QStringLiteral("togglebutton"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("No autofire (toggle)");
    }
    return QStringLiteral("No autofire (normal)");
}

static QString joyportModeConfigValue(const QString &text)
{
    if (text == QStringLiteral("Wheel Mouse")) {
        return QStringLiteral("mouse");
    }
    if (text == QStringLiteral("Mouse")) {
        return QStringLiteral("mousenowheel");
    }
    if (text == QStringLiteral("Joystick")) {
        return QStringLiteral("djoy");
    }
    if (text == QStringLiteral("Gamepad")) {
        return QStringLiteral("gamepad");
    }
    if (text == QStringLiteral("Analog joystick")) {
        return QStringLiteral("ajoy");
    }
    if (text == QStringLiteral("CDTV remote mouse")) {
        return QStringLiteral("cdtvjoy");
    }
    if (text == QStringLiteral("CD32 pad")) {
        return QStringLiteral("cd32joy");
    }
    if (text == QStringLiteral("Generic light pen/gun")) {
        return QStringLiteral("lightpen");
    }
    return QString();
}

static QString joyportModeText(const QString &value)
{
    if (value.compare(QStringLiteral("mouse"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Wheel Mouse");
    }
    if (value.compare(QStringLiteral("mousenowheel"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Mouse");
    }
    if (value.compare(QStringLiteral("djoy"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Joystick");
    }
    if (value.compare(QStringLiteral("gamepad"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Gamepad");
    }
    if (value.compare(QStringLiteral("ajoy"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Analog joystick");
    }
    if (value.compare(QStringLiteral("cdtvjoy"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("CDTV remote mouse");
    }
    if (value.compare(QStringLiteral("cd32joy"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("CD32 pad");
    }
    if (value.compare(QStringLiteral("lightpen"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Generic light pen/gun");
    }
    return QStringLiteral("Default");
}

static QString magicMouseCursorConfigValue(const QString &text)
{
    if (text == QStringLiteral("Show native cursor only")) {
        return QStringLiteral("native");
    }
    if (text == QStringLiteral("Show host cursor only")) {
        return QStringLiteral("host");
    }
    return QStringLiteral("both");
}

static QString magicMouseCursorText(const QString &value)
{
    if (value.compare(QStringLiteral("native"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Show native cursor only");
    }
    if (value.compare(QStringLiteral("host"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Show host cursor only");
    }
    return QStringLiteral("Show both cursors");
}

static constexpr int SoundVolumeCount = 5;
static constexpr int FloppySoundDriveCount = 4;

static QString soundOutputConfigValue(int id)
{
    switch (id) {
    case 0:
        return QStringLiteral("none");
    case 1:
        return QStringLiteral("interrupts");
    case 2:
    default:
        return QStringLiteral("exact");
    }
}

static int soundOutputId(const QString &value)
{
    if (value.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0) {
        return 0;
    }
    if (value.compare(QStringLiteral("interrupts"), Qt::CaseInsensitive) == 0) {
        return 1;
    }
    return 2;
}

static QStringList soundChannelItems()
{
    return {
        QStringLiteral("Mono"),
        QStringLiteral("Stereo"),
        QStringLiteral("Cloned Stereo (4 Channels)"),
        QStringLiteral("4 Channels"),
        QStringLiteral("Cloned Stereo (5.1)"),
        QStringLiteral("5.1"),
        QStringLiteral("Cloned stereo (7.1)"),
        QStringLiteral("7.1")
    };
}

static QString soundChannelConfigValue(const QString &text)
{
    static const QStringList values = {
        QStringLiteral("mono"),
        QStringLiteral("stereo"),
        QStringLiteral("clonedstereo"),
        QStringLiteral("4ch"),
        QStringLiteral("clonedstereo6ch"),
        QStringLiteral("6ch"),
        QStringLiteral("clonedstereo8ch"),
        QStringLiteral("8ch")
    };
    const int index = soundChannelItems().indexOf(text);
    return values.value(index, QStringLiteral("stereo"));
}

static QString soundChannelText(const QString &value)
{
    static const QStringList values = {
        QStringLiteral("mono"),
        QStringLiteral("stereo"),
        QStringLiteral("clonedstereo"),
        QStringLiteral("4ch"),
        QStringLiteral("clonedstereo6ch"),
        QStringLiteral("6ch"),
        QStringLiteral("clonedstereo8ch"),
        QStringLiteral("8ch")
    };
    const int index = values.indexOf(value.toLower());
    return soundChannelItems().value(index, QStringLiteral("Stereo"));
}

static QString soundInterpolationConfigValue(const QString &text)
{
    if (text == QStringLiteral("Anti")) {
        return QStringLiteral("anti");
    }
    if (text == QStringLiteral("Sinc")) {
        return QStringLiteral("sinc");
    }
    if (text == QStringLiteral("RH")) {
        return QStringLiteral("rh");
    }
    if (text == QStringLiteral("Crux")) {
        return QStringLiteral("crux");
    }
    return QStringLiteral("none");
}

static QString soundInterpolationText(const QString &value)
{
    if (value.compare(QStringLiteral("anti"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Anti");
    }
    if (value.compare(QStringLiteral("sinc"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Sinc");
    }
    if (value.compare(QStringLiteral("rh"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("RH");
    }
    if (value.compare(QStringLiteral("crux"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Crux");
    }
    return QStringLiteral("Disabled");
}

static QString soundFilterConfigValue(const QString &text)
{
    if (text == QStringLiteral("Always off")) {
        return QStringLiteral("off");
    }
    return text.startsWith(QStringLiteral("Emulated")) ? QStringLiteral("emulated") : QStringLiteral("on");
}

static QString soundFilterTypeConfigValue(const QString &text)
{
    return text.contains(QStringLiteral("A1200")) ? QStringLiteral("enhanced") : QStringLiteral("standard");
}

static QString soundFilterText(const QString &filter, const QString &type)
{
    const bool enhanced = type.compare(QStringLiteral("enhanced"), Qt::CaseInsensitive) == 0;
    if (filter.compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Always off");
    }
    if (filter.compare(QStringLiteral("emulated"), Qt::CaseInsensitive) == 0) {
        return enhanced ? QStringLiteral("Emulated (A1200)") : QStringLiteral("Emulated (A500)");
    }
    return enhanced ? QStringLiteral("Always on (A1200)") : QStringLiteral("Always on (A500)");
}

static QString soundSwapText(bool paula, bool ahi)
{
    const int index = (paula ? 1 : 0) + (ahi ? 2 : 0);
    return QStringList({ QStringLiteral("-"), QStringLiteral("Paula only"), QStringLiteral("AHI only"), QStringLiteral("Both") }).value(index);
}

static int soundBufferSizeFromIndex(int index)
{
    static const int sizes[] = { 1024, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 32768, 65536 };
    if (index <= 0) {
        return 0;
    }
    return sizes[qBound(1, index, 10) - 1];
}

static int soundBufferIndexFromSize(int size)
{
    static const int sizes[] = { 1024, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 32768, 65536 };
    if (size < sizes[0]) {
        return 0;
    }
    int index = 0;
    while (index < 9 && sizes[index] < size) {
        index++;
    }
    return index + 1;
}

static constexpr int RtgRgbClut = 1 << 1;
static constexpr int RtgRgbR8G8B8 = 1 << 2;
static constexpr int RtgRgbB8G8R8 = 1 << 3;
static constexpr int RtgRgbR5G6B5Pc = 1 << 4;
static constexpr int RtgRgbR5G5B5Pc = 1 << 5;
static constexpr int RtgRgbA8R8G8B8 = 1 << 6;
static constexpr int RtgRgbA8B8G8R8 = 1 << 7;
static constexpr int RtgRgbR8G8B8A8 = 1 << 8;
static constexpr int RtgRgbB8G8R8A8 = 1 << 9;
static constexpr int RtgRgbR5G6B5 = 1 << 10;
static constexpr int RtgRgbR5G5B5 = 1 << 11;
static constexpr int RtgRgbB5G6R5Pc = 1 << 12;
static constexpr int RtgRgbB5G5R5Pc = 1 << 13;
static constexpr int RtgDefaultModeMask = RtgRgbClut | RtgRgbR5G6B5Pc | RtgRgbB8G8R8A8;

static QString rtgScaleConfigValue(bool scale, bool center, bool integer)
{
    if (integer) {
        return QStringLiteral("integer");
    }
    if (center) {
        return QStringLiteral("center");
    }
    if (scale) {
        return QStringLiteral("scale");
    }
    return QStringLiteral("resize");
}

static QString rtgBufferConfigValue(const QString &text)
{
    return text == QStringLiteral("Triple") ? QStringLiteral("2") : QStringLiteral("1");
}

static QString rtgBufferText(const QString &value)
{
    return value == QStringLiteral("2") ? QStringLiteral("Triple") : QStringLiteral("Double");
}

static int rtgColorDepthMask(const QString &text)
{
    if (text == QStringLiteral("8-bit (*)")) {
        return RtgRgbClut;
    }
    if (text == QStringLiteral("All 15/16-bit")) {
        return RtgRgbR5G6B5Pc | RtgRgbR5G5B5Pc | RtgRgbR5G6B5 | RtgRgbR5G5B5 | RtgRgbB5G6R5Pc | RtgRgbB5G5R5Pc;
    }
    if (text == QStringLiteral("R5G6B5PC (*)")) {
        return RtgRgbR5G6B5Pc;
    }
    if (text == QStringLiteral("R5G5B5PC")) {
        return RtgRgbR5G5B5Pc;
    }
    if (text == QStringLiteral("R5G6B5")) {
        return RtgRgbR5G6B5;
    }
    if (text == QStringLiteral("R5G5B5")) {
        return RtgRgbR5G5B5;
    }
    if (text == QStringLiteral("B5G6R5PC")) {
        return RtgRgbB5G6R5Pc;
    }
    if (text == QStringLiteral("B5G5R5PC")) {
        return RtgRgbB5G5R5Pc;
    }
    if (text == QStringLiteral("All 24-bit")) {
        return RtgRgbR8G8B8 | RtgRgbB8G8R8;
    }
    if (text == QStringLiteral("R8G8B8")) {
        return RtgRgbR8G8B8;
    }
    if (text == QStringLiteral("B8G8R8")) {
        return RtgRgbB8G8R8;
    }
    if (text == QStringLiteral("All 32-bit")) {
        return RtgRgbA8R8G8B8 | RtgRgbA8B8G8R8 | RtgRgbR8G8B8A8 | RtgRgbB8G8R8A8;
    }
    if (text == QStringLiteral("A8R8G8B8")) {
        return RtgRgbA8R8G8B8;
    }
    if (text == QStringLiteral("A8B8G8R8")) {
        return RtgRgbA8B8G8R8;
    }
    if (text == QStringLiteral("R8G8B8A8")) {
        return RtgRgbR8G8B8A8;
    }
    if (text == QStringLiteral("B8G8R8A8 (*)")) {
        return RtgRgbB8G8R8A8;
    }
    return 0;
}

static QString rtg8BitText(int mask)
{
    return (mask & RtgRgbClut) ? QStringLiteral("8-bit (*)") : QStringLiteral("(8bit)");
}

static QString rtg16BitText(int mask)
{
    const int all = RtgRgbR5G6B5Pc | RtgRgbR5G5B5Pc | RtgRgbR5G6B5 | RtgRgbR5G5B5 | RtgRgbB5G6R5Pc | RtgRgbB5G5R5Pc;
    if ((mask & all) == all) {
        return QStringLiteral("All 15/16-bit");
    }
    if (mask & RtgRgbR5G6B5Pc) {
        return QStringLiteral("R5G6B5PC (*)");
    }
    if (mask & RtgRgbR5G5B5Pc) {
        return QStringLiteral("R5G5B5PC");
    }
    if (mask & RtgRgbR5G6B5) {
        return QStringLiteral("R5G6B5");
    }
    if (mask & RtgRgbR5G5B5) {
        return QStringLiteral("R5G5B5");
    }
    if (mask & RtgRgbB5G6R5Pc) {
        return QStringLiteral("B5G6R5PC");
    }
    if (mask & RtgRgbB5G5R5Pc) {
        return QStringLiteral("B5G5R5PC");
    }
    return QStringLiteral("(15/16bit)");
}

static QString rtg24BitText(int mask)
{
    const int all = RtgRgbR8G8B8 | RtgRgbB8G8R8;
    if ((mask & all) == all) {
        return QStringLiteral("All 24-bit");
    }
    if (mask & RtgRgbR8G8B8) {
        return QStringLiteral("R8G8B8");
    }
    if (mask & RtgRgbB8G8R8) {
        return QStringLiteral("B8G8R8");
    }
    return QStringLiteral("(24bit)");
}

static QString rtg32BitText(int mask)
{
    const int all = RtgRgbA8R8G8B8 | RtgRgbA8B8G8R8 | RtgRgbR8G8B8A8 | RtgRgbB8G8R8A8;
    if ((mask & all) == all) {
        return QStringLiteral("All 32-bit");
    }
    if (mask & RtgRgbA8R8G8B8) {
        return QStringLiteral("A8R8G8B8");
    }
    if (mask & RtgRgbA8B8G8R8) {
        return QStringLiteral("A8B8G8R8");
    }
    if (mask & RtgRgbR8G8B8A8) {
        return QStringLiteral("R8G8B8A8");
    }
    if (mask & RtgRgbB8G8R8A8) {
        return QStringLiteral("B8G8R8A8 (*)");
    }
    return QStringLiteral("(32bit)");
}

static QString sourceFile(const QString &relative)
{
    return QDir(QString::fromUtf8(WINUAE_UNIX_SOURCE_DIR)).filePath(relative);
}

static QString versionString()
{
    return QStringLiteral("WinUAE %1.%2.%3")
        .arg(WINUAE_UNIX_VERSION_MAJOR)
        .arg(WINUAE_UNIX_VERSION_MINOR)
        .arg(WINUAE_UNIX_VERSION_REVISION);
}

static QStringList contributorLines()
{
    return {
        QStringLiteral("Bernd Schmidt - The Grand-Master"),
        QStringLiteral("Sam Jordan - Custom-chip, floppy-DMA, etc."),
        QStringLiteral("Mathias Ortmann - Original WinUAE Main Guy, BSD Socket support"),
        QStringLiteral("Brian King - Picasso96 Support, Integrated GUI for WinUAE, previous WinUAE Main Guy"),
        QStringLiteral("Toni Wilen - Core updates, WinUAE Main Guy"),
        QStringLiteral("Gustavo Goedert / Peter Remmers / Michael Sontheimer / Tomi Hakala / Tim Gunn / Nemo Pohle - DOS Port Stuff"),
        QStringLiteral("Samuel Devulder / Olaf Barthel / Sam Jordan - Amiga Ports"),
        QStringLiteral("Krister Bergman - XFree86 and OS/2 Port"),
        QStringLiteral("A. Blanchard / Ernesto Corvi - MacOS Port"),
        QStringLiteral("Christian Bauer - BeOS Port"),
        QStringLiteral("Ian Stephenson - NextStep Port"),
        QStringLiteral("Peter Teichmann - Acorn/RiscOS Port"),
        QStringLiteral("Stefan Reinauer - ZorroII/III AutoConfig, Serial Support"),
        QStringLiteral("Christian Schmitt / Chris Hames - Serial Support"),
        QStringLiteral("Herman ten Brugge - 68020/68881 Emulation Code"),
        QStringLiteral("Tauno Taipaleenmaki - Various UAE-Control/UAE-Library Support"),
        QStringLiteral("Brett Eden / Tim Gunn / Paolo Besser / Nemo Pohle - Various Docs and Web-Sites"),
        QStringLiteral("Georg Veichtlbauer - Help File coordinator, German GUI"),
        QStringLiteral("Fulvio Leonardi - Italian translator for WinUAE"),
        QStringLiteral("Arnljot Arntsen, Bill Panagouleas, Cloanto, Zak Jennings - Hardware support"),
        QStringLiteral("Special thanks to Alexander Kneer and Tobias Abt (The Picasso96 Team)"),
        QStringLiteral("Steven Weiser - Postscript printing emulation idea and testing"),
        QStringLiteral("Peter Toth / Balazs Ratkai / Ivan Herczeg / Andras Arato - Hungarian translation"),
        QStringLiteral("Karsten Bock, Gavin Fance, Dirk Trowe and Christian Schindler - Freezer cartridge hardware support"),
        QStringLiteral("Mikko Nieminen - Demo compatibility testing"),
        QStringLiteral("Arabuusimiehet - [This information is on a need-to-know basis]"),
        QStringLiteral("Ross - Chipset torture test programs")
    };
}

static QIcon resourceIcon(const QString &name)
{
    const QString path = sourceFile(QStringLiteral("od-win32/resources/") + name);
    return QFileInfo::exists(path) ? QIcon(path) : QIcon();
}

static QLabel *label(const QString &text)
{
    QLabel *w = new QLabel(text);
    w->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return w;
}

static QComboBox *combo(const QStringList &items, const QString &current = QString())
{
    QComboBox *w = new QComboBox;
    w->setEditable(false);
    w->addItems(items);
    if (!current.isEmpty()) {
        const int index = w->findText(current);
        if (index >= 0) {
            w->setCurrentIndex(index);
        }
    }
    return w;
}

static QComboBox *pathCombo()
{
    QComboBox *w = new QComboBox;
    w->setEditable(true);
    w->setInsertPolicy(QComboBox::NoInsert);
    w->lineEdit()->setClearButtonEnabled(true);
    return w;
}

static QStringList floppyTypeItems(int drive, bool quickstart)
{
    QStringList items = {
        QStringLiteral("3.5 DD"),
        QStringLiteral("3.5 HD"),
        QStringLiteral("Disabled")
    };
    if (quickstart) {
        return items;
    }
    items.insert(2, QStringLiteral("5.25 SD"));
    items.insert(3, QStringLiteral("5.25 (80)"));
    items.insert(4, QStringLiteral("3.5 DD (Escom)"));
    if (drive >= 2) {
        items.insert(5, QStringLiteral("Bridgeboard 5.25 40"));
        items.insert(6, QStringLiteral("Bridgeboard 5.25 80"));
        items.insert(7, QStringLiteral("Bridgeboard 3.5 80"));
    }
    return items;
}

static int floppyTypeConfigValue(const QString &text)
{
    if (text == QStringLiteral("Disabled")) {
        return -1;
    }
    if (text == QStringLiteral("3.5 HD")) {
        return 1;
    }
    if (text == QStringLiteral("5.25 SD")) {
        return 2;
    }
    if (text == QStringLiteral("3.5 DD (Escom)")) {
        return 3;
    }
    if (text == QStringLiteral("Bridgeboard 5.25 40")) {
        return 4;
    }
    if (text == QStringLiteral("Bridgeboard 3.5 80")) {
        return 5;
    }
    if (text == QStringLiteral("Bridgeboard 5.25 80")) {
        return 6;
    }
    if (text == QStringLiteral("5.25 (80)")) {
        return 7;
    }
    return 0;
}

static QString floppyTypeText(int value)
{
    if (value < 0) {
        return QStringLiteral("Disabled");
    }
    if (value == 1) {
        return QStringLiteral("3.5 HD");
    }
    if (value == 2) {
        return QStringLiteral("5.25 SD");
    }
    if (value == 3) {
        return QStringLiteral("3.5 DD (Escom)");
    }
    if (value == 4) {
        return QStringLiteral("Bridgeboard 5.25 40");
    }
    if (value == 5) {
        return QStringLiteral("Bridgeboard 3.5 80");
    }
    if (value == 6) {
        return QStringLiteral("Bridgeboard 5.25 80");
    }
    if (value == 7) {
        return QStringLiteral("5.25 (80)");
    }
    return QStringLiteral("3.5 DD");
}

static int floppySpeedConfigValue(int sliderPosition)
{
    if (sliderPosition <= 0) {
        return 0;
    }
    return 100 << qBound(0, sliderPosition - 1, 3);
}

static int floppySpeedSliderPosition(int value)
{
    if (value <= 0) {
        return 0;
    }
    if (value <= 100) {
        return 1;
    }
    if (value <= 200) {
        return 2;
    }
    if (value <= 400) {
        return 3;
    }
    return 4;
}

static QString floppySpeedText(int value)
{
    if (value <= 0) {
        return QStringLiteral("Turbo");
    }
    if (value == 100) {
        return QStringLiteral("100% (Compatible)");
    }
    return QStringLiteral("%1%").arg(value);
}

static int jitCacheSizeFromPosition(int position)
{
    if (position <= 0) {
        return 0;
    }
    return 1024 << qBound(0, position - 1, 7);
}

static int jitCachePositionFromSize(int size)
{
    if (size <= 0) {
        return 0;
    }
    int position = 1;
    int cacheSize = 1024;
    while (position < 8 && size > cacheSize) {
        cacheSize <<= 1;
        position++;
    }
    return position;
}

static QString jitCacheText(int size)
{
    return QStringLiteral("%1 MB").arg(size > 0 ? size / 1024 : 0);
}

static int cpuMultiplierValue(const QString &text)
{
    if (text.startsWith(QStringLiteral("2x"))) {
        return 2;
    }
    if (text.startsWith(QStringLiteral("4x"))) {
        return 4;
    }
    if (text.startsWith(QStringLiteral("8x"))) {
        return 8;
    }
    if (text.startsWith(QStringLiteral("16x"))) {
        return 16;
    }
    return 1;
}

static QString cpuMultiplierText(int value)
{
    switch (value) {
    case 2:
        return QStringLiteral("2x (A500)");
    case 4:
        return QStringLiteral("4x (A1200)");
    case 8:
        return QStringLiteral("8x");
    case 16:
        return QStringLiteral("16x");
    default:
        return QStringLiteral("1x");
    }
}

static bool configBoolValue(const QString &value)
{
    return value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0
        || value == QStringLiteral("1");
}

static void setComboTextIfChanged(QComboBox *combo, const QString &text)
{
    if (combo && combo->currentText() != text) {
        combo->setCurrentText(text);
    }
}

static void setCheckBoxIfChanged(QCheckBox *box, bool checked)
{
    if (box && box->isChecked() != checked) {
        box->setChecked(checked);
    }
}

static QPushButton *smallButton(const QString &text)
{
    QPushButton *w = new QPushButton(text);
    w->setFixedWidth(text == QStringLiteral("...") ? 24 : 34);
    return w;
}

static QGroupBox *groupBox(const QString &title, QLayout *layout)
{
    QGroupBox *box = new QGroupBox(title);
    QFont titleFont = box->font();
    titleFont.setPixelSize(13);
    box->setFont(titleFont);
    box->setLayout(layout);
    return box;
}

static void addBrowse(QComboBox *field, QWidget *parent, const QString &caption, const QString &filter)
{
    const QString path = QFileDialog::getOpenFileName(parent, caption, field->currentText(), filter);
    if (!path.isEmpty()) {
        field->setCurrentText(path);
    }
}

static QString envString(const char *name)
{
    return QString::fromLocal8Bit(qgetenv(name));
}

class WinUaeQtDialog final : public QDialog {
public:
    enum class StartMode {
        DetachedProcess,
        ReturnConfig
    };

    explicit WinUaeQtDialog(StartMode mode = StartMode::DetachedProcess, QWidget *parent = nullptr)
        : QDialog(parent),
          startMode(mode)
    {
        setWindowTitle(QStringLiteral("WinUAE Properties"));
        setWindowIcon(resourceIcon(QStringLiteral("winuae.ico")));
        resize(820, 560);
        setMinimumSize(760, 520);

        navigation = new QTreeWidget;
        navigation->setHeaderHidden(true);
        navigation->setRootIsDecorated(false);
        navigation->setIndentation(12);
        navigation->setIconSize(QSize(16, 16));
        navigation->setFixedWidth(166);

        pageStack = new QStackedWidget;
        pageStack->setObjectName(QStringLiteral("pageStack"));

        addPage(QStringLiteral("Configurations"), QStringLiteral("configfile.ico"), makeConfigurationsPage());
        addPage(QStringLiteral("Quickstart"), QStringLiteral("quickstart.ico"), makeQuickstartPage());
        addPage(QStringLiteral("CPU and FPU"), QStringLiteral("cpu.ico"), makeCpuPage());
        addPage(QStringLiteral("Chipset"), QStringLiteral("chip.ico"), makeChipsetPage());
        addPage(QStringLiteral("ROM"), QStringLiteral("chip.ico"), makeRomPage());
        addPage(QStringLiteral("RAM"), QStringLiteral("chip.ico"), makeMemoryPage());
        addPage(QStringLiteral("Floppy drives"), QStringLiteral("35floppy.ico"), makeFloppyPage());
        addPage(QStringLiteral("Hard drives"), QStringLiteral("drive.ico"), makeHardDrivesPage());
        addPage(QStringLiteral("Expansion boards"), QStringLiteral("expansion.ico"), makeExpansionPage());
        addPage(QStringLiteral("Display"), QStringLiteral("screen.ico"), makeDisplayPage());
        addPage(QStringLiteral("Sound"), QStringLiteral("sound.ico"), makeSoundPage());
        addPage(QStringLiteral("Game ports"), QStringLiteral("joystick.ico"), makeGamePortsPage());
        addPage(QStringLiteral("Input"), QStringLiteral("port.ico"), makeInputPage());
        addPage(QStringLiteral("Paths"), QStringLiteral("paths.ico"), makePathsPage());
        addPage(QStringLiteral("Miscellaneous"), QStringLiteral("misc.ico"), makeMiscPage());
        addPage(QStringLiteral("About"), QStringLiteral("amigainfo.ico"), makeAboutPage());

        connect(navigation, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
            if (!item) {
                return;
            }
            pageStack->setCurrentIndex(item->data(0, Qt::UserRole).toInt());
        });

        QFrame *outerFrame = new QFrame;
        outerFrame->setFrameShape(QFrame::Box);
        outerFrame->setObjectName(QStringLiteral("outerFrame"));
        QVBoxLayout *frameLayout = new QVBoxLayout(outerFrame);
        frameLayout->setContentsMargins(4, 4, 4, 4);
        frameLayout->addWidget(pageStack);

        QHBoxLayout *content = new QHBoxLayout;
        content->setContentsMargins(0, 0, 0, 0);
        content->setSpacing(5);
        content->addWidget(navigation);
        content->addWidget(outerFrame, 1);

        QPushButton *reset = new QPushButton(QStringLiteral("Reset"));
        QPushButton *quit = new QPushButton(QStringLiteral("Quit"));
        QPushButton *restart = new QPushButton(QStringLiteral("Restart"));
        QPushButton *errorLog = new QPushButton(QStringLiteral("Error log"));
        QPushButton *start = new QPushButton(QStringLiteral("Start"));
        QPushButton *cancel = new QPushButton(QStringLiteral("Cancel"));
        QPushButton *help = new QPushButton(QStringLiteral("Help"));
        restart->setVisible(false);
        errorLog->setVisible(false);
        help->setEnabled(false);
        start->setDefault(true);

        connect(reset, &QPushButton::clicked, this, [this]() { resetDefaults(); });
        connect(quit, &QPushButton::clicked, this, &QDialog::reject);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(start, &QPushButton::clicked, this, [this]() { startEmulator(); });

        QHBoxLayout *buttons = new QHBoxLayout;
        buttons->setContentsMargins(0, 0, 0, 0);
        buttons->addWidget(reset);
        buttons->addWidget(quit);
        buttons->addWidget(restart);
        buttons->addStretch();
        buttons->addWidget(errorLog);
        buttons->addWidget(start);
        buttons->addWidget(cancel);
        buttons->addWidget(help);

        status = new QLabel;
        status->setObjectName(QStringLiteral("statusLine"));
        status->setFrameShape(QFrame::StyledPanel);
        status->setMinimumHeight(22);

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(6, 6, 6, 6);
        root->setSpacing(5);
        root->addLayout(content, 1);
        root->addWidget(status);
        root->addLayout(buttons);

        resetDefaults();
        navigation->setCurrentItem(navigation->topLevelItem(1));
    }

    const WinUaeQtLauncherResult &launcherResult() const
    {
        return result;
    }

private:
    StartMode startMode = StartMode::DetachedProcess;
    QTreeWidget *navigation = nullptr;
    QStackedWidget *pageStack = nullptr;
    QLabel *status = nullptr;

    QComboBox *configName = nullptr;
    QLineEdit *configPath = nullptr;
    QLineEdit *configDescription = nullptr;

    QComboBox *quickModel = nullptr;
    QComboBox *quickConfiguration = nullptr;
    QComboBox *quickHostConfiguration = nullptr;
    QCheckBox *ntsc = nullptr;
    QSlider *compatibility = nullptr;
    QCheckBox *quickDfEnable[2] = {};
    QComboBox *quickDfType[2] = {};
    QComboBox *quickDfPath[2] = {};
    QCheckBox *quickDfWriteProtect[2] = {};

    QComboBox *romFile = nullptr;
    QComboBox *extendedRomFile = nullptr;
    QComboBox *cartFile = nullptr;
    QLineEdit *flashFile = nullptr;
    QLineEdit *rtcFile = nullptr;
    QCheckBox *mapRom = nullptr;
    QCheckBox *kickShifter = nullptr;
    QComboBox *customRomSelect = nullptr;
    QLineEdit *customRomStart = nullptr;
    QLineEdit *customRomEnd = nullptr;
    QLineEdit *customRomFile = nullptr;
    QComboBox *uaeBoardType = nullptr;
    QVector<WinUaeQtRomBoard> customRomBoards;
    int currentCustomRomBoard = 0;
    bool customRomUpdating = false;

    QComboBox *cpuModel = nullptr;
    QButtonGroup *cpuButtons = nullptr;
    QButtonGroup *fpuButtons = nullptr;
    QCheckBox *cpu24Bit = nullptr;
    QCheckBox *moreCompatible = nullptr;
    QCheckBox *cpuDataCache = nullptr;
    QCheckBox *jit = nullptr;
    QCheckBox *cpuUnimplemented = nullptr;
    QButtonGroup *mmuButtons = nullptr;
    QComboBox *chipset = nullptr;
    QComboBox *chipsetCompatible = nullptr;
    QCheckBox *fpuStrict = nullptr;
    QCheckBox *fpuUnimplemented = nullptr;
    QComboBox *fpuMode = nullptr;
    QButtonGroup *cpuSpeedButtons = nullptr;
    QSlider *cpuSpeed = nullptr;
    QLineEdit *cpuSpeedLabel = nullptr;
    QComboBox *cpuFrequency = nullptr;
    QLineEdit *cpuFrequencyCustom = nullptr;
    QSlider *jitCache = nullptr;
    QLineEdit *jitCacheLabel = nullptr;
    QCheckBox *jitFpu = nullptr;
    QCheckBox *jitConstJump = nullptr;
    QCheckBox *jitHardFlush = nullptr;
    QButtonGroup *jitTrust = nullptr;
    QCheckBox *jitNoFlags = nullptr;
    QCheckBox *jitCatchFault = nullptr;

    QComboBox *chipMem = nullptr;
    QComboBox *z2Fast = nullptr;
    QComboBox *slowMem = nullptr;
    QComboBox *z3Fast = nullptr;
    QComboBox *rtgMem = nullptr;
    QComboBox *rtgType = nullptr;
    QComboBox *rtgMonitor = nullptr;
    QCheckBox *rtgScale = nullptr;
    QCheckBox *rtgCenter = nullptr;
    QCheckBox *rtgIntegerScale = nullptr;
    QCheckBox *rtgMultithread = nullptr;
    QCheckBox *rtgHardwareSprite = nullptr;
    QCheckBox *rtgHardwareVBlank = nullptr;
    QCheckBox *rtgAutoswitch = nullptr;
    QCheckBox *rtgInitialMonitor = nullptr;
    QComboBox *rtg8Bit = nullptr;
    QComboBox *rtg16Bit = nullptr;
    QComboBox *rtg24Bit = nullptr;
    QComboBox *rtg32Bit = nullptr;
    QComboBox *rtgRefreshRate = nullptr;
    QComboBox *rtgBuffers = nullptr;

    QCheckBox *dfEnable[4] = {};
    QComboBox *dfType[4] = {};
    QComboBox *dfPath[4] = {};
    QCheckBox *dfWriteProtect[4] = {};
    QSlider *floppySpeed = nullptr;
    QLineEdit *floppySpeedLabel = nullptr;
    QTreeWidget *mountedDrives = nullptr;
    QPushButton *addDirectoryMountButton = nullptr;
    QPushButton *addHardfileMountButton = nullptr;
    QPushButton *addHardDriveMountButton = nullptr;
    QPushButton *addCdMountButton = nullptr;
    QPushButton *addTapeMountButton = nullptr;
    QPushButton *propertiesMountButton = nullptr;
    QPushButton *removeMountButton = nullptr;
    QComboBox *cdSlotNumber = nullptr;
    QComboBox *cdSlotType = nullptr;
    QComboBox *cdSlotPath = nullptr;
    QCheckBox *cdSpeedTurbo = nullptr;
    QVector<WinUaeQtCdSlot> cdSlots;
    int currentCdSlot = 0;
    bool cdSlotUpdating = false;

    QLineEdit *windowWidth = nullptr;
    QLineEdit *windowHeight = nullptr;
    QCheckBox *windowResize = nullptr;
    QComboBox *fullscreenResolution = nullptr;
    QComboBox *nativeMode = nullptr;
    QComboBox *rtgMode = nullptr;
    QComboBox *displayResolution = nullptr;
    QCheckBox *displayCenterHorizontal = nullptr;
    QCheckBox *displayCenterVertical = nullptr;
    QCheckBox *displayFlickerFixer = nullptr;
    QCheckBox *displayLoresSmoothed = nullptr;
    QButtonGroup *displayLineModeButtons = nullptr;
    QButtonGroup *soundOutputButtons = nullptr;
    QCheckBox *soundAutomatic = nullptr;
    QSlider *soundMasterVolume = nullptr;
    QLabel *soundMasterVolumeValue = nullptr;
    QComboBox *soundVolumeSelect = nullptr;
    QSlider *soundSelectedVolume = nullptr;
    QLabel *soundSelectedVolumeValue = nullptr;
    QSlider *soundBufferSize = nullptr;
    QLabel *soundBufferSizeValue = nullptr;
    QComboBox *soundChannels = nullptr;
    QComboBox *soundStereoSeparation = nullptr;
    QComboBox *soundInterpolation = nullptr;
    QComboBox *soundFrequency = nullptr;
    QComboBox *soundSwap = nullptr;
    QComboBox *soundStereoDelay = nullptr;
    QComboBox *soundFilter = nullptr;
    QComboBox *floppySoundDrive = nullptr;
    QComboBox *floppySoundType = nullptr;
    QSlider *floppySoundEmptyVolume = nullptr;
    QLabel *floppySoundEmptyVolumeValue = nullptr;
    QSlider *floppySoundDiskVolume = nullptr;
    QLabel *floppySoundDiskVolumeValue = nullptr;
    int soundVolumeAttenuation[SoundVolumeCount] = {};
    int floppySoundTypeValue[FloppySoundDriveCount] = {};
    int floppySoundEmptyAttenuation[FloppySoundDriveCount] = {};
    int floppySoundDiskAttenuation[FloppySoundDriveCount] = {};
    int currentSoundVolume = 0;
    int currentFloppySoundDrive = 0;
    bool soundVolumeUpdating = false;
    bool floppySoundUpdating = false;
    QComboBox *portDevice[4] = {};
    QComboBox *portAutofire[2] = {};
    QComboBox *portMode[2] = {};
    QCheckBox *portAutoswitch = nullptr;
    QSpinBox *mouseSpeed = nullptr;
    QCheckBox *virtualMouseDriver = nullptr;
    QComboBox *mouseUntrapMode = nullptr;
    QComboBox *magicMouseCursor = nullptr;
    QCheckBox *tabletLibrary = nullptr;
    QComboBox *tabletMode = nullptr;
    QLineEdit *emulatorPath = nullptr;
    QLineEdit *romsPath = nullptr;
    QLineEdit *configsPath = nullptr;
    WinUaeQtLauncherBackend launcherBackend;
    WinUaeQtConfig loadedConfig;
    WinUaeQtLauncherResult result;

    void addPage(const QString &title, const QString &icon, QWidget *page)
    {
        const int index = pageStack->addWidget(page);
        QTreeWidgetItem *item = new QTreeWidgetItem(navigation);
        item->setText(0, title);
        item->setIcon(0, resourceIcon(icon));
        item->setData(0, Qt::UserRole, index);
    }

    QWidget *makePage()
    {
        QWidget *page = new QWidget;
        page->setObjectName(QStringLiteral("page"));
        return page;
    }

    QWidget *makeConfigurationsPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(4);

        QTreeWidget *tree = new QTreeWidget;
        tree->setHeaderHidden(true);
        tree->setRootIsDecorated(true);
        QTreeWidgetItem *configs = new QTreeWidgetItem(tree, QStringList(QStringLiteral("Configurations")));
        configs->setIcon(0, resourceIcon(QStringLiteral("configfile.ico")));
        new QTreeWidgetItem(configs, QStringList(QStringLiteral("Default")));
        new QTreeWidgetItem(configs, QStringList(QStringLiteral("A1200 Install")));
        configs->setExpanded(true);
        root->addWidget(tree, 1);

        QGridLayout *search = new QGridLayout;
        search->setColumnStretch(1, 1);
        search->setColumnStretch(4, 1);
        search->addWidget(label(QStringLiteral("Search:")), 0, 0);
        search->addWidget(new QLineEdit, 0, 1);
        search->addWidget(smallButton(QStringLiteral("X")), 0, 2);
        search->addWidget(label(QStringLiteral("Filter:")), 0, 3);
        search->addWidget(combo({ QStringLiteral("All configurations"), QStringLiteral("Host"), QStringLiteral("Hardware") }), 0, 4);
        root->addLayout(search);

        configName = pathCombo();
        configDescription = new QLineEdit;
        configPath = new QLineEdit;
        configPath->setReadOnly(true);

        QGridLayout *details = new QGridLayout;
        details->setColumnStretch(1, 1);
        details->addWidget(label(QStringLiteral("Name:")), 0, 0);
        details->addWidget(configName, 0, 1);
        details->addWidget(configPath, 0, 2);
        details->addWidget(label(QStringLiteral("Description:")), 1, 0);
        details->addWidget(configDescription, 1, 1, 1, 2);
        root->addLayout(details);

        QHBoxLayout *buttons = new QHBoxLayout;
        QPushButton *load = new QPushButton(QStringLiteral("Load From..."));
        QPushButton *save = new QPushButton(QStringLiteral("Save As..."));
        buttons->addWidget(new QPushButton(QStringLiteral("Load")));
        buttons->addWidget(new QPushButton(QStringLiteral("Save")));
        buttons->addStretch();
        buttons->addWidget(load);
        buttons->addWidget(save);
        buttons->addWidget(new QPushButton(QStringLiteral("Delete")));
        root->addLayout(buttons);

        connect(load, &QPushButton::clicked, this, [this]() { loadConfigDialog(); });
        connect(save, &QPushButton::clicked, this, [this]() { saveConfigDialog(); });
        return page;
    }

    QWidget *makeQuickstartPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(6);

        quickModel = combo({
            QStringLiteral("A500"),
            QStringLiteral("A500+"),
            QStringLiteral("A600"),
            QStringLiteral("A1200"),
            QStringLiteral("A4000")
        }, QStringLiteral("A1200"));
        quickConfiguration = combo({
            QStringLiteral("Basic non-expanded configuration"),
            QStringLiteral("Expanded configuration"),
            QStringLiteral("Cycle-exact compatible")
        }, QStringLiteral("Expanded configuration"));
        ntsc = new QCheckBox(QStringLiteral("NTSC"));

        QGridLayout *hardware = new QGridLayout;
        hardware->setColumnStretch(1, 1);
        hardware->addWidget(label(QStringLiteral("Model:")), 0, 0);
        hardware->addWidget(quickModel, 0, 1);
        hardware->addWidget(ntsc, 0, 2);
        hardware->addWidget(label(QStringLiteral("Configuration:")), 1, 0);
        hardware->addWidget(quickConfiguration, 1, 1, 1, 2);
        root->addWidget(groupBox(QStringLiteral("Emulated Hardware"), hardware));

        compatibility = new QSlider(Qt::Horizontal);
        compatibility->setRange(0, 4);
        compatibility->setTickInterval(1);
        compatibility->setTickPosition(QSlider::TicksAbove);
        compatibility->setValue(1);

        QHBoxLayout *compat = new QHBoxLayout;
        compat->addWidget(new QLabel(QStringLiteral("Best compatibility")));
        compat->addWidget(compatibility, 1);
        compat->addWidget(new QLabel(QStringLiteral("Low compatibility")));
        root->addWidget(groupBox(QStringLiteral("Compatibility vs Required CPU Power"), compat));

        quickHostConfiguration = combo({
            QStringLiteral("Default"),
            QStringLiteral("Windowed"),
            QStringLiteral("Fullscreen")
        }, QStringLiteral("Default"));
        QGridLayout *host = new QGridLayout;
        host->setColumnStretch(1, 1);
        host->addWidget(label(QStringLiteral("Configuration:")), 0, 0);
        host->addWidget(quickHostConfiguration, 0, 1);
        root->addWidget(groupBox(QStringLiteral("Host Configuration"), host));

        QGridLayout *drives = new QGridLayout;
        drives->setColumnStretch(5, 1);
        addQuickDriveRow(drives, 0);
        addQuickDriveRow(drives, 1);
        root->addWidget(groupBox(QStringLiteral("Emulated Drives"), drives), 1);

        connect(quickModel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            applyModelPreset(quickModel->currentText());
        });
        return page;
    }

    void addQuickDriveRow(QGridLayout *layout, int drive)
    {
        quickDfEnable[drive] = new QCheckBox(QStringLiteral("Floppy drive DF%1:").arg(drive));
        QPushButton *select = new QPushButton(QStringLiteral("Select image file"));
        quickDfType[drive] = combo(floppyTypeItems(drive, true));
        quickDfWriteProtect[drive] = new QCheckBox;
        QPushButton *info = smallButton(QStringLiteral("?"));
        QPushButton *eject = new QPushButton(QStringLiteral("Eject"));
        quickDfPath[drive] = pathCombo();
        quickDfPath[drive]->setMinimumWidth(300);
        const int row = drive * 2;
        layout->addWidget(quickDfEnable[drive], row, 0);
        layout->addWidget(select, row, 1);
        layout->addWidget(quickDfType[drive], row, 2);
        layout->addWidget(new QLabel(QStringLiteral("Write-protected")), row, 3);
        layout->addWidget(quickDfWriteProtect[drive], row, 4);
        layout->addWidget(info, row, 5);
        layout->addWidget(eject, row, 6);
        layout->addWidget(quickDfPath[drive], row + 1, 0, 1, 7);
        quickDfEnable[drive]->setChecked(drive == 0);
        info->setEnabled(false);
        connect(select, &QPushButton::clicked, this, [this, drive]() {
            addBrowse(quickDfPath[drive], this, QStringLiteral("Select floppy image"), QStringLiteral("Amiga disk images (*.adf *.adz *.ipf *.dms);;All files (*)"));
        });
        connect(eject, &QPushButton::clicked, this, [this, drive]() {
            quickDfPath[drive]->setCurrentText(QString());
        });
    }

    QWidget *makeCpuPage()
    {
        QWidget *page = makePage();
        QHBoxLayout *root = new QHBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(6);

        QVBoxLayout *left = new QVBoxLayout;
        cpuButtons = new QButtonGroup(this);
        QVBoxLayout *cpu = new QVBoxLayout;
        const QStringList cpus = { QStringLiteral("68000"), QStringLiteral("68010"), QStringLiteral("68020"), QStringLiteral("68030"), QStringLiteral("68040"), QStringLiteral("68060") };
        for (const QString &name : cpus) {
            QRadioButton *button = new QRadioButton(name);
            cpu->addWidget(button);
            cpuButtons->addButton(button, name.mid(2).toInt() + 68000);
            connect(button, &QRadioButton::clicked, this, [this]() {
                updateFpuControls();
                updateCpuControlState();
            });
            if (name == QStringLiteral("68020")) {
                button->setChecked(true);
            }
        }
        cpu24Bit = new QCheckBox(QStringLiteral("24-bit addressing"));
        moreCompatible = new QCheckBox(QStringLiteral("More compatible"));
        cpuDataCache = new QCheckBox(QStringLiteral("Data cache emulation"));
        jit = new QCheckBox(QStringLiteral("JIT"));
        cpuUnimplemented = new QCheckBox(QStringLiteral("Unimplemented CPU emu"));
        cpu->addWidget(cpu24Bit);
        cpu->addWidget(moreCompatible);
        cpu->addWidget(cpuDataCache);
        cpu->addWidget(jit);
        cpu->addWidget(cpuUnimplemented);
        left->addWidget(groupBox(QStringLiteral("CPU"), cpu));

        mmuButtons = new QButtonGroup(this);
        QHBoxLayout *mmu = new QHBoxLayout;
        const QStringList mmus = { QStringLiteral("None"), QStringLiteral("MMU"), QStringLiteral("EC") };
        for (int i = 0; i < mmus.size(); i++) {
            QRadioButton *button = new QRadioButton(mmus[i]);
            mmu->addWidget(button);
            mmuButtons->addButton(button, i);
            if (i == 0) {
                button->setChecked(true);
            }
        }
        left->addWidget(groupBox(QStringLiteral("MMU"), mmu));

        fpuButtons = new QButtonGroup(this);
        QVBoxLayout *fpu = new QVBoxLayout;
        const QStringList fpus = { QStringLiteral("None"), QStringLiteral("68881"), QStringLiteral("68882"), QStringLiteral("CPU internal") };
        for (int i = 0; i < fpus.size(); i++) {
            QRadioButton *button = new QRadioButton(fpus[i]);
            fpu->addWidget(button);
            const int id = i == 1 ? 68881 : (i == 2 ? 68882 : (i == 3 ? FpuInternal : 0));
            fpuButtons->addButton(button, id);
            if (i == 0) {
                button->setChecked(true);
            }
        }
        fpuStrict = new QCheckBox(QStringLiteral("More compatible"));
        fpuUnimplemented = new QCheckBox(QStringLiteral("Unimplemented FPU emu"));
        fpuMode = combo({
            QStringLiteral("Host (64-bit)"),
            QStringLiteral("Host (80-bit)"),
            QStringLiteral("Softfloat (80-bit)")
        }, QStringLiteral("Host (64-bit)"));
        fpu->addWidget(fpuStrict);
        fpu->addWidget(fpuUnimplemented);
        fpu->addWidget(fpuMode);
        left->addWidget(groupBox(QStringLiteral("FPU"), fpu));
        left->addStretch();

        QVBoxLayout *right = new QVBoxLayout;
        QGridLayout *speed = new QGridLayout;
        cpuSpeedButtons = new QButtonGroup(this);
        QRadioButton *fastest = new QRadioButton(QStringLiteral("Fastest possible"));
        QRadioButton *approx = new QRadioButton(QStringLiteral("Approximate A500/A1200 or cycle-exact"));
        approx->setChecked(true);
        cpuSpeedButtons->addButton(fastest, 1);
        cpuSpeedButtons->addButton(approx, 0);
        speed->addWidget(fastest, 0, 0, 1, 2);
        speed->addWidget(approx, 1, 0, 1, 2);
        cpuSpeed = new QSlider(Qt::Horizontal);
        cpuSpeed->setRange(-9, 50);
        cpuSpeed->setTickInterval(1);
        cpuSpeed->setTickPosition(QSlider::TicksAbove);
        cpuSpeedLabel = new QLineEdit;
        cpuSpeedLabel->setReadOnly(true);
        cpuSpeedLabel->setAlignment(Qt::AlignCenter);
        cpuSpeedLabel->setMinimumWidth(60);
        speed->addWidget(label(QStringLiteral("CPU Speed")), 2, 0);
        speed->addWidget(cpuSpeed, 2, 1);
        speed->addWidget(cpuSpeedLabel, 2, 2);
        right->addWidget(groupBox(QStringLiteral("CPU Emulation Speed"), speed));

        QGridLayout *cycle = new QGridLayout;
        cpuFrequency = combo({
            QStringLiteral("1x"),
            QStringLiteral("2x (A500)"),
            QStringLiteral("4x (A1200)"),
            QStringLiteral("8x"),
            QStringLiteral("16x"),
            QStringLiteral("Custom")
        }, QStringLiteral("4x (A1200)"));
        cpuFrequencyCustom = new QLineEdit;
        cpuFrequencyCustom->setPlaceholderText(QStringLiteral("MHz"));
        cycle->addWidget(label(QStringLiteral("CPU Frequency")), 0, 0);
        cycle->addWidget(cpuFrequency, 0, 1);
        cycle->addWidget(cpuFrequencyCustom, 0, 2);
        right->addWidget(groupBox(QStringLiteral("Cycle-exact CPU Emulation Speed"), cycle));

        QGridLayout *jitBox = new QGridLayout;
        jitCache = new QSlider(Qt::Horizontal);
        jitCache->setRange(0, 8);
        jitCache->setTickInterval(1);
        jitCache->setTickPosition(QSlider::TicksAbove);
        jitCacheLabel = new QLineEdit;
        jitCacheLabel->setReadOnly(true);
        jitCacheLabel->setAlignment(Qt::AlignCenter);
        jitCacheLabel->setMinimumWidth(60);
        jitFpu = new QCheckBox(QStringLiteral("FPU support"));
        jitConstJump = new QCheckBox(QStringLiteral("Constant jump"));
        jitHardFlush = new QCheckBox(QStringLiteral("Hard flush"));
        jitTrust = new QButtonGroup(this);
        QRadioButton *jitDirect = new QRadioButton(QStringLiteral("Direct"));
        QRadioButton *jitIndirect = new QRadioButton(QStringLiteral("Indirect"));
        jitTrust->addButton(jitDirect, 0);
        jitTrust->addButton(jitIndirect, 1);
        jitDirect->setChecked(true);
        jitNoFlags = new QCheckBox(QStringLiteral("No flags"));
        jitCatchFault = new QCheckBox(QStringLiteral("Catch unexpected exceptions"));
        jitBox->addWidget(label(QStringLiteral("Cache size:")), 0, 0);
        jitBox->addWidget(jitCache, 0, 1);
        jitBox->addWidget(jitCacheLabel, 0, 2);
        jitBox->addWidget(jitFpu, 1, 0);
        jitBox->addWidget(jitConstJump, 1, 1);
        jitBox->addWidget(jitHardFlush, 1, 2);
        jitBox->addWidget(jitDirect, 2, 0);
        jitBox->addWidget(jitIndirect, 2, 1);
        jitBox->addWidget(jitNoFlags, 2, 2);
        jitBox->addWidget(jitCatchFault, 3, 0, 1, 3);
        right->addWidget(groupBox(QStringLiteral("Advanced JIT Settings"), jitBox));
        right->addStretch();

        connect(cpu24Bit, &QCheckBox::toggled, this, [this]() { updateCpuControlState(); });
        connect(moreCompatible, &QCheckBox::toggled, this, [this]() { updateCpuControlState(); });
        connect(jit, &QCheckBox::toggled, this, [this]() { updateCpuControlState(); });
        connect(fpuButtons, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, [this]() { updateCpuControlState(); });
        connect(cpuSpeed, &QSlider::valueChanged, this, [this]() { updateCpuSpeedLabel(); });
        connect(jitCache, &QSlider::valueChanged, this, [this]() { updateJitCacheLabel(); });
        updateFpuControls();
        updateCpuControlState();

        root->addLayout(left, 1);
        root->addLayout(right, 2);
        return page;
    }

    QWidget *makeChipsetPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);

        chipset = combo({ QStringLiteral("OCS"), QStringLiteral("ECS"), QStringLiteral("AGA") }, QStringLiteral("AGA"));
        chipsetCompatible = combo({
            QStringLiteral("A500"),
            QStringLiteral("A500+"),
            QStringLiteral("A600"),
            QStringLiteral("A1200"),
            QStringLiteral("A4000")
        }, QStringLiteral("A1200"));
        QGridLayout *basic = new QGridLayout;
        basic->setColumnStretch(1, 1);
        basic->addWidget(label(QStringLiteral("Chipset:")), 0, 0);
        basic->addWidget(chipset, 0, 1);
        basic->addWidget(label(QStringLiteral("Compatible:")), 1, 0);
        basic->addWidget(chipsetCompatible, 1, 1);
        basic->addWidget(new QCheckBox(QStringLiteral("Immediate blitter")), 2, 1);
        basic->addWidget(new QCheckBox(QStringLiteral("Cycle-exact")), 3, 1);
        root->addWidget(groupBox(QStringLiteral("Chipset"), basic));
        root->addStretch();
        return page;
    }

    QWidget *makeRomPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(6);

        romFile = pathCombo();
        extendedRomFile = pathCombo();
        mapRom = new QCheckBox(QStringLiteral("MapROM emulation"));
        kickShifter = new QCheckBox(QStringLiteral("ShapeShifter support"));
        QGridLayout *system = new QGridLayout;
        system->setColumnStretch(1, 1);
        addPathRow(system, 0, QStringLiteral("Main ROM file:"), romFile, QStringLiteral("Select main ROM file"), QStringLiteral("ROM files (*.rom *.bin);;All files (*)"));
        addPathRow(system, 1, QStringLiteral("Extended ROM file:"), extendedRomFile, QStringLiteral("Select extended ROM file"), QStringLiteral("ROM files (*.rom *.bin);;All files (*)"));
        system->addWidget(mapRom, 4, 0);
        system->addWidget(kickShifter, 4, 1);
        root->addWidget(groupBox(QStringLiteral("System ROM Settings"), system));

        QGridLayout *advanced = new QGridLayout;
        customRomSelect = combo({});
        for (int i = 0; i < MaxRomBoards; i++) {
            customRomSelect->addItem(QStringLiteral("ROM #%1").arg(i + 1));
        }
        customRomStart = new QLineEdit;
        customRomEnd = new QLineEdit;
        customRomFile = new QLineEdit;
        QPushButton *customRomBrowse = smallButton(QStringLiteral("..."));
        advanced->setColumnStretch(3, 1);
        advanced->addWidget(customRomSelect, 0, 0);
        advanced->addWidget(label(QStringLiteral("Address range")), 0, 1);
        advanced->addWidget(customRomStart, 0, 2);
        advanced->addWidget(customRomEnd, 0, 3);
        advanced->addWidget(customRomFile, 1, 0, 1, 4);
        advanced->addWidget(customRomBrowse, 1, 4);
        root->addWidget(groupBox(QStringLiteral("Advanced Custom ROM Settings"), advanced));

        cartFile = pathCombo();
        flashFile = new QLineEdit;
        rtcFile = new QLineEdit;
        QGridLayout *misc = new QGridLayout;
        misc->setColumnStretch(1, 1);
        addPathRow(misc, 0, QStringLiteral("Cartridge ROM file:"), cartFile, QStringLiteral("Select cartridge ROM file"), QStringLiteral("ROM files (*.rom *.bin);;All files (*)"));
        addLineBrowseRow(misc, 2, QStringLiteral("Flash RAM or A2286/A2386SX BIOS CMOS RAM file:"), flashFile);
        addLineBrowseRow(misc, 3, QStringLiteral("Real Time Clock file"), rtcFile);
        root->addWidget(groupBox(QStringLiteral("Miscellaneous"), misc), 1);

        uaeBoardType = combo({
            QStringLiteral("ROM disabled"),
            QStringLiteral("Original UAE (FS + F0 ROM)"),
            QStringLiteral("New UAE (64k + F0 ROM)"),
            QStringLiteral("New UAE (128k, ROM, Direct)"),
            QStringLiteral("New UAE (128k, ROM, Indirect)")
        }, QStringLiteral("Original UAE (FS + F0 ROM)"));
        QGridLayout *uaeBoard = new QGridLayout;
        uaeBoard->setColumnStretch(1, 1);
        uaeBoard->addWidget(label(QStringLiteral("Board type:")), 0, 0);
        uaeBoard->addWidget(uaeBoardType, 0, 1);
        root->addWidget(groupBox(QStringLiteral("Advanced UAE expansion board/Boot ROM Settings"), uaeBoard));

        connect(customRomSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (customRomUpdating) {
                return;
            }
            storeCurrentCustomRomBoard();
            currentCustomRomBoard = qBound(0, index, MaxRomBoards - 1);
            loadCurrentCustomRomBoard();
        });
        connect(customRomStart, &QLineEdit::textChanged, this, [this](const QString &) { storeCurrentCustomRomBoard(); });
        connect(customRomEnd, &QLineEdit::textChanged, this, [this](const QString &) { storeCurrentCustomRomBoard(); });
        connect(customRomFile, &QLineEdit::textChanged, this, [this](const QString &) { storeCurrentCustomRomBoard(); });
        connect(customRomBrowse, &QPushButton::clicked, this, [this]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select custom ROM file"), customRomFile->text(), QStringLiteral("ROM files (*.rom *.bin);;All files (*)"));
            if (selected.isEmpty()) {
                return;
            }
            customRomFile->setText(selected);
            bool ok = false;
            QString startText = customRomStart->text().trimmed();
            if (startText.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
                startText = startText.mid(2);
            }
            const quint32 start = startText.toUInt(&ok, 16);
            if (ok && customRomEnd->text().trimmed().isEmpty()) {
                const qint64 size = QFileInfo(selected).size();
                if (size > 0) {
                    const quint32 end = ((start + quint32(size) - 1) & ~quint32(0xffff)) | quint32(0xffff);
                    customRomEnd->setText(QStringLiteral("%1").arg(end, 8, 16, QLatin1Char('0')));
                }
            }
            storeCurrentCustomRomBoard();
        });
        clearCustomRomBoards();
        return page;
    }

    QWidget *makeMemoryPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);

        chipMem = combo({ QStringLiteral("512 KB"), QStringLiteral("1 MB"), QStringLiteral("2 MB"), QStringLiteral("4 MB"), QStringLiteral("8 MB") }, QStringLiteral("2 MB"));
        z2Fast = combo({ QStringLiteral("None"), QStringLiteral("1 MB"), QStringLiteral("2 MB"), QStringLiteral("4 MB"), QStringLiteral("8 MB") });
        slowMem = combo({ QStringLiteral("None"), QStringLiteral("512 KB"), QStringLiteral("1 MB"), QStringLiteral("1.5 MB"), QStringLiteral("1.8 MB") });
        z3Fast = combo({ QStringLiteral("None"), QStringLiteral("8 MB"), QStringLiteral("16 MB"), QStringLiteral("32 MB"), QStringLiteral("64 MB"), QStringLiteral("128 MB") });

        QGridLayout *settings = new QGridLayout;
        settings->setColumnStretch(1, 1);
        settings->setColumnStretch(3, 1);
        settings->addWidget(label(QStringLiteral("Chip:")), 0, 0);
        settings->addWidget(chipMem, 0, 1);
        settings->addWidget(label(QStringLiteral("Slow:")), 0, 2);
        settings->addWidget(slowMem, 0, 3);
        settings->addWidget(label(QStringLiteral("Z2 Fast:")), 1, 0);
        settings->addWidget(z2Fast, 1, 1);
        settings->addWidget(label(QStringLiteral("Z3 Fast:")), 1, 2);
        settings->addWidget(z3Fast, 1, 3);
        settings->addWidget(label(QStringLiteral("Processor slot:")), 2, 0);
        settings->addWidget(combo({ QStringLiteral("None"), QStringLiteral("32 MB"), QStringLiteral("64 MB"), QStringLiteral("128 MB") }), 2, 1);
        root->addWidget(groupBox(QStringLiteral("Memory Settings"), settings));

        QGridLayout *advanced = new QGridLayout;
        advanced->addWidget(combo({ QStringLiteral("None"), QStringLiteral("Custom memory") }), 0, 0, 1, 2);
        advanced->addWidget(label(QStringLiteral("Manufacturer")), 1, 0);
        advanced->addWidget(new QLineEdit, 1, 1);
        advanced->addWidget(label(QStringLiteral("Product")), 1, 2);
        advanced->addWidget(new QLineEdit, 1, 3);
        advanced->addWidget(new QCheckBox(QStringLiteral("Manual configuration")), 2, 1);
        advanced->addWidget(new QCheckBox(QStringLiteral("DMA Capable")), 2, 2);
        root->addWidget(groupBox(QStringLiteral("Advanced Memory Settings"), advanced), 1);
        return page;
    }

    QWidget *makeFloppyPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        QGridLayout *drives = new QGridLayout;
        drives->setColumnStretch(5, 1);
        for (int i = 0; i < 4; i++) {
            addFloppyRow(drives, i);
        }
        root->addWidget(groupBox(QStringLiteral("Floppy Drives"), drives), 1);
        floppySpeed = new QSlider(Qt::Horizontal);
        floppySpeed->setRange(0, 4);
        floppySpeed->setTickInterval(1);
        floppySpeed->setTickPosition(QSlider::TicksAbove);
        floppySpeedLabel = new QLineEdit;
        floppySpeedLabel->setReadOnly(true);
        floppySpeedLabel->setAlignment(Qt::AlignCenter);
        floppySpeedLabel->setMinimumWidth(130);
        QGridLayout *speed = new QGridLayout;
        speed->addWidget(floppySpeed, 0, 0);
        speed->addWidget(floppySpeedLabel, 0, 1);
        root->addWidget(groupBox(QStringLiteral("Floppy Drive Emulation Speed"), speed));
        connect(floppySpeed, &QSlider::valueChanged, this, [this]() {
            updateFloppySpeedLabel();
        });
        return page;
    }

    void addFloppyRow(QGridLayout *layout, int drive)
    {
        dfEnable[drive] = new QCheckBox(QStringLiteral("DF%1:").arg(drive));
        dfType[drive] = combo(floppyTypeItems(drive, false));
        dfPath[drive] = pathCombo();
        dfWriteProtect[drive] = new QCheckBox;
        QPushButton *info = smallButton(QStringLiteral("?"));
        QPushButton *eject = new QPushButton(QStringLiteral("Eject"));
        QPushButton *browse = smallButton(QStringLiteral("..."));
        const int row = drive * 2;
        layout->addWidget(dfEnable[drive], row, 0);
        layout->addWidget(dfType[drive], row, 1);
        layout->addWidget(new QLabel(QStringLiteral("Write-protected")), row, 2);
        layout->addWidget(dfWriteProtect[drive], row, 3);
        layout->addWidget(info, row, 4);
        layout->addWidget(eject, row, 5);
        layout->addWidget(browse, row, 6);
        layout->addWidget(dfPath[drive], row + 1, 0, 1, 7);
        dfEnable[drive]->setChecked(drive == 0);
        info->setEnabled(false);
        connect(browse, &QPushButton::clicked, this, [this, drive]() {
            addBrowse(dfPath[drive], this, QStringLiteral("Select floppy image"), QStringLiteral("Amiga disk images (*.adf *.adz *.ipf *.dms);;All files (*)"));
        });
        connect(eject, &QPushButton::clicked, this, [this, drive]() {
            dfPath[drive]->setCurrentText(QString());
        });
        if (drive < 2 && quickDfPath[drive]) {
            connect(dfPath[drive], &QComboBox::currentTextChanged, this, [this, drive](const QString &text) {
                if (quickDfPath[drive]->currentText() != text) {
                    quickDfPath[drive]->setCurrentText(text);
                }
            });
            connect(quickDfPath[drive], &QComboBox::currentTextChanged, this, [this, drive](const QString &text) {
                if (dfPath[drive]->currentText() != text) {
                    dfPath[drive]->setCurrentText(text);
                }
            });
            connect(dfEnable[drive], &QCheckBox::toggled, quickDfEnable[drive], &QCheckBox::setChecked);
            connect(quickDfEnable[drive], &QCheckBox::toggled, dfEnable[drive], &QCheckBox::setChecked);
            connect(dfType[drive], QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, drive]() {
                syncFloppyDriveToQuick(drive);
            });
            connect(quickDfType[drive], QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, drive]() {
                syncQuickDriveToFloppy(drive);
            });
            connect(dfWriteProtect[drive], &QCheckBox::toggled, this, [this, drive]() {
                syncFloppyDriveToQuick(drive);
            });
            connect(quickDfWriteProtect[drive], &QCheckBox::toggled, this, [this, drive]() {
                syncQuickDriveToFloppy(drive);
            });
        }
    }

    QWidget *makeHardDrivesPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        mountedDrives = new QTreeWidget;
        mountedDrives->setRootIsDecorated(false);
        mountedDrives->setSelectionMode(QAbstractItemView::SingleSelection);
        mountedDrives->setDragDropMode(QAbstractItemView::InternalMove);
        mountedDrives->setDragEnabled(true);
        mountedDrives->setAcceptDrops(true);
        mountedDrives->viewport()->setAcceptDrops(true);
        mountedDrives->setDefaultDropAction(Qt::MoveAction);
        mountedDrives->setDropIndicatorShown(true);
        mountedDrives->setHeaderLabels({
            QStringLiteral("*"),
            QStringLiteral("Device"),
            QStringLiteral("Volume"),
            QStringLiteral("Path"),
            QStringLiteral("RW"),
            QStringLiteral("Block size"),
            QStringLiteral("Size"),
            QStringLiteral("BootPri")
        });
        QVBoxLayout *volumeLayout = new QVBoxLayout;
        volumeLayout->addWidget(mountedDrives);
        root->addWidget(groupBox(QStringLiteral("Mounted drives"), volumeLayout), 1);
        QGridLayout *buttons = new QGridLayout;
        addDirectoryMountButton = new QPushButton(QStringLiteral("Add Directory or Archive..."));
        addHardfileMountButton = new QPushButton(QStringLiteral("Add Hardfile..."));
        addHardDriveMountButton = new QPushButton(QStringLiteral("Add Hard Drive..."));
        addCdMountButton = new QPushButton(QStringLiteral("Add SCSI/IDE CD Drive"));
        addTapeMountButton = new QPushButton(QStringLiteral("Add SCSI/IDE Tape Drive"));
        propertiesMountButton = new QPushButton(QStringLiteral("Properties"));
        removeMountButton = new QPushButton(QStringLiteral("Remove"));
        buttons->setColumnStretch(4, 1);
        buttons->addWidget(addDirectoryMountButton, 0, 0);
        buttons->addWidget(addHardfileMountButton, 0, 1);
        buttons->addWidget(addHardDriveMountButton, 0, 2);
        buttons->addWidget(addCdMountButton, 1, 0);
        buttons->addWidget(addTapeMountButton, 1, 1);
        buttons->addWidget(propertiesMountButton, 1, 2);
        buttons->addWidget(removeMountButton, 1, 3);
        root->addLayout(buttons);

        cdSlotNumber = combo({});
        for (int i = 0; i < MaxCdSlots; i++) {
            cdSlotNumber->addItem(QString::number(i + 1));
        }
        cdSlotPath = pathCombo();
        cdSlotType = combo({ QStringLiteral("Autodetect"), QStringLiteral("Image file") }, QStringLiteral("Image file"));
        QPushButton *selectCdImage = new QPushButton(QStringLiteral("Select image file"));
        QPushButton *ejectCd = new QPushButton(QStringLiteral("Eject"));
        cdSpeedTurbo = new QCheckBox(QStringLiteral("CDTV/CDTV-CR/CD32 turbo CD read speed"));

        QGridLayout *optical = new QGridLayout;
        optical->setColumnStretch(1, 1);
        optical->addWidget(new QLabel(QStringLiteral("CD drive/image")), 0, 0);
        optical->addWidget(selectCdImage, 0, 2);
        optical->addWidget(cdSlotType, 0, 3);
        optical->addWidget(ejectCd, 0, 4);
        optical->addWidget(cdSlotNumber, 1, 0);
        optical->addWidget(cdSlotPath, 1, 1, 1, 4);
        optical->addWidget(cdSpeedTurbo, 2, 1, 1, 4);
        root->addWidget(groupBox(QStringLiteral("Optical media options"), optical));

        connect(addDirectoryMountButton, &QPushButton::clicked, this, [this]() { addDirectoryMountDialog(); });
        connect(addHardfileMountButton, &QPushButton::clicked, this, [this]() { addHardfileMountDialog(); });
        connect(addHardDriveMountButton, &QPushButton::clicked, this, [this]() { addHardDriveMountDialog(); });
        connect(addCdMountButton, &QPushButton::clicked, this, [this]() { addCdDriveMountDialog(); });
        connect(addTapeMountButton, &QPushButton::clicked, this, [this]() { addTapeDriveMountDialog(); });
        connect(cdSlotNumber, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (cdSlotUpdating || index < 0 || index >= MaxCdSlots) {
                return;
            }
            storeCurrentCdSlotFromUi();
            currentCdSlot = index;
            loadCdSlotToUi(index);
        });
        connect(cdSlotType, &QComboBox::currentTextChanged, this, [this](const QString &text) {
            if (cdSlotUpdating) {
                return;
            }
            if (text == QStringLiteral("Autodetect") && cdSlotPath->currentText().isEmpty()) {
                setCurrentCdSlotInUse(true);
            }
        });
        connect(cdSlotPath, &QComboBox::currentTextChanged, this, [this](const QString &text) {
            if (cdSlotUpdating) {
                return;
            }
            if (!text.isEmpty()) {
                setComboTextIfChanged(cdSlotType, QStringLiteral("Image file"));
                setCurrentCdSlotInUse(true);
            }
        });
        connect(selectCdImage, &QPushButton::clicked, this, [this]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select CD image"), cdSlotPath->currentText().isEmpty() ? QDir::homePath() : cdSlotPath->currentText(), QStringLiteral("CD images (*.iso *.cue *.ccd *.mds *.chd);;All files (*)"));
            if (!selected.isEmpty()) {
                cdSlotPath->setCurrentText(selected);
                setComboTextIfChanged(cdSlotType, QStringLiteral("Image file"));
                setCurrentCdSlotInUse(true);
            }
        });
        connect(ejectCd, &QPushButton::clicked, this, [this]() {
            cdSlotPath->setCurrentText(QString());
            setComboTextIfChanged(cdSlotType, QStringLiteral("Image file"));
            setCurrentCdSlotInUse(false);
        });
        connect(propertiesMountButton, &QPushButton::clicked, this, [this]() { openSelectedMountProperties(); });
        connect(removeMountButton, &QPushButton::clicked, this, [this]() { removeSelectedMount(); });
        connect(mountedDrives, &QTreeWidget::itemSelectionChanged, this, [this]() { updateMountButtons(); });
        connect(mountedDrives, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *, int) { openSelectedMountProperties(); });
        QShortcut *propertiesShortcut = new QShortcut(QKeySequence(Qt::Key_Return), mountedDrives);
        connect(propertiesShortcut, &QShortcut::activated, this, [this]() { openSelectedMountProperties(); });
        QShortcut *propertiesEnterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), mountedDrives);
        connect(propertiesEnterShortcut, &QShortcut::activated, this, [this]() { openSelectedMountProperties(); });
        QShortcut *removeShortcut = new QShortcut(QKeySequence::Delete, mountedDrives);
        connect(removeShortcut, &QShortcut::activated, this, [this]() { removeSelectedMount(); });
        QShortcut *moveUpShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Up), mountedDrives);
        connect(moveUpShortcut, &QShortcut::activated, this, [this]() { moveSelectedMount(-1); });
        QShortcut *moveDownShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Down), mountedDrives);
        connect(moveDownShortcut, &QShortcut::activated, this, [this]() { moveSelectedMount(1); });
        updateMountButtons();
        return page;
    }

    QWidget *makeExpansionPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);

        rtgMem = combo({ QStringLiteral("None"), QStringLiteral("1 MB"), QStringLiteral("2 MB"), QStringLiteral("4 MB"), QStringLiteral("8 MB"), QStringLiteral("16 MB"), QStringLiteral("32 MB"), QStringLiteral("64 MB"), QStringLiteral("128 MB"), QStringLiteral("256 MB") }, QStringLiteral("None"));
        rtgType = combo({ QStringLiteral("ZorroII"), QStringLiteral("ZorroIII") }, QStringLiteral("ZorroIII"));
        rtgMonitor = combo({ QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("4") }, QStringLiteral("1"));
        rtgScale = new QCheckBox(QStringLiteral("Scale if smaller than display size setting"));
        rtgCenter = new QCheckBox(QStringLiteral("Always center"));
        rtgIntegerScale = new QCheckBox(QStringLiteral("Integer scaling"));
        rtgMultithread = new QCheckBox(QStringLiteral("Multithreaded"));
        rtgHardwareSprite = new QCheckBox(QStringLiteral("Hardware sprite emulation"));
        rtgHardwareVBlank = new QCheckBox(QStringLiteral("Hardware vertical blank interrupt"));
        rtgAutoswitch = new QCheckBox(QStringLiteral("Native/RTG autoswitch"));
        rtgInitialMonitor = new QCheckBox(QStringLiteral("Override initial native chipset display"));
        rtg8Bit = combo({ QStringLiteral("(8bit)"), QStringLiteral("8-bit (*)") }, QStringLiteral("8-bit (*)"));
        rtg16Bit = combo({ QStringLiteral("(15/16bit)"), QStringLiteral("All 15/16-bit"), QStringLiteral("R5G6B5PC (*)"), QStringLiteral("R5G5B5PC"), QStringLiteral("R5G6B5"), QStringLiteral("R5G5B5"), QStringLiteral("B5G6R5PC"), QStringLiteral("B5G5R5PC") }, QStringLiteral("R5G6B5PC (*)"));
        rtg24Bit = combo({ QStringLiteral("(24bit)"), QStringLiteral("All 24-bit"), QStringLiteral("R8G8B8"), QStringLiteral("B8G8R8") }, QStringLiteral("(24bit)"));
        rtg32Bit = combo({ QStringLiteral("(32bit)"), QStringLiteral("All 32-bit"), QStringLiteral("A8R8G8B8"), QStringLiteral("A8B8G8R8"), QStringLiteral("R8G8B8A8"), QStringLiteral("B8G8R8A8 (*)") }, QStringLiteral("B8G8R8A8 (*)"));
        rtgRefreshRate = combo({ QStringLiteral("Chipset"), QStringLiteral("Default"), QStringLiteral("50"), QStringLiteral("60"), QStringLiteral("70"), QStringLiteral("75") }, QStringLiteral("Chipset"));
        rtgRefreshRate->setEditable(true);
        rtgBuffers = combo({ QStringLiteral("Double"), QStringLiteral("Triple") }, QStringLiteral("Double"));

        QGridLayout *rtg = new QGridLayout;
        rtg->setColumnStretch(1, 2);
        rtg->setColumnStretch(3, 1);
        rtg->addWidget(label(QStringLiteral("Board:")), 0, 0);
        rtg->addWidget(rtgType, 0, 1);
        rtg->addWidget(label(QStringLiteral("Monitor:")), 0, 2);
        rtg->addWidget(rtgMonitor, 0, 3);
        rtg->addWidget(label(QStringLiteral("VRAM size:")), 1, 0);
        rtg->addWidget(rtgMem, 1, 1);
        rtg->addWidget(rtgAutoswitch, 2, 0, 1, 2);
        rtg->addWidget(rtgScale, 3, 0, 1, 2);
        rtg->addWidget(rtgCenter, 4, 0, 1, 2);
        rtg->addWidget(rtgIntegerScale, 5, 0, 1, 2);
        rtg->addWidget(rtgMultithread, 3, 2, 1, 2);
        rtg->addWidget(rtgHardwareSprite, 4, 2, 1, 2);
        rtg->addWidget(rtgHardwareVBlank, 5, 2, 1, 2);
        rtg->addWidget(label(QStringLiteral("8-bit:")), 6, 0);
        rtg->addWidget(rtg8Bit, 6, 1);
        rtg->addWidget(label(QStringLiteral("16-bit:")), 6, 2);
        rtg->addWidget(rtg16Bit, 6, 3);
        rtg->addWidget(label(QStringLiteral("24-bit:")), 7, 0);
        rtg->addWidget(rtg24Bit, 7, 1);
        rtg->addWidget(label(QStringLiteral("32-bit:")), 7, 2);
        rtg->addWidget(rtg32Bit, 7, 3);
        rtg->addWidget(label(QStringLiteral("Refresh rate:")), 8, 0);
        rtg->addWidget(rtgRefreshRate, 8, 1);
        rtg->addWidget(label(QStringLiteral("Buffer mode:")), 8, 2);
        rtg->addWidget(rtgBuffers, 8, 3);
        rtg->addWidget(rtgInitialMonitor, 9, 2, 1, 2);
        root->addWidget(groupBox(QStringLiteral("RTG Graphics Card"), rtg));
        root->addWidget(groupBox(QStringLiteral("Expansion boards"), new QVBoxLayout), 1);

        connect(rtgScale, &QCheckBox::toggled, this, [this](bool checked) {
            if (checked) {
                rtgCenter->setChecked(false);
                rtgIntegerScale->setChecked(false);
            }
        });
        connect(rtgCenter, &QCheckBox::toggled, this, [this](bool checked) {
            if (checked) {
                rtgScale->setChecked(false);
                rtgIntegerScale->setChecked(false);
            }
        });
        connect(rtgIntegerScale, &QCheckBox::toggled, this, [this](bool checked) {
            if (checked) {
                rtgScale->setChecked(false);
                rtgCenter->setChecked(false);
            }
        });
        return page;
    }

    QWidget *makeDisplayPage()
    {
        QWidget *page = makePage();
        QHBoxLayout *root = new QHBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);

        QVBoxLayout *left = new QVBoxLayout;
        windowWidth = new QLineEdit(QStringLiteral("720"));
        windowHeight = new QLineEdit(QStringLiteral("568"));
        windowResize = new QCheckBox(QStringLiteral("Window resize"));
        fullscreenResolution = combo({ QStringLiteral("Native"), QStringLiteral("640x480"), QStringLiteral("800x600"), QStringLiteral("1024x768"), QStringLiteral("1280x720"), QStringLiteral("1920x1080") });
        fullscreenResolution->setEditable(true);
        QGridLayout *screen = new QGridLayout;
        screen->addWidget(combo({ QStringLiteral("Default display") }), 0, 0, 1, 3);
        screen->addWidget(label(QStringLiteral("Fullscreen:")), 1, 0);
        screen->addWidget(fullscreenResolution, 1, 1, 1, 2);
        screen->addWidget(label(QStringLiteral("Windowed:")), 2, 0);
        screen->addWidget(windowWidth, 2, 1);
        screen->addWidget(windowHeight, 2, 2);
        screen->addWidget(windowResize, 3, 1, 1, 2);
        left->addWidget(groupBox(QStringLiteral("Screen"), screen));

        nativeMode = combo({ QStringLiteral("Windowed"), QStringLiteral("Fullscreen"), QStringLiteral("Full-window") }, QStringLiteral("Windowed"));
        rtgMode = combo({ QStringLiteral("Windowed"), QStringLiteral("Fullscreen"), QStringLiteral("Full-window") }, QStringLiteral("Windowed"));
        displayResolution = combo({ QStringLiteral("lores"), QStringLiteral("hires"), QStringLiteral("superhires") }, QStringLiteral("hires"));
        displayFlickerFixer = new QCheckBox(QStringLiteral("Remove interlace artifacts"));
        displayLoresSmoothed = new QCheckBox(QStringLiteral("Filtered low resolution"));
        QGridLayout *settings = new QGridLayout;
        settings->addWidget(label(QStringLiteral("Native:")), 0, 0);
        settings->addWidget(nativeMode, 0, 1);
        settings->addWidget(combo({ QStringLiteral("Default"), QStringLiteral("PAL"), QStringLiteral("NTSC") }), 0, 2);
        settings->addWidget(label(QStringLiteral("RTG:")), 1, 0);
        settings->addWidget(rtgMode, 1, 1);
        settings->addWidget(label(QStringLiteral("Resolution:")), 2, 0);
        settings->addWidget(displayResolution, 2, 1);
        settings->addWidget(displayFlickerFixer, 3, 1, 1, 2);
        settings->addWidget(displayLoresSmoothed, 4, 1, 1, 2);
        left->addWidget(groupBox(QStringLiteral("Settings"), settings), 1);

        QVBoxLayout *right = new QVBoxLayout;
        QVBoxLayout *center = new QVBoxLayout;
        displayCenterHorizontal = new QCheckBox(QStringLiteral("Horizontal"));
        displayCenterVertical = new QCheckBox(QStringLiteral("Vertical"));
        center->addWidget(displayCenterHorizontal);
        center->addWidget(displayCenterVertical);
        right->addWidget(groupBox(QStringLiteral("Centering"), center));
        QVBoxLayout *lineMode = new QVBoxLayout;
        displayLineModeButtons = new QButtonGroup(this);
        QRadioButton *singleLine = new QRadioButton(QStringLiteral("Single"));
        QRadioButton *doubleLine = new QRadioButton(QStringLiteral("Double"));
        QRadioButton *scanlines = new QRadioButton(QStringLiteral("Scanlines"));
        QRadioButton *doubleFields = new QRadioButton(QStringLiteral("Double, fields"));
        displayLineModeButtons->addButton(singleLine, 0);
        displayLineModeButtons->addButton(doubleLine, 1);
        displayLineModeButtons->addButton(scanlines, 2);
        displayLineModeButtons->addButton(doubleFields, 5);
        doubleLine->setChecked(true);
        lineMode->addWidget(singleLine);
        lineMode->addWidget(doubleLine);
        lineMode->addWidget(scanlines);
        lineMode->addWidget(doubleFields);
        right->addWidget(groupBox(QStringLiteral("Line mode"), lineMode));
        right->addStretch();

        root->addLayout(left, 3);
        root->addLayout(right, 1);
        return page;
    }

    QWidget *makeSoundPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        root->addWidget(combo({ QStringLiteral("Default audio device") }));

        QHBoxLayout *top = new QHBoxLayout;
        QVBoxLayout *emulation = new QVBoxLayout;
        soundOutputButtons = new QButtonGroup(this);
        QRadioButton *soundDisabled = new QRadioButton(QStringLiteral("Disabled"));
        QRadioButton *soundEmulated = new QRadioButton(QStringLiteral("Disabled, but emulated"));
        QRadioButton *soundEnabled = new QRadioButton(QStringLiteral("Enabled"));
        soundOutputButtons->addButton(soundDisabled, 0);
        soundOutputButtons->addButton(soundEmulated, 1);
        soundOutputButtons->addButton(soundEnabled, 2);
        soundEnabled->setChecked(true);
        soundAutomatic = new QCheckBox(QStringLiteral("Automatic switching"));
        emulation->addWidget(soundDisabled);
        emulation->addWidget(soundEmulated);
        emulation->addWidget(soundEnabled);
        emulation->addSpacing(8);
        emulation->addWidget(soundAutomatic);
        top->addWidget(groupBox(QStringLiteral("Sound Emulation"), emulation), 1);

        QGridLayout *volume = new QGridLayout;
        volume->setColumnStretch(1, 1);
        soundMasterVolume = new QSlider(Qt::Horizontal);
        soundMasterVolume->setRange(0, 100);
        soundMasterVolumeValue = new QLabel;
        soundMasterVolumeValue->setMinimumWidth(44);
        soundVolumeSelect = combo({ QStringLiteral("Paula"), QStringLiteral("CD"), QStringLiteral("AHI"), QStringLiteral("MIDI"), QStringLiteral("Genlock") }, QStringLiteral("Paula"));
        soundSelectedVolume = new QSlider(Qt::Horizontal);
        soundSelectedVolume->setRange(0, 100);
        soundSelectedVolumeValue = new QLabel;
        soundSelectedVolumeValue->setMinimumWidth(44);
        volume->addWidget(label(QStringLiteral("Master")), 0, 0);
        volume->addWidget(soundMasterVolume, 0, 1);
        volume->addWidget(soundMasterVolumeValue, 0, 2);
        volume->addWidget(soundVolumeSelect, 1, 0);
        volume->addWidget(soundSelectedVolume, 1, 1);
        volume->addWidget(soundSelectedVolumeValue, 1, 2);
        top->addWidget(groupBox(QStringLiteral("Volume"), volume), 2);

        QGridLayout *buffer = new QGridLayout;
        buffer->setColumnStretch(0, 1);
        soundBufferSize = new QSlider(Qt::Horizontal);
        soundBufferSize->setRange(0, 10);
        soundBufferSizeValue = new QLabel;
        soundBufferSizeValue->setMinimumWidth(44);
        buffer->addWidget(soundBufferSize, 0, 0);
        buffer->addWidget(soundBufferSizeValue, 0, 1);
        top->addWidget(groupBox(QStringLiteral("Sound Buffer Size"), buffer), 1);
        root->addLayout(top);

        soundChannels = combo(soundChannelItems(), QStringLiteral("Stereo"));
        soundStereoSeparation = combo({});
        for (int i = 10; i >= 0; i--) {
            soundStereoSeparation->addItem(QStringLiteral("%1%").arg(i * 10));
        }
        soundInterpolation = combo({ QStringLiteral("Disabled"), QStringLiteral("Anti"), QStringLiteral("Sinc"), QStringLiteral("RH"), QStringLiteral("Crux") }, QStringLiteral("Anti"));
        soundFrequency = combo({ QStringLiteral("11025"), QStringLiteral("15000"), QStringLiteral("22050"), QStringLiteral("32000"), QStringLiteral("44100"), QStringLiteral("48000") }, QStringLiteral("44100"));
        soundFrequency->setEditable(true);
        soundSwap = combo({ QStringLiteral("-"), QStringLiteral("Paula only"), QStringLiteral("AHI only"), QStringLiteral("Both") }, QStringLiteral("-"));
        soundStereoDelay = combo({ QStringLiteral("-"), QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8"), QStringLiteral("9"), QStringLiteral("10") }, QStringLiteral("-"));
        soundFilter = combo({ QStringLiteral("Always off"), QStringLiteral("Emulated (A500)"), QStringLiteral("Emulated (A1200)"), QStringLiteral("Always on (A500)"), QStringLiteral("Always on (A1200)") }, QStringLiteral("Emulated (A500)"));

        QGridLayout *settings = new QGridLayout;
        settings->setColumnStretch(1, 1);
        settings->setColumnStretch(3, 1);
        settings->setColumnStretch(5, 1);
        settings->addWidget(label(QStringLiteral("Channel mode:")), 0, 0);
        settings->addWidget(soundChannels, 0, 1);
        settings->addWidget(label(QStringLiteral("Stereo separation:")), 0, 2);
        settings->addWidget(soundStereoSeparation, 0, 3);
        settings->addWidget(label(QStringLiteral("Interpolation:")), 0, 4);
        settings->addWidget(soundInterpolation, 0, 5);
        settings->addWidget(label(QStringLiteral("Frequency:")), 1, 0);
        settings->addWidget(soundFrequency, 1, 1);
        settings->addWidget(label(QStringLiteral("Swap channels:")), 1, 2);
        settings->addWidget(soundSwap, 1, 3);
        settings->addWidget(label(QStringLiteral("Stereo delay:")), 1, 4);
        settings->addWidget(soundStereoDelay, 1, 5);
        settings->addWidget(label(QStringLiteral("Audio filter:")), 2, 4);
        settings->addWidget(soundFilter, 2, 5);
        root->addWidget(groupBox(QStringLiteral("Settings"), settings));

        QHBoxLayout *bottom = new QHBoxLayout;
        QGridLayout *floppy = new QGridLayout;
        floppy->setColumnStretch(1, 1);
        floppySoundEmptyVolume = new QSlider(Qt::Horizontal);
        floppySoundEmptyVolume->setRange(0, 100);
        floppySoundEmptyVolumeValue = new QLabel;
        floppySoundEmptyVolumeValue->setMinimumWidth(44);
        floppySoundDiskVolume = new QSlider(Qt::Horizontal);
        floppySoundDiskVolume->setRange(0, 100);
        floppySoundDiskVolumeValue = new QLabel;
        floppySoundDiskVolumeValue->setMinimumWidth(44);
        floppySoundType = combo({ QStringLiteral("No sound"), QStringLiteral("Built-in A500") }, QStringLiteral("No sound"));
        floppySoundDrive = combo({ QStringLiteral("DF0:"), QStringLiteral("DF1:"), QStringLiteral("DF2:"), QStringLiteral("DF3:") }, QStringLiteral("DF0:"));
        floppy->addWidget(label(QStringLiteral("Empty drive")), 0, 0);
        floppy->addWidget(floppySoundEmptyVolume, 0, 1);
        floppy->addWidget(floppySoundEmptyVolumeValue, 0, 2);
        floppy->addWidget(label(QStringLiteral("Disk in drive")), 1, 0);
        floppy->addWidget(floppySoundDiskVolume, 1, 1);
        floppy->addWidget(floppySoundDiskVolumeValue, 1, 2);
        floppy->addWidget(floppySoundType, 2, 0, 1, 2);
        floppy->addWidget(floppySoundDrive, 2, 2);
        bottom->addWidget(groupBox(QStringLiteral("Floppy Drive Sound Emulation"), floppy), 3);

        QVBoxLayout *drivers = new QVBoxLayout;
        QCheckBox *sdlDriver = new QCheckBox(QStringLiteral("SDL"));
        sdlDriver->setChecked(true);
        sdlDriver->setEnabled(false);
        drivers->addWidget(sdlDriver);
        drivers->addStretch();
        bottom->addWidget(groupBox(QStringLiteral("Drivers"), drivers), 1);
        root->addLayout(bottom);

        connect(soundOutputButtons, &QButtonGroup::idClicked, this, [this](int) { updateSoundControlState(); });
        connect(soundMasterVolume, &QSlider::valueChanged, this, [this](int) { updateSoundVolumeLabels(); });
        connect(soundVolumeSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (soundVolumeUpdating) {
                return;
            }
            storeSelectedSoundVolume();
            currentSoundVolume = qBound(0, index, SoundVolumeCount - 1);
            loadSelectedSoundVolume();
        });
        connect(soundSelectedVolume, &QSlider::valueChanged, this, [this](int) { updateSoundVolumeLabels(); });
        connect(soundBufferSize, &QSlider::valueChanged, this, [this](int) { updateSoundBufferLabel(); });
        connect(soundChannels, &QComboBox::currentTextChanged, this, [this](const QString &) { updateSoundControlState(); });
        connect(floppySoundDrive, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (floppySoundUpdating) {
                return;
            }
            storeSelectedFloppySound();
            currentFloppySoundDrive = qBound(0, index, FloppySoundDriveCount - 1);
            loadSelectedFloppySound();
        });
        connect(floppySoundType, &QComboBox::currentTextChanged, this, [this](const QString &) { storeSelectedFloppySound(); });
        connect(floppySoundEmptyVolume, &QSlider::valueChanged, this, [this](int) {
            updateFloppySoundVolumeLabels();
            storeSelectedFloppySound();
        });
        connect(floppySoundDiskVolume, &QSlider::valueChanged, this, [this](int) {
            updateFloppySoundVolumeLabels();
            storeSelectedFloppySound();
        });
        updateSoundControlState();
        return page;
    }

    int soundVolumeAttenuationValue(int index) const
    {
        if (index == currentSoundVolume && soundSelectedVolume) {
            return 100 - soundSelectedVolume->value();
        }
        return soundVolumeAttenuation[qBound(0, index, SoundVolumeCount - 1)];
    }

    int floppySoundTypeConfigValue(int drive) const
    {
        if (drive == currentFloppySoundDrive && floppySoundType) {
            return floppySoundType->currentIndex();
        }
        return floppySoundTypeValue[qBound(0, drive, FloppySoundDriveCount - 1)];
    }

    int floppySoundEmptyAttenuationValue(int drive) const
    {
        if (drive == currentFloppySoundDrive && floppySoundEmptyVolume) {
            return 100 - floppySoundEmptyVolume->value();
        }
        return floppySoundEmptyAttenuation[qBound(0, drive, FloppySoundDriveCount - 1)];
    }

    int floppySoundDiskAttenuationValue(int drive) const
    {
        if (drive == currentFloppySoundDrive && floppySoundDiskVolume) {
            return 100 - floppySoundDiskVolume->value();
        }
        return floppySoundDiskAttenuation[qBound(0, drive, FloppySoundDriveCount - 1)];
    }

    void updateSoundControlState()
    {
        const bool enabled = soundOutputButtons && soundOutputButtons->checkedId() != 0;
        const bool stereo = enabled && soundChannels && soundChannels->currentText() != QStringLiteral("Mono");
        const QList<QWidget *> widgets = {
            soundMasterVolume,
            soundVolumeSelect,
            soundSelectedVolume,
            soundBufferSize,
            soundChannels,
            soundInterpolation,
            soundFrequency,
            soundSwap,
            soundFilter,
            floppySoundDrive,
            floppySoundType,
            floppySoundEmptyVolume,
            floppySoundDiskVolume
        };
        for (QWidget *widget : widgets) {
            if (widget) {
                widget->setEnabled(enabled);
            }
        }
        if (soundStereoSeparation) {
            soundStereoSeparation->setEnabled(stereo);
        }
        if (soundStereoDelay) {
            soundStereoDelay->setEnabled(stereo);
        }
        updateSoundVolumeLabels();
        updateSoundBufferLabel();
        updateFloppySoundVolumeLabels();
    }

    void updateSoundVolumeLabels()
    {
        if (soundMasterVolumeValue && soundMasterVolume) {
            soundMasterVolumeValue->setText(QStringLiteral("%1%").arg(soundMasterVolume->value()));
        }
        if (soundSelectedVolumeValue && soundSelectedVolume) {
            soundSelectedVolumeValue->setText(QStringLiteral("%1%").arg(soundSelectedVolume->value()));
        }
    }

    void updateSoundBufferLabel()
    {
        if (!soundBufferSizeValue || !soundBufferSize) {
            return;
        }
        const int index = soundBufferSize->value();
        soundBufferSizeValue->setText(index <= 0 ? QStringLiteral("Min") : QString::number(index));
    }

    void storeSelectedSoundVolume()
    {
        if (!soundSelectedVolume || soundVolumeUpdating) {
            return;
        }
        soundVolumeAttenuation[qBound(0, currentSoundVolume, SoundVolumeCount - 1)] = 100 - soundSelectedVolume->value();
    }

    void loadSelectedSoundVolume()
    {
        if (!soundSelectedVolume) {
            return;
        }
        QSignalBlocker blocker(soundSelectedVolume);
        soundVolumeUpdating = true;
        soundSelectedVolume->setValue(100 - soundVolumeAttenuation[qBound(0, currentSoundVolume, SoundVolumeCount - 1)]);
        soundVolumeUpdating = false;
        updateSoundVolumeLabels();
    }

    void updateFloppySoundVolumeLabels()
    {
        if (floppySoundEmptyVolumeValue && floppySoundEmptyVolume) {
            floppySoundEmptyVolumeValue->setText(QStringLiteral("%1%").arg(floppySoundEmptyVolume->value()));
        }
        if (floppySoundDiskVolumeValue && floppySoundDiskVolume) {
            floppySoundDiskVolumeValue->setText(QStringLiteral("%1%").arg(floppySoundDiskVolume->value()));
        }
    }

    void storeSelectedFloppySound()
    {
        if (!floppySoundType || !floppySoundEmptyVolume || !floppySoundDiskVolume || floppySoundUpdating) {
            return;
        }
        const int drive = qBound(0, currentFloppySoundDrive, FloppySoundDriveCount - 1);
        floppySoundTypeValue[drive] = floppySoundType->currentIndex();
        floppySoundEmptyAttenuation[drive] = 100 - floppySoundEmptyVolume->value();
        floppySoundDiskAttenuation[drive] = 100 - floppySoundDiskVolume->value();
    }

    void loadSelectedFloppySound()
    {
        if (!floppySoundType || !floppySoundEmptyVolume || !floppySoundDiskVolume) {
            return;
        }
        const int drive = qBound(0, currentFloppySoundDrive, FloppySoundDriveCount - 1);
        floppySoundUpdating = true;
        floppySoundType->setCurrentIndex(qBound(0, floppySoundTypeValue[drive], 1));
        floppySoundEmptyVolume->setValue(100 - floppySoundEmptyAttenuation[drive]);
        floppySoundDiskVolume->setValue(100 - floppySoundDiskAttenuation[drive]);
        floppySoundUpdating = false;
        updateFloppySoundVolumeLabels();
    }

    void setSoundSwapBit(bool paula, bool enabled)
    {
        if (!soundSwap) {
            return;
        }
        const int index = soundSwap->currentIndex();
        bool paulaEnabled = (index & 1) != 0;
        bool ahiEnabled = (index & 2) != 0;
        if (paula) {
            paulaEnabled = enabled;
        } else {
            ahiEnabled = enabled;
        }
        soundSwap->setCurrentText(soundSwapText(paulaEnabled, ahiEnabled));
    }

    QWidget *makeGamePortsPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        portDevice[0] = combo(primaryPortDeviceItems(), QStringLiteral("Mouse"));
        portDevice[1] = combo(primaryPortDeviceItems(), QStringLiteral("Keyboard Layout A"));
        portDevice[2] = combo(parallelPortDeviceItems(), QStringLiteral("<None>"));
        portDevice[3] = combo(parallelPortDeviceItems(), QStringLiteral("<None>"));
        for (int i = 0; i < 2; i++) {
            portAutofire[i] = combo({
                QStringLiteral("No autofire (normal)"),
                QStringLiteral("Autofire"),
                QStringLiteral("Autofire (toggle)"),
                QStringLiteral("Autofire (always)"),
                QStringLiteral("No autofire (toggle)")
            }, QStringLiteral("No autofire (normal)"));
            portMode[i] = combo({
                QStringLiteral("Default"),
                QStringLiteral("Wheel Mouse"),
                QStringLiteral("Mouse"),
                QStringLiteral("Joystick"),
                QStringLiteral("Gamepad"),
                QStringLiteral("Analog joystick"),
                QStringLiteral("CDTV remote mouse"),
                QStringLiteral("CD32 pad"),
                QStringLiteral("Generic light pen/gun")
            }, QStringLiteral("Default"));
        }
        portAutoswitch = new QCheckBox(QStringLiteral("Mouse/Joystick autoswitching"));

        QGridLayout *ports = new QGridLayout;
        ports->setColumnStretch(1, 1);
        ports->addWidget(label(QStringLiteral("Port 1:")), 0, 0);
        ports->addWidget(portDevice[0], 0, 1, 1, 3);
        ports->addWidget(portAutofire[0], 1, 1);
        ports->addWidget(portMode[0], 1, 2);
        ports->addWidget(new QPushButton(QStringLiteral("Remap / Test")), 1, 3);
        ports->addWidget(label(QStringLiteral("Port 2:")), 2, 0);
        ports->addWidget(portDevice[1], 2, 1, 1, 3);
        ports->addWidget(portAutofire[1], 3, 1);
        ports->addWidget(portMode[1], 3, 2);
        ports->addWidget(new QPushButton(QStringLiteral("Remap / Test")), 3, 3);

        QPushButton *swapPorts = new QPushButton(QStringLiteral("Swap ports"));
        ports->addWidget(swapPorts, 4, 1);
        ports->addWidget(portAutoswitch, 4, 2, 1, 2);
        ports->addWidget(label(QStringLiteral("Emulated parallel port joystick adapter")), 5, 0, 1, 4, Qt::AlignLeft | Qt::AlignVCenter);
        ports->addWidget(label(QStringLiteral("Port 1:")), 6, 0);
        ports->addWidget(portDevice[2], 6, 1, 1, 3);
        ports->addWidget(new QPushButton(QStringLiteral("Remap / Test")), 7, 3);
        ports->addWidget(label(QStringLiteral("Port 2:")), 8, 0);
        ports->addWidget(portDevice[3], 8, 1, 1, 3);
        ports->addWidget(new QPushButton(QStringLiteral("Remap / Test")), 9, 3);
        root->addWidget(groupBox(QStringLiteral("Mouse and Joystick settings"), ports));

        connect(swapPorts, &QPushButton::clicked, this, [this]() {
            const QString port0Text = portDevice[0]->currentText();
            const QString port0Autofire = portAutofire[0]->currentText();
            const QString port0Mode = portMode[0]->currentText();
            portDevice[0]->setCurrentText(portDevice[1]->currentText());
            portAutofire[0]->setCurrentText(portAutofire[1]->currentText());
            portMode[0]->setCurrentText(portMode[1]->currentText());
            portDevice[1]->setCurrentText(port0Text);
            portAutofire[1]->setCurrentText(port0Autofire);
            portMode[1]->setCurrentText(port0Mode);
        });

        QGridLayout *mouse = new QGridLayout;
        mouse->setColumnStretch(1, 1);
        mouseSpeed = new QSpinBox;
        mouseSpeed->setRange(1, 1000);
        mouseSpeed->setValue(100);
        virtualMouseDriver = new QCheckBox(QStringLiteral("Install virtual mouse driver"));
        mouseUntrapMode = combo({
            QStringLiteral("None (Alt-Tab)"),
            QStringLiteral("Middle button"),
            QStringLiteral("Magic mouse"),
            QStringLiteral("Both")
        }, QStringLiteral("Middle button"));
        magicMouseCursor = combo({
            QStringLiteral("Show both cursors"),
            QStringLiteral("Show native cursor only"),
            QStringLiteral("Show host cursor only")
        }, QStringLiteral("Show both cursors"));
        tabletLibrary = new QCheckBox(QStringLiteral("Tablet.library emulation"));
        tabletMode = combo({ QStringLiteral("-"), QStringLiteral("Tablet emulation") }, QStringLiteral("-"));

        mouse->addWidget(label(QStringLiteral("Mouse speed:")), 0, 0);
        mouse->addWidget(mouseSpeed, 0, 1);
        mouse->addWidget(label(QStringLiteral("Mouse untrap mode:")), 0, 2);
        mouse->addWidget(mouseUntrapMode, 0, 3);
        mouse->addWidget(virtualMouseDriver, 1, 0, 1, 2);
        mouse->addWidget(label(QStringLiteral("Magic Mouse cursor mode:")), 1, 2);
        mouse->addWidget(magicMouseCursor, 1, 3);
        mouse->addWidget(tabletLibrary, 2, 0, 1, 2);
        mouse->addWidget(label(QStringLiteral("Tablet mode:")), 2, 2);
        mouse->addWidget(tabletMode, 2, 3);
        root->addWidget(groupBox(QStringLiteral("Mouse extra settings"), mouse), 1);

        connect(virtualMouseDriver, &QCheckBox::toggled, this, [this](bool) { updateMouseExtraState(); });
        connect(tabletMode, &QComboBox::currentTextChanged, this, [this](const QString &) { updateMouseExtraState(); });
        updateMouseExtraState();
        return page;
    }

    void updateMouseExtraState()
    {
        const bool tabletEnabled = virtualMouseDriver && (virtualMouseDriver->isChecked() || (tabletMode && tabletMode->currentText() == QStringLiteral("Tablet emulation")));
        if (magicMouseCursor) {
            magicMouseCursor->setEnabled(tabletEnabled);
        }
        if (tabletLibrary) {
            tabletLibrary->setEnabled(tabletEnabled);
            if (!tabletEnabled) {
                tabletLibrary->setChecked(false);
            }
        }
        if (tabletMode) {
            tabletMode->setEnabled(tabletEnabled || (virtualMouseDriver && virtualMouseDriver->isChecked()));
        }
    }

    void setMouseUntrapBit(bool middle, bool enabled)
    {
        if (!mouseUntrapMode) {
            return;
        }
        const int index = mouseUntrapMode->currentIndex();
        bool middleEnabled = index == 1 || index == 3;
        bool magicEnabled = index == 2 || index == 3;
        if (middle) {
            middleEnabled = enabled;
        } else {
            magicEnabled = enabled;
        }
        const int nextIndex = (middleEnabled ? 1 : 0) + (magicEnabled ? 2 : 0);
        mouseUntrapMode->setCurrentIndex(qBound(0, nextIndex, 3));
    }

    QWidget *makeInputPage()
    {
        QWidget *page = makePage();
        QHBoxLayout *root = new QHBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        QListWidget *devices = new QListWidget;
        devices->addItems({ QStringLiteral("Keyboard"), QStringLiteral("Mouse") });
        root->addWidget(devices, 1);
        QVBoxLayout *right = new QVBoxLayout;
        right->addWidget(combo({ QStringLiteral("Configuration #1"), QStringLiteral("Configuration #2") }));
        right->addWidget(new QTableWidget(8, 3), 1);
        right->addWidget(new QPushButton(QStringLiteral("Remap")));
        root->addLayout(right, 2);
        return page;
    }

    QWidget *makePathsPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        QGridLayout *paths = new QGridLayout;
        paths->setColumnStretch(1, 1);
        emulatorPath = new QLineEdit;
        romsPath = new QLineEdit;
        configsPath = new QLineEdit;
        addLineBrowseRow(paths, 0, QStringLiteral("Emulator executable:"), emulatorPath);
        addLineBrowseRow(paths, 1, QStringLiteral("System ROMs:"), romsPath, true);
        addLineBrowseRow(paths, 2, QStringLiteral("Configuration files:"), configsPath, true);
        addLineBrowseRow(paths, 3, QStringLiteral("NVRAM files:"), new QLineEdit, true);
        addLineBrowseRow(paths, 4, QStringLiteral("Screenshots:"), new QLineEdit, true);
        addLineBrowseRow(paths, 5, QStringLiteral("State files:"), new QLineEdit, true);
        addLineBrowseRow(paths, 6, QStringLiteral("Videos:"), new QLineEdit, true);
        root->addLayout(paths);

        QHBoxLayout *actions = new QHBoxLayout;
        actions->addWidget(new QPushButton(QStringLiteral("Set Path")));
        actions->addWidget(combo({ QStringLiteral("System ROMs"), QStringLiteral("Configuration files"), QStringLiteral("State files") }));
        actions->addStretch();
        actions->addWidget(new QCheckBox(QStringLiteral("Use relative paths")));
        actions->addWidget(new QCheckBox(QStringLiteral("Portable mode")));
        root->addLayout(actions);

        QGridLayout *logging = new QGridLayout;
        logging->addWidget(combo({ QStringLiteral("winuaelog.txt"), QStringLiteral("config.log") }), 0, 0);
        logging->addWidget(new QCheckBox(QStringLiteral("Enable full logging")), 0, 1);
        logging->addWidget(new QCheckBox(QStringLiteral("Log window")), 0, 2);
        logging->addWidget(new QPushButton(QStringLiteral("Save All")), 0, 3);
        logging->addWidget(new QLineEdit, 1, 0, 1, 4);
        root->addWidget(groupBox(QStringLiteral("Debug logging"), logging), 1);
        return page;
    }

    QWidget *makeMiscPage()
    {
        QWidget *page = makePage();
        QHBoxLayout *root = new QHBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        QListWidget *misc = new QListWidget;
        misc->addItems({ QStringLiteral("Logging"), QStringLiteral("State files"), QStringLiteral("GUI"), QStringLiteral("Keyboard LEDs") });
        root->addWidget(misc, 2);
        QVBoxLayout *right = new QVBoxLayout;
        right->addWidget(groupBox(QStringLiteral("Miscellaneous Options"), new QVBoxLayout));
        right->addWidget(groupBox(QStringLiteral("GUI"), new QVBoxLayout));
        right->addWidget(groupBox(QStringLiteral("State Files"), new QVBoxLayout));
        root->addLayout(right, 1);
        return page;
    }

    QWidget *makeAboutPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        root->addStretch();
        QLabel *title = new QLabel(QStringLiteral("WinUAE"));
        QFont font = title->font();
        font.setPointSize(22);
        font.setBold(true);
        title->setFont(font);
        title->setAlignment(Qt::AlignCenter);
        root->addWidget(title);
        QLabel *version = new QLabel(versionString());
        QFont versionFont = version->font();
        versionFont.setPointSize(13);
        version->setFont(versionFont);
        version->setAlignment(Qt::AlignCenter);
        root->addWidget(version);
        QLabel *subtitle = new QLabel(QStringLiteral("Unix Qt configuration frontend"));
        subtitle->setAlignment(Qt::AlignCenter);
        root->addWidget(subtitle);
        QPushButton *contributors = new QPushButton(QStringLiteral("Contributors"));
        contributors->setFixedWidth(120);
        connect(contributors, &QPushButton::clicked, this, [this]() { showContributors(); });
        QHBoxLayout *buttonRow = new QHBoxLayout;
        buttonRow->addStretch();
        buttonRow->addWidget(contributors);
        buttonRow->addStretch();
        root->addLayout(buttonRow);
        root->addStretch();
        return page;
    }

    void showContributors()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("UAE Authors and Contributors..."));
        dialog.resize(620, 420);

        QVBoxLayout *root = new QVBoxLayout(&dialog);
        QListWidget *list = new QListWidget;
        list->addItems(contributorLines());
        root->addWidget(list, 1);

        QPushButton *ok = new QPushButton(QStringLiteral("Ok"));
        ok->setDefault(true);
        connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);

        QHBoxLayout *buttons = new QHBoxLayout;
        buttons->addStretch();
        buttons->addWidget(ok);
        buttons->addStretch();
        root->addLayout(buttons);

        dialog.exec();
    }

    void addPathRow(QGridLayout *layout, int row, const QString &caption, QComboBox *field, const QString &dialogTitle, const QString &filter)
    {
        QPushButton *browse = smallButton(QStringLiteral("..."));
        layout->addWidget(new QLabel(caption), row * 2, 0, 1, 2);
        layout->addWidget(field, row * 2 + 1, 0, 1, 2);
        layout->addWidget(browse, row * 2 + 1, 2);
        connect(browse, &QPushButton::clicked, this, [this, field, dialogTitle, filter]() {
            addBrowse(field, this, dialogTitle, filter);
        });
    }

    void addLineBrowseRow(QGridLayout *layout, int row, const QString &caption, QLineEdit *field, bool directory = false)
    {
        QPushButton *browse = smallButton(QStringLiteral("..."));
        layout->addWidget(label(caption), row, 0);
        layout->addWidget(field, row, 1);
        layout->addWidget(browse, row, 2);
        connect(browse, &QPushButton::clicked, this, [this, field, directory, caption]() {
            QString path;
            if (directory) {
                path = QFileDialog::getExistingDirectory(this, caption, field->text());
            } else {
                path = QFileDialog::getOpenFileName(this, caption, field->text(), QStringLiteral("All files (*)"));
            }
            if (!path.isEmpty()) {
                field->setText(path);
            }
        });
    }

    void ensureCdSlots()
    {
        if (cdSlots.size() == MaxCdSlots) {
            return;
        }
        cdSlots = QVector<WinUaeQtCdSlot>(MaxCdSlots);
        for (WinUaeQtCdSlot &slot : cdSlots) {
            slot.type = QStringLiteral("Image file");
        }
    }

    void clearCdSlots()
    {
        ensureCdSlots();
        for (WinUaeQtCdSlot &slot : cdSlots) {
            slot = WinUaeQtCdSlot();
            slot.type = QStringLiteral("Image file");
        }
        currentCdSlot = 0;
        loadCdSlotToUi(currentCdSlot);
    }

    void setCurrentCdSlotInUse(bool inUse)
    {
        ensureCdSlots();
        if (currentCdSlot >= 0 && currentCdSlot < cdSlots.size()) {
            cdSlots[currentCdSlot].inUse = inUse;
        }
    }

    void storeCurrentCdSlotFromUi()
    {
        ensureCdSlots();
        if (!cdSlotPath || !cdSlotType || currentCdSlot < 0 || currentCdSlot >= cdSlots.size()) {
            return;
        }
        WinUaeQtCdSlot &slot = cdSlots[currentCdSlot];
        slot.path = cdSlotPath->currentText().trimmed();
        slot.type = cdSlotType->currentText();
        if (!slot.path.isEmpty()) {
            slot.inUse = true;
        } else if (slot.type == QStringLiteral("Image file")) {
            slot.inUse = false;
        }
    }

    WinUaeQtCdSlot cdSlotState(int index) const
    {
        WinUaeQtCdSlot slot = index >= 0 && index < cdSlots.size() ? cdSlots.value(index) : WinUaeQtCdSlot();
        if (index == currentCdSlot && cdSlotPath && cdSlotType) {
            slot.path = cdSlotPath->currentText().trimmed();
            slot.type = cdSlotType->currentText();
            if (!slot.path.isEmpty()) {
                slot.inUse = true;
            } else if (slot.type == QStringLiteral("Image file")) {
                slot.inUse = false;
            }
        }
        return slot;
    }

    void loadCdSlotToUi(int index)
    {
        ensureCdSlots();
        if (!cdSlotNumber || !cdSlotPath || !cdSlotType || index < 0 || index >= cdSlots.size()) {
            return;
        }
        cdSlotUpdating = true;
        QSignalBlocker numberBlocker(cdSlotNumber);
        QSignalBlocker pathBlocker(cdSlotPath);
        QSignalBlocker typeBlocker(cdSlotType);
        cdSlotNumber->setCurrentIndex(index);
        cdSlotPath->setCurrentText(cdSlots[index].path);
        cdSlotType->setCurrentText(cdSlots[index].type.isEmpty() ? QStringLiteral("Image file") : cdSlots[index].type);
        cdSlotUpdating = false;
    }

    void ensureCustomRomBoards()
    {
        if (customRomBoards.size() == MaxRomBoards) {
            return;
        }
        customRomBoards.clear();
        customRomBoards.resize(MaxRomBoards);
        currentCustomRomBoard = 0;
    }

    void clearCustomRomBoards()
    {
        ensureCustomRomBoards();
        for (WinUaeQtRomBoard &board : customRomBoards) {
            board = WinUaeQtRomBoard();
        }
        currentCustomRomBoard = 0;
        if (customRomSelect) {
            QSignalBlocker blocker(customRomSelect);
            customRomSelect->setCurrentIndex(0);
        }
        loadCurrentCustomRomBoard();
    }

    void storeCurrentCustomRomBoard()
    {
        if (customRomUpdating || !customRomStart || !customRomEnd || !customRomFile) {
            return;
        }
        ensureCustomRomBoards();
        if (currentCustomRomBoard < 0 || currentCustomRomBoard >= customRomBoards.size()) {
            return;
        }
        WinUaeQtRomBoard &board = customRomBoards[currentCustomRomBoard];
        board.start = normalizedRomAddress(customRomStart->text(), false);
        board.end = normalizedRomAddress(customRomEnd->text(), true);
        board.path = customRomFile->text().trimmed();
    }

    void loadCurrentCustomRomBoard()
    {
        if (!customRomStart || !customRomEnd || !customRomFile) {
            return;
        }
        ensureCustomRomBoards();
        currentCustomRomBoard = qBound(0, currentCustomRomBoard, MaxRomBoards - 1);
        customRomUpdating = true;
        QSignalBlocker startBlocker(customRomStart);
        QSignalBlocker endBlocker(customRomEnd);
        QSignalBlocker fileBlocker(customRomFile);
        if (customRomSelect) {
            QSignalBlocker selectBlocker(customRomSelect);
            customRomSelect->setCurrentIndex(currentCustomRomBoard);
        }
        const WinUaeQtRomBoard &board = customRomBoards[currentCustomRomBoard];
        customRomStart->setText(board.start);
        customRomEnd->setText(board.end);
        customRomFile->setText(board.path);
        customRomUpdating = false;
    }

    void applyCustomRomBoard(int index, const QString &value)
    {
        if (index < 0 || index >= MaxRomBoards) {
            return;
        }
        ensureCustomRomBoards();
        customRomBoards[index] = romBoardFromConfigValue(value);
        if (index == currentCustomRomBoard) {
            loadCurrentCustomRomBoard();
        }
    }

    void updateFloppySpeedLabel()
    {
        if (!floppySpeed || !floppySpeedLabel) {
            return;
        }
        floppySpeedLabel->setText(floppySpeedText(floppySpeedConfigValue(floppySpeed->value())));
    }

    void updateCpuSpeedLabel()
    {
        if (!cpuSpeed || !cpuSpeedLabel) {
            return;
        }
        const int value = cpuSpeed->value() * 10;
        cpuSpeedLabel->setText(QStringLiteral("%1%2%").arg(value >= 0 ? QStringLiteral("+") : QString()).arg(value));
    }

    void updateJitCacheLabel()
    {
        if (!jitCache || !jitCacheLabel) {
            return;
        }
        jitCacheLabel->setText(jitCacheText(jit->isChecked() ? jitCacheSizeFromPosition(jitCache->value()) : 0));
    }

    void updateCpuControlState()
    {
        const int cpu = selectedCpuModel();
        const int fpu = fpuModelConfigValue(cpu);
        bool jitEnabled = jit && jit->isChecked();
        if (cpu24Bit) {
            cpu24Bit->setEnabled(cpu <= 68030);
        }
        if (jit) {
            jit->setEnabled(cpu >= 68020 && !cpu24Bit->isChecked());
            if (!jit->isEnabled() && jit->isChecked()) {
                jit->setChecked(false);
            }
            jitEnabled = jit->isChecked();
        }
        if (cpuDataCache) {
            cpuDataCache->setEnabled(cpu >= 68030 && moreCompatible->isChecked() && !jitEnabled);
        }
        if (cpuUnimplemented) {
            cpuUnimplemented->setEnabled(cpu == 68060 && !jitEnabled);
        }
        if (mmuButtons) {
            const bool mmuEnabled = cpu >= 68030 && !jitEnabled;
            for (QAbstractButton *button : mmuButtons->buttons()) {
                button->setEnabled(button == mmuButtons->button(0) || mmuEnabled);
            }
            if (!mmuEnabled && mmuButtons->checkedId() != 0) {
                mmuButtons->button(0)->setChecked(true);
            }
        }
        const bool hasFpu = fpu != 0;
        if (fpuStrict) {
            fpuStrict->setEnabled(hasFpu);
        }
        if (fpuUnimplemented) {
            fpuUnimplemented->setEnabled(hasFpu && !jitEnabled);
        }
        if (fpuMode) {
            fpuMode->setEnabled(hasFpu);
        }
        const bool jitOptions = jitEnabled && cpu >= 68020 && !cpu24Bit->isChecked();
        for (QWidget *widget : { static_cast<QWidget *>(jitCache), static_cast<QWidget *>(jitCacheLabel), static_cast<QWidget *>(jitConstJump), static_cast<QWidget *>(jitHardFlush), static_cast<QWidget *>(jitNoFlags), static_cast<QWidget *>(jitCatchFault) }) {
            if (widget) {
                widget->setEnabled(jitOptions);
            }
        }
        if (jitFpu) {
            jitFpu->setEnabled(jitOptions && hasFpu);
        }
        if (jitTrust) {
            for (QAbstractButton *button : jitTrust->buttons()) {
                button->setEnabled(jitOptions);
            }
        }
        updateJitCacheLabel();
    }

    void resetDefaults()
    {
        loadedConfig = WinUaeQtConfig();

        const QString appDirExe = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("winuae_unix"));
        const QString buildDirExe = QDir(QString::fromUtf8(WINUAE_UNIX_BUILD_DIR)).filePath(QStringLiteral("winuae_unix"));
        if (QFileInfo::exists(appDirExe)) {
            emulatorPath->setText(appDirExe);
        } else {
            emulatorPath->setText(buildDirExe);
        }

        if (configName) {
            configName->setCurrentText(QStringLiteral("A1200 Install"));
        }
        if (configDescription) {
            configDescription->setText(QStringLiteral("A1200, 68020, AGA, 2 MB Chip RAM"));
        }
        if (configPath) {
            configPath->clear();
        }

        quickModel->setCurrentText(QStringLiteral("A1200"));
        quickConfiguration->setCurrentText(QStringLiteral("Expanded configuration"));
        quickHostConfiguration->setCurrentText(QStringLiteral("Default"));
        compatibility->setValue(1);
        ntsc->setChecked(false);

        applyModelPreset(QStringLiteral("A1200"));
        setFpuButton(0);
        moreCompatible->setChecked(false);
        cpuDataCache->setChecked(false);
        cpuUnimplemented->setChecked(true);
        if (QAbstractButton *button = mmuButtons->button(0)) {
            button->setChecked(true);
        }
        fpuStrict->setChecked(false);
        fpuUnimplemented->setChecked(true);
        fpuMode->setCurrentText(QStringLiteral("Host (64-bit)"));
        if (QAbstractButton *button = cpuSpeedButtons->button(0)) {
            button->setChecked(true);
        }
        cpuSpeed->setValue(0);
        updateCpuSpeedLabel();
        cpuFrequency->setCurrentText(QStringLiteral("4x (A1200)"));
        cpuFrequencyCustom->clear();
        jit->setChecked(false);
        jitCache->setValue(jitCachePositionFromSize(8192));
        jitFpu->setChecked(false);
        jitConstJump->setChecked(true);
        jitHardFlush->setChecked(false);
        if (QAbstractButton *button = jitTrust->button(0)) {
            button->setChecked(true);
        }
        jitNoFlags->setChecked(false);
        jitCatchFault->setChecked(true);
        updateCpuControlState();
        chipMem->setCurrentText(QStringLiteral("2 MB"));
        z2Fast->setCurrentText(QStringLiteral("None"));
        slowMem->setCurrentText(QStringLiteral("None"));
        z3Fast->setCurrentText(QStringLiteral("None"));
        rtgMem->setCurrentText(QStringLiteral("None"));
        rtgType->setCurrentText(QStringLiteral("ZorroIII"));
        rtgMonitor->setCurrentText(QStringLiteral("1"));
        rtgScale->setChecked(true);
        rtgCenter->setChecked(false);
        rtgIntegerScale->setChecked(false);
        rtgMultithread->setChecked(false);
        rtgHardwareSprite->setChecked(true);
        rtgHardwareVBlank->setChecked(false);
        rtgAutoswitch->setChecked(true);
        rtgInitialMonitor->setChecked(false);
        rtg8Bit->setCurrentText(QStringLiteral("8-bit (*)"));
        rtg16Bit->setCurrentText(QStringLiteral("R5G6B5PC (*)"));
        rtg24Bit->setCurrentText(QStringLiteral("(24bit)"));
        rtg32Bit->setCurrentText(QStringLiteral("B8G8R8A8 (*)"));
        rtgRefreshRate->setCurrentText(QStringLiteral("Chipset"));
        rtgBuffers->setCurrentText(QStringLiteral("Double"));

        romFile->setCurrentText(envString("WINUAE_KICKSTART_ROM"));
        extendedRomFile->setCurrentText(QString());
        cartFile->setCurrentText(QString());
        flashFile->clear();
        rtcFile->clear();
        mapRom->setChecked(false);
        kickShifter->setChecked(false);
        uaeBoardType->setCurrentText(QStringLiteral("Original UAE (FS + F0 ROM)"));
        clearCustomRomBoards();

        for (int i = 0; i < 4; i++) {
            dfEnable[i]->setChecked(i == 0);
            dfType[i]->setCurrentText(QStringLiteral("3.5 DD"));
            dfWriteProtect[i]->setChecked(false);
            dfPath[i]->setCurrentText(i == 0 ? envString("WINUAE_FLOPPY0") : QString());
        }
        floppySpeed->setValue(floppySpeedSliderPosition(100));
        updateFloppySpeedLabel();
        for (int i = 0; i < 2; i++) {
            quickDfEnable[i]->setChecked(i == 0);
            quickDfType[i]->setCurrentText(QStringLiteral("3.5 DD"));
            quickDfWriteProtect[i]->setChecked(false);
            quickDfPath[i]->setCurrentText(i == 0 ? envString("WINUAE_FLOPPY0") : QString());
        }
        if (mountedDrives) {
            mountedDrives->clear();
            updateMountButtons();
        }
        clearCdSlots();

        windowWidth->setText(QStringLiteral("720"));
        windowHeight->setText(QStringLiteral("568"));
        windowResize->setChecked(true);
        fullscreenResolution->setProperty("winuae_width", QString());
        fullscreenResolution->setProperty("winuae_height", QString());
        fullscreenResolution->setCurrentText(QStringLiteral("Native"));
        nativeMode->setCurrentText(QStringLiteral("Windowed"));
        rtgMode->setCurrentText(QStringLiteral("Windowed"));
        displayResolution->setCurrentText(QStringLiteral("hires"));
        displayCenterHorizontal->setChecked(false);
        displayCenterVertical->setChecked(false);
        displayFlickerFixer->setChecked(false);
        displayLoresSmoothed->setChecked(false);
        if (QAbstractButton *button = displayLineModeButtons->button(1)) {
            button->setChecked(true);
        }
        if (QAbstractButton *button = soundOutputButtons->button(2)) {
            button->setChecked(true);
        }
        soundAutomatic->setChecked(false);
        soundMasterVolume->setValue(100);
        for (int i = 0; i < SoundVolumeCount; i++) {
            soundVolumeAttenuation[i] = 0;
        }
        currentSoundVolume = 0;
        soundVolumeSelect->setCurrentIndex(0);
        loadSelectedSoundVolume();
        soundBufferSize->setValue(soundBufferIndexFromSize(16384));
        soundChannels->setCurrentText(QStringLiteral("Stereo"));
        soundStereoSeparation->setCurrentText(QStringLiteral("70%"));
        soundInterpolation->setCurrentText(QStringLiteral("Anti"));
        soundFrequency->setCurrentText(QStringLiteral("44100"));
        soundSwap->setCurrentText(QStringLiteral("-"));
        soundStereoDelay->setCurrentText(QStringLiteral("-"));
        soundFilter->setCurrentText(QStringLiteral("Emulated (A500)"));
        for (int i = 0; i < FloppySoundDriveCount; i++) {
            floppySoundTypeValue[i] = 0;
            floppySoundEmptyAttenuation[i] = 33;
            floppySoundDiskAttenuation[i] = 33;
        }
        currentFloppySoundDrive = 0;
        floppySoundDrive->setCurrentIndex(0);
        loadSelectedFloppySound();
        updateSoundControlState();
        cdSpeedTurbo->setChecked(false);
        portDevice[0]->setCurrentText(QStringLiteral("Mouse"));
        portDevice[1]->setCurrentText(QStringLiteral("Keyboard Layout A"));
        portDevice[2]->setCurrentText(QStringLiteral("<None>"));
        portDevice[3]->setCurrentText(QStringLiteral("<None>"));
        for (int i = 0; i < 2; i++) {
            portAutofire[i]->setCurrentText(QStringLiteral("No autofire (normal)"));
            portMode[i]->setCurrentText(QStringLiteral("Default"));
        }
        portAutoswitch->setChecked(true);
        mouseSpeed->setValue(100);
        mouseUntrapMode->setCurrentText(QStringLiteral("Middle button"));
        magicMouseCursor->setCurrentText(QStringLiteral("Show both cursors"));
        virtualMouseDriver->setChecked(false);
        tabletMode->setCurrentText(QStringLiteral("-"));
        tabletLibrary->setChecked(false);
        updateMouseExtraState();
        romsPath->setText(QDir::homePath());
        configsPath->setText(QDir::homePath());
        status->setText(QStringLiteral("Ready"));
    }

    void applyModelPreset(const QString &model)
    {
        if (model == QStringLiteral("A1200")) {
            chipset->setCurrentText(QStringLiteral("AGA"));
            chipsetCompatible->setCurrentText(QStringLiteral("A1200"));
            setCpuButton(68020);
            cpu24Bit->setChecked(false);
            chipMem->setCurrentText(QStringLiteral("2 MB"));
        } else if (model == QStringLiteral("A4000")) {
            chipset->setCurrentText(QStringLiteral("AGA"));
            chipsetCompatible->setCurrentText(QStringLiteral("A4000"));
            setCpuButton(68040);
            cpu24Bit->setChecked(false);
            chipMem->setCurrentText(QStringLiteral("2 MB"));
        } else if (model == QStringLiteral("A600")) {
            chipset->setCurrentText(QStringLiteral("ECS"));
            chipsetCompatible->setCurrentText(QStringLiteral("A600"));
            setCpuButton(68000);
            cpu24Bit->setChecked(true);
            chipMem->setCurrentText(QStringLiteral("2 MB"));
        } else if (model == QStringLiteral("A500+")) {
            chipset->setCurrentText(QStringLiteral("ECS"));
            chipsetCompatible->setCurrentText(QStringLiteral("A500+"));
            setCpuButton(68000);
            cpu24Bit->setChecked(true);
            chipMem->setCurrentText(QStringLiteral("1 MB"));
        } else {
            chipset->setCurrentText(QStringLiteral("OCS"));
            chipsetCompatible->setCurrentText(QStringLiteral("A500"));
            setCpuButton(68000);
            cpu24Bit->setChecked(true);
            chipMem->setCurrentText(QStringLiteral("512 KB"));
        }
    }

    void setCpuButton(int model)
    {
        if (QAbstractButton *button = cpuButtons->button(model)) {
            button->setChecked(true);
        }
        updateFpuControls();
        updateCpuControlState();
    }

    void setFpuButton(int model)
    {
        const int id = (model == 68040 || model == 68060) ? FpuInternal : model;
        if (QAbstractButton *button = fpuButtons->button(id)) {
            button->setChecked(true);
        }
        updateFpuControls();
        updateCpuControlState();
    }

    int selectedCpuModel() const
    {
        const int cpu = cpuButtons->checkedId();
        return cpu > 0 ? cpu : 68020;
    }

    int fpuModelConfigValue(int cpu) const
    {
        const int fpu = fpuButtons->checkedId();
        if (fpu == FpuInternal) {
            return cpu >= 68060 ? 68060 : (cpu >= 68040 ? 68040 : 68882);
        }
        return fpu > 0 ? fpu : 0;
    }

    void updateFpuControls()
    {
        const int cpu = selectedCpuModel();
        if (QAbstractButton *internal = fpuButtons->button(FpuInternal)) {
            internal->setEnabled(cpu >= 68040);
            if (!internal->isEnabled() && internal->isChecked()) {
                if (QAbstractButton *none = fpuButtons->button(0)) {
                    none->setChecked(true);
                }
            }
        }
    }

    int chipMemConfigValue() const
    {
        const QString text = chipMem->currentText();
        if (text == QStringLiteral("512 KB")) {
            return 1;
        }
        if (text == QStringLiteral("1 MB")) {
            return 2;
        }
        if (text == QStringLiteral("2 MB")) {
            return 4;
        }
        if (text == QStringLiteral("4 MB")) {
            return 8;
        }
        if (text == QStringLiteral("8 MB")) {
            return 16;
        }
        return 4;
    }

    int megabytesFromText(const QString &text) const
    {
        QString value = text;
        value.remove(QStringLiteral(" MB"));
        return value.toInt();
    }

    int slowMemConfigValue() const
    {
        const QString text = slowMem->currentText();
        if (text == QStringLiteral("512 KB")) {
            return 2;
        }
        if (text == QStringLiteral("1 MB")) {
            return 4;
        }
        if (text == QStringLiteral("1.5 MB")) {
            return 6;
        }
        if (text == QStringLiteral("1.8 MB")) {
            return 7;
        }
        return 0;
    }

    QString nextMountDeviceName() const
    {
        QStringList used;
        if (mountedDrives) {
            for (int i = 0; i < mountedDrives->topLevelItemCount(); i++) {
                const QTreeWidgetItem *item = mountedDrives->topLevelItem(i);
                const QString kind = item->data(0, MountKindRole).toString();
                if (kind == QStringLiteral("dir") || kind == QStringLiteral("hdf")) {
                    used.append(item->data(0, MountDeviceRole).toString().toUpper());
                }
            }
        }
        for (int i = 0; i < MaxMountEntries; i++) {
            const QString device = QStringLiteral("DH%1").arg(i);
            if (!used.contains(device)) {
                return device;
            }
        }
        return QStringLiteral("DH0");
    }

    void addDirectoryMountDialog()
    {
        WinUaeQtMountEntry entry;
        entry.kind = QStringLiteral("dir");
        entry.device = nextMountDeviceName();
        if (showDirectoryMountDialog(&entry, QStringLiteral("Add Directory or Archive"))) {
            addMountEntry(entry);
        }
    }

    void addHardfileMountDialog()
    {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Add hardfile"), QDir::homePath(), QStringLiteral("Hardfiles (*.hdf *.vhd *.chd);;All files (*)"));
        if (path.isEmpty()) {
            return;
        }

        WinUaeQtMountEntry entry;
        entry.kind = QStringLiteral("hdf");
        entry.device = nextMountDeviceName();
        entry.path = path;
        entry.hardfileGeometry = QStringLiteral("32,1,2,512");
        entry.hardfileTail = QStringLiteral(",uae0");
        entry.readOnly = false;
        entry.bootPri = 0;
        if (showHardfileMountDialog(&entry, QStringLiteral("Hardfile Properties"))) {
            addMountEntry(entry);
        }
    }

    int nextMountEmuUnit(const QString &kind) const
    {
        QList<int> used;
        if (mountedDrives) {
            for (int i = 0; i < mountedDrives->topLevelItemCount(); i++) {
                const QTreeWidgetItem *item = mountedDrives->topLevelItem(i);
                if (item->data(0, MountKindRole).toString() == kind) {
                    used.append(item->data(0, MountEmuUnitRole).toInt());
                }
            }
        }
        for (int i = 0; i < MaxControllerUnits; i++) {
            if (!used.contains(i)) {
                return i;
            }
        }
        return 0;
    }

    void addHardDriveMountDialog()
    {
        QMessageBox::information(
            this,
            QStringLiteral("Add Hard Drive"),
            QStringLiteral("Native Unix hard drive enumeration is not implemented yet. The Windows dialog uses a Win32 physical-drive backend; Unix needs a separate block-device backend before this can safely add host disks."));
    }

    void addCdDriveMountDialog()
    {
        WinUaeQtMountEntry entry;
        entry.kind = QStringLiteral("cd");
        entry.emuUnit = nextMountEmuUnit(QStringLiteral("cd"));
        entry.readOnly = true;
        entry.bootPri = 0;
        entry.hardfileGeometry = QStringLiteral("0,0,0,2048");
        entry.hardfileTail = QStringLiteral(",ide0");
        if (showCdDriveMountDialog(&entry, QStringLiteral("Add CD Drive"))) {
            addMountEntry(entry);
        }
    }

    void addTapeDriveMountDialog()
    {
        WinUaeQtMountEntry entry;
        entry.kind = QStringLiteral("tape");
        entry.emuUnit = nextMountEmuUnit(QStringLiteral("tape"));
        entry.readOnly = false;
        entry.bootPri = 0;
        entry.hardfileGeometry = QStringLiteral("0,0,0,512");
        entry.hardfileTail = QStringLiteral(",uae0");
        if (showTapeDriveMountDialog(&entry, QStringLiteral("Add Tape Drive"))) {
            addMountEntry(entry);
        }
    }

    void addMountEntry(const WinUaeQtMountEntry &entry)
    {
        if (!mountedDrives || mountedDrives->topLevelItemCount() >= MaxMountEntries) {
            return;
        }
        QTreeWidgetItem *item = new QTreeWidgetItem(mountedDrives);
        updateMountItem(item, entry);
        updateMountButtons();
    }

    void updateMountItem(QTreeWidgetItem *item, const WinUaeQtMountEntry &entry)
    {
        if (!mountedDrives || !item) {
            return;
        }
        item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
        WinUaeQtMountEntry normalized = entry;
        if (normalized.kind == QStringLiteral("dir")) {
            normalized.device = winUaeQtSanitizedAmigaName(normalized.device, nextMountDeviceName(), true);
            normalized.volume = winUaeQtSanitizedAmigaName(normalized.volume, winUaeQtDefaultVolumeName(normalized.path), false);
        } else if (normalized.kind == QStringLiteral("hdf")) {
            normalized.device = winUaeQtSanitizedAmigaName(normalized.device, nextMountDeviceName(), true);
        } else if (normalized.kind == QStringLiteral("cd")) {
            normalized.device.clear();
            normalized.volume = QStringLiteral("CD");
            normalized.readOnly = true;
            normalized.bootPri = 0;
            if (normalized.hardfileGeometry.isEmpty()) {
                normalized.hardfileGeometry = QStringLiteral("0,0,0,2048");
            }
        } else if (normalized.kind == QStringLiteral("tape")) {
            normalized.device.clear();
            normalized.volume = QStringLiteral("TAPE");
            normalized.bootPri = 0;
            if (normalized.hardfileGeometry.isEmpty()) {
                normalized.hardfileGeometry = QStringLiteral("0,0,0,512");
            }
        }

        QString deviceText = normalized.device;
        QString volumeText = normalized.kind == QStringLiteral("dir") ? normalized.volume : QStringLiteral("n/a");
        QString blockSizeText = QStringLiteral("n/a");
        QString bootPriText = QString::number(normalized.bootPri);
        if (normalized.kind == QStringLiteral("cd")) {
            deviceText = mountControllerDisplay(normalized);
            volumeText = QStringLiteral("CD");
            blockSizeText = QStringLiteral("2048");
            bootPriText = QStringLiteral("n/a");
        } else if (normalized.kind == QStringLiteral("tape")) {
            deviceText = mountControllerDisplay(normalized);
            volumeText = QStringLiteral("TAPE");
            blockSizeText = QStringLiteral("512");
            bootPriText = QStringLiteral("n/a");
        } else if (normalized.kind == QStringLiteral("hdf")) {
            const QStringList geometry = normalized.hardfileGeometry.split(QLatin1Char(','));
            blockSizeText = geometry.value(3, QStringLiteral("512"));
        }

        item->setText(0, QString());
        item->setText(1, deviceText);
        item->setText(2, volumeText);
        item->setText(3, normalized.path.isEmpty() ? QStringLiteral("-") : normalized.path);
        item->setText(4, normalized.readOnly ? QStringLiteral("No") : QStringLiteral("Yes"));
        item->setText(5, blockSizeText);
        item->setText(6, QStringLiteral("n/a"));
        item->setText(7, bootPriText);
        item->setData(0, MountKindRole, entry.kind);
        item->setData(0, MountDeviceRole, normalized.device);
        item->setData(0, MountVolumeRole, normalized.volume);
        item->setData(0, MountPathRole, normalized.path);
        item->setData(0, MountReadOnlyRole, normalized.readOnly);
        item->setData(0, MountBootPriRole, normalized.bootPri);
        item->setData(0, MountEmuUnitRole, normalized.emuUnit);
        item->setData(0, MountRawConfigRole, normalized.rawConfig);
        item->setData(0, MountHardfileGeometryRole, normalized.hardfileGeometry);
        item->setData(0, MountHardfileTailRole, normalized.hardfileTail);
        for (int i = 0; i < mountedDrives->columnCount(); i++) {
            mountedDrives->resizeColumnToContents(i);
        }
    }

    void removeSelectedMount()
    {
        if (!mountedDrives) {
            return;
        }
        qDeleteAll(mountedDrives->selectedItems());
        updateMountButtons();
    }

    void moveSelectedMount(int delta)
    {
        if (!mountedDrives || delta == 0) {
            return;
        }
        QTreeWidgetItem *item = mountedDrives->currentItem();
        if (!item) {
            return;
        }
        const int from = mountedDrives->indexOfTopLevelItem(item);
        if (from < 0) {
            return;
        }
        const int to = qBound(0, from + delta, mountedDrives->topLevelItemCount() - 1);
        if (from == to) {
            return;
        }
        item = mountedDrives->takeTopLevelItem(from);
        mountedDrives->insertTopLevelItem(to, item);
        mountedDrives->setCurrentItem(item);
        item->setSelected(true);
        updateMountButtons();
    }

    void updateMountButtons()
    {
        if (!mountedDrives) {
            return;
        }
        const bool canAdd = mountedDrives->topLevelItemCount() < MaxMountEntries;
        if (addDirectoryMountButton) {
            addDirectoryMountButton->setEnabled(canAdd);
        }
        if (addHardfileMountButton) {
            addHardfileMountButton->setEnabled(canAdd);
        }
        if (addHardDriveMountButton) {
            addHardDriveMountButton->setEnabled(canAdd);
        }
        if (addCdMountButton) {
            addCdMountButton->setEnabled(canAdd);
        }
        if (addTapeMountButton) {
            addTapeMountButton->setEnabled(canAdd);
        }
        if (propertiesMountButton) {
            propertiesMountButton->setEnabled(!mountedDrives->selectedItems().isEmpty());
        }
        if (removeMountButton) {
            removeMountButton->setEnabled(!mountedDrives->selectedItems().isEmpty());
        }
    }

    WinUaeQtMountEntry mountEntryFromItem(const QTreeWidgetItem *item) const
    {
        WinUaeQtMountEntry entry;
        entry.kind = item->data(0, MountKindRole).toString();
        entry.device = item->data(0, MountDeviceRole).toString();
        entry.volume = item->data(0, MountVolumeRole).toString();
        entry.path = item->data(0, MountPathRole).toString();
        entry.readOnly = item->data(0, MountReadOnlyRole).toBool();
        entry.bootPri = item->data(0, MountBootPriRole).toInt();
        entry.emuUnit = item->data(0, MountEmuUnitRole).toInt();
        entry.rawConfig = item->data(0, MountRawConfigRole).toString();
        entry.hardfileGeometry = item->data(0, MountHardfileGeometryRole).toString();
        entry.hardfileTail = item->data(0, MountHardfileTailRole).toString();
        return entry;
    }

    void openSelectedMountProperties()
    {
        if (!mountedDrives) {
            return;
        }
        QTreeWidgetItem *item = mountedDrives->currentItem();
        if (!item) {
            return;
        }

        WinUaeQtMountEntry entry = mountEntryFromItem(item);
        bool accepted = false;
        if (entry.kind == QStringLiteral("dir")) {
            accepted = showDirectoryMountDialog(&entry, QStringLiteral("Directory Properties"));
        } else if (entry.kind == QStringLiteral("hdf")) {
            accepted = showHardfileMountDialog(&entry, QStringLiteral("Hardfile Properties"));
        } else if (entry.kind == QStringLiteral("cd")) {
            accepted = showCdDriveMountDialog(&entry, QStringLiteral("CD Drive Properties"));
        } else if (entry.kind == QStringLiteral("tape")) {
            accepted = showTapeDriveMountDialog(&entry, QStringLiteral("Tape Drive Properties"));
        }
        if (accepted) {
            updateMountItem(item, entry);
        }
    }

    bool showDirectoryMountDialog(WinUaeQtMountEntry *entry, const QString &title)
    {
        if (!entry) {
            return false;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(title);

        QLineEdit *path = new QLineEdit(entry->path);
        QLineEdit *device = new QLineEdit(entry->device.isEmpty() ? nextMountDeviceName() : entry->device);
        QLineEdit *volume = new QLineEdit(entry->volume);
        QSpinBox *bootPri = new QSpinBox;
        bootPri->setRange(-128, 127);
        bootPri->setValue(entry->bootPri);
        QCheckBox *readOnly = new QCheckBox(QStringLiteral("Read-only"));
        readOnly->setChecked(entry->readOnly);
        QPushButton *browse = smallButton(QStringLiteral("..."));

        QGridLayout *fields = new QGridLayout;
        fields->setColumnStretch(1, 1);
        fields->addWidget(label(QStringLiteral("Path:")), 0, 0);
        fields->addWidget(path, 0, 1);
        fields->addWidget(browse, 0, 2);
        fields->addWidget(label(QStringLiteral("Device:")), 1, 0);
        fields->addWidget(device, 1, 1, 1, 2);
        fields->addWidget(label(QStringLiteral("Volume:")), 2, 0);
        fields->addWidget(volume, 2, 1, 1, 2);
        fields->addWidget(label(QStringLiteral("Boot priority:")), 3, 0);
        fields->addWidget(bootPri, 3, 1);
        fields->addWidget(readOnly, 4, 1, 1, 2);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QVBoxLayout *root = new QVBoxLayout(&dialog);
        root->addLayout(fields);
        root->addWidget(buttons);

        connect(browse, &QPushButton::clicked, this, [this, path, volume]() {
            const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral("Select directory"), path->text().isEmpty() ? QDir::homePath() : path->text());
            if (!selected.isEmpty()) {
                path->setText(selected);
                if (volume->text().isEmpty()) {
                    volume->setText(winUaeQtDefaultVolumeName(selected));
                }
            }
        });
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted || path->text().trimmed().isEmpty()) {
            return false;
        }

        entry->kind = QStringLiteral("dir");
        entry->device = winUaeQtSanitizedAmigaName(device->text(), nextMountDeviceName(), true);
        entry->volume = winUaeQtSanitizedAmigaName(volume->text(), winUaeQtDefaultVolumeName(path->text()), false);
        entry->path = path->text().trimmed();
        entry->bootPri = bootPri->value();
        entry->readOnly = readOnly->isChecked();
        return true;
    }

    bool showHardfileMountDialog(WinUaeQtMountEntry *entry, const QString &title)
    {
        if (!entry) {
            return false;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(title);
        dialog.resize(600, 460);

        QLineEdit *path = new QLineEdit(entry->path);
        QLineEdit *geometryFile = new QLineEdit(hardfileTailGeometryFile(*entry));
        QLineEdit *filesys = new QLineEdit(mountControllerParts(entry->hardfileTail).value(0));
        QLineEdit *device = new QLineEdit(entry->device.isEmpty() ? nextMountDeviceName() : entry->device);
        QSpinBox *bootPri = new QSpinBox;
        bootPri->setRange(-129, 127);
        bootPri->setValue(entry->bootPri);
        QCheckBox *readWrite = new QCheckBox(QStringLiteral("Read/write"));
        readWrite->setChecked(!entry->readOnly);
        QCheckBox *autoboot = new QCheckBox(QStringLiteral("Bootable"));
        QCheckBox *doNotMount = new QCheckBox(QStringLiteral("Do not mount"));
        QCheckBox *rdbMode = new QCheckBox(QStringLiteral("Full drive/RDB mode"));
        QCheckBox *manualGeometry = new QCheckBox(QStringLiteral("Manual geometry"));
        rdbMode->setChecked(hardfileIsRdb(*entry));
        manualGeometry->setChecked(hardfileHasPhysicalGeometry(*entry));

        const QStringList geometry = hardfileGeometryParts(*entry);
        QSpinBox *surfaces = new QSpinBox;
        QSpinBox *sectors = new QSpinBox;
        QSpinBox *reserved = new QSpinBox;
        QSpinBox *blockSize = new QSpinBox;
        for (QSpinBox *spin : { surfaces, sectors, reserved }) {
            spin->setRange(0, 1000000000);
        }
        blockSize->setRange(1, 65536);
        sectors->setValue(geometry.value(0).toInt());
        surfaces->setValue(geometry.value(1).toInt());
        reserved->setValue(geometry.value(2).toInt());
        blockSize->setValue(qMax(1, geometry.value(3, QStringLiteral("512")).toInt()));

        const QStringList physicalGeometry = hardfilePhysicalGeometryParts(*entry);
        QSpinBox *physicalCylinders = new QSpinBox;
        QSpinBox *physicalHeads = new QSpinBox;
        QSpinBox *physicalSectors = new QSpinBox;
        for (QSpinBox *spin : { physicalCylinders, physicalHeads, physicalSectors }) {
            spin->setRange(0, 1000000000);
        }
        physicalCylinders->setValue(physicalGeometry.value(0).toInt());
        physicalHeads->setValue(physicalGeometry.value(1).toInt());
        physicalSectors->setValue(physicalGeometry.value(2).toInt());

        QComboBox *controller = combo({
            QStringLiteral("UAE (uaehf.device)"),
            QStringLiteral("IDE (Auto)"),
            QStringLiteral("SCSI (Auto)")
        }, mountControllerFamily(*entry, QStringLiteral("uae0")));
        QSpinBox *controllerUnit = new QSpinBox;
        controllerUnit->setRange(0, MaxControllerUnits - 1);
        controllerUnit->setValue(mountControllerUnit(*entry, QStringLiteral("uae0")));
        QComboBox *mediaType = combo({ QStringLiteral("HD"), QStringLiteral("CF") }, hardfileTailHasToken(*entry, QStringLiteral("CF")) ? QStringLiteral("CF") : QStringLiteral("HD"));
        QComboBox *featureLevel = new QComboBox;

        QPushButton *browse = smallButton(QStringLiteral("..."));
        QPushButton *geometryBrowse = smallButton(QStringLiteral("..."));
        QPushButton *filesysBrowse = smallButton(QStringLiteral("..."));

        QGridLayout *fields = new QGridLayout;
        fields->setColumnStretch(1, 1);
        fields->addWidget(label(QStringLiteral("Path:")), 0, 0);
        fields->addWidget(path, 0, 1, 1, 2);
        fields->addWidget(browse, 0, 3);
        fields->addWidget(label(QStringLiteral("Geometry:")), 1, 0);
        fields->addWidget(geometryFile, 1, 1, 1, 2);
        fields->addWidget(geometryBrowse, 1, 3);
        fields->addWidget(label(QStringLiteral("FileSys:")), 2, 0);
        fields->addWidget(filesys, 2, 1, 1, 2);
        fields->addWidget(filesysBrowse, 2, 3);
        fields->addWidget(label(QStringLiteral("Device:")), 3, 0);
        fields->addWidget(device, 3, 1);
        fields->addWidget(label(QStringLiteral("Boot priority:")), 3, 2);
        fields->addWidget(bootPri, 3, 3);
        fields->addWidget(readWrite, 4, 1);
        fields->addWidget(autoboot, 4, 2);
        fields->addWidget(doNotMount, 5, 1);
        fields->addWidget(rdbMode, 6, 1);
        fields->addWidget(manualGeometry, 6, 2, 1, 2);

        QGridLayout *geometryLayout = new QGridLayout;
        geometryLayout->addWidget(label(QStringLiteral("Surfaces:")), 0, 0);
        geometryLayout->addWidget(surfaces, 0, 1);
        geometryLayout->addWidget(label(QStringLiteral("Sectors:")), 1, 0);
        geometryLayout->addWidget(sectors, 1, 1);
        geometryLayout->addWidget(label(QStringLiteral("Reserved:")), 2, 0);
        geometryLayout->addWidget(reserved, 2, 1);
        geometryLayout->addWidget(label(QStringLiteral("Block size:")), 3, 0);
        geometryLayout->addWidget(blockSize, 3, 1);
        geometryLayout->addWidget(label(QStringLiteral("Physical cyls:")), 0, 2);
        geometryLayout->addWidget(physicalCylinders, 0, 3);
        geometryLayout->addWidget(label(QStringLiteral("Physical heads:")), 1, 2);
        geometryLayout->addWidget(physicalHeads, 1, 3);
        geometryLayout->addWidget(label(QStringLiteral("Physical sectors:")), 2, 2);
        geometryLayout->addWidget(physicalSectors, 2, 3);

        QGridLayout *controllerLayout = new QGridLayout;
        controllerLayout->setColumnStretch(1, 1);
        controllerLayout->addWidget(label(QStringLiteral("HD Controller:")), 0, 0);
        controllerLayout->addWidget(controller, 0, 1);
        controllerLayout->addWidget(label(QStringLiteral("Unit:")), 0, 2);
        controllerLayout->addWidget(controllerUnit, 0, 3);
        controllerLayout->addWidget(label(QStringLiteral("Type:")), 1, 0);
        controllerLayout->addWidget(mediaType, 1, 1);
        controllerLayout->addWidget(label(QStringLiteral("Feature level:")), 1, 2);
        controllerLayout->addWidget(featureLevel, 1, 3);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QVBoxLayout *root = new QVBoxLayout(&dialog);
        root->addWidget(groupBox(QStringLiteral("Settings"), fields));
        root->addWidget(groupBox(QStringLiteral("Geometry"), geometryLayout));
        root->addWidget(groupBox(QStringLiteral("HD Controller"), controllerLayout));
        root->addWidget(buttons);

        auto updateBootChecks = [bootPri, autoboot, doNotMount]() {
            QSignalBlocker blockAutoboot(autoboot);
            QSignalBlocker blockDoNotMount(doNotMount);
            autoboot->setChecked(bootPri->value() > -128);
            doNotMount->setChecked(bootPri->value() <= -129);
        };
        auto updatePhysicalControls = [manualGeometry, physicalCylinders, physicalHeads, physicalSectors]() {
            const bool enabled = manualGeometry->isChecked();
            physicalCylinders->setEnabled(enabled);
            physicalHeads->setEnabled(enabled);
            physicalSectors->setEnabled(enabled);
        };
        auto setFeatureItems = [controller, featureLevel, mediaType](const QString &preferred) {
            const QString family = controller->currentText();
            QSignalBlocker blocker(featureLevel);
            featureLevel->clear();
            featureLevel->addItem(QStringLiteral("Default"));
            if (family == QStringLiteral("IDE (Auto)")) {
                featureLevel->addItems({ QStringLiteral("ATA-1"), QStringLiteral("ATA-2+"), QStringLiteral("ATA-2+ Strict") });
            } else if (family == QStringLiteral("SCSI (Auto)")) {
                featureLevel->addItems({ QStringLiteral("SCSI-1"), QStringLiteral("SCSI-2"), QStringLiteral("SASI"), QStringLiteral("SASI CHS") });
            }
            const int index = featureLevel->findText(preferred);
            featureLevel->setCurrentIndex(index >= 0 ? index : 0);
            featureLevel->setEnabled(family != QStringLiteral("UAE (uaehf.device)"));
            mediaType->setEnabled(family == QStringLiteral("IDE (Auto)"));
        };

        updateBootChecks();
        updatePhysicalControls();
        updateControllerUnitRange(controllerUnit, controller->currentText());
        setFeatureItems(hardfileFeatureText(*entry, controller->currentText()));

        connect(browse, &QPushButton::clicked, this, [this, path]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select hardfile"), path->text().isEmpty() ? QDir::homePath() : path->text(), QStringLiteral("Hardfiles (*.hdf *.vhd *.chd);;All files (*)"));
            if (!selected.isEmpty()) {
                path->setText(selected);
            }
        });
        connect(geometryBrowse, &QPushButton::clicked, this, [this, geometryFile]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select geometry file"), geometryFile->text().isEmpty() ? QDir::homePath() : geometryFile->text(), QStringLiteral("Geometry files (*.geo);;All files (*)"));
            if (!selected.isEmpty()) {
                geometryFile->setText(selected);
            }
        });
        connect(filesysBrowse, &QPushButton::clicked, this, [this, filesys]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select filesystem"), filesys->text().isEmpty() ? QDir::homePath() : filesys->text(), QStringLiteral("Filesystem files (*.fs *.filesystem);;All files (*)"));
            if (!selected.isEmpty()) {
                filesys->setText(selected);
            }
        });
        connect(bootPri, QOverload<int>::of(&QSpinBox::valueChanged), this, updateBootChecks);
        connect(autoboot, &QCheckBox::toggled, this, [bootPri, doNotMount](bool checked) {
            QSignalBlocker blocker(doNotMount);
            if (checked) {
                bootPri->setValue(0);
                doNotMount->setChecked(false);
            } else if (bootPri->value() > -128) {
                bootPri->setValue(-128);
            }
        });
        connect(doNotMount, &QCheckBox::toggled, this, [bootPri, autoboot](bool checked) {
            QSignalBlocker blocker(autoboot);
            if (checked) {
                bootPri->setValue(-129);
                autoboot->setChecked(false);
            } else if (bootPri->value() <= -129) {
                bootPri->setValue(-128);
            }
        });
        connect(rdbMode, &QCheckBox::toggled, this, [sectors, surfaces, reserved, blockSize, device, filesys, bootPri](bool checked) {
            if (checked) {
                sectors->setValue(0);
                surfaces->setValue(0);
                reserved->setValue(0);
                blockSize->setValue(512);
                device->clear();
                filesys->clear();
                bootPri->setValue(0);
            } else if (sectors->value() == 0 && surfaces->value() == 0 && reserved->value() == 0) {
                sectors->setValue(32);
                surfaces->setValue(1);
                reserved->setValue(2);
                blockSize->setValue(512);
            }
        });
        connect(manualGeometry, &QCheckBox::toggled, this, updatePhysicalControls);
        connect(controller, &QComboBox::currentTextChanged, this, [this, controllerUnit, setFeatureItems](const QString &text) {
            updateControllerUnitRange(controllerUnit, text);
            setFeatureItems(QStringLiteral("Default"));
        });
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted || path->text().trimmed().isEmpty()) {
            return false;
        }

        entry->kind = QStringLiteral("hdf");
        entry->device = winUaeQtSanitizedAmigaName(device->text(), nextMountDeviceName(), true);
        entry->path = path->text().trimmed();
        entry->bootPri = bootPri->value();
        entry->readOnly = !readWrite->isChecked();
        entry->hardfileGeometry = QStringLiteral("%1,%2,%3,%4")
            .arg(sectors->value())
            .arg(surfaces->value())
            .arg(reserved->value())
            .arg(blockSize->value());

        QStringList tailFields;
        tailFields.append(filesys->text().trimmed());
        tailFields.append(mountControllerConfigValue(controller->currentText(), controllerUnit->value()));
        if (manualGeometry->isChecked() || !geometryFile->text().trimmed().isEmpty()) {
            tailFields.append(QString::number(physicalCylinders->value()));
            tailFields.append(QStringLiteral("%1/%2/%3")
                .arg(physicalCylinders->value())
                .arg(physicalHeads->value())
                .arg(physicalSectors->value()));
            if (!geometryFile->text().trimmed().isEmpty()) {
                tailFields.append(geometryFile->text().trimmed());
            }
        }
        if (controller->currentText() == QStringLiteral("IDE (Auto)") && mediaType->currentText() == QStringLiteral("CF")) {
            tailFields.append(QStringLiteral("CF"));
        }
        const QString featureToken = hardfileFeatureToken(featureLevel->currentText());
        if (!featureToken.isEmpty()) {
            tailFields.append(featureToken);
        }
        tailFields.append(hardfilePreservedTailExtras(*entry));
        entry->hardfileTail = winUaeQtConfigJoinFields(tailFields);
        return true;
    }

    void updateControllerUnitRange(QSpinBox *unit, const QString &family)
    {
        if (!unit) {
            return;
        }
        const int maximum = family == QStringLiteral("IDE (Auto)") ? 3 : MaxControllerUnits - 1;
        unit->setMaximum(maximum);
    }

    bool showCdDriveMountDialog(WinUaeQtMountEntry *entry, const QString &title)
    {
        if (!entry) {
            return false;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(title);

        QSpinBox *cdUnit = new QSpinBox;
        cdUnit->setRange(0, MaxControllerUnits - 1);
        cdUnit->setValue(qBound(0, entry->emuUnit, MaxControllerUnits - 1));
        QComboBox *controller = combo({ QStringLiteral("IDE (Auto)"), QStringLiteral("SCSI (Auto)") }, mountControllerFamily(*entry, QStringLiteral("ide0")));
        QSpinBox *controllerUnit = new QSpinBox;
        controllerUnit->setRange(0, MaxControllerUnits - 1);
        controllerUnit->setValue(mountControllerUnit(*entry, QStringLiteral("ide0")));
        updateControllerUnitRange(controllerUnit, controller->currentText());

        QGridLayout *fields = new QGridLayout;
        fields->setColumnStretch(1, 1);
        fields->addWidget(label(QStringLiteral("CD unit:")), 0, 0);
        fields->addWidget(cdUnit, 0, 1);
        fields->addWidget(label(QStringLiteral("Controller:")), 1, 0);
        fields->addWidget(controller, 1, 1);
        fields->addWidget(label(QStringLiteral("Controller unit:")), 2, 0);
        fields->addWidget(controllerUnit, 2, 1);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QVBoxLayout *root = new QVBoxLayout(&dialog);
        root->addLayout(fields);
        root->addWidget(buttons);

        connect(controller, &QComboBox::currentTextChanged, this, [this, controllerUnit](const QString &text) {
            updateControllerUnitRange(controllerUnit, text);
        });
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        entry->kind = QStringLiteral("cd");
        entry->device.clear();
        entry->volume = QStringLiteral("CD");
        entry->emuUnit = cdUnit->value();
        entry->readOnly = true;
        entry->bootPri = 0;
        entry->hardfileGeometry = QStringLiteral("0,0,0,2048");
        entry->hardfileTail = mountTailWithController(*entry, mountControllerConfigValue(controller->currentText(), controllerUnit->value()));
        return true;
    }

    bool showTapeDriveMountDialog(WinUaeQtMountEntry *entry, const QString &title)
    {
        if (!entry) {
            return false;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(title);

        QLineEdit *path = new QLineEdit(entry->path);
        QSpinBox *tapeUnit = new QSpinBox;
        tapeUnit->setRange(0, MaxControllerUnits - 1);
        tapeUnit->setValue(qBound(0, entry->emuUnit, MaxControllerUnits - 1));
        QComboBox *controller = combo({ QStringLiteral("UAE (uaehf.device)"), QStringLiteral("IDE (Auto)"), QStringLiteral("SCSI (Auto)") }, mountControllerFamily(*entry, QStringLiteral("uae0")));
        QSpinBox *controllerUnit = new QSpinBox;
        controllerUnit->setRange(0, MaxControllerUnits - 1);
        controllerUnit->setValue(mountControllerUnit(*entry, QStringLiteral("uae0")));
        updateControllerUnitRange(controllerUnit, controller->currentText());
        QCheckBox *readWrite = new QCheckBox(QStringLiteral("Read/write"));
        readWrite->setChecked(!entry->readOnly);
        QPushButton *selectFile = new QPushButton(QStringLiteral("File..."));
        QPushButton *selectDirectory = new QPushButton(QStringLiteral("Directory..."));
        QPushButton *eject = new QPushButton(QStringLiteral("Eject"));

        QGridLayout *fields = new QGridLayout;
        fields->setColumnStretch(1, 1);
        fields->addWidget(label(QStringLiteral("Path:")), 0, 0);
        fields->addWidget(path, 0, 1, 1, 3);
        fields->addWidget(selectFile, 1, 1);
        fields->addWidget(selectDirectory, 1, 2);
        fields->addWidget(eject, 1, 3);
        fields->addWidget(label(QStringLiteral("Tape unit:")), 2, 0);
        fields->addWidget(tapeUnit, 2, 1);
        fields->addWidget(label(QStringLiteral("Controller:")), 3, 0);
        fields->addWidget(controller, 3, 1, 1, 3);
        fields->addWidget(label(QStringLiteral("Controller unit:")), 4, 0);
        fields->addWidget(controllerUnit, 4, 1);
        fields->addWidget(readWrite, 5, 1, 1, 3);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QVBoxLayout *root = new QVBoxLayout(&dialog);
        root->addLayout(fields);
        root->addWidget(buttons);

        connect(selectFile, &QPushButton::clicked, this, [this, path]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select tape image"), path->text().isEmpty() ? QDir::homePath() : path->text(), QStringLiteral("Tape images (*.tap *.raw);;All files (*)"));
            if (!selected.isEmpty()) {
                path->setText(selected);
            }
        });
        connect(selectDirectory, &QPushButton::clicked, this, [this, path]() {
            const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral("Select tape directory"), path->text().isEmpty() ? QDir::homePath() : path->text());
            if (!selected.isEmpty()) {
                path->setText(selected);
            }
        });
        connect(eject, &QPushButton::clicked, path, &QLineEdit::clear);
        connect(controller, &QComboBox::currentTextChanged, this, [this, controllerUnit](const QString &text) {
            updateControllerUnitRange(controllerUnit, text);
        });
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        entry->kind = QStringLiteral("tape");
        entry->device.clear();
        entry->volume = QStringLiteral("TAPE");
        entry->path = path->text().trimmed();
        entry->emuUnit = tapeUnit->value();
        entry->readOnly = !readWrite->isChecked();
        entry->bootPri = 0;
        entry->hardfileGeometry = QStringLiteral("0,0,0,512");
        entry->hardfileTail = mountTailWithController(*entry, mountControllerConfigValue(controller->currentText(), controllerUnit->value()));
        return true;
    }

    WinUaeQtConfig::OrderedSettings currentMountSettings() const
    {
        WinUaeQtConfig::OrderedSettings settings;
        if (!mountedDrives) {
            return settings;
        }
        for (int i = 0; i < mountedDrives->topLevelItemCount(); i++) {
            const WinUaeQtMountEntry entry = mountEntryFromItem(mountedDrives->topLevelItem(i));
            if (entry.kind == QStringLiteral("dir")) {
                settings.append({ QStringLiteral("filesystem2"), serializeWinUaeQtFilesystem2MountValue(entry) });
            } else if (entry.kind == QStringLiteral("hdf")) {
                settings.append({ QStringLiteral("hardfile2"), serializeWinUaeQtHardfile2MountValue(entry) });
            } else if (entry.kind == QStringLiteral("cd")) {
                settings.append({ QStringLiteral("uaehf%1").arg(i), serializeWinUaeQtUaehfCdMountValue(entry) });
            } else if (entry.kind == QStringLiteral("tape")) {
                settings.append({ QStringLiteral("uaehf%1").arg(i), serializeWinUaeQtUaehfTapeMountValue(entry) });
            }
        }
        return settings;
    }

    void syncQuickDriveToFloppy(int drive)
    {
        if (drive < 0 || drive >= 2 || !dfPath[drive] || !quickDfPath[drive]) {
            return;
        }
        setCheckBoxIfChanged(dfEnable[drive], quickDfEnable[drive]->isChecked());
        setComboTextIfChanged(dfType[drive], quickDfType[drive]->currentText());
        setComboTextIfChanged(dfPath[drive], quickDfPath[drive]->currentText());
        setCheckBoxIfChanged(dfWriteProtect[drive], quickDfWriteProtect[drive]->isChecked());
    }

    void syncFloppyDriveToQuick(int drive)
    {
        if (drive < 0 || drive >= 2 || !dfPath[drive] || !quickDfPath[drive]) {
            return;
        }
        setCheckBoxIfChanged(quickDfEnable[drive], dfEnable[drive]->isChecked());
        setComboTextIfChanged(quickDfType[drive], dfType[drive]->currentText());
        setComboTextIfChanged(quickDfPath[drive], dfPath[drive]->currentText());
        setCheckBoxIfChanged(quickDfWriteProtect[drive], dfWriteProtect[drive]->isChecked());
    }

    int rtgModeMask() const
    {
        int mask = 0;
        mask |= rtgColorDepthMask(rtg8Bit->currentText());
        mask |= rtgColorDepthMask(rtg16Bit->currentText());
        mask |= rtgColorDepthMask(rtg24Bit->currentText());
        mask |= rtgColorDepthMask(rtg32Bit->currentText());
        return mask ? mask : RtgDefaultModeMask;
    }

    QString rtgOptionsValue() const
    {
        QStringList parts;
        const int monitorIndex = rtgMonitor->currentText().toInt() - 1;
        if (monitorIndex > 0) {
            parts.append(QStringLiteral("monitor=%1").arg(monitorIndex));
        }
        if (!rtgAutoswitch->isChecked()) {
            parts.append(QStringLiteral("noautoswitch"));
        }
        if (rtgInitialMonitor->isChecked()) {
            parts.append(QStringLiteral("initial"));
        }
        return parts.join(QLatin1Char(','));
    }

    void applyRtgScaleValue(const QString &value)
    {
        const QString lower = value.toLower();
        rtgScale->setChecked(lower == QStringLiteral("scale"));
        rtgCenter->setChecked(lower == QStringLiteral("center"));
        rtgIntegerScale->setChecked(lower == QStringLiteral("integer"));
    }

    void applyRtgOptionsValue(const QString &value)
    {
        bool autoswitch = true;
        bool initial = false;
        int monitor = 0;
        for (const QString &field : winUaeQtConfigFieldList(value)) {
            const QString trimmed = field.trimmed();
            if (trimmed.compare(QStringLiteral("noautoswitch"), Qt::CaseInsensitive) == 0) {
                autoswitch = false;
            } else if (trimmed.compare(QStringLiteral("autoswitch"), Qt::CaseInsensitive) == 0) {
                autoswitch = true;
            } else if (trimmed.compare(QStringLiteral("initial"), Qt::CaseInsensitive) == 0) {
                initial = true;
            } else if (trimmed.startsWith(QStringLiteral("monitor="), Qt::CaseInsensitive)) {
                monitor = qBound(0, trimmed.mid(8).toInt(), 3);
            }
        }
        rtgAutoswitch->setChecked(autoswitch);
        rtgInitialMonitor->setChecked(initial);
        rtgMonitor->setCurrentText(QString::number(monitor + 1));
    }

    WinUaeQtConfig::Settings currentSettings() const
    {
        WinUaeQtConfig::Settings settings;
        const int cpu = selectedCpuModel();
        const int fpu = fpuModelConfigValue(cpu);
        settings.insert(QStringLiteral("kickstart_rom_file"), romFile->currentText());
        if (!extendedRomFile->currentText().isEmpty()) {
            settings.insert(QStringLiteral("kickstart_ext_rom_file"), extendedRomFile->currentText());
        }
        if (!cartFile->currentText().isEmpty()) {
            settings.insert(QStringLiteral("cart_file"), cartFile->currentText());
        }
        if (!flashFile->text().isEmpty()) {
            settings.insert(QStringLiteral("flash_file"), flashFile->text());
        }
        if (!rtcFile->text().isEmpty()) {
            settings.insert(QStringLiteral("rtc_file"), rtcFile->text());
        }
        settings.insert(QStringLiteral("maprom"), mapRom->isChecked() ? QStringLiteral("0x0f000000") : QStringLiteral("0x0"));
        settings.insert(QStringLiteral("kickshifter"), kickShifter->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        if (uaeBoardType->currentText() == QStringLiteral("ROM disabled")) {
            settings.insert(QStringLiteral("boot_rom_uae"), QStringLiteral("disabled"));
            settings.insert(QStringLiteral("uaeboard"), QStringLiteral("disabled"));
        } else {
            settings.insert(QStringLiteral("boot_rom_uae"), QStringLiteral("automatic"));
            settings.insert(QStringLiteral("uaeboard"), uaeBoardConfigValue(uaeBoardType->currentText()));
        }
        for (int i = 0; i < MaxRomBoards; i++) {
            WinUaeQtRomBoard board = customRomBoards.value(i);
            if (i == currentCustomRomBoard && customRomStart && customRomEnd && customRomFile) {
                board.start = customRomStart->text();
                board.end = customRomEnd->text();
                board.path = customRomFile->text();
            }
            const QString value = romBoardConfigValue(board);
            if (!value.isEmpty()) {
                settings.insert(romBoardKey(i), value);
            }
        }
        for (int i = 0; i < 4; i++) {
            const int driveType = dfEnable[i]->isChecked() ? floppyTypeConfigValue(dfType[i]->currentText()) : -1;
            settings.insert(QStringLiteral("floppy%1type").arg(i), QString::number(driveType));
            settings.insert(QStringLiteral("floppy%1wp").arg(i), dfWriteProtect[i]->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
            if (driveType >= 0 && !dfPath[i]->currentText().isEmpty()) {
                settings.insert(QStringLiteral("floppy%1").arg(i), dfPath[i]->currentText());
            }
        }
        settings.insert(QStringLiteral("nr_floppies"), QString::number(enabledFloppyCount()));
        settings.insert(QStringLiteral("floppy_speed"), QString::number(floppySpeedConfigValue(floppySpeed->value())));
        settings.insert(QStringLiteral("chipset"), chipset->currentText().toLower());
        settings.insert(QStringLiteral("chipset_compatible"), chipsetCompatible->currentText());
        settings.insert(QStringLiteral("cpu_model"), QString::number(cpu));
        settings.insert(QStringLiteral("cpu_speed"), cpuSpeedButtons->checkedId() == 1 ? QStringLiteral("max") : QStringLiteral("real"));
        if (cpuSpeed->value() != 0) {
            settings.insert(QStringLiteral("cpu_throttle"), QString::number(cpuSpeed->value() * 100.0, 'f', 1));
        }
        settings.insert(QStringLiteral("cpu_compatible"), moreCompatible->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        if (fpu) {
            settings.insert(QStringLiteral("fpu_model"), QString::number(fpu));
        }
        settings.insert(QStringLiteral("cpu_24bit_addressing"), cpu24Bit->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        if (mmuButtons->checkedId() == 1 && cpu >= 68030) {
            settings.insert(QStringLiteral("mmu_model"), QString::number(cpu));
        } else if (mmuButtons->checkedId() == 2 && cpu >= 68030) {
            settings.insert(QStringLiteral("mmu_model"), QStringLiteral("68ec0%1").arg(cpu % 100, 2, 10, QLatin1Char('0')));
        }
        settings.insert(QStringLiteral("cpu_no_unimplemented"), cpuUnimplemented->isChecked() ? QStringLiteral("false") : QStringLiteral("true"));
        settings.insert(QStringLiteral("fpu_no_unimplemented"), fpuUnimplemented->isChecked() ? QStringLiteral("false") : QStringLiteral("true"));
        settings.insert(QStringLiteral("fpu_strict"), fpuStrict->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("fpu_softfloat"), fpuMode->currentText() == QStringLiteral("Softfloat (80-bit)") ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("fpu_msvc_long_double"), fpuMode->currentText() == QStringLiteral("Host (80-bit)") ? QStringLiteral("true") : QStringLiteral("false"));
        if (cpuFrequency->currentText() == QStringLiteral("Custom")) {
            bool ok = false;
            const double mhz = cpuFrequencyCustom->text().toDouble(&ok);
            if (ok && mhz >= 1.0 && mhz < 99.0) {
                settings.insert(QStringLiteral("cpu_frequency"), QString::number(qRound64(mhz * 1000000.0)));
            }
        } else {
            settings.insert(QStringLiteral("cpu_multiplier"), QString::number(cpuMultiplierValue(cpuFrequency->currentText())));
        }
        settings.insert(QStringLiteral("chipmem_size"), QString::number(chipMemConfigValue()));
        if (z2Fast->currentText() != QStringLiteral("None")) {
            settings.insert(QStringLiteral("fastmem_size"), QString::number(megabytesFromText(z2Fast->currentText())));
        }
        const int slow = slowMemConfigValue();
        if (slow) {
            settings.insert(QStringLiteral("bogomem_size"), QString::number(slow));
        }
        if (z3Fast->currentText() != QStringLiteral("None")) {
            settings.insert(QStringLiteral("z3mem_size"), QString::number(megabytesFromText(z3Fast->currentText())));
        }
        settings.insert(QStringLiteral("cachesize"), QString::number(jit->isChecked() ? jitCacheSizeFromPosition(jitCache->value()) : 0));
        settings.insert(QStringLiteral("compfpu"), jitFpu->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("comp_constjump"), jitConstJump->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("comp_flushmode"), jitHardFlush->isChecked() ? QStringLiteral("hard") : QStringLiteral("soft"));
        settings.insert(QStringLiteral("comp_nf"), jitNoFlags->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("comp_catchfault"), jitCatchFault->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        const QString trust = jitTrust->checkedId() == 1 ? QStringLiteral("indirect") : QStringLiteral("direct");
        settings.insert(QStringLiteral("comp_trustbyte"), trust);
        settings.insert(QStringLiteral("comp_trustword"), trust);
        settings.insert(QStringLiteral("comp_trustlong"), trust);
        settings.insert(QStringLiteral("comp_trustnaddr"), trust);
        settings.insert(QStringLiteral("sound_output"), soundOutputConfigValue(soundOutputButtons->checkedId()));
        settings.insert(QStringLiteral("sound_auto"), soundAutomatic->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("sound_volume"), QString::number(100 - soundMasterVolume->value()));
        settings.insert(QStringLiteral("sound_volume_paula"), QString::number(soundVolumeAttenuationValue(0)));
        settings.insert(QStringLiteral("sound_volume_cd"), QString::number(soundVolumeAttenuationValue(1)));
        settings.insert(QStringLiteral("sound_volume_ahi"), QString::number(soundVolumeAttenuationValue(2)));
        settings.insert(QStringLiteral("sound_volume_midi"), QString::number(soundVolumeAttenuationValue(3)));
        settings.insert(QStringLiteral("sound_volume_genlock"), QString::number(soundVolumeAttenuationValue(4)));
        settings.insert(QStringLiteral("sound_max_buff"), QString::number(soundBufferSizeFromIndex(soundBufferSize->value())));
        settings.insert(QStringLiteral("sound_channels"), soundChannelConfigValue(soundChannels->currentText()));
        if (soundChannels->currentText() == QStringLiteral("Mono")) {
            settings.insert(QStringLiteral("sound_stereo_separation"), QStringLiteral("0"));
            settings.insert(QStringLiteral("sound_stereo_mixing_delay"), QStringLiteral("0"));
        } else {
            settings.insert(QStringLiteral("sound_stereo_separation"), QString::number(qBound(0, 10 - soundStereoSeparation->currentIndex(), 10)));
            settings.insert(QStringLiteral("sound_stereo_mixing_delay"), soundStereoDelay->currentText() == QStringLiteral("-") ? QStringLiteral("0") : soundStereoDelay->currentText());
        }
        settings.insert(QStringLiteral("sound_frequency"), QString::number(qBound(8000, soundFrequency->currentText().toInt(), 768000)));
        settings.insert(QStringLiteral("sound_interpol"), soundInterpolationConfigValue(soundInterpolation->currentText()));
        settings.insert(QStringLiteral("sound_filter"), soundFilterConfigValue(soundFilter->currentText()));
        settings.insert(QStringLiteral("sound_filter_type"), soundFilterTypeConfigValue(soundFilter->currentText()));
        const int swapIndex = soundSwap->currentIndex();
        settings.insert(QStringLiteral("sound_stereo_swap_paula"), (swapIndex & 1) ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("sound_stereo_swap_ahi"), (swapIndex & 2) ? QStringLiteral("true") : QStringLiteral("false"));
        for (int i = 0; i < FloppySoundDriveCount; i++) {
            settings.insert(QStringLiteral("floppy%1sound").arg(i), QString::number(floppySoundTypeConfigValue(i)));
            settings.insert(QStringLiteral("floppy%1soundvolume_empty").arg(i), QString::number(floppySoundEmptyAttenuationValue(i)));
            settings.insert(QStringLiteral("floppy%1soundvolume_disk").arg(i), QString::number(floppySoundDiskAttenuationValue(i)));
        }
        if (rtgMem->currentText() != QStringLiteral("None")) {
            settings.insert(QStringLiteral("gfxcard_size"), QString::number(megabytesFromText(rtgMem->currentText())));
            settings.insert(QStringLiteral("gfxcard_type"), rtgType->currentText());
            const QString options = rtgOptionsValue();
            if (!options.isEmpty()) {
                settings.insert(QStringLiteral("gfxcard_options"), options);
            }
        }
        settings.insert(QStringLiteral("gfx_filter_autoscale_rtg"), rtgScaleConfigValue(rtgScale->isChecked(), rtgCenter->isChecked(), rtgIntegerScale->isChecked()));
        settings.insert(QStringLiteral("gfx_backbuffers_rtg"), rtgBufferConfigValue(rtgBuffers->currentText()));
        if (rtgRefreshRate->currentText() == QStringLiteral("Chipset")) {
            settings.insert(QStringLiteral("gfx_refreshrate_rtg"), QStringLiteral("0"));
        } else if (rtgRefreshRate->currentText() != QStringLiteral("Default")) {
            settings.insert(QStringLiteral("gfx_refreshrate_rtg"), rtgRefreshRate->currentText());
        }
        settings.insert(QStringLiteral("gfxcard_hardware_vblank"), rtgHardwareVBlank->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("gfxcard_hardware_sprite"), rtgHardwareSprite->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("gfxcard_multithread"), rtgMultithread->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("rtg_modes"), QStringLiteral("0x%1").arg(rtgModeMask(), 0, 16));
        if (!windowWidth->text().isEmpty()) {
            settings.insert(QStringLiteral("gfx_width_windowed"), windowWidth->text());
        }
        if (!windowHeight->text().isEmpty()) {
            settings.insert(QStringLiteral("gfx_height_windowed"), windowHeight->text());
        }
        const QString fullscreenText = fullscreenResolution->currentText();
        if (fullscreenText == QStringLiteral("Native")) {
            settings.insert(QStringLiteral("gfx_width_fullscreen"), QStringLiteral("native"));
            settings.insert(QStringLiteral("gfx_height_fullscreen"), QStringLiteral("native"));
        } else {
            const QStringList parts = fullscreenText.split(QLatin1Char('x'));
            if (parts.size() == 2) {
                settings.insert(QStringLiteral("gfx_width_fullscreen"), parts.value(0));
                settings.insert(QStringLiteral("gfx_height_fullscreen"), parts.value(1));
            }
        }
        settings.insert(QStringLiteral("gfx_resize_windowed"), windowResize->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("gfx_fullscreen_amiga"), fullscreenModeConfigValue(nativeMode->currentText()));
        settings.insert(QStringLiteral("gfx_fullscreen_picasso"), fullscreenModeConfigValue(rtgMode->currentText()));
        settings.insert(QStringLiteral("gfx_resolution"), displayResolution->currentText());
        settings.insert(QStringLiteral("gfx_linemode"), lineModeConfigValue(displayLineModeButtons->checkedId()));
        settings.insert(QStringLiteral("gfx_center_horizontal"), displayCenterHorizontal->isChecked() ? QStringLiteral("simple") : QStringLiteral("none"));
        settings.insert(QStringLiteral("gfx_center_vertical"), displayCenterVertical->isChecked() ? QStringLiteral("simple") : QStringLiteral("none"));
        settings.insert(QStringLiteral("gfx_flickerfixer"), displayFlickerFixer->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("gfx_lores_mode"), displayLoresSmoothed->isChecked() ? QStringLiteral("filtered") : QStringLiteral("normal"));
        for (int i = 0; i < MaxCdSlots; i++) {
            const QString value = cdSlotConfigValue(cdSlotState(i));
            if (!value.isEmpty()) {
                settings.insert(QStringLiteral("cdimage%1").arg(i), value);
            }
        }
        settings.insert(QStringLiteral("cd_speed"), cdSpeedTurbo->isChecked() ? QStringLiteral("0") : QStringLiteral("100"));
        for (int i = 0; i < 4; i++) {
            settings.insert(QStringLiteral("joyport%1").arg(i), joyportDeviceConfigValue(portDevice[i]->currentText()));
        }
        for (int i = 0; i < 2; i++) {
            settings.insert(QStringLiteral("joyport%1autofire").arg(i), autofireConfigValue(portAutofire[i]->currentText()));
            const QString mode = joyportModeConfigValue(portMode[i]->currentText());
            if (!mode.isEmpty()) {
                settings.insert(QStringLiteral("joyport%1mode").arg(i), mode);
            }
        }
        settings.insert(QStringLiteral("input.mouse_speed"), QString::number(mouseSpeed->value()));
        settings.insert(QStringLiteral("input.autoswitch"), portAutoswitch->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        const int untrapMode = mouseUntrapMode->currentIndex();
        settings.insert(QStringLiteral("middle_mouse"), (untrapMode == 1 || untrapMode == 3) ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("magic_mouse"), (untrapMode == 2 || untrapMode == 3) ? QStringLiteral("true") : QStringLiteral("false"));
        settings.insert(QStringLiteral("magic_mousecursor"), magicMouseCursorConfigValue(magicMouseCursor->currentText()));
        QString absoluteMouse = QStringLiteral("none");
        if (tabletMode->currentText() == QStringLiteral("Tablet emulation")) {
            absoluteMouse = QStringLiteral("tablet");
        } else if (virtualMouseDriver->isChecked()) {
            absoluteMouse = QStringLiteral("mousehack");
        }
        settings.insert(QStringLiteral("absolute_mouse"), absoluteMouse);
        settings.insert(QStringLiteral("tablet_library"), tabletLibrary->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        return settings;
    }

    QStringList uiOwnedKeys() const
    {
        return {
            QStringLiteral("kickstart_rom_file"),
            QStringLiteral("kickstart_ext_rom_file"),
            QStringLiteral("cart_file"),
            QStringLiteral("flash_file"),
            QStringLiteral("rtc_file"),
            QStringLiteral("maprom"),
            QStringLiteral("kickshifter"),
            QStringLiteral("boot_rom_uae"),
            QStringLiteral("uaeboard"),
            QStringLiteral("romboard_options"),
            QStringLiteral("romboard2_options"),
            QStringLiteral("romboard3_options"),
            QStringLiteral("romboard4_options"),
            QStringLiteral("floppy0"),
            QStringLiteral("floppy1"),
            QStringLiteral("floppy2"),
            QStringLiteral("floppy3"),
            QStringLiteral("floppy0type"),
            QStringLiteral("floppy1type"),
            QStringLiteral("floppy2type"),
            QStringLiteral("floppy3type"),
            QStringLiteral("floppy0wp"),
            QStringLiteral("floppy1wp"),
            QStringLiteral("floppy2wp"),
            QStringLiteral("floppy3wp"),
            QStringLiteral("nr_floppies"),
            QStringLiteral("floppy_speed"),
            QStringLiteral("chipset"),
            QStringLiteral("chipset_compatible"),
            QStringLiteral("cpu_model"),
            QStringLiteral("cpu_speed"),
            QStringLiteral("cpu_throttle"),
            QStringLiteral("cpu_compatible"),
            QStringLiteral("fpu_model"),
            QStringLiteral("cpu_24bit_addressing"),
            QStringLiteral("mmu_model"),
            QStringLiteral("cpu_no_unimplemented"),
            QStringLiteral("fpu_no_unimplemented"),
            QStringLiteral("fpu_strict"),
            QStringLiteral("fpu_softfloat"),
            QStringLiteral("fpu_msvc_long_double"),
            QStringLiteral("cpu_multiplier"),
            QStringLiteral("cpu_frequency"),
            QStringLiteral("chipmem_size"),
            QStringLiteral("fastmem_size"),
            QStringLiteral("bogomem_size"),
            QStringLiteral("z3mem_size"),
            QStringLiteral("cachesize"),
            QStringLiteral("compfpu"),
            QStringLiteral("comp_constjump"),
            QStringLiteral("comp_flushmode"),
            QStringLiteral("comp_nf"),
            QStringLiteral("comp_catchfault"),
            QStringLiteral("comp_trustbyte"),
            QStringLiteral("comp_trustword"),
            QStringLiteral("comp_trustlong"),
            QStringLiteral("comp_trustnaddr"),
            QStringLiteral("sound_output"),
            QStringLiteral("sound_auto"),
            QStringLiteral("sound_volume"),
            QStringLiteral("sound_volume_paula"),
            QStringLiteral("sound_volume_cd"),
            QStringLiteral("sound_volume_ahi"),
            QStringLiteral("sound_volume_midi"),
            QStringLiteral("sound_volume_genlock"),
            QStringLiteral("sound_max_buff"),
            QStringLiteral("sound_channels"),
            QStringLiteral("sound_stereo_separation"),
            QStringLiteral("sound_stereo_mixing_delay"),
            QStringLiteral("sound_frequency"),
            QStringLiteral("sound_interpol"),
            QStringLiteral("sound_filter"),
            QStringLiteral("sound_filter_type"),
            QStringLiteral("sound_stereo_swap_paula"),
            QStringLiteral("sound_stereo_swap_ahi"),
            QStringLiteral("floppy0sound"),
            QStringLiteral("floppy1sound"),
            QStringLiteral("floppy2sound"),
            QStringLiteral("floppy3sound"),
            QStringLiteral("floppy0soundvolume_empty"),
            QStringLiteral("floppy1soundvolume_empty"),
            QStringLiteral("floppy2soundvolume_empty"),
            QStringLiteral("floppy3soundvolume_empty"),
            QStringLiteral("floppy0soundvolume_disk"),
            QStringLiteral("floppy1soundvolume_disk"),
            QStringLiteral("floppy2soundvolume_disk"),
            QStringLiteral("floppy3soundvolume_disk"),
            QStringLiteral("gfxcard_size"),
            QStringLiteral("gfxcard_type"),
            QStringLiteral("gfxcard_options"),
            QStringLiteral("gfx_filter_autoscale_rtg"),
            QStringLiteral("gfx_backbuffers_rtg"),
            QStringLiteral("gfx_refreshrate_rtg"),
            QStringLiteral("gfxcard_hardware_vblank"),
            QStringLiteral("gfxcard_hardware_sprite"),
            QStringLiteral("gfxcard_multithread"),
            QStringLiteral("rtg_modes"),
            QStringLiteral("gfx_width_windowed"),
            QStringLiteral("gfx_height_windowed"),
            QStringLiteral("gfx_width_fullscreen"),
            QStringLiteral("gfx_height_fullscreen"),
            QStringLiteral("gfx_resize_windowed"),
            QStringLiteral("gfx_fullscreen_amiga"),
            QStringLiteral("gfx_fullscreen_picasso"),
            QStringLiteral("gfx_resolution"),
            QStringLiteral("gfx_linemode"),
            QStringLiteral("gfx_center_horizontal"),
            QStringLiteral("gfx_center_vertical"),
            QStringLiteral("gfx_flickerfixer"),
            QStringLiteral("gfx_lores_mode"),
            QStringLiteral("cdimage0"),
            QStringLiteral("cdimage1"),
            QStringLiteral("cdimage2"),
            QStringLiteral("cdimage3"),
            QStringLiteral("cdimage4"),
            QStringLiteral("cdimage5"),
            QStringLiteral("cdimage6"),
            QStringLiteral("cdimage7"),
            QStringLiteral("cd_speed"),
            QStringLiteral("joyport0"),
            QStringLiteral("joyport1"),
            QStringLiteral("joyport2"),
            QStringLiteral("joyport3"),
            QStringLiteral("joyport0autofire"),
            QStringLiteral("joyport1autofire"),
            QStringLiteral("joyport0mode"),
            QStringLiteral("joyport1mode"),
            QStringLiteral("input.mouse_speed"),
            QStringLiteral("input.autoswitch"),
            QStringLiteral("middle_mouse"),
            QStringLiteral("magic_mouse"),
            QStringLiteral("magic_mousecursor"),
            QStringLiteral("absolute_mouse"),
            QStringLiteral("tablet_library")
        };
    }

    QStringList uiOwnedMountKeys() const
    {
        return {
            QStringLiteral("filesystem2"),
            QStringLiteral("hardfile2"),
            QStringLiteral("uaehf0"),
            QStringLiteral("uaehf1"),
            QStringLiteral("uaehf2"),
            QStringLiteral("uaehf3"),
            QStringLiteral("uaehf4"),
            QStringLiteral("uaehf5"),
            QStringLiteral("uaehf6"),
            QStringLiteral("uaehf7")
        };
    }

    WinUaeQtConfig mergedConfig() const
    {
        WinUaeQtConfig config = loadedConfig;
        config.applySettings(currentSettings(), uiOwnedKeys());
        config.applyRepeatedSettings(currentMountSettings(), uiOwnedMountKeys());
        return config;
    }

    int enabledFloppyCount() const
    {
        int count = 0;
        for (int i = 0; i < 4; i++) {
            if (dfEnable[i]->isChecked() && floppyTypeConfigValue(dfType[i]->currentText()) >= 0) {
                count = i + 1;
            }
        }
        return qMax(1, count);
    }

    void loadConfigDialog()
    {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Load configuration"), configsPath->text(), QStringLiteral("WinUAE configuration (*.uae);;All files (*)"));
        if (!path.isEmpty()) {
            loadConfig(path);
        }
    }

    void saveConfigDialog()
    {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save configuration"), configsPath->text(), QStringLiteral("WinUAE configuration (*.uae);;All files (*)"));
        if (!path.isEmpty()) {
            saveConfig(path);
        }
    }

    void loadConfig(const QString &path)
    {
        WinUaeQtConfig config;
        QString error;
        if (!config.load(path, &error)) {
            QMessageBox::warning(this, windowTitle(), error);
            return;
        }
        if (mountedDrives) {
            mountedDrives->clear();
        }
        clearCdSlots();
        for (const WinUaeQtConfig::Setting &setting : config.orderedSettings()) {
            applySetting(setting.key, setting.value);
        }
        updateMountButtons();
        loadedConfig = config;
        configPath->setText(path);
        configName->setCurrentText(QFileInfo(path).completeBaseName());
        status->setText(QStringLiteral("Loaded %1").arg(path));
    }

    void applySetting(const QString &key, const QString &value)
    {
        if (key == QStringLiteral("kickstart_rom_file")) {
            romFile->setCurrentText(value);
        } else if (key == QStringLiteral("kickstart_ext_rom_file")) {
            extendedRomFile->setCurrentText(value);
        } else if (key == QStringLiteral("cart_file")) {
            cartFile->setCurrentText(value);
        } else if (key == QStringLiteral("flash_file")) {
            flashFile->setText(value);
        } else if (key == QStringLiteral("rtc_file")) {
            rtcFile->setText(value);
        } else if (key == QStringLiteral("maprom")) {
            bool ok = false;
            const uint mapValue = value.toUInt(&ok, 0);
            mapRom->setChecked(ok && mapValue != 0);
        } else if (key == QStringLiteral("kickshifter")) {
            kickShifter->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("boot_rom_uae")) {
            if (value.compare(QStringLiteral("disabled"), Qt::CaseInsensitive) == 0) {
                uaeBoardType->setCurrentText(QStringLiteral("ROM disabled"));
            } else if (uaeBoardType->currentText() == QStringLiteral("ROM disabled")) {
                uaeBoardType->setCurrentText(QStringLiteral("Original UAE (FS + F0 ROM)"));
            }
        } else if (key == QStringLiteral("uaeboard")) {
            if (value.compare(QStringLiteral("disabled"), Qt::CaseInsensitive) == 0
                || value.compare(QStringLiteral("disabled_off"), Qt::CaseInsensitive) == 0) {
                if (uaeBoardType->currentText() != QStringLiteral("ROM disabled")) {
                    uaeBoardType->setCurrentText(QStringLiteral("Original UAE (FS + F0 ROM)"));
                }
            } else {
                uaeBoardType->setCurrentText(uaeBoardText(value));
            }
        } else if (romBoardIndexFromKey(key) >= 0) {
            applyCustomRomBoard(romBoardIndexFromKey(key), value);
        } else if (key == QStringLiteral("floppy_speed")) {
            floppySpeed->setValue(floppySpeedSliderPosition(value.toInt()));
            updateFloppySpeedLabel();
        } else if (const int drive = floppyKeyDrive(key, QStringLiteral("type")); drive >= 0) {
            const int driveType = value.toInt();
            dfEnable[drive]->setChecked(driveType >= 0);
            dfType[drive]->setCurrentText(floppyTypeText(driveType));
            syncFloppyDriveToQuick(drive);
        } else if (const int drive = floppyKeyDrive(key, QStringLiteral("wp")); drive >= 0) {
            dfWriteProtect[drive]->setChecked(configBoolValue(value));
            syncFloppyDriveToQuick(drive);
        } else if (const int drive = floppyKeyDrive(key); drive >= 0) {
            dfEnable[drive]->setChecked(true);
            dfPath[drive]->setCurrentText(value);
            syncFloppyDriveToQuick(drive);
        } else if (key.startsWith(QStringLiteral("uaehf"))) {
            WinUaeQtMountEntry entry;
            if (parseWinUaeQtUaehfMountValue(value, &entry)) {
                addMountEntry(entry);
            }
        } else if (key == QStringLiteral("filesystem2")) {
            WinUaeQtMountEntry entry;
            if (parseWinUaeQtFilesystem2MountValue(value, &entry)) {
                addMountEntry(entry);
            }
        } else if (key == QStringLiteral("hardfile2")) {
            WinUaeQtMountEntry entry;
            if (parseWinUaeQtHardfile2MountValue(value, &entry)) {
                addMountEntry(entry);
            }
        } else if (key.startsWith(QStringLiteral("cdimage"))) {
            bool ok = false;
            const int slot = key.mid(7).toInt(&ok);
            if (ok && slot >= 0 && slot < MaxCdSlots) {
                ensureCdSlots();
                cdSlots[slot] = cdSlotFromConfigValue(value);
                if (slot == currentCdSlot) {
                    loadCdSlotToUi(slot);
                }
            }
        } else if (key == QStringLiteral("cd_speed")) {
            cdSpeedTurbo->setChecked(value == QStringLiteral("0"));
        } else if (key == QStringLiteral("chipset")) {
            chipset->setCurrentText(value.toUpper());
        } else if (key == QStringLiteral("chipset_compatible")) {
            chipsetCompatible->setCurrentText(value);
            quickModel->setCurrentText(value);
        } else if (key == QStringLiteral("cpu_model")) {
            setCpuButton(value.toInt());
        } else if (key == QStringLiteral("cpu_speed")) {
            const bool fastest = value.compare(QStringLiteral("max"), Qt::CaseInsensitive) == 0;
            if (QAbstractButton *button = cpuSpeedButtons->button(fastest ? 1 : 0)) {
                button->setChecked(true);
            }
        } else if (key == QStringLiteral("cpu_throttle")) {
            cpuSpeed->setValue(qBound(cpuSpeed->minimum(), qRound(value.toDouble() / 100.0), cpuSpeed->maximum()));
            updateCpuSpeedLabel();
        } else if (key == QStringLiteral("cpu_compatible")) {
            moreCompatible->setChecked(configBoolValue(value));
            updateCpuControlState();
        } else if (key == QStringLiteral("fpu_model")) {
            setFpuButton(value.toInt());
        } else if (key == QStringLiteral("cpu_24bit_addressing")) {
            cpu24Bit->setChecked(value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
            updateCpuControlState();
        } else if (key == QStringLiteral("mmu_model")) {
            const QString lower = value.toLower();
            const int id = lower.startsWith(QStringLiteral("68ec")) ? 2 : (value.toInt() > 0 ? 1 : 0);
            if (QAbstractButton *button = mmuButtons->button(id)) {
                button->setChecked(true);
            }
        } else if (key == QStringLiteral("cpu_no_unimplemented")) {
            cpuUnimplemented->setChecked(!configBoolValue(value));
        } else if (key == QStringLiteral("fpu_no_unimplemented")) {
            fpuUnimplemented->setChecked(!configBoolValue(value));
        } else if (key == QStringLiteral("fpu_strict")) {
            fpuStrict->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("fpu_softfloat")) {
            if (configBoolValue(value)) {
                fpuMode->setCurrentText(QStringLiteral("Softfloat (80-bit)"));
            } else if (fpuMode->currentText() == QStringLiteral("Softfloat (80-bit)")) {
                fpuMode->setCurrentText(QStringLiteral("Host (64-bit)"));
            }
        } else if (key == QStringLiteral("fpu_msvc_long_double")) {
            if (configBoolValue(value)) {
                fpuMode->setCurrentText(QStringLiteral("Host (80-bit)"));
            } else if (fpuMode->currentText() == QStringLiteral("Host (80-bit)")) {
                fpuMode->setCurrentText(QStringLiteral("Host (64-bit)"));
            }
        } else if (key == QStringLiteral("cpu_multiplier")) {
            cpuFrequency->setCurrentText(cpuMultiplierText(value.toInt()));
        } else if (key == QStringLiteral("cpu_frequency")) {
            cpuFrequency->setCurrentText(QStringLiteral("Custom"));
            cpuFrequencyCustom->setText(QString::number(value.toDouble() / 1000000.0, 'f', 6));
        } else if (key == QStringLiteral("cachesize")) {
            const int size = value.toInt();
            jit->setChecked(size > 0);
            jitCache->setValue(jitCachePositionFromSize(size));
            updateCpuControlState();
        } else if (key == QStringLiteral("compfpu")) {
            jitFpu->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("comp_constjump")) {
            jitConstJump->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("comp_flushmode")) {
            jitHardFlush->setChecked(value.compare(QStringLiteral("hard"), Qt::CaseInsensitive) == 0 || configBoolValue(value));
        } else if (key == QStringLiteral("comp_nf")) {
            jitNoFlags->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("comp_catchfault")) {
            jitCatchFault->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("comp_trustbyte")
            || key == QStringLiteral("comp_trustword")
            || key == QStringLiteral("comp_trustlong")
            || key == QStringLiteral("comp_trustnaddr")) {
            if (QAbstractButton *button = jitTrust->button(value.compare(QStringLiteral("indirect"), Qt::CaseInsensitive) == 0 ? 1 : 0)) {
                button->setChecked(true);
            }
        } else if (key == QStringLiteral("chipmem_size")) {
            const QMap<int, QString> map = { { 1, QStringLiteral("512 KB") }, { 2, QStringLiteral("1 MB") }, { 4, QStringLiteral("2 MB") }, { 8, QStringLiteral("4 MB") }, { 16, QStringLiteral("8 MB") } };
            chipMem->setCurrentText(map.value(value.toInt(), QStringLiteral("2 MB")));
        } else if (key == QStringLiteral("fastmem_size")) {
            z2Fast->setCurrentText(value == QStringLiteral("0") ? QStringLiteral("None") : value + QStringLiteral(" MB"));
        } else if (key == QStringLiteral("z3mem_size")) {
            z3Fast->setCurrentText(value == QStringLiteral("0") ? QStringLiteral("None") : value + QStringLiteral(" MB"));
        } else if (key == QStringLiteral("gfxcard_size")) {
            rtgMem->setCurrentText(value == QStringLiteral("0") ? QStringLiteral("None") : value + QStringLiteral(" MB"));
        } else if (key == QStringLiteral("gfxcard_type")) {
            rtgType->setCurrentText(value);
        } else if (key == QStringLiteral("gfxcard_options")) {
            applyRtgOptionsValue(value);
        } else if (key == QStringLiteral("gfx_filter_autoscale_rtg")) {
            applyRtgScaleValue(value);
        } else if (key == QStringLiteral("gfx_backbuffers_rtg")) {
            rtgBuffers->setCurrentText(rtgBufferText(value));
        } else if (key == QStringLiteral("gfx_refreshrate_rtg")) {
            if (value == QStringLiteral("0")) {
                rtgRefreshRate->setCurrentText(QStringLiteral("Chipset"));
            } else {
                rtgRefreshRate->setCurrentText(value);
            }
        } else if (key == QStringLiteral("gfxcard_hardware_vblank")) {
            rtgHardwareVBlank->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("gfxcard_hardware_sprite")) {
            rtgHardwareSprite->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("gfxcard_multithread")) {
            rtgMultithread->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("rtg_modes")) {
            bool ok = false;
            int mask = value.toInt(&ok, 0);
            if (!ok) {
                mask = RtgDefaultModeMask;
            }
            rtg8Bit->setCurrentText(rtg8BitText(mask));
            rtg16Bit->setCurrentText(rtg16BitText(mask));
            rtg24Bit->setCurrentText(rtg24BitText(mask));
            rtg32Bit->setCurrentText(rtg32BitText(mask));
        } else if (key == QStringLiteral("sound_output")) {
            if (QAbstractButton *button = soundOutputButtons->button(soundOutputId(value))) {
                button->setChecked(true);
            }
            updateSoundControlState();
        } else if (key == QStringLiteral("sound_auto")) {
            soundAutomatic->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("sound_volume")) {
            soundMasterVolume->setValue(100 - qBound(0, value.toInt(), 100));
        } else if (key == QStringLiteral("sound_volume_paula")
            || key == QStringLiteral("sound_volume_cd")
            || key == QStringLiteral("sound_volume_ahi")
            || key == QStringLiteral("sound_volume_midi")
            || key == QStringLiteral("sound_volume_genlock")) {
            const QMap<QString, int> volumeIndex = {
                { QStringLiteral("sound_volume_paula"), 0 },
                { QStringLiteral("sound_volume_cd"), 1 },
                { QStringLiteral("sound_volume_ahi"), 2 },
                { QStringLiteral("sound_volume_midi"), 3 },
                { QStringLiteral("sound_volume_genlock"), 4 }
            };
            const int index = volumeIndex.value(key, 0);
            soundVolumeAttenuation[index] = qBound(0, value.toInt(), 100);
            if (index == currentSoundVolume) {
                loadSelectedSoundVolume();
            }
        } else if (key == QStringLiteral("sound_max_buff")) {
            soundBufferSize->setValue(soundBufferIndexFromSize(value.toInt()));
        } else if (key == QStringLiteral("sound_channels")) {
            soundChannels->setCurrentText(soundChannelText(value));
            updateSoundControlState();
        } else if (key == QStringLiteral("sound_stereo_separation")) {
            soundStereoSeparation->setCurrentText(QStringLiteral("%1%").arg(qBound(0, value.toInt(), 10) * 10));
        } else if (key == QStringLiteral("sound_stereo_mixing_delay")) {
            const int delay = qBound(0, value.toInt(), 10);
            soundStereoDelay->setCurrentText(delay > 0 ? QString::number(delay) : QStringLiteral("-"));
        } else if (key == QStringLiteral("sound_frequency")) {
            soundFrequency->setCurrentText(QString::number(qBound(8000, value.toInt(), 768000)));
        } else if (key == QStringLiteral("sound_interpol")) {
            soundInterpolation->setCurrentText(soundInterpolationText(value));
        } else if (key == QStringLiteral("sound_filter")) {
            soundFilter->setCurrentText(soundFilterText(value, soundFilterTypeConfigValue(soundFilter->currentText())));
        } else if (key == QStringLiteral("sound_filter_type")) {
            soundFilter->setCurrentText(soundFilterText(soundFilterConfigValue(soundFilter->currentText()), value));
        } else if (key == QStringLiteral("sound_stereo_swap_paula")) {
            setSoundSwapBit(true, configBoolValue(value));
        } else if (key == QStringLiteral("sound_stereo_swap_ahi")) {
            setSoundSwapBit(false, configBoolValue(value));
        } else if (key == QStringLiteral("floppy_volume")) {
            const int attenuation = qBound(0, value.toInt(), 100);
            for (int i = 0; i < FloppySoundDriveCount; i++) {
                floppySoundEmptyAttenuation[i] = attenuation;
                floppySoundDiskAttenuation[i] = attenuation;
            }
            loadSelectedFloppySound();
        } else if (const int drive = floppyKeyDrive(key, QStringLiteral("sound")); drive >= 0) {
            floppySoundTypeValue[drive] = qBound(0, value.toInt(), 1);
            if (drive == currentFloppySoundDrive) {
                loadSelectedFloppySound();
            }
        } else if (const int drive = floppyKeyDrive(key, QStringLiteral("soundvolume_empty")); drive >= 0) {
            floppySoundEmptyAttenuation[drive] = qBound(0, value.toInt(), 100);
            if (drive == currentFloppySoundDrive) {
                loadSelectedFloppySound();
            }
        } else if (const int drive = floppyKeyDrive(key, QStringLiteral("soundvolume_disk")); drive >= 0) {
            floppySoundDiskAttenuation[drive] = qBound(0, value.toInt(), 100);
            if (drive == currentFloppySoundDrive) {
                loadSelectedFloppySound();
            }
        } else if (key.startsWith(QStringLiteral("joyport")) && key.size() == 8) {
            bool ok = false;
            const int port = key.mid(7, 1).toInt(&ok);
            if (ok && port >= 0 && port < 4) {
                portDevice[port]->setCurrentText(joyportDeviceText(value, port < 2));
            }
        } else if (key.startsWith(QStringLiteral("joyport")) && key.endsWith(QStringLiteral("autofire"))) {
            bool ok = false;
            const int port = key.mid(7, 1).toInt(&ok);
            if (ok && port >= 0 && port < 2) {
                portAutofire[port]->setCurrentText(autofireText(value));
            }
        } else if (key.startsWith(QStringLiteral("joyport")) && key.endsWith(QStringLiteral("mode"))) {
            bool ok = false;
            const int port = key.mid(7, 1).toInt(&ok);
            if (ok && port >= 0 && port < 2) {
                portMode[port]->setCurrentText(joyportModeText(value));
            }
        } else if (key == QStringLiteral("input.mouse_speed")) {
            mouseSpeed->setValue(qBound(mouseSpeed->minimum(), value.toInt(), mouseSpeed->maximum()));
        } else if (key == QStringLiteral("input.autoswitch")) {
            portAutoswitch->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("middle_mouse")) {
            setMouseUntrapBit(true, configBoolValue(value));
        } else if (key == QStringLiteral("magic_mouse")) {
            setMouseUntrapBit(false, configBoolValue(value));
        } else if (key == QStringLiteral("magic_mousecursor")) {
            magicMouseCursor->setCurrentText(magicMouseCursorText(value));
        } else if (key == QStringLiteral("absolute_mouse")) {
            if (value.compare(QStringLiteral("tablet"), Qt::CaseInsensitive) == 0) {
                virtualMouseDriver->setChecked(true);
                tabletMode->setCurrentText(QStringLiteral("Tablet emulation"));
            } else if (value.compare(QStringLiteral("mousehack"), Qt::CaseInsensitive) == 0) {
                virtualMouseDriver->setChecked(true);
                tabletMode->setCurrentText(QStringLiteral("-"));
            } else {
                virtualMouseDriver->setChecked(false);
                tabletMode->setCurrentText(QStringLiteral("-"));
            }
            updateMouseExtraState();
        } else if (key == QStringLiteral("tablet_library")) {
            tabletLibrary->setChecked(configBoolValue(value));
            updateMouseExtraState();
        } else if (key == QStringLiteral("gfx_width_windowed")) {
            windowWidth->setText(value);
        } else if (key == QStringLiteral("gfx_height_windowed")) {
            windowHeight->setText(value);
        } else if (key == QStringLiteral("gfx_width_fullscreen")) {
            fullscreenResolution->setProperty("winuae_width", value);
            if (value == QStringLiteral("native")) {
                fullscreenResolution->setCurrentText(QStringLiteral("Native"));
            } else {
                const QString currentHeight = fullscreenResolution->property("winuae_height").toString();
                if (!currentHeight.isEmpty() && currentHeight != QStringLiteral("native")) {
                    fullscreenResolution->setCurrentText(value + QStringLiteral("x") + currentHeight);
                }
            }
        } else if (key == QStringLiteral("gfx_height_fullscreen")) {
            fullscreenResolution->setProperty("winuae_height", value);
            const QString currentWidth = fullscreenResolution->property("winuae_width").toString();
            if (value == QStringLiteral("native") || currentWidth == QStringLiteral("native")) {
                fullscreenResolution->setCurrentText(QStringLiteral("Native"));
            } else if (!currentWidth.isEmpty()) {
                fullscreenResolution->setCurrentText(currentWidth + QStringLiteral("x") + value);
            }
        } else if (key == QStringLiteral("gfx_resize_windowed")) {
            windowResize->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("gfx_fullscreen_amiga")) {
            nativeMode->setCurrentText(fullscreenModeText(value));
        } else if (key == QStringLiteral("gfx_fullscreen_picasso")) {
            rtgMode->setCurrentText(fullscreenModeText(value));
        } else if (key == QStringLiteral("gfx_resolution")) {
            displayResolution->setCurrentText(value);
        } else if (key == QStringLiteral("gfx_linemode")) {
            if (QAbstractButton *button = displayLineModeButtons->button(lineModeId(value))) {
                button->setChecked(true);
            }
        } else if (key == QStringLiteral("gfx_center_horizontal")) {
            displayCenterHorizontal->setChecked(value.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0 && !value.isEmpty());
        } else if (key == QStringLiteral("gfx_center_vertical")) {
            displayCenterVertical->setChecked(value.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0 && !value.isEmpty());
        } else if (key == QStringLiteral("gfx_flickerfixer")) {
            displayFlickerFixer->setChecked(configBoolValue(value));
        } else if (key == QStringLiteral("gfx_lores_mode")) {
            displayLoresSmoothed->setChecked(value.compare(QStringLiteral("filtered"), Qt::CaseInsensitive) == 0);
        }
    }

    void saveConfig(const QString &path)
    {
        WinUaeQtConfig config = mergedConfig();
        QString error;
        if (!config.save(path, &error)) {
            QMessageBox::warning(this, windowTitle(), error);
            return;
        }
        loadedConfig = config;
        configPath->setText(path);
        configName->setCurrentText(QFileInfo(path).completeBaseName());
        status->setText(QStringLiteral("Saved %1").arg(path));
    }

    void startEmulator()
    {
        const WinUaeQtConfig config = mergedConfig();
        const QStringList validationErrors = config.validateForLaunch();
        if (!validationErrors.isEmpty()) {
            QMessageBox::warning(this, windowTitle(), validationErrors.join(QLatin1Char('\n')));
            navigation->setCurrentItem(navigation->topLevelItem(4));
            return;
        }

        if (startMode == StartMode::ReturnConfig) {
            result.status = WinUaeQtLauncherStatus::StartRequested;
            result.config = config;
            accept();
            return;
        }

        const QString program = emulatorPath->text();
        if (program.isEmpty() || !QFileInfo::exists(program)) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("Emulator executable not found."));
            return;
        }

        QString error;
        if (!launcherBackend.start(program, config, &error)) {
            QMessageBox::warning(this, windowTitle(), error);
            return;
        }
        status->setText(QStringLiteral("Started %1").arg(program));
    }
};

static void setupApplicationStyle(QApplication &app)
{
    if (QStyle *style = QStyleFactory::create(QStringLiteral("Windows"))) {
        app.setStyle(style);
    } else if (QStyle *style = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app.setStyle(style);
    }
    QString family = QStringLiteral("MS Sans Serif");
    const QStringList families = QFontDatabase::families();
    if (!families.contains(family)) {
        const QStringList fallbacks = {
            QStringLiteral("Microsoft Sans Serif"),
            QStringLiteral("Tahoma"),
            QStringLiteral("Arial")
        };
        for (const QString &candidate : fallbacks) {
            if (families.contains(candidate)) {
                family = candidate;
                break;
            }
        }
        if (!families.contains(family)) {
            family = app.font().family();
        }
    }
    QFont font(family);
    font.setPixelSize(12);
    app.setFont(font);
    app.setStyleSheet(QStringLiteral(
        "QDialog, QWidget#page, QStackedWidget#pageStack { background: #f0f0f0; }"
        "QFrame#outerFrame { border: 1px solid #808080; background: #f0f0f0; }"
        "QTreeWidget { background: white; border: 1px solid #7f9db9; }"
        "QGroupBox { margin-top: 14px; padding: 9px 6px 6px 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; font-size: 13px; }"
        "QPushButton { min-height: 20px; padding: 1px 10px; }"
        "QLineEdit, QComboBox { min-height: 22px; }"
        "QLabel#statusLine { padding-left: 6px; background: #f0f0f0; }"
    ));
}

int runWinUaeQtLauncher(QApplication &app)
{
    setupApplicationStyle(app);
    WinUaeQtDialog dialog;
    dialog.show();
    return app.exec();
}

int runWinUaeQtLauncher(int argc, char **argv)
{
    QApplication app(argc, argv);
    return runWinUaeQtLauncher(app);
}

WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(QApplication &app)
{
    setupApplicationStyle(app);
    WinUaeQtDialog dialog(WinUaeQtDialog::StartMode::ReturnConfig);
    if (dialog.exec() == QDialog::Accepted) {
        return dialog.launcherResult();
    }

    WinUaeQtLauncherResult result;
    result.status = WinUaeQtLauncherStatus::Canceled;
    return result;
}

WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(int argc, char **argv)
{
    QApplication app(argc, argv);
    return runWinUaeQtLauncherForConfig(app);
}
