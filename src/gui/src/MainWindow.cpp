/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2008 Volker Lanz (vl@fidra.de)
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "AboutDialog.h"
#include "ServerConfigDialog.h"
#include "SettingsDialog.h"
#include "UdpDiscovery.h"
#include "FingerprintAcceptDialog.h"
#include "QUtility.h"
#include "SslCertificate.h"
#include "base/String.h"
#include "common/DataDirectories.h"
#include "net/FingerprintDatabase.h"
#include "net/SecureUtils.h"

#include <QtCore>
#include <QtGui>
#include <QtNetwork>
#include <QNetworkAccessManager>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QRegularExpression>

#if defined(Q_OS_MAC)
#include <ApplicationServices/ApplicationServices.h>
#endif

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <signal.h>

namespace {

static const QString allFilesFilter(QObject::tr("All files (*.*)"));
#if defined(Q_OS_WIN)
static const char APP_CONFIG_NAME[] = "inputserver.sgc";
static const QString APP_CONFIG_FILTER(QObject::tr("InputServer Configurations (*.sgc)"));
#else
static const char APP_CONFIG_NAME[] = "inputserver.conf";
static const QString APP_CONFIG_FILTER(QObject::tr("InputServer Configurations (*.conf)"));
#endif
static const QString APP_CONFIG_OPEN_FILTER(APP_CONFIG_FILTER + ";;" + allFilesFilter);
static const QString APP_CONFIG_SAVE_FILTER(APP_CONFIG_FILTER);

const char* icon_file_for_connection_state(AppConnectionState state)
{
#if defined(Q_OS_MAC)
    switch (state) {
        default:
        case AppConnectionState::DISCONNECTED: return ":/res/icons/128x128/inputserver-disconnected-mask.png";
        case AppConnectionState::CONNECTING:   return ":/res/icons/128x128/inputserver-disconnected-mask.png";
        case AppConnectionState::CONNECTED:    return ":/res/icons/128x128/inputserver-connected-mask.png";
        case AppConnectionState::TRANSFERRING: return ":/res/icons/128x128/inputserver-transfering-mask.png";
    }
#else
    switch (state) {
        default:
        case AppConnectionState::DISCONNECTED: return ":/res/icons/128x128/inputserver-disconnected.png";
        case AppConnectionState::CONNECTING:   return ":/res/icons/128x128/inputserver-disconnected.png";
        case AppConnectionState::CONNECTED:    return ":/res/icons/128x128/inputserver-connected.png";
        case AppConnectionState::TRANSFERRING: return ":/res/icons/128x128/inputserver-transfering.png";
    }
#endif
}

const char* icon_name_for_connection_state(AppConnectionState state)
{
    switch (state) {
        default:
        case AppConnectionState::DISCONNECTED: return "inputserver-disconnected";
        case AppConnectionState::CONNECTING: return "inputserver-disconnected";
        case AppConnectionState::CONNECTED: return "inputserver-connected";
        case AppConnectionState::TRANSFERRING: return "inputserver-transfering";
    }
}

static const char* APP_LARGE_ICON = ":/res/icons/256x256/inputserver.png";

} // namespace

MainWindow::MainWindow(QSettings& settings, AppConfig& appConfig) :
    ui_{std::make_unique<Ui::MainWindow>()},
    m_Settings(settings),
    m_AppConfig(&appConfig),
    cmd_app_process_(nullptr),
    m_ServerConfig(&m_Settings, 5, 3, m_AppConfig->screenName(), this),
    m_pTempConfigFile(nullptr),
    m_pTrayIcon(nullptr),
    m_pTrayIconMenu(nullptr),
    m_AlreadyHidden(false),
    m_pMenuBar(nullptr),
    main_menu_(nullptr),
    m_pMenuHelp(nullptr),
    m_SuppressEmptyServerWarning(false),
    m_ExpectedRunningState(kStopped),
    m_pSslCertificate(nullptr),
    m_pLogWindow(new LogWindow(nullptr))
{
    // explicitly unset DeleteOnClose so the window can be show and hidden
    // repeatedly until InputLeap is finished
    setAttribute(Qt::WA_DeleteOnClose, false);
    // mark the windows as sort of "dialog" window so that tiling window
    // managers will float it by default (X11)
    setAttribute(Qt::WA_X11NetWmWindowTypeDialog, true);

    ui_->setupUi(this);
    setWindowTitle("InputServer");
    ui_->m_pActionQuit->setText(tr("Stop and quit InputServer"));
    setWindowIcon(QIcon(APP_LARGE_ICON));
    createMenuBar();
    loadSettings();
    initConnections();
    restartTimer_.setSingleShot(true);
    restartTimer_.setInterval(1000);
    connect(&restartTimer_, &QTimer::timeout, this, [this] {
        if (m_ExpectedRunningState == kStarted) {
            start_cmd_app();
        }
    });

    ui_->m_pLabelScreenName->setText(getScreenName());
    ui_->m_pLabelIpAddresses->setText(getIPAddresses());

#if defined(Q_OS_WIN)
    // InputServer owns the engine directly and never talks to the legacy daemon.
    if (appConfig.processMode() == Service) {
        connect(&m_IpcClient, &IpcClient::readLogLine, this, &MainWindow::appendLogRaw);
        connect(&m_IpcClient, &IpcClient::errorMessage, this, &MainWindow::appendLogError);
        connect(&m_IpcClient, &IpcClient::infoMessage, this, &MainWindow::appendLogInfo);
        m_IpcClient.connectToHost();
    }
#endif

    // change default size based on os
#if defined(Q_OS_MAC)
    resize(720, 550);
    setMinimumSize(720, 0);
#elif defined(Q_OS_LINUX)
    resize(700, 530);
    setMinimumSize(700, 0);
#endif

    ui_->m_pCheckBoxAutoConfig->setChecked(appConfig.autoConfig());

    ui_->m_pComboServerList->hide();
    ui_->m_pLabelPadlock->hide();
    ui_->m_pLabelPadlock->setPixmap(QPixmap(":/res/icons/64x64/padlock.png").scaledToHeight(fontMetrics().height() * 1.5, Qt::SmoothTransformation));
    ui_->frame_fingerprint_details->hide();

    updateSSLFingerprint();

    connect(ui_->toolbutton_show_fingerprint, &QToolButton::clicked, this, [this](bool checked)
    {
        (void) checked;

        m_fingerprint_expanded = !m_fingerprint_expanded;
        if (m_fingerprint_expanded) {
            ui_->frame_fingerprint_details->show();
            ui_->toolbutton_show_fingerprint->setArrowType(Qt::ArrowType::UpArrow);
        } else {
            ui_->frame_fingerprint_details->hide();
            ui_->toolbutton_show_fingerprint->setArrowType(Qt::ArrowType::DownArrow);
        }
    });

    // resize window to smallest reasonable size
    resize(0, 0);
}

