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

#define TRAY_RETRY_COUNT 5
#define TRAY_RETRY_WAIT 2000

#include "QInputLeapApplication.h"
#include "MainWindow.h"
#include "AppConfig.h"
#include "SingleInstance.h"
#include "Theme.h"

#include <QtCore>
#include <QtGui>
#include <QSettings>
#include <QMessageBox>

#if defined(Q_OS_MAC)
#include <Carbon/Carbon.h>
#endif

#ifdef Q_OS_DARWIN
#include <cstdlib>
#endif

class QThreadImpl : public QThread
{
public:
	static void msleep(unsigned long msecs)
	{
		QThread::msleep(msecs);
	}
};

int waitForTray();

#if defined(Q_OS_MAC)
bool checkMacAssistiveDevices();
#endif

int main(int argc, char* argv[])
{
#if (defined(WINAPI_XWINDOWS) || \
    (defined(HAVE_LIBPORTAL_INPUTCAPTURE) || \
    defined(HAVE_LIBPORTAL_SESSION_CONNECT_TO_EIS)))
    const auto platformType = QGuiApplication::platformName();

    if (platformType == "xcb") {
        // We're running on X11, all good.
        // Continue running.
    } else if (platformType == "wayland") {
        QMessageBox::information(nullptr, "InputServer",
                                 "You are using Wayland. InputServer supports Wayland via `libei` "
                                 "but not all desktop environment/window managers support our "
                                 "implementation at this time. Therefore, your mileage may vary.");
    }
#endif

#ifdef Q_OS_DARWIN
    /* Workaround for QTBUG-40332 - "High ping when QNetworkAccessManager is instantiated" */
    ::setenv ("QT_BEARER_POLL_TIMEOUT", "-1", 1);
#endif
    QCoreApplication::setOrganizationName("InputServer");
	QCoreApplication::setOrganizationDomain("github.com");
    QCoreApplication::setApplicationName("InputServer");
    QCoreApplication::setApplicationVersion("0.1.0");

    QInputLeapApplication app(argc, argv);
    app.setApplicationDisplayName("InputServer");
    theme::install(app);
    theme::install(app);
    QByteArray instanceCommand = "show";
    for (const auto& argument : app.arguments()) {
        if (argument == "--stop" || argument == "--start" || argument == "--quit") {
            instanceCommand = argument.mid(2).toLatin1();
        }
    }

    // Acquire before permissions prompts, setup or starting any child process.
    SingleInstance instance(QStandardPaths::writableLocation(
                                QStandardPaths::AppLocalDataLocation) + "/gui");
    if (!instance.acquire(instanceCommand)) {
        return 0;
    }
    if (instanceCommand != "show") {
        fprintf(stderr, "InputServer is not running.\n");
        return 3;
    }

    app.setDesktopFileName(QStringLiteral("inputserver"));

#if defined(Q_OS_MAC)
	if (app.applicationDirPath().startsWith("/Volumes/")) {
        // macOS preferences track applications allowed assistive access by path
        // Unfortunately, there's no user-friendly way to allow assistive access
        // to applications that are not in default paths (/Applications),
        // especially if an identically named application already exists in
        // /Applications). Thus we require InputLeap to reside in the /Applications
        // folder
        QMessageBox::information(nullptr, "InputServer",
                                 "Please drag InputServer to the Applications folder, "
                                 "and open it from there.");
		return 1;
	}

	if (!checkMacAssistiveDevices())
	{
		return 1;
	}
#endif

	int trayAvailable = waitForTray();

	QApplication::setQuitOnLastWindowClosed(false);

	QSettings settings;

	AppConfig appConfig (&settings);

	if (appConfig.getAutoHide() && !trayAvailable)
	{
		// force auto hide to false - otherwise there is no way to get the GUI back
		fprintf(stdout, "System tray not available, force disabling auto hide!\n");
		appConfig.setAutoHide(false);
	}

	app.switchTranslator(appConfig.language());

	MainWindow mainWindow(settings, appConfig);
    instance.onCommand(&mainWindow, [&](QByteArray command) {
        if (command == "stop" || command == "quit") {
            if (command == "quit") mainWindow.quit_cmd_app();
            else QMetaObject::invokeMethod(&mainWindow, "stop_cmd_app", Qt::DirectConnection);
            return;
        }
        if (command == "start") {
            mainWindow.start_cmd_app();
            return;
        }
        mainWindow.showNormal();
        mainWindow.raise();
        mainWindow.activateWindow();
    });

    // No first-run wizard: the role switch and settings live in the main window.
    mainWindow.open();
    QObject::connect(&mainWindow, &MainWindow::requestLanguageChange, &app, &QInputLeapApplication::switchTranslator);
	return app.exec();
}

int waitForTray()
{
	// on linux, the system tray may not be available immediately after logging in,
	// so keep retrying but give up after a short time.
	int trayAttempts = 0;
	while (true)
	{
		if (QSystemTrayIcon::isSystemTrayAvailable())
		{
			break;
		}

		if (++trayAttempts > TRAY_RETRY_COUNT)
		{
			fprintf(stdout, "System tray is unavailable.\n");
			return false;
		}

		QThreadImpl::msleep(TRAY_RETRY_WAIT);
	}
	return true;
}

#if defined(Q_OS_MAC)
bool checkMacAssistiveDevices()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090 // mavericks

	// new in mavericks, applications are trusted individually
	// with use of the accessibility api. this call will show a
	// prompt which can show the security/privacy/accessibility
    // tab, with a list of allowed applications. InputLeap should
	// show up there automatically, but will be unchecked.

	if (AXIsProcessTrusted()) {
		return true;
	}

	const void* keys[] = { kAXTrustedCheckOptionPrompt };
	const void* trueValue[] = { kCFBooleanTrue };
	CFDictionaryRef options = CFDictionaryCreate(nullptr, keys, trueValue, 1, nullptr, nullptr);

	bool result = AXIsProcessTrustedWithOptions(options);
	CFRelease(options);
	return result;

#else

	// now deprecated in mavericks.
	bool result = AXAPIEnabled();
	if (!result) {
		QMessageBox::information(
            nullptr, "InputServer",
			"Please enable access to assistive devices "
			"System Preferences -> Security & Privacy -> "
            "Privacy -> Accessibility, then re-open InputServer.");
	}
	return result;

#endif
}
#endif
