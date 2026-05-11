#pragma once

#include <QString>

struct WinUaeQtMountEntry {
    QString kind;
    QString device;
    QString volume;
    QString path;
    QString rawConfig;
    QString hardfileGeometry;
    QString hardfileTail;
    bool readOnly = false;
    int bootPri = 0;
};

QString winUaeQtConfigAccessValue(bool readOnly);
QString winUaeQtConfigEscapeMin(QString value);
QString winUaeQtSanitizedAmigaName(QString value, const QString &fallback, bool uppercase);
QString winUaeQtDefaultVolumeName(const QString &path);
bool parseWinUaeQtUaehfMountValue(const QString &value, WinUaeQtMountEntry *entry);
bool parseWinUaeQtFilesystem2MountValue(const QString &value, WinUaeQtMountEntry *entry);
bool parseWinUaeQtHardfile2MountValue(const QString &value, WinUaeQtMountEntry *entry);
QString serializeWinUaeQtFilesystem2MountValue(const WinUaeQtMountEntry &entry);
QString serializeWinUaeQtHardfile2MountValue(const WinUaeQtMountEntry &entry);
QString serializeWinUaeQtUaehfDirectoryMountValue(const WinUaeQtMountEntry &entry);
QString serializeWinUaeQtUaehfHardfileMountValue(const WinUaeQtMountEntry &entry);