MainWindow::~MainWindow()
{
    stop_cmd_app();

    saveSettings();

    discovery_.reset();
    delete m_pSslCertificate;

    // LogWindow is created as a sibling of the MainWindow rather than a child
    // so that the main window can be hidden without hiding the log. because of
    // this it does not get properly cleaned up by the QObject system. also by
    // the time this destructor is called the event loop will no longer be able
    // to clean up the LogWindow so ->deleteLater() will not work
    delete m_pLogWindow;
}

void MainWindow::open()
{
    createTrayIcon();

    if (appConfig().getAutoHide()) {
        hide();
    } else {
        showNormal();
    }

    // only start if user has previously started. this stops the gui from
    // auto hiding before the user has configured InputLeap (which of course
    // confuses first time users, who think InputLeap has crashed).
    if (appConfig().startedBefore() && appConfig().getAutoStart()) {
        m_SuppressEmptyServerWarning = true;
        start_cmd_app();
        m_SuppressEmptyServerWarning = false;
    }
}

void MainWindow::setStatus(const QString &status)
{
    ui_->m_pStatusLabel->setText(status);
    if (m_pTrayIcon) {
        m_pTrayIcon->setToolTip("InputServer: " + status);
    }
}

void MainWindow::createTrayIcon()
{
    m_pTrayIconMenu = new QMenu(this);

    m_pTrayIconMenu->addAction(ui_->m_pActionStartCmdApp);
    m_pTrayIconMenu->addAction(ui_->m_pActionStopCmdApp);
    m_pTrayIconMenu->addAction(ui_->m_pActionShowLog);
    m_pTrayIconMenu->addSeparator();

    m_pTrayIconMenu->addAction(ui_->m_pActionMinimize);
    m_pTrayIconMenu->addAction(ui_->m_pActionRestore);
    m_pTrayIconMenu->addSeparator();
    m_pTrayIconMenu->addAction(ui_->m_pActionQuit);

    m_pTrayIcon = new QSystemTrayIcon(this);
    m_pTrayIcon->setContextMenu(m_pTrayIconMenu);
    m_pTrayIcon->setToolTip("InputServer");

    connect(m_pTrayIcon, &QSystemTrayIcon::activated, this, &MainWindow::trayActivated);

    set_icon(AppConnectionState::DISCONNECTED);

    m_pTrayIcon->show();
}

void MainWindow::retranslateMenuBar()
{
#ifndef Q_OS_DARWIN
    main_menu_->setTitle(tr("&InputServer"));
    m_pMenuHelp->setTitle(tr("&Help"));
#else
    m_pMenuHelp->setTitle(tr("&File"));
    main_menu_->setTitle(tr("&Window"));
#endif

}

void MainWindow::createMenuBar()
{
    m_pMenuBar = new QMenuBar(this);
    main_menu_ = new QMenu("", m_pMenuBar);
    m_pMenuHelp = new QMenu("", m_pMenuBar);
    retranslateMenuBar();

#ifndef Q_OS_DARWIN
    m_pMenuBar->addAction(main_menu_->menuAction());
    m_pMenuBar->addAction(m_pMenuHelp->menuAction());
#else
    m_pMenuBar->addAction(m_pMenuHelp->menuAction());
    m_pMenuBar->addAction(main_menu_->menuAction());
#endif

    main_menu_->addAction(ui_->m_pActionShowLog);
    main_menu_->addAction(ui_->m_pActionSettings);
    main_menu_->addAction(ui_->m_pActionMinimize);
    main_menu_->addSeparator();

#ifndef Q_OS_DARWIN
    main_menu_->addAction(ui_->m_pActionSave);
#endif
    main_menu_->addSeparator();
    main_menu_->addAction(ui_->m_pActionQuit);
    m_pMenuHelp->addAction(ui_->m_pActionAbout);

#ifdef Q_OS_DARWIN
    m_pMenuHelp->addAction(ui_->m_pActionSave);
#endif

    setMenuBar(m_pMenuBar);
}

