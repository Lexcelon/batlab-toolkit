#include "batlabmainwindow.h"
#include <QFileDialog>
#include "inputstringdialog.h"
#include <QScrollBar>
#include <cellmodulewidget.h>
#include <QSpacerItem>

BatlabMainWindow::BatlabMainWindow(QWidget *parent) :
    QMainWindow(parent)
{
    // Basic main window setup
    centralWidget = new QWidget();
    setCentralWidget(centralWidget);
    this->resize(800, 600);
    this->showMaximized();
    this->setWindowTitle(tr("Batlab Toolkit GUI"));

    // Set state for the main window logic
    batlabManager = new BatlabManager;

    // Setup the UI
    initializeMainWindowUI();

    connect(this, &BatlabMainWindow::emitUpdateText,
            this, &BatlabMainWindow::updateLiveViewTextBrowser);

    createActions();
    createMenus();

    statusBar()->showMessage(tr("Welcome to Batlab Toolkit GUI"));

    // TODO remove
    // Managing data from cells
    cellManager = new batlabCellManager;

    // Check for updates when the program opens and only display anything if updates are available
    // I have disabled this because it asks if maintenancetool.exe can make changes to your computer every time you open the program
    // Will reconsider in future especially if I can make it not intrusive. For now user can run "Check for updates"
    // updaterController->start(QtAutoUpdater::UpdateController::InfoLevel);
}

void BatlabMainWindow::initializeMainWindowUI()
{
    cellPlaylistButton = new QPushButton(tr("Cell Playlist"));
    batlabsButton = new QPushButton(tr("Batlabs"));
    liveViewButton = new QPushButton(tr("Live View"));
    resultsButton = new QPushButton(tr("Results"));

    tabButtonBox = new QDialogButtonBox;
    tabButtonBox->setOrientation(Qt::Vertical);
    tabButtonBox->addButton(cellPlaylistButton, QDialogButtonBox::ActionRole);
    tabButtonBox->addButton(batlabsButton, QDialogButtonBox::ActionRole);
    tabButtonBox->addButton(liveViewButton, QDialogButtonBox::ActionRole);
    tabButtonBox->addButton(resultsButton, QDialogButtonBox::ActionRole);

    mainStackedWidget = new QStackedWidget;

    cellPlaylistTabWidget = new QFrame;
    cellPlaylistTabWidget->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    cellPlaylistTabWidget->setLineWidth(2);

    batlabsTabWidget = new QFrame;
    batlabsTabWidget->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    batlabsTabWidget->setLineWidth(2);

    liveViewTabWidget = new QFrame;
    liveViewTabWidget->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    liveViewTabWidget->setLineWidth(2);

    resultsTabWidget = new QFrame;
    resultsTabWidget->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    resultsTabWidget->setLineWidth(2);

    cellPlaylistNotLoadedWidget = new QWidget;
    cellPlaylistNotLoadedLayout = new QGridLayout;

    noCellPlaylistLoadedLabel = new QLabel(tr("No cell playlist is loaded. Create a new playlist or open an existing one."));
    newCellPlaylistButton = new QPushButton(tr("New Cell Playlist"));
    connect(newCellPlaylistButton, &QPushButton::clicked, this, &BatlabMainWindow::showNewCellPlaylistWizard);
    openCellPlaylistButton = new QPushButton(tr("Open Cell Playlist"));
    connect(openCellPlaylistButton, &QPushButton::clicked, this, &BatlabMainWindow::openCellPlaylist);

    cellPlaylistNotLoadedLayout->addWidget(noCellPlaylistLoadedLabel, 1, 1, 1, 3, Qt::AlignCenter);
    cellPlaylistNotLoadedLayout->addWidget(newCellPlaylistButton, 3, 1, Qt::AlignCenter);
    cellPlaylistNotLoadedLayout->addWidget(openCellPlaylistButton, 3, 3, Qt::AlignCenter);
    cellPlaylistNotLoadedLayout->setColumnStretch(0, 16);
    cellPlaylistNotLoadedLayout->setColumnStretch(2, 1);
    cellPlaylistNotLoadedLayout->setColumnStretch(4, 16);
    cellPlaylistNotLoadedLayout->setRowStretch(0, 6);
    cellPlaylistNotLoadedLayout->setRowStretch(2, 1);
    cellPlaylistNotLoadedLayout->setRowStretch(4, 16);
    cellPlaylistNotLoadedWidget->setLayout(cellPlaylistNotLoadedLayout);

    cellPlaylistLoadedWidget = new QWidget;
    cellPlaylistLoadedLayout = new QGridLayout;

    cellPlaylistLoadedLayout->addWidget(openCellPlaylistButton, 1, 0);
    cellPlaylistLoadedLayout->addWidget(newCellPlaylistButton, 2, 0);
    cellPlaylistLoadedWidget->setLayout(cellPlaylistLoadedLayout);

    cellPlaylistStackedWidget = new QStackedWidget;
    cellPlaylistStackedWidget->addWidget(cellPlaylistNotLoadedWidget);
    cellPlaylistStackedWidget->addWidget(cellPlaylistLoadedWidget);
    cellPlaylistStackedWidget->setCurrentWidget(cellPlaylistLoadedWidget); // TODO temporary for testing

    cellPlaylistTabLayout = new QGridLayout;
    cellPlaylistTabLayout->addWidget(cellPlaylistStackedWidget, 0, 0);
    cellPlaylistTabWidget->setLayout(cellPlaylistTabLayout);

    liveViewTextBrowser = new QTextBrowser;
    liveViewTextBrowser->insertPlainText(QString(">> Welcome to Batlab Toolkit GUI\n" ));

    liveViewTabLayout = new QGridLayout;
    liveViewTabLayout->addWidget(liveViewTextBrowser, 0, 0, Qt::AlignTop);
    liveViewTabWidget->setLayout(liveViewTabLayout);

    mainStackedWidget->addWidget(cellPlaylistTabWidget);
    mainStackedWidget->addWidget(batlabsTabWidget);
    mainStackedWidget->addWidget(liveViewTabWidget);
    mainStackedWidget->addWidget(resultsTabWidget);

    // Some fun functor syntax to pass arguments to the signal https://stackoverflow.com/a/22411267
    // You have to capture ''this'' and then access variables from there https://stackoverflow.com/questions/7895879/using-member-variable-in-lambda-capture-list-inside-a-member-function
    connect(cellPlaylistButton, &QPushButton::clicked, this, [this]{ mainStackedWidget->setCurrentWidget(cellPlaylistTabWidget); });
    connect(batlabsButton, &QPushButton::clicked, this, [this]{ mainStackedWidget->setCurrentWidget(batlabsTabWidget); });
    connect(liveViewButton, &QPushButton::clicked, this, [this]{ mainStackedWidget->setCurrentWidget(liveViewTabWidget); });
    connect(resultsButton, &QPushButton::clicked, this, [this]{ mainStackedWidget->setCurrentWidget(resultsTabWidget); });

    testCellsTabLayout = new QGridLayout;
    testCellsTabLayout->addWidget(tabButtonBox, 0, 0);
    testCellsTabLayout->addWidget(mainStackedWidget, 0, 1);

    testCellsTab = new QWidget;
    testCellsTab->setLayout(testCellsTabLayout);

    configurePackTabLayout = new QHBoxLayout;

    configurePackTab = new QWidget;
    configurePackTab->setLayout(configurePackTabLayout);

    mainTabWidget = new QTabWidget;
    mainTabWidget->addTab(testCellsTab, tr("Test Cells"));
    mainTabWidget->addTab(configurePackTab, tr("Configure Pack"));

    centralWidgetLayout = new QGridLayout;
    centralWidgetLayout->addWidget(mainTabWidget);
    centralWidget->setLayout(centralWidgetLayout);
}

