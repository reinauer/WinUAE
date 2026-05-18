#pragma once

#include <QString>

QString winUaeQtEnvString(const char *name);
QString winUaeQtDefaultDataPath();
QString winUaeQtDefaultDataSubPath(const QString &name);
QString winUaeQtExpandUnixPath(QString path);
QString winUaeQtExpandedPathText(const QString &path);