void MainWindow::loadSettings()
{
    // the next two must come BEFORE loading groupServerChecked and groupClientChecked or
    // disabling and/or enabling the right widgets won't automatically work
    ui_->m_pRadioExternalConfig->setChecked(settings().value("useExternalConfig", false).toBool());
    ui_->m_pRadioInternalConfig->setChecked(settings().value("useInternalConfig", true).toBool());

    ui_->m_pGroupServer->setChecked(settings().value("groupServerChecked", false).toBool());
    ui_->m_pLineEditConfigFile->setText(settings().value("configFile",
                                                    QDir::homePath() + "/" + APP_CONFIG_NAME).toString());
    ui_->m_pGroupClient->setChecked(settings().value("groupClientChecked", true).toBool());
    ui_->m_pLineEditHostname->setText(settings().value("serverHostname").toString());
}

void MainWindow::initConnections()
{
    connect(ui_->m_pActionMinimize, &QAction::triggered, this, &MainWindow::hide);
    connect(ui_->m_pComboServerList, &QComboBox::currentTextChanged, this, &MainWindow::comboServerList_currentIndexChanged);
    connect(ui_->m_pActionRestore, &QAction::triggered, this, &MainWindow::showNormal);
    connect(ui_->m_pActionStartCmdApp, &QAction::triggered, this, &MainWindow::start_cmd_app);
    connect(ui_->m_pActionStopCmdApp, &QAction::triggered, this, &MainWindow::stop_cmd_app);
    connect(ui_->m_pActionShowLog, &QAction::triggered, this, &MainWindow::showLogWindow);
    connect(ui_->m_pActionQuit, &QAction::triggered, this, [this] {
        stop_cmd_app();
        qApp->quit();
    });
}

void MainWindow::saveSettings()
{
    // program settings
    settings().setValue("groupServerChecked", ui_->m_pGroupServer->isChecked());
    settings().setValue("useExternalConfig", ui_->m_pRadioExternalConfig->isChecked());
    settings().setValue("configFile", ui_->m_pLineEditConfigFile->text());
    settings().setValue("useInternalConfig", ui_->m_pRadioInternalConfig->isChecked());
    settings().setValue("groupClientChecked", ui_->m_pGroupClient->isChecked());
    settings().setValue("serverHostname", ui_->m_pLineEditHostname->text());

    settings().sync();
}

void MainWindow::set_icon(AppConnectionState state)
{
    if (m_pTrayIcon) {
        QIcon icon = QIcon::fromTheme(icon_name_for_connection_state(state),
                                      QIcon(icon_file_for_connection_state(state)));
#if defined(Q_OS_MAC)
        icon.setIsMask(true);
#endif
        m_pTrayIcon->setIcon(icon);
    }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        if (isVisible())
        {
            hide();
        }
        else
        {
            showNormal();
            activateWindow();
        }
    }
}

void MainWindow::logOutput()
{
    if (cmd_app_process_)
    {
        QString text(cmd_app_process_->readAllStandardOutput());
        const auto results = text.split(QRegularExpression("\r|\n|\r\n"));
        for (const auto& line : results) {
            if (!line.isEmpty())
            {
                appendLogRaw(line);
            }
        }
    }
}

void MainWindow::logError()
{
    if (cmd_app_process_)
    {
        appendLogRaw(cmd_app_process_->readAllStandardError());
    }
}

void MainWindow::appendLogInfo(const QString& text)
{
    if (appConfig().logLevel() >= 3) {
        m_pLogWindow->appendInfo(text);
    }
}

void MainWindow::appendLogDebug(const QString& text) {
    if (appConfig().logLevel() >= 4) {
        m_pLogWindow->appendDebug(text);
    }
}

void MainWindow::appendLogError(const QString& text)
{
    m_pLogWindow->appendError(text);
}

void MainWindow::appendLogRaw(const QString& text)
{
    for (const auto& line : text.split(QRegularExpression("\r|\n|\r\n"))) {
        if (!line.isEmpty()) {
            m_pLogWindow->appendRaw(line);
            updateFromLogLine(line);
        }
    }
}

void MainWindow::updateFromLogLine(const QString &line)
{
    // TODO: this code makes Andrew cry
    checkConnected(line);
    checkFingerprint(line);
}

void MainWindow::checkConnected(const QString& line)
{
    // TODO: implement ipc connection state messages to replace this hack.
    if (line.contains("started server") ||
        line.contains("connected to server") ||
        line.contains("server status: active"))
    {
        set_connection_state(AppConnectionState::CONNECTED);

        if (!appConfig().startedBefore() && isVisible()) {
                QMessageBox::information(
                    this, "InputServer",
                    tr("InputServer is now connected. You can close the "
                    "config window and InputServer will remain connected in "
                    "the background."));

            appConfig().setStartedBefore(true);
            appConfig().saveSettings();
        }
    }
}

