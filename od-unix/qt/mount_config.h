#pragma once

#include <QString>

struct WinUaeQtMountEntry {
    QString kind;
    QString device;
    QString volume;
    QString path;
    QString rawConfig;
    bool readOnly = false;
    int bootPri = 0;
};

QString winUaeQtConfigAccessValue(bool readOnly);
QString winUaeQtConfigEscapeMin(QString value);
QString winUaeQtSanitizedAmigaName(QString value, const QString &fallback, bool uppercase);
QString winUaeQtDefaultVolumeName(const QString &path);
bool parseWinUaeQtUaehfMountValue(const QString &value, WinUaeQtMountEntry *entry);
QString serializeWinUaeQtDirectoryMountValue(const WinUaeQtMountEntry &entry);
QString serializeWinUaeQtHardfileMountValue(const WinUaeQtMountEntry &entry);
