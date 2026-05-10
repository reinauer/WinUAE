#include <QtWidgets>

#ifndef WINUAE_UNIX_SOURCE_DIR
#define WINUAE_UNIX_SOURCE_DIR "."
#endif

#ifndef WINUAE_UNIX_BUILD_DIR
#define WINUAE_UNIX_BUILD_DIR "."
#endif

static QString sourceFile(const QString &relative)
{
    return QDir(QString::fromUtf8(WINUAE_UNIX_SOURCE_DIR)).filePath(relative);
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

static QPushButton *smallButton(const QString &text)
{
    QPushButton *w = new QPushButton(text);
    w->setFixedWidth(text == QStringLiteral("...") ? 24 : 34);
    return w;
}

static QGroupBox *groupBox(const QString &title, QLayout *layout)
{
    QGroupBox *box = new QGroupBox(title);
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
    explicit WinUaeQtDialog(QWidget *parent = nullptr)
        : QDialog(parent)
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

private:
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
            fpuButtons->addButton(button, i);
            if (i == 0) {
                button->setChecked(true);
            }
        }
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

        chipset = combo({ QStringLiteral("ocs"), QStringLiteral("ecs"), QStringLiteral("aga") }, QStringLiteral("aga"));
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
        system->addWidget(new QCheckBox(QStringLiteral("MapROM emulation")), 2, 1);
        system->addWidget(new QCheckBox(QStringLiteral("ShapeShifter support")), 2, 2);
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
        addLineBrowseRow(misc, 1, QStringLiteral("Flash RAM or A2286/A2386SX BIOS CMOS RAM file:"), flashFile);
        addLineBrowseRow(misc, 2, QStringLiteral("Real Time Clock file"), rtcFile);
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
        }
    }

    QWidget *makeHardDrivesPage()
    {
        QWidget *page = makePage();
        QVBoxLayout *root = new QVBoxLayout(page);
        root->setContentsMargins(4, 4, 4, 4);
        QTreeWidget *volumes = new QTreeWidget;
        volumes->setHeaderLabels({ QStringLiteral("Device"), QStringLiteral("Volume"), QStringLiteral("Path") });
        QVBoxLayout *volumeLayout = new QVBoxLayout;
        volumeLayout->addWidget(volumes);
        root->addWidget(groupBox(QStringLiteral("Mounted drives"), volumeLayout), 1);
        QHBoxLayout *buttons = new QHBoxLayout;
        buttons->addWidget(new QPushButton(QStringLiteral("Add Directory or Archive...")));
        buttons->addWidget(new QPushButton(QStringLiteral("Add Hardfile...")));
        buttons->addWidget(new QPushButton(QStringLiteral("Remove")));
        buttons->addStretch();
        root->addLayout(buttons);
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
        QLabel *subtitle = new QLabel(QStringLiteral("Unix Qt configuration frontend"));
        subtitle->setAlignment(Qt::AlignCenter);
        root->addWidget(subtitle);
        QPushButton *contributors = new QPushButton(QStringLiteral("Contributors"));
        contributors->setFixedWidth(120);
        QHBoxLayout *buttonRow = new QHBoxLayout;
        buttonRow->addStretch();
        buttonRow->addWidget(contributors);
        buttonRow->addStretch();
        root->addLayout(buttonRow);
        root->addStretch();
        return page;
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

    void resetDefaults()
    {
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

        windowWidth->setText(QStringLiteral("720"));
        windowHeight->setText(QStringLiteral("568"));
        windowResize->setChecked(true);
        nativeMode->setCurrentText(QStringLiteral("Windowed"));
        rtgMode->setCurrentText(QStringLiteral("Windowed"));
        soundOutput->setCurrentText(QStringLiteral("normal"));
        port0->setCurrentText(QStringLiteral("Mouse"));
        port1->setCurrentText(QStringLiteral("Keyboard Layout B"));
        romsPath->setText(QDir::homePath());
        configsPath->setText(QDir::homePath());
        status->setText(QStringLiteral("Ready"));
    }

    void applyModelPreset(const QString &model)
    {
        if (model == QStringLiteral("A1200")) {
            chipset->setCurrentText(QStringLiteral("aga"));
            chipsetCompatible->setCurrentText(QStringLiteral("A1200"));
            setCpuButton(68020);
            cpu24Bit->setChecked(false);
            chipMem->setCurrentText(QStringLiteral("2 MB"));
        } else if (model == QStringLiteral("A4000")) {
            chipset->setCurrentText(QStringLiteral("aga"));
            chipsetCompatible->setCurrentText(QStringLiteral("A4000"));
            setCpuButton(68040);
            cpu24Bit->setChecked(false);
            chipMem->setCurrentText(QStringLiteral("2 MB"));
        } else if (model == QStringLiteral("A600")) {
            chipset->setCurrentText(QStringLiteral("ecs"));
            chipsetCompatible->setCurrentText(QStringLiteral("A600"));
            setCpuButton(68000);
            cpu24Bit->setChecked(true);
            chipMem->setCurrentText(QStringLiteral("2 MB"));
        } else if (model == QStringLiteral("A500+")) {
            chipset->setCurrentText(QStringLiteral("ecs"));
            chipsetCompatible->setCurrentText(QStringLiteral("A500+"));
            setCpuButton(68000);
            cpu24Bit->setChecked(true);
            chipMem->setCurrentText(QStringLiteral("1 MB"));
        } else {
            chipset->setCurrentText(QStringLiteral("ocs"));
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

    QMap<QString, QString> currentSettings() const
    {
        QMap<QString, QString> settings;
        const int cpu = cpuButtons->checkedId();
        settings.insert(QStringLiteral("kickstart_rom_file"), romFile->currentText());
        if (!extendedRomFile->currentText().isEmpty()) {
            settings.insert(QStringLiteral("kickstart_ext_rom_file"), extendedRomFile->currentText());
        }
        for (int i = 0; i < 4; i++) {
            if (dfEnable[i]->isChecked() && !dfPath[i]->currentText().isEmpty()) {
                settings.insert(QStringLiteral("floppy%1").arg(i), dfPath[i]->currentText());
            }
        }
        settings.insert(QStringLiteral("nr_floppies"), QString::number(enabledFloppyCount()));
        settings.insert(QStringLiteral("chipset"), chipset->currentText());
        settings.insert(QStringLiteral("chipset_compatible"), chipsetCompatible->currentText());
        settings.insert(QStringLiteral("cpu_model"), QString::number(cpu > 0 ? cpu : 68020));
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
        return settings;
    }

    int enabledFloppyCount() const
    {
        int count = 0;
        for (int i = 0; i < 4; i++) {
            if (dfEnable[i]->isChecked() && !dfPath[i]->currentText().isEmpty()) {
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
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, windowTitle(), file.errorString());
            return;
        }
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            const int sep = line.indexOf(QLatin1Char('='));
            if (sep <= 0) {
                continue;
            }
            applySetting(line.left(sep).trimmed(), line.mid(sep + 1).trimmed());
        }
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
        } else if (key.startsWith(QStringLiteral("floppy"))) {
            bool ok = false;
            const int drive = key.mid(6, 1).toInt(&ok);
            if (ok && drive >= 0 && drive < 4) {
                dfEnable[drive]->setChecked(true);
                dfPath[drive]->setCurrentText(value);
            }
        } else if (key == QStringLiteral("chipset")) {
            chipset->setCurrentText(value);
        } else if (key == QStringLiteral("chipset_compatible")) {
            chipsetCompatible->setCurrentText(value);
            quickModel->setCurrentText(value);
        } else if (key == QStringLiteral("cpu_model")) {
            setCpuButton(value.toInt());
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
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QMessageBox::warning(this, windowTitle(), file.errorString());
            return;
        }
        QTextStream out(&file);
        out << "; WinUAE Unix Qt configuration\n";
        const QMap<QString, QString> settings = currentSettings();
        for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
            if (!it.value().isEmpty()) {
                out << it.key() << "=" << it.value() << "\n";
            }
        }
        configPath->setText(path);
        configName->setCurrentText(QFileInfo(path).completeBaseName());
        status->setText(QStringLiteral("Saved %1").arg(path));
    }

    void startEmulator()
    {
        const QString program = emulatorPath->text();
        if (program.isEmpty() || !QFileInfo::exists(program)) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("Emulator executable not found."));
            return;
        }
        if (romFile->currentText().isEmpty()) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("Main ROM file is required."));
            navigation->setCurrentItem(navigation->topLevelItem(4));
            return;
        }

        QStringList args;
        const QMap<QString, QString> settings = currentSettings();
        for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
            if (!it.value().isEmpty()) {
                args << QStringLiteral("-s") << QStringLiteral("%1=%2").arg(it.key(), it.value());
            }
        }
        if (!QProcess::startDetached(program, args)) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("Failed to start emulator."));
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
        family = families.contains(QStringLiteral("Arial")) ? QStringLiteral("Arial") : app.font().family();
    }
    QFont font(family, 8);
    app.setFont(font);
    app.setStyleSheet(QStringLiteral(
        "QDialog, QWidget#page, QStackedWidget#pageStack { background: #f0f0f0; }"
        "QFrame#outerFrame { border: 1px solid #808080; background: #f0f0f0; }"
        "QTreeWidget { background: white; border: 1px solid #7f9db9; }"
        "QGroupBox { margin-top: 12px; padding: 8px 6px 6px 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; }"
        "QPushButton { min-height: 20px; padding: 1px 10px; }"
        "QLineEdit, QComboBox { min-height: 20px; }"
        "QLabel#statusLine { padding-left: 6px; background: #f0f0f0; }"
    ));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    setupApplicationStyle(app);
    WinUaeQtDialog dialog;
    dialog.show();
    return app.exec();
}
