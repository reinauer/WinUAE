#include "mount_config.h"

#include <QDebug>

static bool require(bool condition, const char *message)
{
    if (!condition) {
        qWarning().noquote() << message;
    }
    return condition;
}

static bool requireText(const QString &actual, const QString &expected, const char *field)
{
    if (actual == expected) {
        return true;
    }
    qWarning().noquote() << field << "expected" << expected << "got" << actual;
    return false;
}

static bool requireInt(int actual, int expected, const char *field)
{
    if (actual == expected) {
        return true;
    }
    qWarning().noquote() << field << "expected" << expected << "got" << actual;
    return false;
}

static bool testDirectoryMount()
{
    WinUaeQtMountEntry entry;
    bool ok = parseWinUaeQtUaehfMountValue(QStringLiteral("dir,ro,DH1:Work:\"/tmp/Work,Disk\",5"), &entry);
    ok = require(ok, "directory mount did not parse") && ok;
    ok = requireText(entry.kind, QStringLiteral("dir"), "directory kind") && ok;
    ok = requireText(entry.device, QStringLiteral("DH1"), "directory device") && ok;
    ok = requireText(entry.volume, QStringLiteral("Work"), "directory volume") && ok;
    ok = requireText(entry.path, QStringLiteral("/tmp/Work,Disk"), "directory path") && ok;
    ok = require(entry.readOnly, "directory read-only") && ok;
    ok = requireInt(entry.bootPri, 5, "directory boot priority") && ok;
    ok = requireText(serializeWinUaeQtDirectoryMountValue(entry), QStringLiteral("dir,ro,DH1:Work:\"/tmp/Work,Disk\",5"), "directory serialized value") && ok;
    return ok;
}

static bool testHardfileMount()
{
    const QString config = QStringLiteral("hdf,rw,DH2:/tmp/disk.hdf,32,1,2,512,0,,uae0");
    WinUaeQtMountEntry entry;
    bool ok = parseWinUaeQtUaehfMountValue(config, &entry);
    ok = require(ok, "hardfile mount did not parse") && ok;
    ok = requireText(entry.kind, QStringLiteral("hdf"), "hardfile kind") && ok;
    ok = requireText(entry.device, QStringLiteral("DH2"), "hardfile device") && ok;
    ok = requireText(entry.path, QStringLiteral("/tmp/disk.hdf"), "hardfile path") && ok;
    ok = require(!entry.readOnly, "hardfile read/write") && ok;
    ok = requireText(serializeWinUaeQtHardfileMountValue(entry), config, "hardfile serialized value") && ok;
    return ok;
}

int main()
{
    bool ok = true;
    ok = testDirectoryMount() && ok;
    ok = testHardfileMount() && ok;
    ok = requireText(winUaeQtSanitizedAmigaName(QStringLiteral("dh:0, "), QStringLiteral("DH0"), true), QStringLiteral("DH_0_"), "sanitized name") && ok;
    return ok ? 0 : 1;
}
