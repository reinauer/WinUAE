#include "config.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QDebug>

static bool writeText(const QString &path, const QString &text)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning().noquote() << file.errorString();
        return false;
    }
    QTextStream out(&file);
    out << text;
    return true;
}

static QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning().noquote() << file.errorString();
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

static bool requireContains(const QString &text, const QString &needle)
{
    if (text.contains(needle)) {
        return true;
    }
    qWarning().noquote() << "missing expected text:" << needle;
    return false;
}

static bool requireNotContains(const QString &text, const QString &needle)
{
    if (!text.contains(needle)) {
        return true;
    }
    qWarning().noquote() << "unexpected text:" << needle;
    return false;
}

int main()
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qWarning().noquote() << "failed to create temporary directory";
        return 1;
    }

    const QString inputPath = QDir(tempDir.path()).filePath(QStringLiteral("input.uae"));
    const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("output.uae"));
    const QString input =
        QStringLiteral("; keep this comment\n")
        + QStringLiteral("unknown_setting=keep-me\n")
        + QStringLiteral("kickstart_rom_file=/old.rom\n")
        + QStringLiteral("kickstart_ext_rom_file=/old-ext.rom\n")
        + QStringLiteral("malformed line without separator\n")
        + QStringLiteral("# keep this too\n")
        + QStringLiteral("chipset=ecs\n");

    if (!writeText(inputPath, input)) {
        return 1;
    }

    WinUaeQtConfig config;
    QString error;
    if (!config.load(inputPath, &error)) {
        qWarning().noquote() << error;
        return 1;
    }

    WinUaeQtConfig::Settings edited;
    edited.insert(QStringLiteral("kickstart_rom_file"), QStringLiteral("/new.rom"));
    edited.insert(QStringLiteral("chipset"), QStringLiteral("aga"));
    edited.insert(QStringLiteral("cpu_model"), QStringLiteral("68020"));
    config.applySettings(edited, {
        QStringLiteral("kickstart_rom_file"),
        QStringLiteral("kickstart_ext_rom_file"),
        QStringLiteral("chipset"),
        QStringLiteral("cpu_model")
    });

    if (!config.save(outputPath, &error)) {
        qWarning().noquote() << error;
        return 1;
    }

    const QString output = readText(outputPath);
    bool ok = true;
    ok = requireContains(output, QStringLiteral("; keep this comment\n")) && ok;
    ok = requireContains(output, QStringLiteral("unknown_setting=keep-me\n")) && ok;
    ok = requireContains(output, QStringLiteral("malformed line without separator\n")) && ok;
    ok = requireContains(output, QStringLiteral("# keep this too\n")) && ok;
    ok = requireContains(output, QStringLiteral("kickstart_rom_file=/new.rom\n")) && ok;
    ok = requireContains(output, QStringLiteral("chipset=aga\n")) && ok;
    ok = requireContains(output, QStringLiteral("cpu_model=68020\n")) && ok;
    ok = requireNotContains(output, QStringLiteral("kickstart_ext_rom_file=")) && ok;
    ok = requireNotContains(output, QStringLiteral("/old.rom")) && ok;

    const QStringList args = config.commandArguments();
    ok = args.contains(QStringLiteral("unknown_setting=keep-me")) && ok;
    ok = args.contains(QStringLiteral("kickstart_rom_file=/new.rom")) && ok;
    ok = !args.contains(QStringLiteral("kickstart_ext_rom_file=/old-ext.rom")) && ok;

    return ok ? 0 : 1;
}
