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

static bool requireCount(const QString &text, const QString &needle, int expected)
{
    int count = 0;
    int offset = 0;
    for (;;) {
        offset = text.indexOf(needle, offset);
        if (offset < 0) {
            break;
        }
        count++;
        offset += needle.size();
    }
    if (count == expected) {
        return true;
    }
    qWarning().noquote() << needle << "expected count" << expected << "got" << count;
    return false;
}

static bool requireBefore(const QString &text, const QString &first, const QString &second)
{
    const int firstIndex = text.indexOf(first);
    const int secondIndex = text.indexOf(second);
    if (firstIndex >= 0 && secondIndex >= 0 && firstIndex < secondIndex) {
        return true;
    }
    qWarning().noquote() << first << "was not before" << second;
    return false;
}

static bool requireArgBefore(const QStringList &args, const QString &first, const QString &second)
{
    const int firstIndex = args.indexOf(first);
    const int secondIndex = args.indexOf(second);
    if (firstIndex >= 0 && secondIndex >= 0 && firstIndex < secondIndex) {
        return true;
    }
    qWarning().noquote() << first << "argument was not before" << second;
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
        + QStringLiteral("chipset=ecs\n")
        + QStringLiteral("filesystem2=rw,DH0:Old:/old/System,0\n")
        + QStringLiteral("; keep mount comment\n")
        + QStringLiteral("filesystem2=rw,DH1:Old2:/old/Work,0\n")
        + QStringLiteral("hardfile2=rw,DH2:/old/disk.hdf,32,1,2,512,0,,uae0\n");

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
    edited.insert(QStringLiteral("unix.ui.config_path"), QStringLiteral("/configs"));
    config.applySettings(edited, {
        QStringLiteral("kickstart_rom_file"),
        QStringLiteral("kickstart_ext_rom_file"),
        QStringLiteral("chipset"),
        QStringLiteral("cpu_model"),
        QStringLiteral("unix.ui.config_path")
    });
    config.applyRepeatedSettings({
        { QStringLiteral("filesystem2"), QStringLiteral("rw,DH0:System:/new/System,0") },
        { QStringLiteral("filesystem2"), QStringLiteral("ro,DH1:Work:\"/new/Work,Disk\",5") }
    }, {
        QStringLiteral("filesystem2"),
        QStringLiteral("hardfile2"),
        QStringLiteral("uaehf0")
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
    ok = requireContains(output, QStringLiteral("; keep mount comment\n")) && ok;
    ok = requireContains(output, QStringLiteral("kickstart_rom_file=/new.rom\n")) && ok;
    ok = requireContains(output, QStringLiteral("chipset=aga\n")) && ok;
    ok = requireContains(output, QStringLiteral("cpu_model=68020\n")) && ok;
    ok = requireContains(output, QStringLiteral("unix.ui.config_path=/configs\n")) && ok;
    ok = requireContains(output, QStringLiteral("filesystem2=rw,DH0:System:/new/System,0\n")) && ok;
    ok = requireContains(output, QStringLiteral("filesystem2=ro,DH1:Work:\"/new/Work,Disk\",5\n")) && ok;
    ok = requireCount(output, QStringLiteral("filesystem2="), 2) && ok;
    ok = requireBefore(output, QStringLiteral("filesystem2=rw,DH0:System:/new/System,0\n"), QStringLiteral("filesystem2=ro,DH1:Work:\"/new/Work,Disk\",5\n")) && ok;
    ok = requireNotContains(output, QStringLiteral("kickstart_ext_rom_file=")) && ok;
    ok = requireNotContains(output, QStringLiteral("/old.rom")) && ok;
    ok = requireNotContains(output, QStringLiteral("/old/System")) && ok;
    ok = requireNotContains(output, QStringLiteral("hardfile2=")) && ok;

    const QStringList args = config.commandArguments();
    ok = args.contains(QStringLiteral("unknown_setting=keep-me")) && ok;
    ok = args.contains(QStringLiteral("kickstart_rom_file=/new.rom")) && ok;
    ok = !args.contains(QStringLiteral("unix.ui.config_path=/configs")) && ok;
    ok = args.contains(QStringLiteral("filesystem2=rw,DH0:System:/new/System,0")) && ok;
    ok = args.contains(QStringLiteral("filesystem2=ro,DH1:Work:\"/new/Work,Disk\",5")) && ok;
    ok = requireArgBefore(args, QStringLiteral("filesystem2=rw,DH0:System:/new/System,0"), QStringLiteral("filesystem2=ro,DH1:Work:\"/new/Work,Disk\",5")) && ok;
    ok = !args.contains(QStringLiteral("kickstart_ext_rom_file=/old-ext.rom")) && ok;
    ok = requireCount(args.join(QLatin1Char('\n')), QStringLiteral("filesystem2="), 2) && ok;

    return ok ? 0 : 1;
}
