#pragma once

#include <QMap>
#include <QList>
#include <QString>
#include <QStringList>

class WinUaeQtConfig {
public:
    using Settings = QMap<QString, QString>;

    WinUaeQtConfig() = default;
    explicit WinUaeQtConfig(Settings settings);

    const Settings &settings() const;
    void setSettings(Settings settings);
    void applySettings(const Settings &settings, const QStringList &ownedKeys);

    QString value(const QString &key, const QString &defaultValue = QString()) const;
    void setValue(const QString &key, const QString &value);
    void removeValue(const QString &key);

    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;

    QStringList commandArguments() const;
    QStringList validateForLaunch() const;

private:
    struct DocumentLine {
        QString text;
        QString key;
        QString value;
        bool setting = false;
    };

    static DocumentLine makeSettingLine(const QString &key, const QString &value);

    Settings configSettings;
    QList<DocumentLine> documentLines;
    bool documentLoaded = false;
};