void MainWindow::checkFingerprint(const QString& line)
{
    QRegularExpression fingerprintRegex("peer fingerprint \\(SHA1\\): ([A-F0-9:]+) \\(SHA256\\): ([A-F0-9:]+)$");
    QRegularExpressionMatch match = fingerprintRegex.match(line);
    if (!match.hasMatch()) {
        return;
    }

    auto match1 = match.captured(1).toStdString();
    auto match2 = match.captured(2).toStdString();

    inputleap::FingerprintData fingerprint_sha1 = {
        inputleap::fingerprint_type_to_string(inputleap::FingerprintType::SHA1),
        inputleap::string::from_hex(match1)
    };

    inputleap::FingerprintData fingerprint_sha256 = {
        inputleap::fingerprint_type_to_string(inputleap::FingerprintType::SHA256),
        inputleap::string::from_hex(match2)
    };

    bool is_client = app_role() == AppRole::Client;

    auto db_path = is_client
            ? inputleap::DataDirectories::trusted_servers_ssl_fingerprints_path()
            : inputleap::DataDirectories::trusted_clients_ssl_fingerprints_path();

    auto db_dir = db_path.parent_path();
    if (!inputleap::fs::exists(db_dir)) {
        inputleap::fs::create_directories(db_dir);
    }

    // We compare only SHA256 fingerprints, but show both SHA1 and SHA256 so that the users can
    // still verify fingerprints on old InputLeap servers. This way the only time when we are
    // exposed to SHA1 vulnerabilities is when the user is reconnecting again.
    inputleap::FingerprintDatabase db;
    db.read(db_path);
    if (db.is_trusted(fingerprint_sha256)) {
        return;
    }

    static bool messageBoxAlreadyShown = false;

    if (!messageBoxAlreadyShown) {
        if (is_client) {
            stop_cmd_app();
        }

        messageBoxAlreadyShown = true;
        FingerprintAcceptDialog dialog{this, app_role(), fingerprint_sha1, fingerprint_sha256};
        if (dialog.exec() == QDialog::Accepted) {
            // restart core process after trusting fingerprint.
            db.add_trusted(fingerprint_sha256);
            db.write(db_path);
            if (is_client) {
                start_cmd_app();
            }
        }

        messageBoxAlreadyShown = false;
    }
}

void MainWindow::restart_cmd_app()
{
    stop_cmd_app();
    start_cmd_app();
}

void MainWindow::proofreadInfo()
{
    AppConnectionState old = connection_state_;
    connection_state_ = AppConnectionState::DISCONNECTED;
    set_connection_state(old);
}

void MainWindow::start_cmd_app()
{
    waiting_for_server_ = false;
    // Repeated Start signals must never replace the owned process pointer.
    if (cmd_app_process_ && cmd_app_process_->state() != QProcess::NotRunning) {
        return;
    }
    if (appConfig().processMode() == Service &&
        m_ExpectedRunningState == kStarted &&
        connection_state() != AppConnectionState::DISCONNECTED) {
        return;
    }
    restartTimer_.stop();
    if (cmd_app_process_) {
        cmd_app_process_->deleteLater();
        cmd_app_process_ = nullptr;
    }
    bool desktopMode = appConfig().processMode() == Desktop;
    bool serviceMode = appConfig().processMode() == Service;

    appendLogDebug("starting process");
    m_ExpectedRunningState = kStarted;
    set_connection_state(AppConnectionState::CONNECTING);

    QString app;
    QStringList args;

    args << "-f" << "--no-tray" << "--debug" << appConfig().logLevelText();


    args << "--name" << getScreenName();

    if (desktopMode)
    {
        cmd_app_process_ = new QProcess(this);
    }
    else
    {
        // tell client/server to talk to daemon through ipc.
        args << "--ipc";

#if defined(Q_OS_WIN)
        // tell the client/server to shut down when a ms windows desk
        // is switched; this is because we may need to elevate or not
        // based on which desk the user is in (login always needs
        // elevation, where as default desk does not).
        // Note that this is only enabled when InputLeap is set to elevate
        // 'as needed' (e.g. on a UAC dialog popup) in order to prevent
        // unnecessary restarts when InputLeap was started elevated or
        // when it is not allowed to elevate. In these cases restarting
        // the server is fruitless.
        if (appConfig().elevateMode() == ElevateAsNeeded) {
                args << "--stop-on-desk-switch";
        }
#endif
    }

#ifndef Q_OS_LINUX

    if (m_ServerConfig.enableDragAndDrop()) {
        args << "--enable-drag-drop";
    }

#endif

    if (!m_AppConfig->getCryptoEnabled()) {
        args << "--disable-crypto";
    }

#if defined(Q_OS_WIN)
    // on windows, the profile directory changes depending on the user that
    // launched the process (e.g. when launched with elevation). setting the
    // profile dir on launch ensures it uses the same profile dir is used
    // no matter how its relaunched.
    const QString profile = QString::fromStdString(inputleap::DataDirectories::profile().u8string());
    args << "--profile-dir" << (serviceMode ? "\"" + profile + "\"" : profile);
#endif

    if ((app_role() == AppRole::Client && !clientArgs(args, app))
        || (app_role() == AppRole::Server && !serverArgs(args, app)))
    {
        stop_cmd_app();
        return;
    }

    if (desktopMode)
    {
        connect(cmd_app_process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::cmd_app_finished);
        connect(cmd_app_process_, &QProcess::readyReadStandardOutput, this, &MainWindow::logOutput);
        connect(cmd_app_process_, &QProcess::readyReadStandardError, this, &MainWindow::logError);
    }

    m_pLogWindow->startNewInstance();

    appendLogInfo("starting " + QString(app_role() == AppRole::Server ? "server" : "client"));

    qDebug() << args;

    appendLogDebug(QString("command: %1 %2").arg(app, args.join(" ")));

    appendLogInfo("config file: " + configFilename());
    appendLogInfo("log level: " + appConfig().logLevelText());

    if (appConfig().logToFile())
        appendLogInfo("log file: " + appConfig().logFilename());

    if (desktopMode)
    {
        cmd_app_process_->start(app, args);
        if (!cmd_app_process_->waitForStarted())
        {
            show();
            QMessageBox::warning(this, tr("Program can not be started"), QString(tr("The executable<br><br>%1<br><br>could not be successfully started, although it does exist. Please check if you have sufficient permissions to run this program.").arg(app)));
            stop_cmd_app();
            return;
        }
        if (!processGuard_.attach(cmd_app_process_->processId())) {
            appendLogError("cannot establish process ownership; stopping engine");
            stop_cmd_app();
            return;
        }
    }

    if (serviceMode)
    {
        QString command(app + " " + args.join(" "));
        m_IpcClient.sendCommand(command, appConfig().elevateMode());
    }
}