void BatlabMainWindow::createActions()
{
    newCellPlaylistAct = new QAction(tr("&New Cell Playlist"), this);
    newCellPlaylistAct->setShortcuts(QKeySequence::New);
    newCellPlaylistAct->setStatusTip(tr("Create a new cell playlist"));
    connect(newCellPlaylistAct, &QAction::triggered, this, &BatlabMainWindow::newCellPlaylist);

    openCellPlaylistAct = new QAction(tr("&Open Cell Playlist"), this);
    openCellPlaylistAct->setShortcuts(QKeySequence::Open);
    openCellPlaylistAct->setStatusTip(tr("Open an existing cell playlist"));
    connect(openCellPlaylistAct, &QAction::triggered, this, &BatlabMainWindow::openCellPlaylist);

    exitBatlabToolkitGUIAct = new QAction(tr("Exit"), this);
    exitBatlabToolkitGUIAct->setShortcuts(QKeySequence::Close);
    exitBatlabToolkitGUIAct->setStatusTip(tr("Close Batlab Toolkit GUI"));
    connect(exitBatlabToolkitGUIAct, &QAction::triggered, this, &BatlabMainWindow::exitBatlabToolkitGUI);

    debugBatlabAct = new QAction(tr("Debug Batlab"), this);
    debugBatlabAct->setStatusTip(tr("Debug a Batlab by reading and writing registers"));
    connect(debugBatlabAct, &QAction::triggered, this, &BatlabMainWindow::debugBatlab);

    aboutBatlabToolkitGUIAct = new QAction(tr("About Batlab Toolkit GUI"), this);
    aboutBatlabToolkitGUIAct->setStatusTip(tr("Information about the Batlab Toolkit GUI program"));
    connect(aboutBatlabToolkitGUIAct, &QAction::triggered, this, &BatlabMainWindow::aboutBatlabToolkitGUI);

    applicationUpdateController = new QtAutoUpdater::UpdateController("maintenancetool", this, qApp);
    applicationUpdateController->setDetailedUpdateInfo(true);
    checkForUpdatesAct = applicationUpdateController->createUpdateAction(this);
    checkForUpdatesAct->setIconVisibleInMenu(true);
}

void BatlabMainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newCellPlaylistAct);
    fileMenu->addAction(openCellPlaylistAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitBatlabToolkitGUIAct);

    toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(debugBatlabAct);

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutBatlabToolkitGUIAct);
    helpMenu->addSeparator();
    helpMenu->addAction(checkForUpdatesAct);
}

void BatlabMainWindow::newCellPlaylist()
{
    showNewCellPlaylistWizard();
}

void BatlabMainWindow::openCellPlaylist()
{
    // First do the file thing
    // Then actually load the settings into the GUI
    loadPlaylistIntoGUI();
}

void BatlabMainWindow::exitBatlabToolkitGUI()
{
    this->close();
}

void BatlabMainWindow::debugBatlab()
{
    // For testing communications with batlab - TODO BRING THIS BACK EVENTUALLY
    if (batlabDebugDialog == nullptr) {
        batlabDebugDialog = new BatlabDebugDialog(this, BatlabObjects);
//           connect(testObj,SIGNAL(emitReadReg(int,int)),BatlabObjects.first(),SLOT(onReadReg(int,int)));
//           connect(testObj,SIGNAL(emitWriteReg(int,int,int)),BatlabObjects.first(),SLOT(onWriteReg(int,int,int)));
//           connect(testObj,SIGNAL(emitPrint(uchar,properties)),cellManager,SLOT(onPrintCell(uchar,properties)));
    }

    //Can move window around
    batlabDebugDialog->setModal(false);
        batlabDebugDialog->show();
}

void BatlabMainWindow::checkForBatlabFirmwareUpdates()
{
    // TODO
}

void BatlabMainWindow::aboutBatlabToolkitGUI()
{
    QString msgText = QString("<p>Batlab Toolkit GUI, Version %1"
                              "<p>© Lexcelon, LLC %2"
                              "<hr>"
                              "<p>Batlab Toolkit GUI is provided under the GPL license."
                              "<p>Source code is available on <a href=\"https://www.github.com/lexcelon/batlab-toolkit-gui\">GitHub</a>."
                              "<p>Documentation is available on the <a href=\"https://www.lexcelon.com/resources/\">resources</a> page on our website."
                              "<p>Please <a href=\"https://www.lexcelon.com\">visit our website</a>"
                              " or <a href=\"mailto:support@lexcelon.com\">contact us</a> for more information."
                              "<hr>"
                              "<p>The Batlab is made possible through the support and participation of our backers and customers. Thank you!"
                              ).arg(BATLAB_TOOLKIT_GUI_VERSION, QDate::currentDate().toString("yyyy"));
    QMessageBox::information(this, tr("About Batlab Toolkit GUI"), msgText);
}

BatlabMainWindow::~BatlabMainWindow()
{

}

void BatlabMainWindow::closeEvent(QCloseEvent *event)
{

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Exit Batlab Toolkit GUI", "Are you sure you want to quit?",
                                  QMessageBox::Yes|QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // TODO update to set all batlabs idle using whatever new comm standard is developed
        for (int i = 0; i < BatlabObjects.size(); ++i) {
            BatlabObjects[i]->setAllIdle();
        }
        event->accept();
    } else {
        event->ignore();
    }
}

void BatlabMainWindow::updateLiveViewWithWriteCommand(int serialNumber, int nameSpace, int batlabRegister, int value)
{
    QString str;
    str += QString("WRITE: Batlab #%1 - ").arg(serialNumber);
    if (nameSpace >=0 && nameSpace < 4) {
        str += QString("Cell #%1 - ").arg(nameSpace);
    } else if (nameSpace == 4) {
        str += QString("Unit Namespace - ");
    } else {
        str += QString("If you see this, then we have problems.");
    }

    str += QString("Register #%1 - ").arg(batlabRegister);
    str += QString("Value = %1 \n").arg(value);

    emit emitUpdateText(str);
}

