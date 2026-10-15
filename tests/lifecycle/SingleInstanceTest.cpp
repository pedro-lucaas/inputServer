// SPDX-License-Identifier: GPL-2.0-or-later
#include "../../src/gui/src/SingleInstance.h"
#include "../../src/gui/src/ProcessGuard.h"
#include <QProcess>
#include <QThread>
#include <QTemporaryDir>
#include <QtTest>

class SingleInstanceTest : public QObject
{
    Q_OBJECT
private slots:
    void rejectsDuplicateAndActivatesOwner()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SingleInstance owner(directory.path());
        QVERIFY(owner.acquire());
        bool activated = false;
        owner.onActivation(this, [&] { activated = true; });
        QProcess duplicate;
        duplicate.start(QCoreApplication::applicationFilePath(),
                        {"--command", directory.path(), "show"});
        QTRY_VERIFY(activated);
        QTRY_COMPARE(duplicate.state(), QProcess::NotRunning);
        QCOMPARE(duplicate.exitCode(), 0);
    }

    void releasesLockOnExit()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        {
            SingleInstance first(directory.path());
            QVERIFY(first.acquire());
        }
        SingleInstance second(directory.path());
        QVERIFY(second.acquire());
    }

    void deliversStopToOwner()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SingleInstance owner(directory.path());
        QVERIFY(owner.acquire());
        QByteArray received;
        owner.onCommand(this, [&](QByteArray command) { received = command; });
        QProcess controller;
        controller.start(QCoreApplication::applicationFilePath(),
                         {"--command", directory.path(), "stop"});
        QTRY_COMPARE(received, QByteArray("stop"));
        QTRY_COMPARE(controller.state(), QProcess::NotRunning);
        QCOMPARE(controller.exitCode(), 0);
    }

#ifdef Q_OS_WIN
    void jobStopsOnlyOwnedChild()
    {
        QProcess owned;
        QProcess unrelated;
        owned.start(QCoreApplication::applicationFilePath(), {"--child"});
        unrelated.start(QCoreApplication::applicationFilePath(), {"--child"});
        QVERIFY(owned.waitForStarted());
        QVERIFY(unrelated.waitForStarted());
        ProcessGuard guard;
        QVERIFY(guard.attach(owned.processId()));
        guard.reset();
        QVERIFY(owned.waitForFinished(3000));
        QCOMPARE(unrelated.state(), QProcess::Running);
        unrelated.kill();
        QVERIFY(unrelated.waitForFinished(3000));
    }
#endif
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains("--child")) {
        QThread::sleep(30);
        return 0;
    }
    if (app.arguments().size() == 4 && app.arguments().at(1) == "--command") {
        SingleInstance controller(app.arguments().at(2));
        return controller.acquire(app.arguments().at(3).toLatin1()) ? 2 : 0;
    }
    SingleInstanceTest tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "SingleInstanceTest.moc"