void MainWindow::setServerMode(bool isServerMode)
{
    ui_->m_pGroupServer->setChecked(isServerMode);
    ui_->m_pGroupClient->setChecked(!isServerMode);
}

bool MainWindow::clientArgs(QStringList& args, QString& app)
{
    app = appPath(appConfig().client_name());

    if (!QFile::exists(app))
    {
        show();
        QMessageBox::warning(this, tr("InputServer client not found"),
                             tr("The executable for the InputServer client does not exist."));
        return false;
    }

#if defined(Q_OS_WIN)
    // wrap in quotes so a malicious user can't start \Program.exe as admin.
    if (appConfig().processMode() == Service) {
        app = QString("\"%1\"").arg(app);
    }
#endif

    if (appConfig().logToFile())
    {
        appConfig().persistLogDir();
        args << "--log" << (appConfig().processMode() == Service ?
                            appConfig().logFilenameCmd() : appConfig().logFilename());
    }

    // check auto config first, if it is disabled or no server detected,
    // use line edit host name if it is not empty
    if (ui_->m_pCheckBoxAutoConfig->isChecked()) {
        if (ui_->m_pComboServerList->count() != 0) {
            // item data is {address, port} as announced by the server
            const QStringList server = ui_->m_pComboServerList->currentData().toStringList();
            if (server.size() == 2) {
                args << "[" + server[0] + "]:" + server[1];
                return true;
            }
        }
        if (ui_->m_pLineEditHostname->text().isEmpty()) {
            // Not an error: the client starts as soon as a server answers.
            appendLogInfo(tr("waiting for a server on the network"));
            waiting_for_server_ = true;
            return false;
        }
    } else if (ui_->m_pLineEditHostname->text().isEmpty()) {
        show();
        if (!m_SuppressEmptyServerWarning) {
            QMessageBox::warning(this, tr("Hostname is empty"),
                             tr("Please fill in a hostname for the InputServer client to connect to."));
        }
        return false;
    }

    args << "[" + ui_->m_pLineEditHostname->text() + "]:" + QString::number(appConfig().port());

    return true;
}