void BatlabMainWindow::updateLiveViewWithReadCommand(int serialNumber, int nameSpace, int batlabRegister)
{
    QString str;
    str += QString("READ: Batlab #%1 - ").arg(serialNumber);
    if (nameSpace >=0 && nameSpace < 4) {
        str += QString("Cell #%1 - ").arg(nameSpace);
    } else if (nameSpace == 4) {
        str += QString("Unit Namespace - ");
    } else {
        str += QString("If you see this, then we have problems.");
    }

    str += QString("Register #%1 - \n").arg(batlabRegister);
    qDebug() << str;

    emit emitUpdateText(str);
}

void BatlabMainWindow::updateLiveViewWithWriteResponse(int nameSpace, int batlabRegister, int lsb, int msb)
{
    QString str = QString("WRITE RESPONSE: ");
    if (nameSpace >=0 && nameSpace < 4) {
        str += QString("Cell #%1 - ").arg(nameSpace);
    } else if (nameSpace == 4) {
        str += QString("Unit Namespace - ");
    } else {
        str += QString("If you see this, then we have problems.");
    }

    str += QString("Register #%1 - ").arg(batlabRegister);
    str += QString("MSB: %1 - ").arg(msb,4,16);
    str += QString("LSB: %1 \n").arg(lsb,4,16);
qDebug() << str;


    emit emitUpdateText(str);
}

void BatlabMainWindow::updateLiveViewWithReadResponse(int nameSpace, int batlabRegister, int lsb, int msb)
{
    QString str = QString("READ RESPONSE: ");
    if (nameSpace >=0 && nameSpace < 4) {
        str += QString("Cell #%1 - ").arg(nameSpace);
    } else if (nameSpace == 4) {
        str += QString("Unit Namespace - ");
    } else {
        str += QString("If you see this, then we have problems.");
    }

    str += QString("Register #%1 - ").arg(batlabRegister);
    str += QString("MSB: %1 - ").arg(msb,4,16);
    str += QString("LSB: %1 \n").arg(lsb,4,16);
    qDebug() << str;


    emit emitUpdateText(str);
}

void BatlabMainWindow::updateLiveViewWithReceivedStream(int cell,int mode,int status,float temp, float current, float voltage)
{
    QString str = QString("STREAM PACKET: ");

    for (int i = 0; i < BatlabObjects.size(); ++i) {
        if (sender() == BatlabObjects[i]) {
            str += QString("Batlab #%1").arg(BatlabObjects[i]->getSerialNumber());
        }
    }

    str += QString("Cell #%1 - Mode: %2 - Status: %3 - Temp: %4 C - Current: %5 A - Voltage: %6 V \n")
            .arg(cell)
            .arg(mode)
            .arg(status)
            .arg(temp)
            .arg(current)
            .arg(voltage);
    qDebug() << str;

    emit emitUpdateText(str);
}

void BatlabMainWindow::showNewCellPlaylistWizard() {
    NewCellPlaylistWizard * wizard = new NewCellPlaylistWizard();
    wizard->setWizardStyle(QWizard::ModernStyle);
    wizard->show();
}

void BatlabMainWindow::loadPlaylistIntoGUI() {

}

void clearLayout(QLayout *layout) {
    QLayoutItem *item;
    while((item = layout->takeAt(0))) {
        if (item->layout()) {
            clearLayout(item->layout());
            delete item->layout();
        }
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void BatlabMainWindow::updateLiveViewTextBrowser(QString str)
{
    static int i = 0;
    str = QString("%1: %2 ").arg(++i).arg(QDateTime::currentDateTime().toString()) + str;
    if (liveViewTextBrowser->verticalScrollBar()->value() >= (liveViewTextBrowser->verticalScrollBar()->maximum()-10)) {
        liveViewTextBrowser->insertPlainText(str);
        liveViewTextBrowser->moveCursor(QTextCursor::End);
    } else {
        liveViewTextBrowser->insertPlainText(str);
    }
}

// TODO move into batpool
// Disconnecting a Batlab unit drives the Batlab Comm class to send its port name to this function
// so that it may be removed from the list of connected Batlab units.
void BatlabMainWindow::showBatlabRemoved(QString batlabUnitPortName) {

    bool foundIndexToDelete = false;
    int currentIndex = 0;

    while (!foundIndexToDelete && (currentIndex < BatlabObjects.size())) {
        if (BatlabObjects[currentIndex]->getName() != batlabUnitPortName) {
                currentIndex++;
            }

        else {
            foundIndexToDelete = true;
        }
    }

    if (foundIndexToDelete) {
        BatlabObjects.removeAt(currentIndex);
    }

    return;
}
