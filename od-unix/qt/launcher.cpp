#include <QtWidgets>

#include "config.h"
#include "launcher.h"
#include "launcher_backend.h"
#include "mount_config.h"

#ifndef WINUAE_UNIX_SOURCE_DIR
#define WINUAE_UNIX_SOURCE_DIR "."
#endif

#ifndef WINUAE_UNIX_BUILD_DIR
#define WINUAE_UNIX_BUILD_DIR "."
#endif

#ifndef WINUAE_UNIX_VERSION_MAJOR
#define WINUAE_UNIX_VERSION_MAJOR 0
#endif

#ifndef WINUAE_UNIX_VERSION_MINOR
#define WINUAE_UNIX_VERSION_MINOR 0
#endif

#ifndef WINUAE_UNIX_VERSION_REVISION
#define WINUAE_UNIX_VERSION_REVISION 0
#endif

static constexpr int FpuInternal = -1;
static constexpr int MaxMountEntries = 8;
static constexpr int MaxControllerUnits = 8;
static constexpr int MaxCdSlots = 8;

struct WinUaeQtCdSlot {
    QString path;
    QString type;
    bool inUse = false;
};

enum MountDataRole {
    MountKindRole = Qt::UserRole,
    MountDeviceRole,
    MountVolumeRole,
    MountPathRole,
    MountReadOnlyRole,
    MountBootPriRole,
    MountEmuUnitRole,
    MountRawConfigRole,
    MountHardfileGeometryRole,
    MountHardfileTailRole
};

static QStringList mountControllerParts(QString tail)
{
    if (!tail.startsWith(QLatin1Char(','))) {
        tail.prepend(QLatin1Char(','));
    }
    QStringList parts = winUaeQtConfigFieldList(tail);
    while (parts.size() < 2) {
        parts.append(QString());
    }
    return parts;
}

static QString mountControllerValue(const WinUaeQtMountEntry &entry, const QString &fallback)
{
    const QStringList parts = mountControllerParts(entry.hardfileTail);
    return parts.value(1).isEmpty() ? fallback : parts.value(1);
}

static QString mountTailWithController(const WinUaeQtMountEntry &entry, const QString &controller)
{
    QStringList parts = mountControllerParts(entry.hardfileTail);
    parts[1] = controller;
    return winUaeQtConfigJoinFields(parts);
}

static QString mountControllerFamily(const WinUaeQtMountEntry &entry, const QString &fallback)
{
    const QString value = mountControllerValue(entry, fallback).toLower();
    if (value.startsWith(QStringLiteral("scsi"))) {
        return QStringLiteral("SCSI (Auto)");
    }
    if (value.startsWith(QStringLiteral("ide"))) {
        return QStringLiteral("IDE (Auto)");
    }
    return QStringLiteral("UAE (uaehf.device)");
}

static int mountControllerUnit(const WinUaeQtMountEntry &entry, const QString &fallback)
{
    const QString value = mountControllerValue(entry, fallback).toLower();
    int index = 0;
    while (index < value.size() && !value.at(index).isDigit()) {
        index++;
    }
    if (index >= value.size()) {
        return 0;
    }
    return qBound(0, value.mid(index).toInt(), MaxControllerUnits - 1);
}

static QString mountControllerConfigValue(const QString &family, int unit)
{
    const int clampedUnit = qBound(0, unit, MaxControllerUnits - 1);
    if (family == QStringLiteral("SCSI (Auto)")) {
        return QStringLiteral("scsi%1").arg(clampedUnit);
    }
    if (family == QStringLiteral("IDE (Auto)")) {
        return QStringLiteral("ide%1").arg(qBound(0, clampedUnit, 3));
    }
    return QStringLiteral("uae%1").arg(clampedUnit);
}

static QString mountControllerDisplay(const WinUaeQtMountEntry &entry)
{
    const QString fallback = entry.kind == QStringLiteral("cd") ? QStringLiteral("ide0") : QStringLiteral("uae0");
    const QString value = mountControllerValue(entry, fallback).toUpper();
    if (value.startsWith(QStringLiteral("IDE"))) {
        return QStringLiteral("IDE:%1").arg(mountControllerUnit(entry, fallback));
    }
    if (value.startsWith(QStringLiteral("SCSI"))) {
        return QStringLiteral("SCSI:%1").arg(mountControllerUnit(entry, fallback));
    }
    if (value.startsWith(QStringLiteral("UAE"))) {
        return QStringLiteral("UAE:%1").arg(mountControllerUnit(entry, fallback));
    }
    return value;
}

static bool isIntegerText(const QString &value)
{
    bool ok = false;
    value.toInt(&ok);
    return ok;
}

static QStringList hardfileGeometryParts(const WinUaeQtMountEntry &entry)
{
    QStringList parts = entry.hardfileGeometry.split(QLatin1Char(','));
    while (parts.size() < 4) {
        parts.append(QStringLiteral("0"));
    }
    return parts;
}

static bool hardfileIsRdb(const WinUaeQtMountEntry &entry)
{
    const QStringList geometry = hardfileGeometryParts(entry);
    return geometry.value(0).toInt() == 0
        && geometry.value(1).toInt() == 0
        && geometry.value(2).toInt() == 0;
}

static bool hardfileHasPhysicalGeometry(const WinUaeQtMountEntry &entry)
{
    const QStringList parts = mountControllerParts(entry.hardfileTail);
    return parts.size() > 3
        && isIntegerText(parts.value(2))
        && parts.value(3).contains(QLatin1Char('/'));
}

static QStringList hardfilePhysicalGeometryParts(const WinUaeQtMountEntry &entry)
{
    QStringList geometry;
    if (hardfileHasPhysicalGeometry(entry)) {
        geometry = mountControllerParts(entry.hardfileTail).value(3).split(QLatin1Char('/'));
    }
    while (geometry.size() < 3) {
        geometry.append(QStringLiteral("0"));
    }
    return geometry;
}

static QString hardfileTailGeometryFile(const WinUaeQtMountEntry &entry)
{
    const QStringList parts = mountControllerParts(entry.hardfileTail);
    return hardfileHasPhysicalGeometry(entry) ? parts.value(4) : QString();
}

static int hardfileTailExtraStart(const WinUaeQtMountEntry &entry)
{
    return hardfileHasPhysicalGeometry(entry) ? 5 : 2;
}

