#pragma once

#include <QVector>
#include <QString>

class QWidget;

struct WinUaeQtFloppyBridgeProfile {
    unsigned int id;
    QString name;
};

bool winuaeQtFloppyBridgeAvailable();
QVector<WinUaeQtFloppyBridgeProfile> winuaeQtFloppyBridgeProfiles();
bool winuaeQtManageFloppyBridgeProfiles(QWidget *parent);
QString winuaeQtFloppyBridgeInformation();