QString MainWindow::configFilename()
{
    QString filename;
    if (ui_->m_pRadioInternalConfig->isChecked())
    {
        if (m_pTempConfigFile) {
            return m_pTempConfigFile->fileName();
        }
        // TODO: no need to use a temporary file, since we need it to
        // be permanent (since it'll be used for Windows services, etc).
        m_pTempConfigFile = new QTemporaryFile();
        if (!m_pTempConfigFile->open())
        {
            QMessageBox::critical(this, tr("Cannot write configuration file"),
                                  tr("The temporary configuration file required to start InputServer can not be written."));
            return "";
        }

        serverConfig().save(*m_pTempConfigFile);
        filename = m_pTempConfigFile->fileName();

        m_pTempConfigFile->close();
    }
    else
    {
        if (!QFile::exists(ui_->m_pLineEditConfigFile->text()))
        {
            if (QMessageBox::warning(this, tr("Configuration filename invalid"),
                tr("You have not filled in a valid configuration file for the InputServer server. "
                        "Do you want to browse for the configuration file now?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes
                    || !on_m_pButtonBrowseConfigFile_clicked())
                return "";
        }

        filename = ui_->m_pLineEditConfigFile->text();
    }
    return filename;
}

AppRole MainWindow::app_role() const
{
    return ui_->m_pGroupClient->isChecked() ? AppRole::Client : AppRole::Server;
}

QString MainWindow::hostname() const
{
    return ui_->m_pLineEditHostname->text();
}

QString MainWindow::address()
{
    QString address = appConfig().networkInterface();
    if (!address.isEmpty())
        address = "[" + address + "]";
    return address + ":" + QString::number(appConfig().port());
}

QString MainWindow::appPath(const QString& name)
{
    return appConfig().program_dir() + name;
}

bool MainWindow::serverArgs(QStringList& args, QString& app)
{
    app = appPath(appConfig().server_name());

    if (!QFile::exists(app))
    {
        QMessageBox::warning(this, tr("InputServer server not found"),
                             tr("The executable for the InputServer server does not exist."));
        return false;
    }

#if defined(Q_OS_WIN)
    // wrap in quotes so a malicious user can't start \Program.exe as admin.
    if (appConfig().processMode() == Service) {
        app = QString("\"%1\"").arg(app);
    }
#endif

    if (appConfig().logToFile())
    {
        appConfig().persistLogDir();

        args << "--log" << (appConfig().processMode() == Service ?
                            appConfig().logFilenameCmd() : appConfig().logFilename());
    }

    if (!appConfig().getRequireClientCertificate()) {
        args << "--disable-client-cert-checking";
    }

    QString configFilename = this->configFilename();
#if defined(Q_OS_WIN)
    // wrap in quotes in case username contains spaces.
    if (appConfig().processMode() == Service) {
        configFilename = QString("\"%1\"").arg(configFilename);
    }
#endif
    args << "-c" << configFilename << "--address" << address();

    return true;
}

void MainWindow::stop_cmd_app()
{
    appendLogDebug("stopping process");

    m_ExpectedRunningState = kStopped;
    restartTimer_.stop();

    if (appConfig().processMode() == Service)
    {
        stopService();
    }
    else if (appConfig().processMode() == Desktop)
    {
        if (!stopDesktop()) {
            setStatus(tr("Unable to stop the process. See the log."));
            return;
        }
    }

    set_connection_state(AppConnectionState::DISCONNECTED);

    // HACK: deleting the object deletes the physical file, which is
    // bad, since it could be in use by the Windows service!
    if (appConfig().processMode() == Desktop) {
        delete m_pTempConfigFile;
    }
    m_pTempConfigFile = nullptr;

    // reset so that new connects cause auto-hide.
    m_AlreadyHidden = false;
}

void MainWindow::stopService()
{
    // send empty command to stop service from launching anything.
    m_IpcClient.sendCommand("", appConfig().elevateMode());
}

bool MainWindow::stopDesktop()
{
    QMutexLocker locker(&m_StopDesktopMutex);
    if (!cmd_app_process_) {
        return true;
    }

    appendLogInfo("stopping InputServer desktop process");

    // Prevent finished() from scheduling a restart while Stop waits for exit.
    disconnect(cmd_app_process_, nullptr, this, nullptr);
    if (cmd_app_process_->state() != QProcess::NotRunning) {
        cmd_app_process_->terminate();
        if (!cmd_app_process_->waitForFinished(3000)) {
            appendLogInfo("process did not stop gracefully; terminating owned process");
            cmd_app_process_->kill();
            if (!cmd_app_process_->waitForFinished(2000)) {
                appendLogError("process is still running; Stop was not completed");
                return false;
            }
        }
    }

    delete cmd_app_process_;
    cmd_app_process_ = nullptr;
    processGuard_.reset();
    return true;
}

void MainWindow::cmd_app_finished(int exitCode, QProcess::ExitStatus)
{
    if (exitCode == 0) {
        appendLogInfo(QString("process exited normally"));
    }
    else {
        appendLogError(QString("process exited with error code: %1").arg(exitCode));
    }

    if (m_ExpectedRunningState == kStarted) {
        restartTimer_.start();
        appendLogInfo(QString("detected process not running, auto restarting"));
    }
    else {
        set_connection_state(AppConnectionState::DISCONNECTED);
    }
}

void MainWindow::set_connection_state(AppConnectionState state)
{
    if (connection_state() == state)
        return;

    if (state == AppConnectionState::CONNECTED || state == AppConnectionState::CONNECTING)
    {
        disconnect(ui_->m_pButtonToggleStart, &QPushButton::clicked, ui_->m_pActionStartCmdApp, &QAction::trigger);
        connect(ui_->m_pButtonToggleStart, &QPushButton::clicked, ui_->m_pActionStopCmdApp, &QAction::trigger);
        ui_->m_pButtonToggleStart->setText(tr("&Stop"));
        ui_->m_pButtonReload->setEnabled(true);
    }
    else if (state == AppConnectionState::DISCONNECTED)
    {
        disconnect(ui_->m_pButtonToggleStart, &QPushButton::clicked, ui_->m_pActionStopCmdApp, &QAction::trigger);
        connect(ui_->m_pButtonToggleStart, &QPushButton::clicked, ui_->m_pActionStartCmdApp, &QAction::trigger);
        ui_->m_pButtonToggleStart->setText(tr("&Start"));
        ui_->m_pButtonReload->setEnabled(false);
    }

    const bool active = state != AppConnectionState::DISCONNECTED;
    ui_->m_pActionStartCmdApp->setEnabled(!active);
    ui_->m_pActionStopCmdApp->setEnabled(active);

    switch (state)
    {
    case AppConnectionState::CONNECTED: {
        if (m_AppConfig->getCryptoEnabled()) {
            ui_->m_pLabelPadlock->show();
        }
        else {
            ui_->m_pLabelPadlock->hide();
        }

        setStatus(tr("InputServer is running."));

        break;
    }
    case AppConnectionState::CONNECTING:
        ui_->m_pLabelPadlock->hide();
        setStatus(tr("InputServer is starting."));
        break;
    case AppConnectionState::DISCONNECTED:
        ui_->m_pLabelPadlock->hide();
        setStatus(tr("InputServer is not running."));
        break;
    case AppConnectionState::TRANSFERRING:
        break;
    default:
        break;
    }

    set_icon(state);

    connection_state_ = state;
}

void MainWindow::setVisible(bool visible)
{
    QMainWindow::setVisible(visible);
    ui_->m_pActionMinimize->setEnabled(visible);
    ui_->m_pActionRestore->setEnabled(!visible);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070 // lion
    // dock hide only supported on lion :(
    ProcessSerialNumber psn = { 0, kCurrentProcess };
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GetCurrentProcess(&psn);
#pragma GCC diagnostic pop
    if (visible)
        TransformProcessType(&psn, kProcessTransformToForegroundApplication);
    else
        TransformProcessType(&psn, kProcessTransformToBackgroundApplication);
#endif
}

QString MainWindow::getIPAddresses()
{
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();

    bool hinted = false;
    QString result;
    for (int i = 0; i < addresses.size(); i++) {
        if (addresses[i].protocol() == QAbstractSocket::IPv4Protocol &&
            addresses[i] != QHostAddress(QHostAddress::LocalHost)) {

            QString address = addresses[i].toString();
            QString format = "%1, ";

            // usually 192.168.x.x is a useful ip for the user, so indicate
            // this by making it bold.
            if (!hinted && address.startsWith("192.168")) {
                hinted = true;
                format = "<b>%1</b>, ";
            }

            result += format.arg(address);
        }
    }

    if (result == "") {
        return tr("Unknown");
    }

    // remove trailing comma.
    result.chop(2);

    return result;
}

QString MainWindow::getScreenName()
{
    if (appConfig().screenName() == "") {
        return QHostInfo::localHostName();
    }
    else {
        return appConfig().screenName();
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event != nullptr)
    {
        switch (event->type())
        {
        case QEvent::LanguageChange:
        {
            ui_->retranslateUi(this);
            setWindowTitle("InputServer");
            ui_->m_pActionQuit->setText(tr("Stop and quit InputServer"));
            retranslateMenuBar();

            proofreadInfo();

            break;
        }
        case QEvent::WindowStateChange:
        {
            windowStateChanged();
            break;
        }
        default:
        {
            break;
        }
        }
    }
    // all that do not return are allowing the event to propagate
    QMainWindow::changeEvent(event);
}

bool MainWindow::event(QEvent* event)
{
    if (event->type() == QEvent::LayoutRequest) {
        setFixedSize(sizeHint());
    }
    return QMainWindow::event(event);
}

void MainWindow::updateDiscovery()
{
    discovery_.reset();
    waiting_for_server_ = false;
    discovered_clients_.clear();
    ui_->m_pComboServerList->clear();
    ui_->m_pComboServerList->hide();

    if (m_AppConfig->wizardShouldRun()) {
        return;
    }

    const bool server = app_role() == AppRole::Server;
    if (!server && !m_AppConfig->autoConfig()) {
        return;
    }

    // The discovery port is the one right after the connection port.
    const auto tcp_port = static_cast<quint16>(appConfig().port());
    discovery_ = std::make_unique<UdpDiscovery>(
        server ? UdpDiscovery::Role::Server : UdpDiscovery::Role::Client,
        getScreenName(), tcp_port, static_cast<quint16>(tcp_port + 1));
    connect(discovery_.get(), &UdpDiscovery::serverFound, this, &MainWindow::serverDetected);
    connect(discovery_.get(), &UdpDiscovery::serverLost, this, &MainWindow::serverLost);
    connect(discovery_.get(), &UdpDiscovery::clientFound, this, &MainWindow::clientDetected);

    if (!discovery_->start()) {
        appendLogError(tr("automatic discovery unavailable: %1").arg(discovery_->errorString()));
        discovery_.reset();
    }
}

void MainWindow::serverDetected(const QString& name, const QHostAddress& address, quint16 port)
{
    const QString host = address.toString();
    const QString label = QStringLiteral("%1 (%2)").arg(name, host);
    if (ui_->m_pComboServerList->findText(label) != -1) {
        return;
    }

    appendLogInfo(tr("server found on the network: %1").arg(label));
    // Note: the first added item triggers a (re)start of the client
    ui_->m_pComboServerList->addItem(label, QStringList{host, QString::number(port)});

    if (ui_->m_pComboServerList->count() > 1) {
        ui_->m_pComboServerList->show();
    }
}

void MainWindow::serverLost(const QString& name, const QHostAddress& address)
{
    const QString label = QStringLiteral("%1 (%2)").arg(name, address.toString());
    const int index = ui_->m_pComboServerList->findText(label);
    if (index == -1) {
        return;
    }

    appendLogInfo(tr("server no longer answering: %1").arg(label));
    // Keep the server being used: a missed reply must not drop the connection.
    if (index != ui_->m_pComboServerList->currentIndex()) {
        ui_->m_pComboServerList->removeItem(index);
    }
    if (ui_->m_pComboServerList->count() <= 1) {
        ui_->m_pComboServerList->hide();
    }
}

void MainWindow::clientDetected(const QString& name)
{
    // Clients ask every few seconds; offer each one only once per session.
    if (discovered_clients_.contains(name)) {
        return;
    }
    discovered_clients_.insert(name);
    appendLogInfo(tr("client found on the network: %1").arg(name));
    autoAddScreen(name);
}

void MainWindow::updateSSLFingerprint()
{
    if (m_AppConfig->getCryptoEnabled() && m_pSslCertificate == nullptr) {
        m_pSslCertificate = new SslCertificate(this);
        connect(m_pSslCertificate, &SslCertificate::info, this, &MainWindow::appendLogInfo);
        m_pSslCertificate->generateCertificate();
    }

    ui_->toolbutton_show_fingerprint->setEnabled(false);
    ui_->m_pLabelLocalFingerprint->setText("Disabled");

    if (!m_AppConfig->getCryptoEnabled()) {
        return;
    }

    auto local_path = inputleap::DataDirectories::local_ssl_fingerprints_path();
    if (!inputleap::fs::exists(local_path)) {
        return;
    }

    inputleap::FingerprintDatabase db;
    db.read(local_path);
    if (db.fingerprints().size() != 2) {
        return;
    }

    for (const auto& fingerprint : db.fingerprints()) {
        if (fingerprint.algorithm == "sha1") {
            auto fingerprint_str = inputleap::format_ssl_fingerprint(fingerprint.data);
            ui_->label_sha1_fingerprint_full->setText(QString::fromStdString(fingerprint_str));
            continue;
        }

        if (fingerprint.algorithm == "sha256") {
            auto fingerprint_str = inputleap::format_ssl_fingerprint(fingerprint.data);
            fingerprint_str.resize(40);
            fingerprint_str += " ...";

            auto fingerprint_str_cols = inputleap::format_ssl_fingerprint_columns(fingerprint.data);
            auto fingerprint_randomart = inputleap::create_fingerprint_randomart(fingerprint.data);

            ui_->m_pLabelLocalFingerprint->setText(QString::fromStdString(fingerprint_str));
            ui_->label_sha256_fingerprint_full->setText(QString::fromStdString(fingerprint_str_cols));
            ui_->label_sha256_randomart->setText(QString::fromStdString(fingerprint_randomart));
        }
    }

    ui_->toolbutton_show_fingerprint->setEnabled(true);
}

void MainWindow::on_m_pGroupClient_toggled(bool on)
{
    ui_->m_pGroupServer->setChecked(!on);
    if (on) {
        updateDiscovery();
    }
}

void MainWindow::on_m_pGroupServer_toggled(bool on)
{
    ui_->m_pGroupClient->setChecked(!on);
    if (on) {
        updateDiscovery();
    }
}

bool MainWindow::on_m_pButtonBrowseConfigFile_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Browse for a InputServer config file"), QString(), APP_CONFIG_OPEN_FILTER);

    if (!fileName.isEmpty())
    {
        ui_->m_pLineEditConfigFile->setText(fileName);
        return true;
    }

    return false;
}

bool MainWindow::on_m_pActionSave_triggered()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save configuration as..."), QString(), APP_CONFIG_SAVE_FILTER);

    if (!fileName.isEmpty() && !serverConfig().save(fileName))
    {
        QMessageBox::warning(this, tr("Save failed"), tr("Could not save configuration to file."));
        return true;
    }

    return false;
}