static bool isManagedHardfileTailToken(const QString &token)
{
    static const QStringList managed = {
        QStringLiteral("HD"),
        QStringLiteral("CF"),
        QStringLiteral("SCSI1"),
        QStringLiteral("SCSI2"),
        QStringLiteral("SASIE"),
        QStringLiteral("SASI"),
        QStringLiteral("SASI_CHS"),
        QStringLiteral("ATA1"),
        QStringLiteral("ATA2+"),
        QStringLiteral("ATA2+S")
    };
    for (const QString &item : managed) {
        if (token.compare(item, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

static QStringList hardfilePreservedTailExtras(const WinUaeQtMountEntry &entry)
{
    const QStringList parts = mountControllerParts(entry.hardfileTail);
    QStringList extras;
    for (int i = hardfileTailExtraStart(entry); i < parts.size(); i++) {
        if (!parts.value(i).isEmpty() && !isManagedHardfileTailToken(parts.value(i))) {
            extras.append(parts.value(i));
        }
    }
    return extras;
}

static bool hardfileTailHasToken(const WinUaeQtMountEntry &entry, const QString &token)
{
    const QStringList parts = mountControllerParts(entry.hardfileTail);
    for (int i = hardfileTailExtraStart(entry); i < parts.size(); i++) {
        if (parts.value(i).compare(token, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

static QString hardfileFeatureText(const WinUaeQtMountEntry &entry, const QString &controllerFamily)
{
    if (controllerFamily == QStringLiteral("IDE (Auto)")) {
        if (hardfileTailHasToken(entry, QStringLiteral("ATA2+S"))) {
            return QStringLiteral("ATA-2+ Strict");
        }
        if (hardfileTailHasToken(entry, QStringLiteral("ATA2+"))) {
            return QStringLiteral("ATA-2+");
        }
        if (hardfileTailHasToken(entry, QStringLiteral("ATA1"))) {
            return QStringLiteral("ATA-1");
        }
    } else if (controllerFamily == QStringLiteral("SCSI (Auto)")) {
        if (hardfileTailHasToken(entry, QStringLiteral("SASI_CHS"))) {
            return QStringLiteral("SASI CHS");
        }
        if (hardfileTailHasToken(entry, QStringLiteral("SASI"))) {
            return QStringLiteral("SASI");
        }
        if (hardfileTailHasToken(entry, QStringLiteral("SCSI1"))) {
            return QStringLiteral("SCSI-1");
        }
        if (hardfileTailHasToken(entry, QStringLiteral("SCSI2"))) {
            return QStringLiteral("SCSI-2");
        }
    }
    return QStringLiteral("Default");
}

static QString hardfileFeatureToken(const QString &featureText)
{
    if (featureText == QStringLiteral("ATA-1")) {
        return QStringLiteral("ATA1");
    }
    if (featureText == QStringLiteral("ATA-2+ Strict")) {
        return QStringLiteral("ATA2+S");
    }
    if (featureText == QStringLiteral("SCSI-1")) {
        return QStringLiteral("SCSI1");
    }
    if (featureText == QStringLiteral("SASI")) {
        return QStringLiteral("SASI");
    }
    if (featureText == QStringLiteral("SASI CHS")) {
        return QStringLiteral("SASI_CHS");
    }
    return QString();
}

static WinUaeQtCdSlot cdSlotFromConfigValue(const QString &value)
{
    WinUaeQtCdSlot slot;
    if (value.compare(QStringLiteral("autodetect"), Qt::CaseInsensitive) == 0) {
        slot.type = QStringLiteral("Autodetect");
        slot.inUse = true;
        return slot;
    }
    if (value.isEmpty()
        || value.compare(QStringLiteral("empty"), Qt::CaseInsensitive) == 0
        || value == QStringLiteral(".")) {
        slot.type = QStringLiteral("Image file");
        return slot;
    }

    const QStringList fields = winUaeQtConfigFieldList(value);
    slot.path = fields.value(0);
    slot.type = fields.value(1).compare(QStringLiteral("image"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("Image file")
        : QStringLiteral("Image file");
    slot.inUse = !slot.path.isEmpty();
    return slot;
}

static QString cdSlotConfigValue(const WinUaeQtCdSlot &slot)
{
    if (!slot.inUse && slot.path.isEmpty()) {
        return QString();
    }
    if (slot.type == QStringLiteral("Autodetect") && slot.path.isEmpty()) {
        return QStringLiteral("autodetect");
    }
    if (slot.path.isEmpty()) {
        return QString();
    }
    return winUaeQtConfigEscapeMin(slot.path);
}

static QString sourceFile(const QString &relative)
{
    return QDir(QString::fromUtf8(WINUAE_UNIX_SOURCE_DIR)).filePath(relative);
}

static QString versionString()
{
    return QStringLiteral("WinUAE %1.%2.%3")
        .arg(WINUAE_UNIX_VERSION_MAJOR)
        .arg(WINUAE_UNIX_VERSION_MINOR)
        .arg(WINUAE_UNIX_VERSION_REVISION);
}

static QStringList contributorLines()
{
    return {
        QStringLiteral("Bernd Schmidt - The Grand-Master"),
        QStringLiteral("Sam Jordan - Custom-chip, floppy-DMA, etc."),
        QStringLiteral("Mathias Ortmann - Original WinUAE Main Guy, BSD Socket support"),
        QStringLiteral("Brian King - Picasso96 Support, Integrated GUI for WinUAE, previous WinUAE Main Guy"),
        QStringLiteral("Toni Wilen - Core updates, WinUAE Main Guy"),
        QStringLiteral("Gustavo Goedert / Peter Remmers / Michael Sontheimer / Tomi Hakala / Tim Gunn / Nemo Pohle - DOS Port Stuff"),
        QStringLiteral("Samuel Devulder / Olaf Barthel / Sam Jordan - Amiga Ports"),
        QStringLiteral("Krister Bergman - XFree86 and OS/2 Port"),
        QStringLiteral("A. Blanchard / Ernesto Corvi - MacOS Port"),
        QStringLiteral("Christian Bauer - BeOS Port"),
        QStringLiteral("Ian Stephenson - NextStep Port"),
        QStringLiteral("Peter Teichmann - Acorn/RiscOS Port"),
        QStringLiteral("Stefan Reinauer - ZorroII/III AutoConfig, Serial Support"),
        QStringLiteral("Christian Schmitt / Chris Hames - Serial Support"),
        QStringLiteral("Herman ten Brugge - 68020/68881 Emulation Code"),
        QStringLiteral("Tauno Taipaleenmaki - Various UAE-Control/UAE-Library Support"),
        QStringLiteral("Brett Eden / Tim Gunn / Paolo Besser / Nemo Pohle - Various Docs and Web-Sites"),
        QStringLiteral("Georg Veichtlbauer - Help File coordinator, German GUI"),
        QStringLiteral("Fulvio Leonardi - Italian translator for WinUAE"),
        QStringLiteral("Arnljot Arntsen, Bill Panagouleas, Cloanto, Zak Jennings - Hardware support"),
        QStringLiteral("Special thanks to Alexander Kneer and Tobias Abt (The Picasso96 Team)"),
        QStringLiteral("Steven Weiser - Postscript printing emulation idea and testing"),
        QStringLiteral("Peter Toth / Balazs Ratkai / Ivan Herczeg / Andras Arato - Hungarian translation"),
        QStringLiteral("Karsten Bock, Gavin Fance, Dirk Trowe and Christian Schindler - Freezer cartridge hardware support"),
        QStringLiteral("Mikko Nieminen - Demo compatibility testing"),
        QStringLiteral("Arabuusimiehet - [This information is on a need-to-know basis]"),
        QStringLiteral("Ross - Chipset torture test programs")
    };
}

static QIcon resourceIcon(const QString &name)
{
    const QString path = sourceFile(QStringLiteral("od-win32/resources/") + name);
    return QFileInfo::exists(path) ? QIcon(path) : QIcon();
}

static QLabel *label(const QString &text)
{
    QLabel *w = new QLabel(text);
    w->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return w;
}

static QComboBox *combo(const QStringList &items, const QString &current = QString())
{
    QComboBox *w = new QComboBox;
    w->setEditable(false);
    w->addItems(items);
    if (!current.isEmpty()) {
        const int index = w->findText(current);
        if (index >= 0) {
            w->setCurrentIndex(index);
        }
    }
    return w;
}

static QComboBox *pathCombo()
{
    QComboBox *w = new QComboBox;
    w->setEditable(true);
    w->setInsertPolicy(QComboBox::NoInsert);
    w->lineEdit()->setClearButtonEnabled(true);
    return w;
}

static int floppyTypeConfigValue(const QString &text)
{
    if (text == QStringLiteral("Disabled")) {
        return -1;
    }
    if (text == QStringLiteral("3.5 HD")) {
        return 1;
    }
    return 0;
}

static QString floppyTypeText(int value)
{
    if (value < 0) {
        return QStringLiteral("Disabled");
    }
    if (value == 1) {
        return QStringLiteral("3.5 HD");
    }
    return QStringLiteral("3.5 DD");
}

static bool configBoolValue(const QString &value)
{
    return value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0
        || value == QStringLiteral("1");
}

static void setComboTextIfChanged(QComboBox *combo, const QString &text)
{
    if (combo && combo->currentText() != text) {
        combo->setCurrentText(text);
    }
}

static void setCheckBoxIfChanged(QCheckBox *box, bool checked)
{
    if (box && box->isChecked() != checked) {
        box->setChecked(checked);
    }
}

static QPushButton *smallButton(const QString &text)
{
    QPushButton *w = new QPushButton(text);
    w->setFixedWidth(text == QStringLiteral("...") ? 24 : 34);
    return w;
}

static QGroupBox *groupBox(const QString &title, QLayout *layout)
{
    QGroupBox *box = new QGroupBox(title);
    QFont titleFont = box->font();
    titleFont.setPixelSize(13);
    box->setFont(titleFont);
    box->setLayout(layout);
    return box;
}

static void addBrowse(QComboBox *field, QWidget *parent, const QString &caption, const QString &filter)
{
    const QString path = QFileDialog::getOpenFileName(parent, caption, field->currentText(), filter);
    if (!path.isEmpty()) {
        field->setCurrentText(path);
    }
}

static QString envString(const char *name)
{
    return QString::fromLocal8Bit(qgetenv(name));
}

class WinUaeQtDialog final : public QDialog {
public:
    enum class StartMode {
        DetachedProcess,
        ReturnConfig
    };

    explicit WinUaeQtDialog(StartMode mode = StartMode::DetachedProcess, QWidget *parent = nullptr)
        : QDialog(parent),
          startMode(mode)
    {
        setWindowTitle(QStringLiteral("WinUAE Properties"));
        setWindowIcon(resourceIcon(QStringLiteral("winuae.ico")));
        resize(820, 560);
        setMinimumSize(760, 520);

        navigation = new QTreeWidget;
        navigation->setHeaderHidden(true);
        navigation->setRootIsDecorated(false);
        navigation->setIndentation(12);
        navigation->setIconSize(QSize(16, 16));
        navigation->setFixedWidth(166);

        pageStack = new QStackedWidget;
        pageStack->setObjectName(QStringLiteral("pageStack"));

        addPage(QStringLiteral("Configurations"), QStringLiteral("configfile.ico"), makeConfigurationsPage());
        addPage(QStringLiteral("Quickstart"), QStringLiteral("quickstart.ico"), makeQuickstartPage());
        addPage(QStringLiteral("CPU and FPU"), QStringLiteral("cpu.ico"), makeCpuPage());
        addPage(QStringLiteral("Chipset"), QStringLiteral("chip.ico"), makeChipsetPage());
        addPage(QStringLiteral("ROM"), QStringLiteral("chip.ico"), makeRomPage());
        addPage(QStringLiteral("RAM"), QStringLiteral("chip.ico"), makeMemoryPage());
        addPage(QStringLiteral("Floppy drives"), QStringLiteral("35floppy.ico"), makeFloppyPage());
        addPage(QStringLiteral("Hard drives"), QStringLiteral("drive.ico"), makeHardDrivesPage());
        addPage(QStringLiteral("Expansion boards"), QStringLiteral("expansion.ico"), makeExpansionPage());
        addPage(QStringLiteral("Display"), QStringLiteral("screen.ico"), makeDisplayPage());
        addPage(QStringLiteral("Sound"), QStringLiteral("sound.ico"), makeSoundPage());
        addPage(QStringLiteral("Game ports"), QStringLiteral("joystick.ico"), makeGamePortsPage());
        addPage(QStringLiteral("Input"), QStringLiteral("port.ico"), makeInputPage());
        addPage(QStringLiteral("Paths"), QStringLiteral("paths.ico"), makePathsPage());
        addPage(QStringLiteral("Miscellaneous"), QStringLiteral("misc.ico"), makeMiscPage());
        addPage(QStringLiteral("About"), QStringLiteral("amigainfo.ico"), makeAboutPage());

        connect(navigation, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
            if (!item) {
                return;
            }
            pageStack->setCurrentIndex(item->data(0, Qt::UserRole).toInt());
        });

        QFrame *outerFrame = new QFrame;
        outerFrame->setFrameShape(QFrame::Box);
        outerFrame->setObjectName(QStringLiteral("outerFrame"));
        QVBoxLayout *frameLayout = new QVBoxLayout(outerFrame);
        frameLayout->setContentsMargins(4, 4, 4, 4);
        frameLayout->addWidget(pageStack);

        QHBoxLayout *content = new QHBoxLayout;
        content->setContentsMargins(0, 0, 0, 0);
        content->setSpacing(5);
        content->addWidget(navigation);
        content->addWidget(outerFrame, 1);

        QPushButton *reset = new QPushButton(QStringLiteral("Reset"));
        QPushButton *quit = new QPushButton(QStringLiteral("Quit"));
        QPushButton *restart = new QPushButton(QStringLiteral("Restart"));
        QPushButton *errorLog = new QPushButton(QStringLiteral("Error log"));
        QPushButton *start = new QPushButton(QStringLiteral("Start"));
        QPushButton *cancel = new QPushButton(QStringLiteral("Cancel"));
        QPushButton *help = new QPushButton(QStringLiteral("Help"));
        restart->setVisible(false);
        errorLog->setVisible(false);
        help->setEnabled(false);
        start->setDefault(true);

        connect(reset, &QPushButton::clicked, this, [this]() { resetDefaults(); });
        connect(quit, &QPushButton::clicked, this, &QDialog::reject);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(start, &QPushButton::clicked, this, [this]() { startEmulator(); });

        QHBoxLayout *buttons = new QHBoxLayout;
        buttons->setContentsMargins(0, 0, 0, 0);
        buttons->addWidget(reset);
        buttons->addWidget(quit);
        buttons->addWidget(restart);
        buttons->addStretch();
        buttons->addWidget(errorLog);
        buttons->addWidget(start);
        buttons->addWidget(cancel);
        buttons->addWidget(help);

        status = new QLabel;
        status->setObjectName(QStringLiteral("statusLine"));
        status->setFrameShape(QFrame::StyledPanel);
        status->setMinimumHeight(22);

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(6, 6, 6, 6);
        root->setSpacing(5);
        root->addLayout(content, 1);
        root->addWidget(status);
        root->addLayout(buttons);

        resetDefaults();
        navigation->setCurrentItem(navigation->topLevelItem(1));
    }

    const WinUaeQtLauncherResult &launcherResult() const
    {
        return result;
    }

private:
    StartMode startMode = StartMode::DetachedProcess;
    QTreeWidget *navigation = nullptr;
    QStackedWidget *pageStack = nullptr;
    QLabel *status = nullptr;

    QComboBox *configName = nullptr;
    QLineEdit *configPath = nullptr;
    QLineEdit *configDescription = nullptr;

    QComboBox *quickModel = nullptr;
    QComboBox *quickConfiguration = nullptr;
    QComboBox *quickHostConfiguration = nullptr;
    QCheckBox *ntsc = nullptr;
    QSlider *compatibility = nullptr;
    QCheckBox *quickDfEnable[2] = {};
    QComboBox *quickDfType[2] = {};
    QComboBox *quickDfPath[2] = {};
    QCheckBox *quickDfWriteProtect[2] = {};

    QComboBox *romFile = nullptr;
    QComboBox *extendedRomFile = nullptr;
    QComboBox *cartFile = nullptr;
    QLineEdit *flashFile = nullptr;
    QLineEdit *rtcFile = nullptr;

    QComboBox *cpuModel = nullptr;
    QButtonGroup *cpuButtons = nullptr;
    QButtonGroup *fpuButtons = nullptr;
    QCheckBox *cpu24Bit = nullptr;
    QCheckBox *moreCompatible = nullptr;
    QCheckBox *jit = nullptr;
    QComboBox *chipset = nullptr;
    QComboBox *chipsetCompatible = nullptr;

    QComboBox *chipMem = nullptr;
    QComboBox *z2Fast = nullptr;
    QComboBox *slowMem = nullptr;
    QComboBox *z3Fast = nullptr;
    QComboBox *rtgMem = nullptr;
    QComboBox *rtgType = nullptr;

    QCheckBox *dfEnable[4] = {};
    QComboBox *dfType[4] = {};
    QComboBox *dfPath[4] = {};
    QCheckBox *dfWriteProtect[4] = {};
    QTreeWidget *mountedDrives = nullptr;
    QPushButton *addDirectoryMountButton = nullptr;
    QPushButton *addHardfileMountButton = nullptr;
    QPushButton *addHardDriveMountButton = nullptr;
    QPushButton *addCdMountButton = nullptr;
    QPushButton *addTapeMountButton = nullptr;
    QPushButton *propertiesMountButton = nullptr;
    QPushButton *removeMountButton = nullptr;
    QComboBox *cdSlotNumber = nullptr;
    QComboBox *cdSlotType = nullptr;
    QComboBox *cdSlotPath = nullptr;
    QCheckBox *cdSpeedTurbo = nullptr;
    QVector<WinUaeQtCdSlot> cdSlots;
    int currentCdSlot = 0;
    bool cdSlotUpdating = false;

    QLineEdit *windowWidth = nullptr;
    QLineEdit *windowHeight = nullptr;
    QCheckBox *windowResize = nullptr;
    QComboBox *nativeMode = nullptr;
    QComboBox *rtgMode = nullptr;
    QComboBox *soundOutput = nullptr;
    QComboBox *port0 = nullptr;
    QComboBox *port1 = nullptr;
    QLineEdit *emulatorPath = nullptr;
    QLineEdit *romsPath = nullptr;
    QLineEdit *configsPath = nullptr;
    WinUaeQtLauncherBackend launcherBackend;
    WinUaeQtConfig loadedConfig;
    WinUaeQtLauncherResult result;

    void addPage(const QString &title, const QString &icon, QWidget *page)
    {
        const int index = pageStack->addWidget(page);
        QTreeWidgetItem *item = new QTreeWidgetItem(navigation);
        item->setText(0, title);
        item->setIcon(0, resourceIcon(icon));
        item->setData(0, Qt::UserRole, index);
    }

    QWidget *makePage()
    {
        QWidget *page = new QWidget;
        page->setObjectName(QStringLiteral("page"));
        return page;
    }

    QWidget *makeConfigurationsPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(4);

        QTreeWidget *tree = new QTreeWidget;
        tree->setHeaderHidden(true);
        tree->setRootIsDecorated(true);
        QTreeWidgetItem *configs = new QTreeWidgetItem(tree, QStringList(QStringLiteral("Configurations")));
        configs->setIcon(0, resourceIcon(QStringLiteral("configfile.ico")));
        new QTreeWidgetItem(configs, QStringList(QStringLiteral("Default")));
        new QTreeWidgetItem(configs, QStringList(QStringLiteral("A1200 Install")));
        configs->setExpanded(true);
        root->addWidget(tree, 1);

        QGridLayout *search = new QGridLayout;
        search->setColumnStretch(1, 1);
        search->setColumnStretch(4, 1);
        search->addWidget(label(QStringLiteral("Search:")), 0, 0);
        search->addWidget(new QLineEdit, 0, 1);
        search->addWidget(smallButton(QStringLiteral("X")), 0, 2);
        search->addWidget(label(QStringLiteral("Filter:")), 0, 3);
        search->addWidget(combo({ QStringLiteral("All configurations"), QStringLiteral("Host"), QStringLiteral("Hardware") }), 0, 4);
        root->addLayout(search);

        configName = pathCombo();
        configDescription = new QLineEdit;
        configPath = new QLineEdit;
        configPath->setReadOnly(true);

        QGridLayout *details = new QGridLayout;
        details->setColumnStretch(1, 1);
        details->addWidget(label(QStringLiteral("Name:")), 0, 0);
        details->addWidget(configName, 0, 1);
        details->addWidget(configPath, 0, 2);
        details->addWidget(label(QStringLiteral("Description:")), 1, 0);
        details->addWidget(configDescription, 1, 1, 1, 2);
        root->addLayout(details);

        QHBoxLayout *buttons = new QHBoxLayout;
        QPushButton *load = new QPushButton(QStringLiteral("Load From..."));
        QPushButton *save = new QPushButton(QStringLiteral("Save As..."));
        buttons->addWidget(new QPushButton(QStringLiteral("Load")));
        buttons->addWidget(new QPushButton(QStringLiteral("Save")));
        buttons->addStretch();
        buttons->addWidget(load);
        buttons->addWidget(save);
        buttons->addWidget(new QPushButton(QStringLiteral("Delete")));
        root->addLayout(buttons);

        connect(load, &QPushButton::clicked, this, [this]() { loadConfigDialog(); });
        connect(save, &QPushButton::clicked, this, [this]() { saveConfigDialog(); });
        return page;
    }

    QWidget *makeQuickstartPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(6);

        quickModel = combo({
            QStringLiteral("A500"),
            QStringLiteral("A500+"),
            QStringLiteral("A600"),
            QStringLiteral("A1200"),
            QStringLiteral("A4000")
        }, QStringLiteral("A1200"));
        quickConfiguration = combo({
            QStringLiteral("Basic non-expanded configuration"),
            QStringLiteral("Expanded configuration"),
            QStringLiteral("Cycle-exact compatible")
        }, QStringLiteral("Expanded configuration"));
        ntsc = new QCheckBox(QStringLiteral("NTSC"));

        QGridLayout *hardware = new QGridLayout;
        hardware->setColumnStretch(1, 1);
        hardware->addWidget(label(QStringLiteral("Model:")), 0, 0);
        hardware->addWidget(quickModel, 0, 1);
        hardware->addWidget(ntsc, 0, 2);
        hardware->addWidget(label(QStringLiteral("Configuration:")), 1, 0);
        hardware->addWidget(quickConfiguration, 1, 1, 1, 2);
        root->addWidget(groupBox(QStringLiteral("Emulated Hardware"), hardware));

        compatibility = new QSlider(Qt::Horizontal);
        compatibility->setRange(0, 4);
        compatibility->setTickInterval(1);
        compatibility->setTickPosition(QSlider::TicksAbove);
        compatibility->setValue(1);

        QHBoxLayout *compat = new QHBoxLayout;
        compat->addWidget(new QLabel(QStringLiteral("Best compatibility")));
        compat->addWidget(compatibility, 1);
        compat->addWidget(new QLabel(QStringLiteral("Low compatibility")));
        root->addWidget(groupBox(QStringLiteral("Compatibility vs Required CPU Power"), compat));

        quickHostConfiguration = combo({
            QStringLiteral("Default"),
            QStringLiteral("Windowed"),
            QStringLiteral("Fullscreen")
        }, QStringLiteral("Default"));
        QGridLayout *host = new QGridLayout;
        host->setColumnStretch(1, 1);
        host->addWidget(label(QStringLiteral("Configuration:")), 0, 0);
        host->addWidget(quickHostConfiguration, 0, 1);
        root->addWidget(groupBox(QStringLiteral("Host Configuration"), host));

        QGridLayout *drives = new QGridLayout;
        drives->setColumnStretch(5, 1);
        addQuickDriveRow(drives, 0);
        addQuickDriveRow(drives, 1);
        root->addWidget(groupBox(QStringLiteral("Emulated Drives"), drives), 1);

        connect(quickModel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            applyModelPreset(quickModel->currentText());
        });
        return page;
    }

    void addQuickDriveRow(QGridLayout *layout, int drive)
    {
        quickDfEnable[drive] = new QCheckBox(QStringLiteral("Floppy drive DF%1:").arg(drive));
        QPushButton *select = new QPushButton(QStringLiteral("Select image file"));
        quickDfType[drive] = combo({ QStringLiteral("3.5 DD"), QStringLiteral("3.5 HD"), QStringLiteral("Disabled") });
        quickDfWriteProtect[drive] = new QCheckBox;
        QPushButton *info = smallButton(QStringLiteral("?"));
        QPushButton *eject = new QPushButton(QStringLiteral("Eject"));
        quickDfPath[drive] = pathCombo();
        quickDfPath[drive]->setMinimumWidth(300);
        const int row = drive * 2;
        layout->addWidget(quickDfEnable[drive], row, 0);
        layout->addWidget(select, row, 1);
        layout->addWidget(quickDfType[drive], row, 2);
        layout->addWidget(new QLabel(QStringLiteral("Write-protected")), row, 3);
        layout->addWidget(quickDfWriteProtect[drive], row, 4);
        layout->addWidget(info, row, 5);
        layout->addWidget(eject, row, 6);
        layout->addWidget(quickDfPath[drive], row + 1, 0, 1, 7);
        quickDfEnable[drive]->setChecked(drive == 0);
        info->setEnabled(false);
        connect(select, &QPushButton::clicked, this, [this, drive]() {
            addBrowse(quickDfPath[drive], this, QStringLiteral("Select floppy image"), QStringLiteral("Amiga disk images (*.adf *.adz *.ipf *.dms);;All files (*)"));
        });
        connect(eject, &QPushButton::clicked, this, [this, drive]() {
            quickDfPath[drive]->setCurrentText(QString());
        });
    }

    QWidget *makeCpuPage()
    {
        QWidget *page = makePage();
        QHBoxLayout *root = new QHBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(6);

        QVBoxLayout *left = new QVBoxLayout;
        cpuButtons = new QButtonGroup(this);
        QVBoxLayout *cpu = new QVBoxLayout;
        const QStringList cpus = { QStringLiteral("68000"), QStringLiteral("68010"), QStringLiteral("68020"), QStringLiteral("68030"), QStringLiteral("68040"), QStringLiteral("68060") };
        for (const QString &name : cpus) {
            QRadioButton *button = new QRadioButton(name);
            cpu->addWidget(button);
            cpuButtons->addButton(button, name.mid(2).toInt() + 68000);
            connect(button, &QRadioButton::clicked, this, [this]() {
                updateFpuControls();
            });
            if (name == QStringLiteral("68020")) {
                button->setChecked(true);
            }
        }
        cpu24Bit = new QCheckBox(QStringLiteral("24-bit addressing"));
        moreCompatible = new QCheckBox(QStringLiteral("More compatible"));
        jit = new QCheckBox(QStringLiteral("JIT"));
        cpu->addWidget(cpu24Bit);
        cpu->addWidget(moreCompatible);
        cpu->addWidget(jit);
        left->addWidget(groupBox(QStringLiteral("CPU"), cpu));

        fpuButtons = new QButtonGroup(this);
        QVBoxLayout *fpu = new QVBoxLayout;
        const QStringList fpus = { QStringLiteral("None"), QStringLiteral("68881"), QStringLiteral("68882"), QStringLiteral("CPU internal") };
        for (int i = 0; i < fpus.size(); i++) {
            QRadioButton *button = new QRadioButton(fpus[i]);
            fpu->addWidget(button);
            const int id = i == 1 ? 68881 : (i == 2 ? 68882 : (i == 3 ? FpuInternal : 0));
            fpuButtons->addButton(button, id);
            if (i == 0) {
                button->setChecked(true);
            }
        }
        updateFpuControls();
        left->addWidget(groupBox(QStringLiteral("FPU"), fpu));
        left->addStretch();

        QVBoxLayout *right = new QVBoxLayout;
        QVBoxLayout *speed = new QVBoxLayout;
        speed->addWidget(new QRadioButton(QStringLiteral("Fastest possible")));
        QRadioButton *approx = new QRadioButton(QStringLiteral("Approximate A500/A1200 or cycle-exact"));
        approx->setChecked(true);
        speed->addWidget(approx);
        QSlider *cpuSpeed = new QSlider(Qt::Horizontal);
        cpuSpeed->setRange(0, 10);
        cpuSpeed->setTickInterval(1);
        cpuSpeed->setTickPosition(QSlider::TicksAbove);
        speed->addWidget(cpuSpeed);
        right->addWidget(groupBox(QStringLiteral("CPU Emulation Speed"), speed));

        QGridLayout *cycle = new QGridLayout;
        cycle->addWidget(label(QStringLiteral("CPU Frequency")), 0, 0);
        cycle->addWidget(combo({ QStringLiteral("Default"), QStringLiteral("7 MHz"), QStringLiteral("14 MHz"), QStringLiteral("28 MHz") }), 0, 1);
        cycle->addWidget(new QLineEdit, 0, 2);
        right->addWidget(groupBox(QStringLiteral("Cycle-exact CPU Emulation Speed"), cycle));

        QGridLayout *jitBox = new QGridLayout;
        jitBox->addWidget(label(QStringLiteral("Cache size:")), 0, 0);
        jitBox->addWidget(new QSlider(Qt::Horizontal), 0, 1);
        jitBox->addWidget(new QLineEdit(QStringLiteral("0")), 0, 2);
        jitBox->addWidget(new QCheckBox(QStringLiteral("FPU support")), 1, 0);
        jitBox->addWidget(new QCheckBox(QStringLiteral("Constant jump")), 1, 1);
        jitBox->addWidget(new QCheckBox(QStringLiteral("No flags")), 1, 2);
        right->addWidget(groupBox(QStringLiteral("Advanced JIT Settings"), jitBox));
        right->addStretch();

        root->addLayout(left, 1);
        root->addLayout(right, 2);
        return page;
    }

    QWidget *makeChipsetPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);

        chipset = combo({ QStringLiteral("OCS"), QStringLiteral("ECS"), QStringLiteral("AGA") }, QStringLiteral("AGA"));
        chipsetCompatible = combo({
            QStringLiteral("A500"),
            QStringLiteral("A500+"),
            QStringLiteral("A600"),
            QStringLiteral("A1200"),
            QStringLiteral("A4000")
        }, QStringLiteral("A1200"));
        QGridLayout *basic = new QGridLayout;
        basic->setColumnStretch(1, 1);
        basic->addWidget(label(QStringLiteral("Chipset:")), 0, 0);
        basic->addWidget(chipset, 0, 1);
        basic->addWidget(label(QStringLiteral("Compatible:")), 1, 0);
        basic->addWidget(chipsetCompatible, 1, 1);
        basic->addWidget(new QCheckBox(QStringLiteral("Immediate blitter")), 2, 1);
        basic->addWidget(new QCheckBox(QStringLiteral("Cycle-exact")), 3, 1);
        root->addWidget(groupBox(QStringLiteral("Chipset"), basic));
        root->addStretch();
        return page;
    }

    QWidget *makeRomPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(6);

        romFile = pathCombo();
        extendedRomFile = pathCombo();
        QGridLayout *system = new QGridLayout;
        system->setColumnStretch(1, 1);
        addPathRow(system, 0, QStringLiteral("Main ROM file:"), romFile, QStringLiteral("Select main ROM file"), QStringLiteral("ROM files (*.rom *.bin);;All files (*)"));
        addPathRow(system, 1, QStringLiteral("Extended ROM file:"), extendedRomFile, QStringLiteral("Select extended ROM file"), QStringLiteral("ROM files (*.rom *.bin);;All files (*)"));
        system->addWidget(new QCheckBox(QStringLiteral("MapROM emulation")), 4, 0);
        system->addWidget(new QCheckBox(QStringLiteral("ShapeShifter support")), 4, 1);
        root->addWidget(groupBox(QStringLiteral("System ROM Settings"), system));

        QGridLayout *advanced = new QGridLayout;
        advanced->addWidget(combo({ QStringLiteral("Custom ROM") }), 0, 0);
        advanced->addWidget(label(QStringLiteral("Address range")), 0, 1);
        advanced->addWidget(new QLineEdit, 0, 2);
        advanced->addWidget(new QLineEdit, 0, 3);
        QLineEdit *customRom = new QLineEdit;
        customRom->setEnabled(false);
        advanced->addWidget(customRom, 1, 0, 1, 4);
        root->addWidget(groupBox(QStringLiteral("Advanced Custom ROM Settings"), advanced));

        cartFile = pathCombo();
        flashFile = new QLineEdit;
        rtcFile = new QLineEdit;
        QGridLayout *misc = new QGridLayout;
        misc->setColumnStretch(1, 1);
        addPathRow(misc, 0, QStringLiteral("Cartridge ROM file:"), cartFile, QStringLiteral("Select cartridge ROM file"), QStringLiteral("ROM files (*.rom *.bin);;All files (*)"));
        addLineBrowseRow(misc, 2, QStringLiteral("Flash RAM or A2286/A2386SX BIOS CMOS RAM file:"), flashFile);
        addLineBrowseRow(misc, 3, QStringLiteral("Real Time Clock file"), rtcFile);
        root->addWidget(groupBox(QStringLiteral("Miscellaneous"), misc), 1);
        return page;
    }

    QWidget *makeMemoryPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);

        chipMem = combo({ QStringLiteral("512 KB"), QStringLiteral("1 MB"), QStringLiteral("2 MB"), QStringLiteral("4 MB"), QStringLiteral("8 MB") }, QStringLiteral("2 MB"));
        z2Fast = combo({ QStringLiteral("None"), QStringLiteral("1 MB"), QStringLiteral("2 MB"), QStringLiteral("4 MB"), QStringLiteral("8 MB") });
        slowMem = combo({ QStringLiteral("None"), QStringLiteral("512 KB"), QStringLiteral("1 MB"), QStringLiteral("1.5 MB"), QStringLiteral("1.8 MB") });
        z3Fast = combo({ QStringLiteral("None"), QStringLiteral("8 MB"), QStringLiteral("16 MB"), QStringLiteral("32 MB"), QStringLiteral("64 MB"), QStringLiteral("128 MB") });

        QGridLayout *settings = new QGridLayout;
        settings->setColumnStretch(1, 1);
        settings->setColumnStretch(3, 1);
        settings->addWidget(label(QStringLiteral("Chip:")), 0, 0);
        settings->addWidget(chipMem, 0, 1);
        settings->addWidget(label(QStringLiteral("Slow:")), 0, 2);
        settings->addWidget(slowMem, 0, 3);
        settings->addWidget(label(QStringLiteral("Z2 Fast:")), 1, 0);
        settings->addWidget(z2Fast, 1, 1);
        settings->addWidget(label(QStringLiteral("Z3 Fast:")), 1, 2);
        settings->addWidget(z3Fast, 1, 3);
        settings->addWidget(label(QStringLiteral("Processor slot:")), 2, 0);
        settings->addWidget(combo({ QStringLiteral("None"), QStringLiteral("32 MB"), QStringLiteral("64 MB"), QStringLiteral("128 MB") }), 2, 1);
        root->addWidget(groupBox(QStringLiteral("Memory Settings"), settings));

        QGridLayout *advanced = new QGridLayout;
        advanced->addWidget(combo({ QStringLiteral("None"), QStringLiteral("Custom memory") }), 0, 0, 1, 2);
        advanced->addWidget(label(QStringLiteral("Manufacturer")), 1, 0);
        advanced->addWidget(new QLineEdit, 1, 1);
        advanced->addWidget(label(QStringLiteral("Product")), 1, 2);
        advanced->addWidget(new QLineEdit, 1, 3);
        advanced->addWidget(new QCheckBox(QStringLiteral("Manual configuration")), 2, 1);
        advanced->addWidget(new QCheckBox(QStringLiteral("DMA Capable")), 2, 2);
        root->addWidget(groupBox(QStringLiteral("Advanced Memory Settings"), advanced), 1);
        return page;
    }

    QWidget *makeFloppyPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        QGridLayout *drives = new QGridLayout;
        drives->setColumnStretch(5, 1);
        for (int i = 0; i < 4; i++) {
            addFloppyRow(drives, i);
        }
        root->addWidget(groupBox(QStringLiteral("Floppy Drives"), drives), 1);
        QHBoxLayout *speed = new QHBoxLayout;
        speed->addWidget(new QRadioButton(QStringLiteral("100%")));
        speed->addWidget(new QRadioButton(QStringLiteral("Turbo")));
        speed->addWidget(new QCheckBox(QStringLiteral("Accurate disk access")));
        speed->addStretch();
        root->addWidget(groupBox(QStringLiteral("Floppy Drive Emulation Speed"), speed));
        return page;
    }

    void addFloppyRow(QGridLayout *layout, int drive)
    {
        dfEnable[drive] = new QCheckBox(QStringLiteral("DF%1:").arg(drive));
        dfType[drive] = combo({ QStringLiteral("3.5 DD"), QStringLiteral("3.5 HD"), QStringLiteral("Disabled") });
        dfPath[drive] = pathCombo();
        dfWriteProtect[drive] = new QCheckBox;
        QPushButton *info = smallButton(QStringLiteral("?"));
        QPushButton *eject = new QPushButton(QStringLiteral("Eject"));
        QPushButton *browse = smallButton(QStringLiteral("..."));
        const int row = drive * 2;
        layout->addWidget(dfEnable[drive], row, 0);
        layout->addWidget(dfType[drive], row, 1);
        layout->addWidget(new QLabel(QStringLiteral("Write-protected")), row, 2);
        layout->addWidget(dfWriteProtect[drive], row, 3);
        layout->addWidget(info, row, 4);
        layout->addWidget(eject, row, 5);
        layout->addWidget(browse, row, 6);
        layout->addWidget(dfPath[drive], row + 1, 0, 1, 7);
        dfEnable[drive]->setChecked(drive == 0);
        info->setEnabled(false);
        connect(browse, &QPushButton::clicked, this, [this, drive]() {
            addBrowse(dfPath[drive], this, QStringLiteral("Select floppy image"), QStringLiteral("Amiga disk images (*.adf *.adz *.ipf *.dms);;All files (*)"));
        });
        connect(eject, &QPushButton::clicked, this, [this, drive]() {
            dfPath[drive]->setCurrentText(QString());
        });
        if (drive < 2 && quickDfPath[drive]) {
            connect(dfPath[drive], &QComboBox::currentTextChanged, this, [this, drive](const QString &text) {
                if (quickDfPath[drive]->currentText() != text) {
                    quickDfPath[drive]->setCurrentText(text);
                }
            });
            connect(quickDfPath[drive], &QComboBox::currentTextChanged, this, [this, drive](const QString &text) {
                if (dfPath[drive]->currentText() != text) {
                    dfPath[drive]->setCurrentText(text);
                }
            });
            connect(dfEnable[drive], &QCheckBox::toggled, quickDfEnable[drive], &QCheckBox::setChecked);
            connect(quickDfEnable[drive], &QCheckBox::toggled, dfEnable[drive], &QCheckBox::setChecked);
            connect(dfType[drive], QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, drive]() {
                syncFloppyDriveToQuick(drive);
            });
            connect(quickDfType[drive], QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, drive]() {
                syncQuickDriveToFloppy(drive);
            });
            connect(dfWriteProtect[drive], &QCheckBox::toggled, this, [this, drive]() {
                syncFloppyDriveToQuick(drive);
            });
            connect(quickDfWriteProtect[drive], &QCheckBox::toggled, this, [this, drive]() {
                syncQuickDriveToFloppy(drive);
            });
        }
    }

    QWidget *makeHardDrivesPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        mountedDrives = new QTreeWidget;
        mountedDrives->setRootIsDecorated(false);
        mountedDrives->setSelectionMode(QAbstractItemView::SingleSelection);
        mountedDrives->setDragDropMode(QAbstractItemView::InternalMove);
        mountedDrives->setDragEnabled(true);
        mountedDrives->setAcceptDrops(true);
        mountedDrives->viewport()->setAcceptDrops(true);
        mountedDrives->setDefaultDropAction(Qt::MoveAction);
        mountedDrives->setDropIndicatorShown(true);
        mountedDrives->setHeaderLabels({
            QStringLiteral("*"),
            QStringLiteral("Device"),
            QStringLiteral("Volume"),
            QStringLiteral("Path"),
            QStringLiteral("RW"),
            QStringLiteral("Block size"),
            QStringLiteral("Size"),
            QStringLiteral("BootPri")
        });
        QVBoxLayout *volumeLayout = new QVBoxLayout;
        volumeLayout->addWidget(mountedDrives);
        root->addWidget(groupBox(QStringLiteral("Mounted drives"), volumeLayout), 1);
        QGridLayout *buttons = new QGridLayout;
        addDirectoryMountButton = new QPushButton(QStringLiteral("Add Directory or Archive..."));
        addHardfileMountButton = new QPushButton(QStringLiteral("Add Hardfile..."));
        addHardDriveMountButton = new QPushButton(QStringLiteral("Add Hard Drive..."));
        addCdMountButton = new QPushButton(QStringLiteral("Add SCSI/IDE CD Drive"));
        addTapeMountButton = new QPushButton(QStringLiteral("Add SCSI/IDE Tape Drive"));
        propertiesMountButton = new QPushButton(QStringLiteral("Properties"));
        removeMountButton = new QPushButton(QStringLiteral("Remove"));
        buttons->setColumnStretch(4, 1);
        buttons->addWidget(addDirectoryMountButton, 0, 0);
        buttons->addWidget(addHardfileMountButton, 0, 1);
        buttons->addWidget(addHardDriveMountButton, 0, 2);
        buttons->addWidget(addCdMountButton, 1, 0);
        buttons->addWidget(addTapeMountButton, 1, 1);
        buttons->addWidget(propertiesMountButton, 1, 2);
        buttons->addWidget(removeMountButton, 1, 3);
        root->addLayout(buttons);

        cdSlotNumber = combo({});
        for (int i = 0; i < MaxCdSlots; i++) {
            cdSlotNumber->addItem(QString::number(i + 1));
        }
        cdSlotPath = pathCombo();
        cdSlotType = combo({ QStringLiteral("Autodetect"), QStringLiteral("Image file") }, QStringLiteral("Image file"));
        QPushButton *selectCdImage = new QPushButton(QStringLiteral("Select image file"));
        QPushButton *ejectCd = new QPushButton(QStringLiteral("Eject"));
        cdSpeedTurbo = new QCheckBox(QStringLiteral("CDTV/CDTV-CR/CD32 turbo CD read speed"));

        QGridLayout *optical = new QGridLayout;
        optical->setColumnStretch(1, 1);
        optical->addWidget(new QLabel(QStringLiteral("CD drive/image")), 0, 0);
        optical->addWidget(selectCdImage, 0, 2);
        optical->addWidget(cdSlotType, 0, 3);
        optical->addWidget(ejectCd, 0, 4);
        optical->addWidget(cdSlotNumber, 1, 0);
        optical->addWidget(cdSlotPath, 1, 1, 1, 4);
        optical->addWidget(cdSpeedTurbo, 2, 1, 1, 4);
        root->addWidget(groupBox(QStringLiteral("Optical media options"), optical));

        connect(addDirectoryMountButton, &QPushButton::clicked, this, [this]() { addDirectoryMountDialog(); });
        connect(addHardfileMountButton, &QPushButton::clicked, this, [this]() { addHardfileMountDialog(); });
        connect(addHardDriveMountButton, &QPushButton::clicked, this, [this]() { addHardDriveMountDialog(); });
        connect(addCdMountButton, &QPushButton::clicked, this, [this]() { addCdDriveMountDialog(); });
        connect(addTapeMountButton, &QPushButton::clicked, this, [this]() { addTapeDriveMountDialog(); });
        connect(cdSlotNumber, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (cdSlotUpdating || index < 0 || index >= MaxCdSlots) {
                return;
            }
            storeCurrentCdSlotFromUi();
            currentCdSlot = index;
            loadCdSlotToUi(index);
        });
        connect(cdSlotType, &QComboBox::currentTextChanged, this, [this](const QString &text) {
            if (cdSlotUpdating) {
                return;
            }
            if (text == QStringLiteral("Autodetect") && cdSlotPath->currentText().isEmpty()) {
                setCurrentCdSlotInUse(true);
            }
        });
        connect(cdSlotPath, &QComboBox::currentTextChanged, this, [this](const QString &text) {
            if (cdSlotUpdating) {
                return;
            }
            if (!text.isEmpty()) {
                setComboTextIfChanged(cdSlotType, QStringLiteral("Image file"));
                setCurrentCdSlotInUse(true);
            }
        });
        connect(selectCdImage, &QPushButton::clicked, this, [this]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select CD image"), cdSlotPath->currentText().isEmpty() ? QDir::homePath() : cdSlotPath->currentText(), QStringLiteral("CD images (*.iso *.cue *.ccd *.mds *.chd);;All files (*)"));
            if (!selected.isEmpty()) {
                cdSlotPath->setCurrentText(selected);
                setComboTextIfChanged(cdSlotType, QStringLiteral("Image file"));
                setCurrentCdSlotInUse(true);
            }
        });
        connect(ejectCd, &QPushButton::clicked, this, [this]() {
            cdSlotPath->setCurrentText(QString());
            setComboTextIfChanged(cdSlotType, QStringLiteral("Image file"));
            setCurrentCdSlotInUse(false);
        });
        connect(propertiesMountButton, &QPushButton::clicked, this, [this]() { openSelectedMountProperties(); });
        connect(removeMountButton, &QPushButton::clicked, this, [this]() { removeSelectedMount(); });
        connect(mountedDrives, &QTreeWidget::itemSelectionChanged, this, [this]() { updateMountButtons(); });
        connect(mountedDrives, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *, int) { openSelectedMountProperties(); });
        QShortcut *propertiesShortcut = new QShortcut(QKeySequence(Qt::Key_Return), mountedDrives);
        connect(propertiesShortcut, &QShortcut::activated, this, [this]() { openSelectedMountProperties(); });
        QShortcut *propertiesEnterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), mountedDrives);
        connect(propertiesEnterShortcut, &QShortcut::activated, this, [this]() { openSelectedMountProperties(); });
        QShortcut *removeShortcut = new QShortcut(QKeySequence::Delete, mountedDrives);
        connect(removeShortcut, &QShortcut::activated, this, [this]() { removeSelectedMount(); });
        QShortcut *moveUpShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Up), mountedDrives);
        connect(moveUpShortcut, &QShortcut::activated, this, [this]() { moveSelectedMount(-1); });
        QShortcut *moveDownShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Down), mountedDrives);
        connect(moveDownShortcut, &QShortcut::activated, this, [this]() { moveSelectedMount(1); });
        updateMountButtons();
        return page;
    }

    QWidget *makeExpansionPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);

        rtgMem = combo({ QStringLiteral("None"), QStringLiteral("4 MB"), QStringLiteral("8 MB"), QStringLiteral("16 MB"), QStringLiteral("32 MB") }, QStringLiteral("None"));
        rtgType = combo({ QStringLiteral("ZorroII"), QStringLiteral("ZorroIII") }, QStringLiteral("ZorroIII"));

        QGridLayout *rtg = new QGridLayout;
        rtg->setColumnStretch(1, 1);
        rtg->addWidget(label(QStringLiteral("RTG board:")), 0, 0);
        rtg->addWidget(combo({ QStringLiteral("UAE RTG") }), 0, 1);
        rtg->addWidget(label(QStringLiteral("Memory:")), 1, 0);
        rtg->addWidget(rtgMem, 1, 1);
        rtg->addWidget(label(QStringLiteral("Type:")), 2, 0);
        rtg->addWidget(rtgType, 2, 1);
        root->addWidget(groupBox(QStringLiteral("Graphics board"), rtg));
        root->addWidget(groupBox(QStringLiteral("Expansion boards"), new QVBoxLayout), 1);
        return page;
    }

    QWidget *makeDisplayPage()
    {
        QWidget *page = makePage();
        QHBoxLayout *root = new QHBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);

        QVBoxLayout *left = new QVBoxLayout;
        windowWidth = new QLineEdit(QStringLiteral("720"));
        windowHeight = new QLineEdit(QStringLiteral("568"));
        windowResize = new QCheckBox(QStringLiteral("Window resize"));
        QGridLayout *screen = new QGridLayout;
        screen->addWidget(combo({ QStringLiteral("Default display") }), 0, 0, 1, 3);
        screen->addWidget(label(QStringLiteral("Fullscreen:")), 1, 0);
        screen->addWidget(combo({ QStringLiteral("Native"), QStringLiteral("640x480"), QStringLiteral("800x600"), QStringLiteral("1024x768") }), 1, 1, 1, 2);
        screen->addWidget(label(QStringLiteral("Windowed:")), 2, 0);
        screen->addWidget(windowWidth, 2, 1);
        screen->addWidget(windowHeight, 2, 2);
        screen->addWidget(windowResize, 3, 1, 1, 2);
        left->addWidget(groupBox(QStringLiteral("Screen"), screen));

        nativeMode = combo({ QStringLiteral("Windowed"), QStringLiteral("Fullscreen") }, QStringLiteral("Windowed"));
        rtgMode = combo({ QStringLiteral("Windowed"), QStringLiteral("Fullscreen") }, QStringLiteral("Windowed"));
        QGridLayout *settings = new QGridLayout;
        settings->addWidget(label(QStringLiteral("Native:")), 0, 0);
        settings->addWidget(nativeMode, 0, 1);
        settings->addWidget(combo({ QStringLiteral("Default"), QStringLiteral("PAL"), QStringLiteral("NTSC") }), 0, 2);
        settings->addWidget(label(QStringLiteral("RTG:")), 1, 0);
        settings->addWidget(rtgMode, 1, 1);
        settings->addWidget(new QCheckBox(QStringLiteral("Remove interlace artifacts")), 2, 1, 1, 2);
        settings->addWidget(new QCheckBox(QStringLiteral("Filtered low resolution")), 3, 1, 1, 2);
        left->addWidget(groupBox(QStringLiteral("Settings"), settings), 1);

        QVBoxLayout *right = new QVBoxLayout;
        QVBoxLayout *center = new QVBoxLayout;
        center->addWidget(new QCheckBox(QStringLiteral("Horizontal")));
        center->addWidget(new QCheckBox(QStringLiteral("Vertical")));
        right->addWidget(groupBox(QStringLiteral("Centering"), center));
        QVBoxLayout *lineMode = new QVBoxLayout;
        lineMode->addWidget(new QRadioButton(QStringLiteral("Single")));
        lineMode->addWidget(new QRadioButton(QStringLiteral("Double")));
        lineMode->addWidget(new QRadioButton(QStringLiteral("Scanlines")));
        lineMode->addWidget(new QRadioButton(QStringLiteral("Double, fields")));
        right->addWidget(groupBox(QStringLiteral("Line mode"), lineMode));
        right->addStretch();

        root->addLayout(left, 3);
        root->addLayout(right, 1);
        return page;
    }

    QWidget *makeSoundPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        soundOutput = combo({ QStringLiteral("normal"), QStringLiteral("none") }, QStringLiteral("normal"));
        QGridLayout *sound = new QGridLayout;
        sound->setColumnStretch(1, 1);
        sound->addWidget(label(QStringLiteral("Sound card:")), 0, 0);
        sound->addWidget(combo({ QStringLiteral("Default audio device") }), 0, 1);
        sound->addWidget(label(QStringLiteral("Sound output:")), 1, 0);
        sound->addWidget(soundOutput, 1, 1);
        sound->addWidget(label(QStringLiteral("Frequency:")), 2, 0);
        sound->addWidget(combo({ QStringLiteral("44100"), QStringLiteral("48000") }, QStringLiteral("48000")), 2, 1);
        sound->addWidget(new QCheckBox(QStringLiteral("Stereo")), 3, 1);
        root->addWidget(groupBox(QStringLiteral("Sound Emulation"), sound));
        root->addStretch();
        return page;
    }

    QWidget *makeGamePortsPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        port0 = combo({ QStringLiteral("Mouse"), QStringLiteral("Keyboard Layout B"), QStringLiteral("None") }, QStringLiteral("Mouse"));
        port1 = combo({ QStringLiteral("Keyboard Layout B"), QStringLiteral("Mouse"), QStringLiteral("None") }, QStringLiteral("Keyboard Layout B"));
        QGridLayout *ports = new QGridLayout;
        ports->setColumnStretch(1, 1);
        ports->addWidget(label(QStringLiteral("Port 1:")), 0, 0);
        ports->addWidget(port0, 0, 1);
        ports->addWidget(new QPushButton(QStringLiteral("Remap / Test")), 1, 2);
        ports->addWidget(label(QStringLiteral("Port 2:")), 2, 0);
        ports->addWidget(port1, 2, 1);
        ports->addWidget(new QPushButton(QStringLiteral("Remap / Test")), 3, 2);
        ports->addWidget(new QPushButton(QStringLiteral("Swap ports")), 4, 1);
        ports->addWidget(new QCheckBox(QStringLiteral("Mouse/Joystick autoswitching")), 4, 2);
        root->addWidget(groupBox(QStringLiteral("Mouse and Joystick settings"), ports));
        QGridLayout *mouse = new QGridLayout;
        mouse->addWidget(label(QStringLiteral("Mouse speed:")), 0, 0);
        mouse->addWidget(new QLineEdit(QStringLiteral("100")), 0, 1);
        mouse->addWidget(new QCheckBox(QStringLiteral("Install virtual mouse driver")), 1, 0, 1, 2);
        root->addWidget(groupBox(QStringLiteral("Mouse extra settings"), mouse), 1);
        return page;
    }

    QWidget *makeInputPage()
    {
        QWidget *page = makePage();
        QHBoxLayout *root = new QHBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        QListWidget *devices = new QListWidget;
        devices->addItems({ QStringLiteral("Keyboard"), QStringLiteral("Mouse") });
        root->addWidget(devices, 1);
        QVBoxLayout *right = new QVBoxLayout;
        right->addWidget(combo({ QStringLiteral("Configuration #1"), QStringLiteral("Configuration #2") }));
        right->addWidget(new QTableWidget(8, 3), 1);
        right->addWidget(new QPushButton(QStringLiteral("Remap")));
        root->addLayout(right, 2);
        return page;
    }

    QWidget *makePathsPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        QGridLayout *paths = new QGridLayout;
        paths->setColumnStretch(1, 1);
        emulatorPath = new QLineEdit;
        romsPath = new QLineEdit;
        configsPath = new QLineEdit;
        addLineBrowseRow(paths, 0, QStringLiteral("Emulator executable:"), emulatorPath);
        addLineBrowseRow(paths, 1, QStringLiteral("System ROMs:"), romsPath, true);
        addLineBrowseRow(paths, 2, QStringLiteral("Configuration files:"), configsPath, true);
        addLineBrowseRow(paths, 3, QStringLiteral("NVRAM files:"), new QLineEdit, true);
        addLineBrowseRow(paths, 4, QStringLiteral("Screenshots:"), new QLineEdit, true);
        addLineBrowseRow(paths, 5, QStringLiteral("State files:"), new QLineEdit, true);
        addLineBrowseRow(paths, 6, QStringLiteral("Videos:"), new QLineEdit, true);
        root->addLayout(paths);

        QHBoxLayout *actions = new QHBoxLayout;
        actions->addWidget(new QPushButton(QStringLiteral("Set Path")));
        actions->addWidget(combo({ QStringLiteral("System ROMs"), QStringLiteral("Configuration files"), QStringLiteral("State files") }));
        actions->addStretch();
        actions->addWidget(new QCheckBox(QStringLiteral("Use relative paths")));
        actions->addWidget(new QCheckBox(QStringLiteral("Portable mode")));
        root->addLayout(actions);

        QGridLayout *logging = new QGridLayout;
        logging->addWidget(combo({ QStringLiteral("winuaelog.txt"), QStringLiteral("config.log") }), 0, 0);
        logging->addWidget(new QCheckBox(QStringLiteral("Enable full logging")), 0, 1);
        logging->addWidget(new QCheckBox(QStringLiteral("Log window")), 0, 2);
        logging->addWidget(new QPushButton(QStringLiteral("Save All")), 0, 3);
        logging->addWidget(new QLineEdit, 1, 0, 1, 4);
        root->addWidget(groupBox(QStringLiteral("Debug logging"), logging), 1);
        return page;
    }

    QWidget *makeMiscPage()
    {
        QWidget *page = makePage();
        QHBoxLayout *root = new QHBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        QListWidget *misc = new QListWidget;
        misc->addItems({ QStringLiteral("Logging"), QStringLiteral("State files"), QStringLiteral("GUI"), QStringLiteral("Keyboard LEDs") });
        root->addWidget(misc, 2);
        QVBoxLayout *right = new QVBoxLayout;
        right->addWidget(groupBox(QStringLiteral("Miscellaneous Options"), new QVBoxLayout));
        right->addWidget(groupBox(QStringLiteral("GUI"), new QVBoxLayout));
        right->addWidget(groupBox(QStringLiteral("State Files"), new QVBoxLayout));
        root->addLayout(right, 1);
        return page;
    }

    QWidget *makeAboutPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        root->addStretch();
        QLabel *title = new QLabel(QStringLiteral("WinUAE"));
        QFont font = title->font();
        font.setPointSize(22);
        font.setBold(true);
        title->setFont(font);
        title->setAlignment(Qt::AlignCenter);
        root->addWidget(title);
        QLabel *version = new QLabel(versionString());
        QFont versionFont = version->font();
        versionFont.setPointSize(13);
        version->setFont(versionFont);
        version->setAlignment(Qt::AlignCenter);
        root->addWidget(version);
        QLabel *subtitle = new QLabel(QStringLiteral("Unix Qt configuration frontend"));
        subtitle->setAlignment(Qt::AlignCenter);
        root->addWidget(subtitle);
        QPushButton *contributors = new QPushButton(QStringLiteral("Contributors"));
        contributors->setFixedWidth(120);
        connect(contributors, &QPushButton::clicked, this, [this]() { showContributors(); });
        QHBoxLayout *buttonRow = new QHBoxLayout;
        buttonRow->addStretch();
        buttonRow->addWidget(contributors);
        buttonRow->addStretch();
        root->addLayout(buttonRow);
        root->addStretch();
        return page;
    }

    void showContributors()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("UAE Authors and Contributors..."));
        dialog.resize(620, 420);

        QVBoxLayout *root = new QVBoxLayout(&dialog);
        QListWidget *list = new QListWidget;
        list->addItems(contributorLines());
        root->addWidget(list, 1);

        QPushButton *ok = new QPushButton(QStringLiteral("Ok"));
        ok->setDefault(true);
        connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);

        QHBoxLayout *buttons = new QHBoxLayout;
        buttons->addStretch();
        buttons->addWidget(ok);
        buttons->addStretch();
        root->addLayout(buttons);

        dialog.exec();
    }

    void addPathRow(QGridLayout *layout, int row, const QString &caption, QComboBox *field, const QString &dialogTitle, const QString &filter)
    {
        QPushButton *browse = smallButton(QStringLiteral("..."));
        layout->addWidget(new QLabel(caption), row * 2, 0, 1, 2);
        layout->addWidget(field, row * 2 + 1, 0, 1, 2);
        layout->addWidget(browse, row * 2 + 1, 2);
        connect(browse, &QPushButton::clicked, this, [this, field, dialogTitle, filter]() {
            addBrowse(field, this, dialogTitle, filter);
        });
    }

    void addLineBrowseRow(QGridLayout *layout, int row, const QString &caption, QLineEdit *field, bool directory = false)
    {
        QPushButton *browse = smallButton(QStringLiteral("..."));
        layout->addWidget(label(caption), row, 0);
        layout->addWidget(field, row, 1);
        layout->addWidget(browse, row, 2);
        connect(browse, &QPushButton::clicked, this, [this, field, directory, caption]() {
            QString path;
            if (directory) {
                path = QFileDialog::getExistingDirectory(this, caption, field->text());
            } else {
                path = QFileDialog::getOpenFileName(this, caption, field->text(), QStringLiteral("All files (*)"));
            }
            if (!path.isEmpty()) {
                field->setText(path);
            }
        });
    }

    void ensureCdSlots()
    {
        if (cdSlots.size() == MaxCdSlots) {
            return;
        }
        cdSlots = QVector<WinUaeQtCdSlot>(MaxCdSlots);
        for (WinUaeQtCdSlot &slot : cdSlots) {
            slot.type = QStringLiteral("Image file");
        }
    }

    void clearCdSlots()
    {
        ensureCdSlots();
        for (WinUaeQtCdSlot &slot : cdSlots) {
            slot = WinUaeQtCdSlot();
            slot.type = QStringLiteral("Image file");
        }
        currentCdSlot = 0;
        loadCdSlotToUi(currentCdSlot);
    }

    void setCurrentCdSlotInUse(bool inUse)
    {
        ensureCdSlots();
        if (currentCdSlot >= 0 && currentCdSlot < cdSlots.size()) {
            cdSlots[currentCdSlot].inUse = inUse;
        }
    }

    void storeCurrentCdSlotFromUi()
    {
        ensureCdSlots();
        if (!cdSlotPath || !cdSlotType || currentCdSlot < 0 || currentCdSlot >= cdSlots.size()) {
            return;
        }
        WinUaeQtCdSlot &slot = cdSlots[currentCdSlot];
        slot.path = cdSlotPath->currentText().trimmed();
        slot.type = cdSlotType->currentText();
        if (!slot.path.isEmpty()) {
            slot.inUse = true;
        } else if (slot.type == QStringLiteral("Image file")) {
            slot.inUse = false;
        }
    }

    WinUaeQtCdSlot cdSlotState(int index) const
    {
        WinUaeQtCdSlot slot = index >= 0 && index < cdSlots.size() ? cdSlots.value(index) : WinUaeQtCdSlot();
        if (index == currentCdSlot && cdSlotPath && cdSlotType) {
            slot.path = cdSlotPath->currentText().trimmed();
            slot.type = cdSlotType->currentText();
            if (!slot.path.isEmpty()) {
                slot.inUse = true;
            } else if (slot.type == QStringLiteral("Image file")) {
                slot.inUse = false;
            }
        }
        return slot;
    }

    void loadCdSlotToUi(int index)
    {
        ensureCdSlots();
        if (!cdSlotNumber || !cdSlotPath || !cdSlotType || index < 0 || index >= cdSlots.size()) {
            return;
        }
        cdSlotUpdating = true;
        QSignalBlocker numberBlocker(cdSlotNumber);
        QSignalBlocker pathBlocker(cdSlotPath);
        QSignalBlocker typeBlocker(cdSlotType);
        cdSlotNumber->setCurrentIndex(index);
        cdSlotPath->setCurrentText(cdSlots[index].path);
        cdSlotType->setCurrentText(cdSlots[index].type.isEmpty() ? QStringLiteral("Image file") : cdSlots[index].type);
        cdSlotUpdating = false;
    }

    void resetDefaults()
    {
        loadedConfig = WinUaeQtConfig();

        const QString appDirExe = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("winuae_unix"));
        const QString buildDirExe = QDir(QString::fromUtf8(WINUAE_UNIX_BUILD_DIR)).filePath(QStringLiteral("winuae_unix"));
        if (QFileInfo::exists(appDirExe)) {
            emulatorPath->setText(appDirExe);
        } else {
            emulatorPath->setText(buildDirExe);
        }

        if (configName) {
            configName->setCurrentText(QStringLiteral("A1200 Install"));
        }
        if (configDescription) {
            configDescription->setText(QStringLiteral("A1200, 68020, AGA, 2 MB Chip RAM"));
        }
        if (configPath) {
            configPath->clear();
        }

        quickModel->setCurrentText(QStringLiteral("A1200"));
        quickConfiguration->setCurrentText(QStringLiteral("Expanded configuration"));
        quickHostConfiguration->setCurrentText(QStringLiteral("Default"));
        compatibility->setValue(1);
        ntsc->setChecked(false);

        applyModelPreset(QStringLiteral("A1200"));
        chipMem->setCurrentText(QStringLiteral("2 MB"));
        z2Fast->setCurrentText(QStringLiteral("None"));
        slowMem->setCurrentText(QStringLiteral("None"));
        z3Fast->setCurrentText(QStringLiteral("None"));
        rtgMem->setCurrentText(QStringLiteral("None"));
        rtgType->setCurrentText(QStringLiteral("ZorroIII"));

        romFile->setCurrentText(envString("WINUAE_KICKSTART_ROM"));
        extendedRomFile->setCurrentText(QString());
        cartFile->setCurrentText(QString());
        flashFile->clear();
        rtcFile->clear();

        for (int i = 0; i < 4; i++) {
            dfEnable[i]->setChecked(i == 0);
            dfType[i]->setCurrentText(QStringLiteral("3.5 DD"));
            dfWriteProtect[i]->setChecked(false);
            dfPath[i]->setCurrentText(i == 0 ? envString("WINUAE_FLOPPY0") : QString());
        }
        for (int i = 0; i < 2; i++) {
            quickDfEnable[i]->setChecked(i == 0);
            quickDfType[i]->setCurrentText(QStringLiteral("3.5 DD"));
            quickDfWriteProtect[i]->setChecked(false);
            quickDfPath[i]->setCurrentText(i == 0 ? envString("WINUAE_FLOPPY0") : QString());
        }
        if (mountedDrives) {
            mountedDrives->clear();
            updateMountButtons();
        }
        clearCdSlots();

        windowWidth->setText(QStringLiteral("720"));
        windowHeight->setText(QStringLiteral("568"));
        windowResize->setChecked(true);
        nativeMode->setCurrentText(QStringLiteral("Windowed"));
        rtgMode->setCurrentText(QStringLiteral("Windowed"));
        soundOutput->setCurrentText(QStringLiteral("normal"));
        cdSpeedTurbo->setChecked(false);
        port0->setCurrentText(QStringLiteral("Mouse"));
        port1->setCurrentText(QStringLiteral("Keyboard Layout B"));
        romsPath->setText(QDir::homePath());
        configsPath->setText(QDir::homePath());
        status->setText(QStringLiteral("Ready"));
    }

    void applyModelPreset(const QString &model)
    {
        if (model == QStringLiteral("A1200")) {
            chipset->setCurrentText(QStringLiteral("AGA"));
            chipsetCompatible->setCurrentText(QStringLiteral("A1200"));
            setCpuButton(68020);
            cpu24Bit->setChecked(false);
            chipMem->setCurrentText(QStringLiteral("2 MB"));
        } else if (model == QStringLiteral("A4000")) {
            chipset->setCurrentText(QStringLiteral("AGA"));
            chipsetCompatible->setCurrentText(QStringLiteral("A4000"));
            setCpuButton(68040);
            cpu24Bit->setChecked(false);
            chipMem->setCurrentText(QStringLiteral("2 MB"));
        } else if (model == QStringLiteral("A600")) {
            chipset->setCurrentText(QStringLiteral("ECS"));
            chipsetCompatible->setCurrentText(QStringLiteral("A600"));
            setCpuButton(68000);
            cpu24Bit->setChecked(true);
            chipMem->setCurrentText(QStringLiteral("2 MB"));
        } else if (model == QStringLiteral("A500+")) {
            chipset->setCurrentText(QStringLiteral("ECS"));
            chipsetCompatible->setCurrentText(QStringLiteral("A500+"));
            setCpuButton(68000);
            cpu24Bit->setChecked(true);
            chipMem->setCurrentText(QStringLiteral("1 MB"));
        } else {
            chipset->setCurrentText(QStringLiteral("OCS"));
            chipsetCompatible->setCurrentText(QStringLiteral("A500"));
            setCpuButton(68000);
            cpu24Bit->setChecked(true);
            chipMem->setCurrentText(QStringLiteral("512 KB"));
        }
    }

    void setCpuButton(int model)
    {
        if (QAbstractButton *button = cpuButtons->button(model)) {
            button->setChecked(true);
        }
        updateFpuControls();
    }

    void setFpuButton(int model)
    {
        const int id = (model == 68040 || model == 68060) ? FpuInternal : model;
        if (QAbstractButton *button = fpuButtons->button(id)) {
            button->setChecked(true);
        }
        updateFpuControls();
    }

    int selectedCpuModel() const
    {
        const int cpu = cpuButtons->checkedId();
        return cpu > 0 ? cpu : 68020;
    }

    int fpuModelConfigValue(int cpu) const
    {
        const int fpu = fpuButtons->checkedId();
        if (fpu == FpuInternal) {
            return cpu >= 68060 ? 68060 : (cpu >= 68040 ? 68040 : 68882);
        }
        return fpu > 0 ? fpu : 0;
    }

    void updateFpuControls()
    {
        const int cpu = selectedCpuModel();
        if (QAbstractButton *internal = fpuButtons->button(FpuInternal)) {
            internal->setEnabled(cpu >= 68040);
        }
    }

    int chipMemConfigValue() const
    {
        const QString text = chipMem->currentText();
        if (text == QStringLiteral("512 KB")) {
            return 1;
        }
        if (text == QStringLiteral("1 MB")) {
            return 2;
        }
        if (text == QStringLiteral("2 MB")) {
            return 4;
        }
        if (text == QStringLiteral("4 MB")) {
            return 8;
        }
        if (text == QStringLiteral("8 MB")) {
            return 16;
        }
        return 4;
    }

    int megabytesFromText(const QString &text) const
    {
        QString value = text;
        value.remove(QStringLiteral(" MB"));
        return value.toInt();
    }

    int slowMemConfigValue() const
    {
        const QString text = slowMem->currentText();
        if (text == QStringLiteral("512 KB")) {
            return 2;
        }
        if (text == QStringLiteral("1 MB")) {
            return 4;
        }
        if (text == QStringLiteral("1.5 MB")) {
            return 6;
        }
        if (text == QStringLiteral("1.8 MB")) {
            return 7;
        }
        return 0;
    }

    QString nextMountDeviceName() const
    {
        QStringList used;
        if (mountedDrives) {
            for (int i = 0; i < mountedDrives->topLevelItemCount(); i++) {
                const QTreeWidgetItem *item = mountedDrives->topLevelItem(i);
                const QString kind = item->data(0, MountKindRole).toString();
                if (kind == QStringLiteral("dir") || kind == QStringLiteral("hdf")) {
                    used.append(item->data(0, MountDeviceRole).toString().toUpper());
                }
            }
        }
        for (int i = 0; i < MaxMountEntries; i++) {
            const QString device = QStringLiteral("DH%1").arg(i);
            if (!used.contains(device)) {
                return device;
            }
        }
        return QStringLiteral("DH0");
    }

    void addDirectoryMountDialog()
    {
        WinUaeQtMountEntry entry;
        entry.kind = QStringLiteral("dir");
        entry.device = nextMountDeviceName();
        if (showDirectoryMountDialog(&entry, QStringLiteral("Add Directory or Archive"))) {
            addMountEntry(entry);
        }
    }

    void addHardfileMountDialog()
    {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Add hardfile"), QDir::homePath(), QStringLiteral("Hardfiles (*.hdf *.vhd *.chd);;All files (*)"));
        if (path.isEmpty()) {
            return;
        }

        WinUaeQtMountEntry entry;
        entry.kind = QStringLiteral("hdf");
        entry.device = nextMountDeviceName();
        entry.path = path;
        entry.hardfileGeometry = QStringLiteral("32,1,2,512");
        entry.hardfileTail = QStringLiteral(",uae0");
        entry.readOnly = false;
        entry.bootPri = 0;
        if (showHardfileMountDialog(&entry, QStringLiteral("Hardfile Properties"))) {
            addMountEntry(entry);
        }
    }

    int nextMountEmuUnit(const QString &kind) const
    {
        QList<int> used;
        if (mountedDrives) {
            for (int i = 0; i < mountedDrives->topLevelItemCount(); i++) {
                const QTreeWidgetItem *item = mountedDrives->topLevelItem(i);
                if (item->data(0, MountKindRole).toString() == kind) {
                    used.append(item->data(0, MountEmuUnitRole).toInt());
                }
            }
        }
        for (int i = 0; i < MaxControllerUnits; i++) {
            if (!used.contains(i)) {
                return i;
            }
        }
        return 0;
    }

    void addHardDriveMountDialog()
    {
        QMessageBox::information(
            this,
            QStringLiteral("Add Hard Drive"),
            QStringLiteral("Native Unix hard drive enumeration is not implemented yet. The Windows dialog uses a Win32 physical-drive backend; Unix needs a separate block-device backend before this can safely add host disks."));
    }

    void addCdDriveMountDialog()
    {
        WinUaeQtMountEntry entry;
        entry.kind = QStringLiteral("cd");
        entry.emuUnit = nextMountEmuUnit(QStringLiteral("cd"));
        entry.readOnly = true;
        entry.bootPri = 0;
        entry.hardfileGeometry = QStringLiteral("0,0,0,2048");
        entry.hardfileTail = QStringLiteral(",ide0");
        if (showCdDriveMountDialog(&entry, QStringLiteral("Add CD Drive"))) {
            addMountEntry(entry);
        }
    }

    void addTapeDriveMountDialog()
    {
        WinUaeQtMountEntry entry;
        entry.kind = QStringLiteral("tape");
        entry.emuUnit = nextMountEmuUnit(QStringLiteral("tape"));
        entry.readOnly = false;
        entry.bootPri = 0;
        entry.hardfileGeometry = QStringLiteral("0,0,0,512");
        entry.hardfileTail = QStringLiteral(",uae0");
        if (showTapeDriveMountDialog(&entry, QStringLiteral("Add Tape Drive"))) {
            addMountEntry(entry);
        }
    }

    void addMountEntry(const WinUaeQtMountEntry &entry)
    {
        if (!mountedDrives || mountedDrives->topLevelItemCount() >= MaxMountEntries) {
            return;
        }
        QTreeWidgetItem *item = new QTreeWidgetItem(mountedDrives);
        updateMountItem(item, entry);
        updateMountButtons();
    }

    void updateMountItem(QTreeWidgetItem *item, const WinUaeQtMountEntry &entry)
    {
        if (!mountedDrives || !item) {
            return;
        }
        item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
        WinUaeQtMountEntry normalized = entry;
        if (normalized.kind == QStringLiteral("dir")) {
            normalized.device = winUaeQtSanitizedAmigaName(normalized.device, nextMountDeviceName(), true);
            normalized.volume = winUaeQtSanitizedAmigaName(normalized.volume, winUaeQtDefaultVolumeName(normalized.path), false);
        } else if (normalized.kind == QStringLiteral("hdf")) {
            normalized.device = winUaeQtSanitizedAmigaName(normalized.device, nextMountDeviceName(), true);
        } else if (normalized.kind == QStringLiteral("cd")) {
            normalized.device.clear();
            normalized.volume = QStringLiteral("CD");
            normalized.readOnly = true;
            normalized.bootPri = 0;
            if (normalized.hardfileGeometry.isEmpty()) {
                normalized.hardfileGeometry = QStringLiteral("0,0,0,2048");
            }
        } else if (normalized.kind == QStringLiteral("tape")) {
            normalized.device.clear();
            normalized.volume = QStringLiteral("TAPE");
            normalized.bootPri = 0;
            if (normalized.hardfileGeometry.isEmpty()) {
                normalized.hardfileGeometry = QStringLiteral("0,0,0,512");
            }
        }

        QString deviceText = normalized.device;
        QString volumeText = normalized.kind == QStringLiteral("dir") ? normalized.volume : QStringLiteral("n/a");
        QString blockSizeText = QStringLiteral("n/a");
        QString bootPriText = QString::number(normalized.bootPri);
        if (normalized.kind == QStringLiteral("cd")) {
            deviceText = mountControllerDisplay(normalized);
            volumeText = QStringLiteral("CD");
            blockSizeText = QStringLiteral("2048");
            bootPriText = QStringLiteral("n/a");
        } else if (normalized.kind == QStringLiteral("tape")) {
            deviceText = mountControllerDisplay(normalized);
            volumeText = QStringLiteral("TAPE");
            blockSizeText = QStringLiteral("512");
            bootPriText = QStringLiteral("n/a");
        } else if (normalized.kind == QStringLiteral("hdf")) {
            const QStringList geometry = normalized.hardfileGeometry.split(QLatin1Char(','));
            blockSizeText = geometry.value(3, QStringLiteral("512"));
        }

        item->setText(0, QString());
        item->setText(1, deviceText);
        item->setText(2, volumeText);
        item->setText(3, normalized.path.isEmpty() ? QStringLiteral("-") : normalized.path);
        item->setText(4, normalized.readOnly ? QStringLiteral("No") : QStringLiteral("Yes"));
        item->setText(5, blockSizeText);
        item->setText(6, QStringLiteral("n/a"));
        item->setText(7, bootPriText);
        item->setData(0, MountKindRole, entry.kind);
        item->setData(0, MountDeviceRole, normalized.device);
        item->setData(0, MountVolumeRole, normalized.volume);
        item->setData(0, MountPathRole, normalized.path);
        item->setData(0, MountReadOnlyRole, normalized.readOnly);
        item->setData(0, MountBootPriRole, normalized.bootPri);
        item->setData(0, MountEmuUnitRole, normalized.emuUnit);
        item->setData(0, MountRawConfigRole, normalized.rawConfig);
        item->setData(0, MountHardfileGeometryRole, normalized.hardfileGeometry);
        item->setData(0, MountHardfileTailRole, normalized.hardfileTail);
        for (int i = 0; i < mountedDrives->columnCount(); i++) {
            mountedDrives->resizeColumnToContents(i);
        }
    }

    void removeSelectedMount()
    {
        if (!mountedDrives) {
            return;
        }
        qDeleteAll(mountedDrives->selectedItems());
        updateMountButtons();
    }

    void moveSelectedMount(int delta)
    {
        if (!mountedDrives || delta == 0) {
            return;
        }
        QTreeWidgetItem *item = mountedDrives->currentItem();
        if (!item) {
            return;
        }
        const int from = mountedDrives->indexOfTopLevelItem(item);
        if (from < 0) {
            return;
        }
        const int to = qBound(0, from + delta, mountedDrives->topLevelItemCount() - 1);
        if (from == to) {
            return;
        }
        item = mountedDrives->takeTopLevelItem(from);
        mountedDrives->insertTopLevelItem(to, item);
        mountedDrives->setCurrentItem(item);
        item->setSelected(true);
        updateMountButtons();
    }

    void updateMountButtons()
    {
        if (!mountedDrives) {
            return;
        }
        const bool canAdd = mountedDrives->topLevelItemCount() < MaxMountEntries;
        if (addDirectoryMountButton) {
            addDirectoryMountButton->setEnabled(canAdd);
        }
        if (addHardfileMountButton) {
            addHardfileMountButton->setEnabled(canAdd);
        }
        if (addHardDriveMountButton) {
            addHardDriveMountButton->setEnabled(canAdd);
        }
        if (addCdMountButton) {
            addCdMountButton->setEnabled(canAdd);
        }
        if (addTapeMountButton) {
            addTapeMountButton->setEnabled(canAdd);
        }
        if (propertiesMountButton) {
            propertiesMountButton->setEnabled(!mountedDrives->selectedItems().isEmpty());
        }
        if (removeMountButton) {
            removeMountButton->setEnabled(!mountedDrives->selectedItems().isEmpty());
        }
    }

    WinUaeQtMountEntry mountEntryFromItem(const QTreeWidgetItem *item) const
    {
        WinUaeQtMountEntry entry;
        entry.kind = item->data(0, MountKindRole).toString();
        entry.device = item->data(0, MountDeviceRole).toString();
        entry.volume = item->data(0, MountVolumeRole).toString();
        entry.path = item->data(0, MountPathRole).toString();
        entry.readOnly = item->data(0, MountReadOnlyRole).toBool();
        entry.bootPri = item->data(0, MountBootPriRole).toInt();
        entry.emuUnit = item->data(0, MountEmuUnitRole).toInt();
        entry.rawConfig = item->data(0, MountRawConfigRole).toString();
        entry.hardfileGeometry = item->data(0, MountHardfileGeometryRole).toString();
        entry.hardfileTail = item->data(0, MountHardfileTailRole).toString();
        return entry;
    }

    void openSelectedMountProperties()
    {
        if (!mountedDrives) {
            return;
        }
        QTreeWidgetItem *item = mountedDrives->currentItem();
        if (!item) {
            return;
        }

        WinUaeQtMountEntry entry = mountEntryFromItem(item);
        bool accepted = false;
        if (entry.kind == QStringLiteral("dir")) {
            accepted = showDirectoryMountDialog(&entry, QStringLiteral("Directory Properties"));
        } else if (entry.kind == QStringLiteral("hdf")) {
            accepted = showHardfileMountDialog(&entry, QStringLiteral("Hardfile Properties"));
        } else if (entry.kind == QStringLiteral("cd")) {
            accepted = showCdDriveMountDialog(&entry, QStringLiteral("CD Drive Properties"));
        } else if (entry.kind == QStringLiteral("tape")) {
            accepted = showTapeDriveMountDialog(&entry, QStringLiteral("Tape Drive Properties"));
        }
        if (accepted) {
            updateMountItem(item, entry);
        }
    }

    bool showDirectoryMountDialog(WinUaeQtMountEntry *entry, const QString &title)
    {
        if (!entry) {
            return false;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(title);

        QLineEdit *path = new QLineEdit(entry->path);
        QLineEdit *device = new QLineEdit(entry->device.isEmpty() ? nextMountDeviceName() : entry->device);
        QLineEdit *volume = new QLineEdit(entry->volume);
        QSpinBox *bootPri = new QSpinBox;
        bootPri->setRange(-128, 127);
        bootPri->setValue(entry->bootPri);
        QCheckBox *readOnly = new QCheckBox(QStringLiteral("Read-only"));
        readOnly->setChecked(entry->readOnly);
        QPushButton *browse = smallButton(QStringLiteral("..."));

        QGridLayout *fields = new QGridLayout;
        fields->setColumnStretch(1, 1);
        fields->addWidget(label(QStringLiteral("Path:")), 0, 0);
        fields->addWidget(path, 0, 1);
        fields->addWidget(browse, 0, 2);
        fields->addWidget(label(QStringLiteral("Device:")), 1, 0);
        fields->addWidget(device, 1, 1, 1, 2);
        fields->addWidget(label(QStringLiteral("Volume:")), 2, 0);
        fields->addWidget(volume, 2, 1, 1, 2);
        fields->addWidget(label(QStringLiteral("Boot priority:")), 3, 0);
        fields->addWidget(bootPri, 3, 1);
        fields->addWidget(readOnly, 4, 1, 1, 2);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QVBoxLayout *root = new QVBoxLayout(&dialog);
        root->addLayout(fields);
        root->addWidget(buttons);

        connect(browse, &QPushButton::clicked, this, [this, path, volume]() {
            const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral("Select directory"), path->text().isEmpty() ? QDir::homePath() : path->text());
            if (!selected.isEmpty()) {
                path->setText(selected);
                if (volume->text().isEmpty()) {
                    volume->setText(winUaeQtDefaultVolumeName(selected));
                }
            }
        });
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted || path->text().trimmed().isEmpty()) {
            return false;
        }

        entry->kind = QStringLiteral("dir");
        entry->device = winUaeQtSanitizedAmigaName(device->text(), nextMountDeviceName(), true);
        entry->volume = winUaeQtSanitizedAmigaName(volume->text(), winUaeQtDefaultVolumeName(path->text()), false);
        entry->path = path->text().trimmed();
        entry->bootPri = bootPri->value();
        entry->readOnly = readOnly->isChecked();
        return true;
    }

    bool showHardfileMountDialog(WinUaeQtMountEntry *entry, const QString &title)
    {
        if (!entry) {
            return false;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(title);
        dialog.resize(600, 460);

        QLineEdit *path = new QLineEdit(entry->path);
        QLineEdit *geometryFile = new QLineEdit(hardfileTailGeometryFile(*entry));
        QLineEdit *filesys = new QLineEdit(mountControllerParts(entry->hardfileTail).value(0));
        QLineEdit *device = new QLineEdit(entry->device.isEmpty() ? nextMountDeviceName() : entry->device);
        QSpinBox *bootPri = new QSpinBox;
        bootPri->setRange(-129, 127);
        bootPri->setValue(entry->bootPri);
        QCheckBox *readWrite = new QCheckBox(QStringLiteral("Read/write"));
        readWrite->setChecked(!entry->readOnly);
        QCheckBox *autoboot = new QCheckBox(QStringLiteral("Bootable"));
        QCheckBox *doNotMount = new QCheckBox(QStringLiteral("Do not mount"));
        QCheckBox *rdbMode = new QCheckBox(QStringLiteral("Full drive/RDB mode"));
        QCheckBox *manualGeometry = new QCheckBox(QStringLiteral("Manual geometry"));
        rdbMode->setChecked(hardfileIsRdb(*entry));
        manualGeometry->setChecked(hardfileHasPhysicalGeometry(*entry));

        const QStringList geometry = hardfileGeometryParts(*entry);
        QSpinBox *surfaces = new QSpinBox;
        QSpinBox *sectors = new QSpinBox;
        QSpinBox *reserved = new QSpinBox;
        QSpinBox *blockSize = new QSpinBox;
        for (QSpinBox *spin : { surfaces, sectors, reserved }) {
            spin->setRange(0, 1000000000);
        }
        blockSize->setRange(1, 65536);
        sectors->setValue(geometry.value(0).toInt());
        surfaces->setValue(geometry.value(1).toInt());
        reserved->setValue(geometry.value(2).toInt());
        blockSize->setValue(qMax(1, geometry.value(3, QStringLiteral("512")).toInt()));

        const QStringList physicalGeometry = hardfilePhysicalGeometryParts(*entry);
        QSpinBox *physicalCylinders = new QSpinBox;
        QSpinBox *physicalHeads = new QSpinBox;
        QSpinBox *physicalSectors = new QSpinBox;
        for (QSpinBox *spin : { physicalCylinders, physicalHeads, physicalSectors }) {
            spin->setRange(0, 1000000000);
        }
        physicalCylinders->setValue(physicalGeometry.value(0).toInt());
        physicalHeads->setValue(physicalGeometry.value(1).toInt());
        physicalSectors->setValue(physicalGeometry.value(2).toInt());

        QComboBox *controller = combo({
            QStringLiteral("UAE (uaehf.device)"),
            QStringLiteral("IDE (Auto)"),
            QStringLiteral("SCSI (Auto)")
        }, mountControllerFamily(*entry, QStringLiteral("uae0")));
        QSpinBox *controllerUnit = new QSpinBox;
        controllerUnit->setRange(0, MaxControllerUnits - 1);
        controllerUnit->setValue(mountControllerUnit(*entry, QStringLiteral("uae0")));
        QComboBox *mediaType = combo({ QStringLiteral("HD"), QStringLiteral("CF") }, hardfileTailHasToken(*entry, QStringLiteral("CF")) ? QStringLiteral("CF") : QStringLiteral("HD"));
        QComboBox *featureLevel = new QComboBox;

        QPushButton *browse = smallButton(QStringLiteral("..."));
        QPushButton *geometryBrowse = smallButton(QStringLiteral("..."));
        QPushButton *filesysBrowse = smallButton(QStringLiteral("..."));

        QGridLayout *fields = new QGridLayout;
        fields->setColumnStretch(1, 1);
        fields->addWidget(label(QStringLiteral("Path:")), 0, 0);
        fields->addWidget(path, 0, 1, 1, 2);
        fields->addWidget(browse, 0, 3);
        fields->addWidget(label(QStringLiteral("Geometry:")), 1, 0);
        fields->addWidget(geometryFile, 1, 1, 1, 2);
        fields->addWidget(geometryBrowse, 1, 3);
        fields->addWidget(label(QStringLiteral("FileSys:")), 2, 0);
        fields->addWidget(filesys, 2, 1, 1, 2);
        fields->addWidget(filesysBrowse, 2, 3);
        fields->addWidget(label(QStringLiteral("Device:")), 3, 0);
        fields->addWidget(device, 3, 1);
        fields->addWidget(label(QStringLiteral("Boot priority:")), 3, 2);
        fields->addWidget(bootPri, 3, 3);
        fields->addWidget(readWrite, 4, 1);
        fields->addWidget(autoboot, 4, 2);
        fields->addWidget(doNotMount, 5, 1);
        fields->addWidget(rdbMode, 6, 1);
        fields->addWidget(manualGeometry, 6, 2, 1, 2);

        QGridLayout *geometryLayout = new QGridLayout;
        geometryLayout->addWidget(label(QStringLiteral("Surfaces:")), 0, 0);
        geometryLayout->addWidget(surfaces, 0, 1);
        geometryLayout->addWidget(label(QStringLiteral("Sectors:")), 1, 0);
        geometryLayout->addWidget(sectors, 1, 1);
        geometryLayout->addWidget(label(QStringLiteral("Reserved:")), 2, 0);
        geometryLayout->addWidget(reserved, 2, 1);
        geometryLayout->addWidget(label(QStringLiteral("Block size:")), 3, 0);
        geometryLayout->addWidget(blockSize, 3, 1);
        geometryLayout->addWidget(label(QStringLiteral("Physical cyls:")), 0, 2);
        geometryLayout->addWidget(physicalCylinders, 0, 3);
        geometryLayout->addWidget(label(QStringLiteral("Physical heads:")), 1, 2);
        geometryLayout->addWidget(physicalHeads, 1, 3);
        geometryLayout->addWidget(label(QStringLiteral("Physical sectors:")), 2, 2);
        geometryLayout->addWidget(physicalSectors, 2, 3);

        QGridLayout *controllerLayout = new QGridLayout;
        controllerLayout->setColumnStretch(1, 1);
        controllerLayout->addWidget(label(QStringLiteral("HD Controller:")), 0, 0);
        controllerLayout->addWidget(controller, 0, 1);
        controllerLayout->addWidget(label(QStringLiteral("Unit:")), 0, 2);
        controllerLayout->addWidget(controllerUnit, 0, 3);
        controllerLayout->addWidget(label(QStringLiteral("Type:")), 1, 0);
        controllerLayout->addWidget(mediaType, 1, 1);
        controllerLayout->addWidget(label(QStringLiteral("Feature level:")), 1, 2);
        controllerLayout->addWidget(featureLevel, 1, 3);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QVBoxLayout *root = new QVBoxLayout(&dialog);
        root->addWidget(groupBox(QStringLiteral("Settings"), fields));
        root->addWidget(groupBox(QStringLiteral("Geometry"), geometryLayout));
        root->addWidget(groupBox(QStringLiteral("HD Controller"), controllerLayout));
        root->addWidget(buttons);

        auto updateBootChecks = [bootPri, autoboot, doNotMount]() {
            QSignalBlocker blockAutoboot(autoboot);
            QSignalBlocker blockDoNotMount(doNotMount);
            autoboot->setChecked(bootPri->value() > -128);
            doNotMount->setChecked(bootPri->value() <= -129);
        };
        auto updatePhysicalControls = [manualGeometry, physicalCylinders, physicalHeads, physicalSectors]() {
            const bool enabled = manualGeometry->isChecked();
            physicalCylinders->setEnabled(enabled);
            physicalHeads->setEnabled(enabled);
            physicalSectors->setEnabled(enabled);
        };
        auto setFeatureItems = [controller, featureLevel, mediaType](const QString &preferred) {
            const QString family = controller->currentText();
            QSignalBlocker blocker(featureLevel);
            featureLevel->clear();
            featureLevel->addItem(QStringLiteral("Default"));
            if (family == QStringLiteral("IDE (Auto)")) {
                featureLevel->addItems({ QStringLiteral("ATA-1"), QStringLiteral("ATA-2+"), QStringLiteral("ATA-2+ Strict") });
            } else if (family == QStringLiteral("SCSI (Auto)")) {
                featureLevel->addItems({ QStringLiteral("SCSI-1"), QStringLiteral("SCSI-2"), QStringLiteral("SASI"), QStringLiteral("SASI CHS") });
            }
            const int index = featureLevel->findText(preferred);
            featureLevel->setCurrentIndex(index >= 0 ? index : 0);
            featureLevel->setEnabled(family != QStringLiteral("UAE (uaehf.device)"));
            mediaType->setEnabled(family == QStringLiteral("IDE (Auto)"));
        };

        updateBootChecks();
        updatePhysicalControls();
        updateControllerUnitRange(controllerUnit, controller->currentText());
        setFeatureItems(hardfileFeatureText(*entry, controller->currentText()));

        connect(browse, &QPushButton::clicked, this, [this, path]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select hardfile"), path->text().isEmpty() ? QDir::homePath() : path->text(), QStringLiteral("Hardfiles (*.hdf *.vhd *.chd);;All files (*)"));
            if (!selected.isEmpty()) {
                path->setText(selected);
            }
        });
        connect(geometryBrowse, &QPushButton::clicked, this, [this, geometryFile]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select geometry file"), geometryFile->text().isEmpty() ? QDir::homePath() : geometryFile->text(), QStringLiteral("Geometry files (*.geo);;All files (*)"));
            if (!selected.isEmpty()) {
                geometryFile->setText(selected);
            }
        });
        connect(filesysBrowse, &QPushButton::clicked, this, [this, filesys]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select filesystem"), filesys->text().isEmpty() ? QDir::homePath() : filesys->text(), QStringLiteral("Filesystem files (*.fs *.filesystem);;All files (*)"));
            if (!selected.isEmpty()) {
                filesys->setText(selected);
            }
        });
        connect(bootPri, QOverload<int>::of(&QSpinBox::valueChanged), this, updateBootChecks);
        connect(autoboot, &QCheckBox::toggled, this, [bootPri, doNotMount](bool checked) {
            QSignalBlocker blocker(doNotMount);
            if (checked) {
                bootPri->setValue(0);
                doNotMount->setChecked(false);
            } else if (bootPri->value() > -128) {
                bootPri->setValue(-128);
            }
        });
        connect(doNotMount, &QCheckBox::toggled, this, [bootPri, autoboot](bool checked) {
            QSignalBlocker blocker(autoboot);
            if (checked) {
                bootPri->setValue(-129);
                autoboot->setChecked(false);
            } else if (bootPri->value() <= -129) {
                bootPri->setValue(-128);
            }
        });
        connect(rdbMode, &QCheckBox::toggled, this, [sectors, surfaces, reserved, blockSize, device, filesys, bootPri](bool checked) {
            if (checked) {
                sectors->setValue(0);
                surfaces->setValue(0);
                reserved->setValue(0);
                blockSize->setValue(512);
                device->clear();
                filesys->clear();
                bootPri->setValue(0);
            } else if (sectors->value() == 0 && surfaces->value() == 0 && reserved->value() == 0) {
                sectors->setValue(32);
                surfaces->setValue(1);
                reserved->setValue(2);
                blockSize->setValue(512);
            }
        });
        connect(manualGeometry, &QCheckBox::toggled, this, updatePhysicalControls);
        connect(controller, &QComboBox::currentTextChanged, this, [this, controllerUnit, setFeatureItems](const QString &text) {
            updateControllerUnitRange(controllerUnit, text);
            setFeatureItems(QStringLiteral("Default"));
        });
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted || path->text().trimmed().isEmpty()) {
            return false;
        }

        entry->kind = QStringLiteral("hdf");
        entry->device = winUaeQtSanitizedAmigaName(device->text(), nextMountDeviceName(), true);
        entry->path = path->text().trimmed();
        entry->bootPri = bootPri->value();
        entry->readOnly = !readWrite->isChecked();
        entry->hardfileGeometry = QStringLiteral("%1,%2,%3,%4")
            .arg(sectors->value())
            .arg(surfaces->value())
            .arg(reserved->value())
            .arg(blockSize->value());

        QStringList tailFields;
        tailFields.append(filesys->text().trimmed());
        tailFields.append(mountControllerConfigValue(controller->currentText(), controllerUnit->value()));
        if (manualGeometry->isChecked() || !geometryFile->text().trimmed().isEmpty()) {
            tailFields.append(QString::number(physicalCylinders->value()));
            tailFields.append(QStringLiteral("%1/%2/%3")
                .arg(physicalCylinders->value())
                .arg(physicalHeads->value())
                .arg(physicalSectors->value()));
            if (!geometryFile->text().trimmed().isEmpty()) {
                tailFields.append(geometryFile->text().trimmed());
            }
        }
        if (controller->currentText() == QStringLiteral("IDE (Auto)") && mediaType->currentText() == QStringLiteral("CF")) {
            tailFields.append(QStringLiteral("CF"));
        }
        const QString featureToken = hardfileFeatureToken(featureLevel->currentText());
        if (!featureToken.isEmpty()) {
            tailFields.append(featureToken);
        }
        tailFields.append(hardfilePreservedTailExtras(*entry));
        entry->hardfileTail = winUaeQtConfigJoinFields(tailFields);
        return true;
    }

    void updateControllerUnitRange(QSpinBox *unit, const QString &family)
    {
        if (!unit) {
            return;
        }
        const int maximum = family == QStringLiteral("IDE (Auto)") ? 3 : MaxControllerUnits - 1;
        unit->setMaximum(maximum);
    }

    bool showCdDriveMountDialog(WinUaeQtMountEntry *entry, const QString &title)
    {
        if (!entry) {
            return false;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(title);

        QSpinBox *cdUnit = new QSpinBox;
        cdUnit->setRange(0, MaxControllerUnits - 1);
        cdUnit->setValue(qBound(0, entry->emuUnit, MaxControllerUnits - 1));
        QComboBox *controller = combo({ QStringLiteral("IDE (Auto)"), QStringLiteral("SCSI (Auto)") }, mountControllerFamily(*entry, QStringLiteral("ide0")));
        QSpinBox *controllerUnit = new QSpinBox;
        controllerUnit->setRange(0, MaxControllerUnits - 1);
        controllerUnit->setValue(mountControllerUnit(*entry, QStringLiteral("ide0")));
        updateControllerUnitRange(controllerUnit, controller->currentText());

        QGridLayout *fields = new QGridLayout;
        fields->setColumnStretch(1, 1);
        fields->addWidget(label(QStringLiteral("CD unit:")), 0, 0);
        fields->addWidget(cdUnit, 0, 1);
        fields->addWidget(label(QStringLiteral("Controller:")), 1, 0);
        fields->addWidget(controller, 1, 1);
        fields->addWidget(label(QStringLiteral("Controller unit:")), 2, 0);
        fields->addWidget(controllerUnit, 2, 1);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QVBoxLayout *root = new QVBoxLayout(&dialog);
        root->addLayout(fields);
        root->addWidget(buttons);

        connect(controller, &QComboBox::currentTextChanged, this, [this, controllerUnit](const QString &text) {
            updateControllerUnitRange(controllerUnit, text);
        });
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        entry->kind = QStringLiteral("cd");
        entry->device.clear();
        entry->volume = QStringLiteral("CD");
        entry->emuUnit = cdUnit->value();
        entry->readOnly = true;
        entry->bootPri = 0;
        entry->hardfileGeometry = QStringLiteral("0,0,0,2048");
        entry->hardfileTail = mountTailWithController(*entry, mountControllerConfigValue(controller->currentText(), controllerUnit->value()));
        return true;
    }

    bool showTapeDriveMountDialog(WinUaeQtMountEntry *entry, const QString &title)
    {
        if (!entry) {
            return false;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(title);

        QLineEdit *path = new QLineEdit(entry->path);
        QSpinBox *tapeUnit = new QSpinBox;
        tapeUnit->setRange(0, MaxControllerUnits - 1);
        tapeUnit->setValue(qBound(0, entry->emuUnit, MaxControllerUnits - 1));
        QComboBox *controller = combo({ QStringLiteral("UAE (uaehf.device)"), QStringLiteral("IDE (Auto)"), QStringLiteral("SCSI (Auto)") }, mountControllerFamily(*entry, QStringLiteral("uae0")));
        QSpinBox *controllerUnit = new QSpinBox;
        controllerUnit->setRange(0, MaxControllerUnits - 1);
        controllerUnit->setValue(mountControllerUnit(*entry, QStringLiteral("uae0")));
        updateControllerUnitRange(controllerUnit, controller->currentText());
        QCheckBox *readWrite = new QCheckBox(QStringLiteral("Read/write"));
        readWrite->setChecked(!entry->readOnly);
        QPushButton *selectFile = new QPushButton(QStringLiteral("File..."));
        QPushButton *selectDirectory = new QPushButton(QStringLiteral("Directory..."));
        QPushButton *eject = new QPushButton(QStringLiteral("Eject"));

        QGridLayout *fields = new QGridLayout;
        fields->setColumnStretch(1, 1);
        fields->addWidget(label(QStringLiteral("Path:")), 0, 0);
        fields->addWidget(path, 0, 1, 1, 3);
        fields->addWidget(selectFile, 1, 1);
        fields->addWidget(selectDirectory, 1, 2);
        fields->addWidget(eject, 1, 3);
        fields->addWidget(label(QStringLiteral("Tape unit:")), 2, 0);
        fields->addWidget(tapeUnit, 2, 1);
        fields->addWidget(label(QStringLiteral("Controller:")), 3, 0);
        fields->addWidget(controller, 3, 1, 1, 3);
        fields->addWidget(label(QStringLiteral("Controller unit:")), 4, 0);
        fields->addWidget(controllerUnit, 4, 1);
        fields->addWidget(readWrite, 5, 1, 1, 3);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QVBoxLayout *root = new QVBoxLayout(&dialog);
        root->addLayout(fields);
        root->addWidget(buttons);

        connect(selectFile, &QPushButton::clicked, this, [this, path]() {
            const QString selected = QFileDialog::getOpenFileName(this, QStringLiteral("Select tape image"), path->text().isEmpty() ? QDir::homePath() : path->text(), QStringLiteral("Tape images (*.tap *.raw);;All files (*)"));
            if (!selected.isEmpty()) {
                path->setText(selected);
            }
        });
        connect(selectDirectory, &QPushButton::clicked, this, [this, path]() {
            const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral("Select tape directory"), path->text().isEmpty() ? QDir::homePath() : path->text());
            if (!selected.isEmpty()) {
                path->setText(selected);
            }
        });
        connect(eject, &QPushButton::clicked, path, &QLineEdit::clear);
        connect(controller, &QComboBox::currentTextChanged, this, [this, controllerUnit](const QString &text) {
            updateControllerUnitRange(controllerUnit, text);
        });
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        entry->kind = QStringLiteral("tape");
        entry->device.clear();
        entry->volume = QStringLiteral("TAPE");
        entry->path = path->text().trimmed();
        entry->emuUnit = tapeUnit->value();
        entry->readOnly = !readWrite->isChecked();
        entry->bootPri = 0;
        entry->hardfileGeometry = QStringLiteral("0,0,0,512");
        entry->hardfileTail = mountTailWithController(*entry, mountControllerConfigValue(controller->currentText(), controllerUnit->value()));
        return true;
    }

    WinUaeQtConfig::OrderedSettings currentMountSettings() const
    {
        WinUaeQtConfig::OrderedSettings settings;
        if (!mountedDrives) {
            return settings;
        }
        for (int i = 0; i < mountedDrives->topLevelItemCount(); i++) {
            const WinUaeQtMountEntry entry = mountEntryFromItem(mountedDrives->topLevelItem(i));
            if (entry.kind == QStringLiteral("dir")) {
                settings.append({ QStringLiteral("filesystem2"), serializeWinUaeQtFilesystem2MountValue(entry) });
            } else if (entry.kind == QStringLiteral("hdf")) {
                settings.append({ QStringLiteral("hardfile2"), serializeWinUaeQtHardfile2MountValue(entry) });
            } else if (entry.kind == QStringLiteral("cd")) {
                settings.append({ QStringLiteral("uaehf%1").arg(i), serializeWinUaeQtUaehfCdMountValue(entry) });
            } else if (entry.kind == QStringLiteral("tape")) {
                settings.append({ QStringLiteral("uaehf%1").arg(i), serializeWinUaeQtUaehfTapeMountValue(entry) });
            }
        }
        return settings;
    }

    void syncQuickDriveToFloppy(int drive)
    {
        if (drive < 0 || drive >= 2 || !dfPath[drive] || !quickDfPath[drive]) {
            return;
        }
        setCheckBoxIfChanged(dfEnable[drive], quickDfEnable[drive]->isChecked());
        setComboTextIfChanged(dfType[drive], quickDfType[drive]->currentText());
        setComboTextIfChanged(dfPath[drive], quickDfPath[drive]->currentText());
        setCheckBoxIfChanged(dfWriteProtect[drive], quickDfWriteProtect[drive]->isChecked());
    }

    void syncFloppyDriveToQuick(int drive)
    {
        if (drive < 0 || drive >= 2 || !dfPath[drive] || !quickDfPath[drive]) {
            return;
        }
        setCheckBoxIfChanged(quickDfEnable[drive], dfEnable[drive]->isChecked());
        setComboTextIfChanged(quickDfType[drive], dfType[drive]->currentText());
        setComboTextIfChanged(quickDfPath[drive], dfPath[drive]->currentText());
        setCheckBoxIfChanged(quickDfWriteProtect[drive], dfWriteProtect[drive]->isChecked());
    }

    WinUaeQtConfig::Settings currentSettings() const
    {
        WinUaeQtConfig::Settings settings;
        const int cpu = selectedCpuModel();
        const int fpu = fpuModelConfigValue(cpu);
        settings.insert(QStringLiteral("kickstart_rom_file"), romFile->currentText());
        if (!extendedRomFile->currentText().isEmpty()) {
            settings.insert(QStringLiteral("kickstart_ext_rom_file"), extendedRomFile->currentText());
        }
        for (int i = 0; i < 4; i++) {
            const int driveType = dfEnable[i]->isChecked() ? floppyTypeConfigValue(dfType[i]->currentText()) : -1;
            settings.insert(QStringLiteral("floppy%1type").arg(i), QString::number(driveType));
            settings.insert(QStringLiteral("floppy%1wp").arg(i), dfWriteProtect[i]->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
            if (driveType >= 0 && !dfPath[i]->currentText().isEmpty()) {
                settings.insert(QStringLiteral("floppy%1").arg(i), dfPath[i]->currentText());
            }
        }
        settings.insert(QStringLiteral("nr_floppies"), QString::number(enabledFloppyCount()));
        settings.insert(QStringLiteral("chipset"), chipset->currentText().toLower());
        settings.insert(QStringLiteral("chipset_compatible"), chipsetCompatible->currentText());
        settings.insert(QStringLiteral("cpu_model"), QString::number(cpu));
        if (fpu) {
            settings.insert(QStringLiteral("fpu_model"), QString::number(fpu));
        }
        if (cpu >= 68020) {
            settings.insert(QStringLiteral("cpu_24bit_addressing"), cpu24Bit->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        }
        settings.insert(QStringLiteral("chipmem_size"), QString::number(chipMemConfigValue()));
        if (z2Fast->currentText() != QStringLiteral("None")) {
            settings.insert(QStringLiteral("fastmem_size"), QString::number(megabytesFromText(z2Fast->currentText())));
        }
        const int slow = slowMemConfigValue();
        if (slow) {
            settings.insert(QStringLiteral("bogomem_size"), QString::number(slow));
        }
        if (z3Fast->currentText() != QStringLiteral("None")) {
            settings.insert(QStringLiteral("z3mem_size"), QString::number(megabytesFromText(z3Fast->currentText())));
        }
        settings.insert(QStringLiteral("cachesize"), jit->isChecked() ? QStringLiteral("8") : QStringLiteral("0"));
        settings.insert(QStringLiteral("sound_output"), soundOutput->currentText());
        if (rtgMem->currentText() != QStringLiteral("None")) {
            settings.insert(QStringLiteral("gfxcard_size"), QString::number(megabytesFromText(rtgMem->currentText())));
            settings.insert(QStringLiteral("gfxcard_type"), rtgType->currentText());
        }
        if (!windowWidth->text().isEmpty()) {
            settings.insert(QStringLiteral("gfx_width_windowed"), windowWidth->text());
        }
        if (!windowHeight->text().isEmpty()) {
            settings.insert(QStringLiteral("gfx_height_windowed"), windowHeight->text());
        }
        for (int i = 0; i < MaxCdSlots; i++) {
            const QString value = cdSlotConfigValue(cdSlotState(i));
            if (!value.isEmpty()) {
                settings.insert(QStringLiteral("cdimage%1").arg(i), value);
            }
        }
        settings.insert(QStringLiteral("cd_speed"), cdSpeedTurbo->isChecked() ? QStringLiteral("0") : QStringLiteral("100"));
        return settings;
    }

    QStringList uiOwnedKeys() const
    {
        return {
            QStringLiteral("kickstart_rom_file"),
            QStringLiteral("kickstart_ext_rom_file"),
            QStringLiteral("floppy0"),
            QStringLiteral("floppy1"),
            QStringLiteral("floppy2"),
            QStringLiteral("floppy3"),
            QStringLiteral("floppy0type"),
            QStringLiteral("floppy1type"),
            QStringLiteral("floppy2type"),
            QStringLiteral("floppy3type"),
            QStringLiteral("floppy0wp"),
            QStringLiteral("floppy1wp"),
            QStringLiteral("floppy2wp"),
            QStringLiteral("floppy3wp"),
            QStringLiteral("nr_floppies"),
            QStringLiteral("chipset"),
            QStringLiteral("chipset_compatible"),
            QStringLiteral("cpu_model"),
            QStringLiteral("fpu_model"),
            QStringLiteral("cpu_24bit_addressing"),
            QStringLiteral("chipmem_size"),
            QStringLiteral("fastmem_size"),
            QStringLiteral("bogomem_size"),
            QStringLiteral("z3mem_size"),
            QStringLiteral("cachesize"),
            QStringLiteral("sound_output"),
            QStringLiteral("gfxcard_size"),
            QStringLiteral("gfxcard_type"),
            QStringLiteral("gfx_width_windowed"),
            QStringLiteral("gfx_height_windowed"),
            QStringLiteral("cdimage0"),
            QStringLiteral("cdimage1"),
            QStringLiteral("cdimage2"),
            QStringLiteral("cdimage3"),
            QStringLiteral("cdimage4"),
            QStringLiteral("cdimage5"),
            QStringLiteral("cdimage6"),
            QStringLiteral("cdimage7"),
            QStringLiteral("cd_speed")
        };
    }

    QStringList uiOwnedMountKeys() const
    {
        return {
            QStringLiteral("filesystem2"),
            QStringLiteral("hardfile2"),
            QStringLiteral("uaehf0"),
            QStringLiteral("uaehf1"),
            QStringLiteral("uaehf2"),
            QStringLiteral("uaehf3"),
            QStringLiteral("uaehf4"),
            QStringLiteral("uaehf5"),
            QStringLiteral("uaehf6"),
            QStringLiteral("uaehf7")
        };
    }

    WinUaeQtConfig mergedConfig() const
    {
        WinUaeQtConfig config = loadedConfig;
        config.applySettings(currentSettings(), uiOwnedKeys());
        config.applyRepeatedSettings(currentMountSettings(), uiOwnedMountKeys());
        return config;
    }

    int enabledFloppyCount() const
    {
        int count = 0;
        for (int i = 0; i < 4; i++) {
            if (dfEnable[i]->isChecked() && floppyTypeConfigValue(dfType[i]->currentText()) >= 0) {
                count = i + 1;
            }
        }
        return qMax(1, count);
    }

    void loadConfigDialog()
    {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Load configuration"), configsPath->text(), QStringLiteral("WinUAE configuration (*.uae);;All files (*)"));
        if (!path.isEmpty()) {
            loadConfig(path);
        }
    }

    void saveConfigDialog()
    {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save configuration"), configsPath->text(), QStringLiteral("WinUAE configuration (*.uae);;All files (*)"));
        if (!path.isEmpty()) {
            saveConfig(path);
        }
    }

    void loadConfig(const QString &path)
    {
        WinUaeQtConfig config;
        QString error;
        if (!config.load(path, &error)) {
            QMessageBox::warning(this, windowTitle(), error);
            return;
        }
        if (mountedDrives) {
            mountedDrives->clear();
        }
        clearCdSlots();
        for (const WinUaeQtConfig::Setting &setting : config.orderedSettings()) {
            applySetting(setting.key, setting.value);
        }
        updateMountButtons();
        loadedConfig = config;
        configPath->setText(path);
        configName->setCurrentText(QFileInfo(path).completeBaseName());
        status->setText(QStringLiteral("Loaded %1").arg(path));
    }

    void applySetting(const QString &key, const QString &value)
    {
        if (key == QStringLiteral("kickstart_rom_file")) {
            romFile->setCurrentText(value);
        } else if (key == QStringLiteral("kickstart_ext_rom_file")) {
            extendedRomFile->setCurrentText(value);
        } else if (key.startsWith(QStringLiteral("floppy")) && key.endsWith(QStringLiteral("type"))) {
            bool ok = false;
            const int drive = key.mid(6, 1).toInt(&ok);
            if (ok && drive >= 0 && drive < 4) {
                const int driveType = value.toInt();
                dfEnable[drive]->setChecked(driveType >= 0);
                dfType[drive]->setCurrentText(floppyTypeText(driveType));
                syncFloppyDriveToQuick(drive);
            }
        } else if (key.startsWith(QStringLiteral("floppy")) && key.endsWith(QStringLiteral("wp"))) {
            bool ok = false;
            const int drive = key.mid(6, 1).toInt(&ok);
            if (ok && drive >= 0 && drive < 4) {
                dfWriteProtect[drive]->setChecked(configBoolValue(value));
                syncFloppyDriveToQuick(drive);
            }
        } else if (key.startsWith(QStringLiteral("floppy"))) {
            bool ok = false;
            const int drive = key.mid(6, 1).toInt(&ok);
            if (ok && drive >= 0 && drive < 4) {
                dfEnable[drive]->setChecked(true);
                dfPath[drive]->setCurrentText(value);
                syncFloppyDriveToQuick(drive);
            }
        } else if (key.startsWith(QStringLiteral("uaehf"))) {
            WinUaeQtMountEntry entry;
            if (parseWinUaeQtUaehfMountValue(value, &entry)) {
                addMountEntry(entry);
            }
        } else if (key == QStringLiteral("filesystem2")) {
            WinUaeQtMountEntry entry;
            if (parseWinUaeQtFilesystem2MountValue(value, &entry)) {
                addMountEntry(entry);
            }
        } else if (key == QStringLiteral("hardfile2")) {
            WinUaeQtMountEntry entry;
            if (parseWinUaeQtHardfile2MountValue(value, &entry)) {
                addMountEntry(entry);
            }
        } else if (key.startsWith(QStringLiteral("cdimage"))) {
            bool ok = false;
            const int slot = key.mid(7).toInt(&ok);
            if (ok && slot >= 0 && slot < MaxCdSlots) {
                ensureCdSlots();
                cdSlots[slot] = cdSlotFromConfigValue(value);
                if (slot == currentCdSlot) {
                    loadCdSlotToUi(slot);
                }
            }
        } else if (key == QStringLiteral("cd_speed")) {
            cdSpeedTurbo->setChecked(value == QStringLiteral("0"));
        } else if (key == QStringLiteral("chipset")) {
            chipset->setCurrentText(value.toUpper());
        } else if (key == QStringLiteral("chipset_compatible")) {
            chipsetCompatible->setCurrentText(value);
            quickModel->setCurrentText(value);
        } else if (key == QStringLiteral("cpu_model")) {
            setCpuButton(value.toInt());
        } else if (key == QStringLiteral("fpu_model")) {
            setFpuButton(value.toInt());
        } else if (key == QStringLiteral("cpu_24bit_addressing")) {
            cpu24Bit->setChecked(value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
        } else if (key == QStringLiteral("chipmem_size")) {
            const QMap<int, QString> map = { { 1, QStringLiteral("512 KB") }, { 2, QStringLiteral("1 MB") }, { 4, QStringLiteral("2 MB") }, { 8, QStringLiteral("4 MB") }, { 16, QStringLiteral("8 MB") } };
            chipMem->setCurrentText(map.value(value.toInt(), QStringLiteral("2 MB")));
        } else if (key == QStringLiteral("fastmem_size")) {
            z2Fast->setCurrentText(value == QStringLiteral("0") ? QStringLiteral("None") : value + QStringLiteral(" MB"));
        } else if (key == QStringLiteral("z3mem_size")) {
            z3Fast->setCurrentText(value == QStringLiteral("0") ? QStringLiteral("None") : value + QStringLiteral(" MB"));
        } else if (key == QStringLiteral("gfxcard_size")) {
            rtgMem->setCurrentText(value == QStringLiteral("0") ? QStringLiteral("None") : value + QStringLiteral(" MB"));
        } else if (key == QStringLiteral("gfxcard_type")) {
            rtgType->setCurrentText(value);
        } else if (key == QStringLiteral("sound_output")) {
            soundOutput->setCurrentText(value);
        } else if (key == QStringLiteral("gfx_width_windowed")) {
            windowWidth->setText(value);
        } else if (key == QStringLiteral("gfx_height_windowed")) {
            windowHeight->setText(value);
        }
    }

    void saveConfig(const QString &path)
    {
        WinUaeQtConfig config = mergedConfig();
        QString error;
        if (!config.save(path, &error)) {
            QMessageBox::warning(this, windowTitle(), error);
            return;
        }
        loadedConfig = config;
        configPath->setText(path);
        configName->setCurrentText(QFileInfo(path).completeBaseName());
        status->setText(QStringLiteral("Saved %1").arg(path));
    }

    void startEmulator()
    {
        const WinUaeQtConfig config = mergedConfig();
        const QStringList validationErrors = config.validateForLaunch();
        if (!validationErrors.isEmpty()) {
            QMessageBox::warning(this, windowTitle(), validationErrors.join(QLatin1Char('\n')));
            navigation->setCurrentItem(navigation->topLevelItem(4));
            return;
        }

        if (startMode == StartMode::ReturnConfig) {
            result.status = WinUaeQtLauncherStatus::StartRequested;
            result.config = config;
            accept();
            return;
        }

        const QString program = emulatorPath->text();
        if (program.isEmpty() || !QFileInfo::exists(program)) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("Emulator executable not found."));
            return;
        }

        QString error;
        if (!launcherBackend.start(program, config, &error)) {
            QMessageBox::warning(this, windowTitle(), error);
            return;
        }
        status->setText(QStringLiteral("Started %1").arg(program));
    }
};

static void setupApplicationStyle(QApplication &app)
{
    if (QStyle *style = QStyleFactory::create(QStringLiteral("Windows"))) {
        app.setStyle(style);
    } else if (QStyle *style = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app.setStyle(style);
    }
    QString family = QStringLiteral("MS Sans Serif");
    const QStringList families = QFontDatabase::families();
    if (!families.contains(family)) {
        const QStringList fallbacks = {
            QStringLiteral("Microsoft Sans Serif"),
            QStringLiteral("Tahoma"),
            QStringLiteral("Arial")
        };
        for (const QString &candidate : fallbacks) {
            if (families.contains(candidate)) {
                family = candidate;
                break;
            }
        }
        if (!families.contains(family)) {
            family = app.font().family();
        }
    }
    QFont font(family);
    font.setPixelSize(12);
    app.setFont(font);
    app.setStyleSheet(QStringLiteral(
        "QDialog, QWidget#page, QStackedWidget#pageStack { background: #f0f0f0; }"
        "QFrame#outerFrame { border: 1px solid #808080; background: #f0f0f0; }"
        "QTreeWidget { background: white; border: 1px solid #7f9db9; }"
        "QGroupBox { margin-top: 14px; padding: 9px 6px 6px 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; font-size: 13px; }"
        "QPushButton { min-height: 20px; padding: 1px 10px; }"
        "QLineEdit, QComboBox { min-height: 22px; }"
        "QLabel#statusLine { padding-left: 6px; background: #f0f0f0; }"
    ));
}

int runWinUaeQtLauncher(QApplication &app)
{
    setupApplicationStyle(app);
    WinUaeQtDialog dialog;
    dialog.show();
    return app.exec();
}

int runWinUaeQtLauncher(int argc, char **argv)
{
    QApplication app(argc, argv);
    return runWinUaeQtLauncher(app);
}

WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(QApplication &app)
{
    setupApplicationStyle(app);
    WinUaeQtDialog dialog(WinUaeQtDialog::StartMode::ReturnConfig);
    if (dialog.exec() == QDialog::Accepted) {
        return dialog.launcherResult();
    }

    WinUaeQtLauncherResult result;
    result.status = WinUaeQtLauncherStatus::Canceled;
    return result;
}

WinUaeQtLauncherResult runWinUaeQtLauncherForConfig(int argc, char **argv)
{
    QApplication app(argc, argv);
    return runWinUaeQtLauncherForConfig(app);
}
