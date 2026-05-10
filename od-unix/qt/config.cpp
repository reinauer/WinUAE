#include "config.h"

#include <QFile>
#include <QTextStream>

#include <utility>

WinUaeQtConfig::WinUaeQtConfig(Settings settings)
    : configSettings(std::move(settings))
{
}

const WinUaeQtConfig::Settings &WinUaeQtConfig::settings() const
{
    return configSettings;
}

void WinUaeQtConfig::setSettings(Settings settings)
{
    configSettings = std::move(settings);
}

QString WinUaeQtConfig::value(const QString &key, const QString &defaultValue) const
{
    return configSettings.value(key, defaultValue);
}

void WinUaeQtConfig::setValue(const QString &key, const QString &value)
{
    if (value.isEmpty()) {
        configSettings.remove(key);
    } else {
        configSettings.insert(key, value);
    }
}

void WinUaeQtConfig::removeValue(const QString &key)
{
    configSettings.remove(key);
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
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const int separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            continue;
        }

        const QString key = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1).trimmed();
        if (!key.isEmpty()) {
            loaded.insert(key, value);
        }
    }

    configSettings = loaded;
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