void MainWindow::on_m_pActionAbout_triggered()
{
    AboutDialog(this, appPath(appConfig().client_name())).exec();
}

void MainWindow::on_m_pActionSettings_triggered()
{
    auto dialog = std::make_unique<SettingsDialog>(this, appConfig());
    connect(dialog.get(), &SettingsDialog::requestLanguageChange, this, &MainWindow::requestLanguageChange);
    if (dialog.get()->exec() == QDialog::Accepted)
        updateSSLFingerprint();
    disconnect(dialog.get(), &SettingsDialog::requestLanguageChange, this, &MainWindow::requestLanguageChange);
}

void MainWindow::autoAddScreen(const QString name)
{
    if (!m_ServerConfig.ignoreAutoConfigClient()) {
        int r = m_ServerConfig.autoAddScreen(name);
        if (r != kAutoAddScreenOk) {
            switch (r) {
            case kAutoAddScreenManualServer:
                showConfigureServer(
                    tr("Please add the server (%1) to the grid.")
                        .arg(appConfig().screenName()));
                break;

            case kAutoAddScreenManualClient:
                showConfigureServer(
                    tr("Please drag the new client screen (%1) "
                        "to the desired position on the grid.")
                        .arg(name));
                break;
            default:
                break;
            }
        }
        else {
            restart_cmd_app();
        }
    }
}

void MainWindow::showConfigureServer(const QString& message)
{
    ServerConfigDialog dlg(this, serverConfig(), appConfig().screenName());
    dlg.message(message);
    dlg.exec();
}

void MainWindow::on_m_pButtonConfigureServer_clicked()
{
    showConfigureServer();
}

void MainWindow::on_m_pButtonReload_clicked()
{
    restart_cmd_app();
}

void MainWindow::comboServerList_currentIndexChanged(QString )
{
    // Only (re)start when the user wants to be connected: either the client
    // is running or Start was pressed while no server had answered yet.
    if (ui_->m_pComboServerList->count() == 0) {
        return;
    }
    if (waiting_for_server_ || m_ExpectedRunningState == kStarted) {
        waiting_for_server_ = false;
        restart_cmd_app();
    }
}

void MainWindow::on_m_pCheckBoxAutoConfig_toggled(bool checked)
{
    ui_->m_pLineEditHostname->setDisabled(checked);
    appConfig().setAutoConfig(checked);
    updateDiscovery();
}

void MainWindow::windowStateChanged()
{
    if (windowState() == Qt::WindowMinimized && appConfig().getMinimizeToTray())
        hide();
}

void MainWindow::showLogWindow()
{
    m_pLogWindow->show();
}
