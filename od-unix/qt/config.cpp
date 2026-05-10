#include "config.h"

#include <QFile>
#include <QTextStream>

#include <utility>

WinUaeQtConfig::WinUaeQtConfig(Settings settings)
    : configSettings(std::move(settings))
{
}

WinUaeQtConfig::DocumentLine WinUaeQtConfig::makeSettingLine(const QString &key, const QString &value)
{
    DocumentLine line;
    line.text = QStringLiteral("%1=%2").arg(key, value);
    line.key = key;
    line.value = value;
    line.setting = true;
    return line;
}

const WinUaeQtConfig::Settings &WinUaeQtConfig::settings() const
{
    return configSettings;
}

void WinUaeQtConfig::setSettings(Settings settings)
{
    configSettings = std::move(settings);
    documentLines.clear();
    documentLoaded = false;
}

void WinUaeQtConfig::applySettings(const Settings &settings, const QStringList &ownedKeys)
{
    for (const QString &key : ownedKeys) {
        const QString value = settings.value(key);
        if (settings.contains(key) && !value.isEmpty()) {
            configSettings.insert(key, value);
        } else {
            configSettings.remove(key);
        }
    }

    if (!documentLoaded) {
        return;
    }

    QList<DocumentLine> updated;
    QStringList emitted;
    for (const DocumentLine &line : std::as_const(documentLines)) {
        if (!line.setting || !ownedKeys.contains(line.key)) {
            updated.append(line);
            continue;
        }

        const QString value = settings.value(line.key);
        if (settings.contains(line.key) && !value.isEmpty() && !emitted.contains(line.key)) {
            updated.append(makeSettingLine(line.key, value));
            emitted.append(line.key);
        }
    }

    for (const QString &key : ownedKeys) {
        const QString value = settings.value(key);
        if (settings.contains(key) && !value.isEmpty() && !emitted.contains(key)) {
            updated.append(makeSettingLine(key, value));
            emitted.append(key);
        }
    }

    documentLines = updated;
}

QString WinUaeQtConfig::value(const QString &key, const QString &defaultValue) const
{
    return configSettings.value(key, defaultValue);
}

void WinUaeQtConfig::setValue(const QString &key, const QString &value)
{
    Settings singleSetting;
    if (!value.isEmpty()) {
        singleSetting.insert(key, value);
    }
    applySettings(singleSetting, QStringList { key });
}

void WinUaeQtConfig::removeValue(const QString &key)
{
    applySettings(Settings(), QStringList { key });
}

bool WinUaeQtConfig::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    Settings loaded;
    QList<DocumentLine> lines;
    while (!file.atEnd()) {
        QString text = QString::fromUtf8(file.readLine());
        while (text.endsWith(QLatin1Char('\n')) || text.endsWith(QLatin1Char('\r'))) {
            text.chop(1);
        }

        DocumentLine line;
        line.text = text;

        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char(';')) || trimmed.startsWith(QLatin1Char('#'))) {
            lines.append(line);
            continue;
        }

        const int separator = text.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            lines.append(line);
            continue;
        }

        const QString key = text.left(separator).trimmed();
        const QString value = text.mid(separator + 1).trimmed();
        if (!key.isEmpty()) {
            line.key = key;
            line.value = value;
            line.setting = true;
            loaded.insert(key, value);
        }
        lines.append(line);
    }

    configSettings = loaded;
    documentLines = lines;
    documentLoaded = true;
    return true;
}

bool WinUaeQtConfig::save(const QString &path, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    QTextStream out(&file);
    if (documentLoaded) {
        for (const DocumentLine &line : documentLines) {
            out << line.text << "\n";
        }
        return true;
    }

    out << "; WinUAE Unix Qt configuration\n";
    for (auto it = configSettings.constBegin(); it != configSettings.constEnd(); ++it) {
        if (!it.value().isEmpty()) {
            out << it.key() << "=" << it.value() << "\n";
        }
    }
    return true;
}

QStringList WinUaeQtConfig::commandArguments() const
{
    QStringList args;
    for (auto it = configSettings.constBegin(); it != configSettings.constEnd(); ++it) {
        if (!it.value().isEmpty()) {
            args << QStringLiteral("-s") << QStringLiteral("%1=%2").arg(it.key(), it.value());
        }
    }
    return args;
}

QStringList WinUaeQtConfig::validateForLaunch() const
{
    QStringList errors;
    if (value(QStringLiteral("kickstart_rom_file")).trimmed().isEmpty()) {
        errors << QStringLiteral("Main ROM file is required.");
    }
    return errors;
}
