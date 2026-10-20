// SPDX-License-Identifier: GPL-2.0-or-later
#include "../../src/gui/src/AsyncProcessStopper.h"
#include <QCoreApplication>
#include <QTest>
#include <csignal>
#include <iostream>

class ProcessStopTest : public QObject
{
    Q_OBJECT
private:
    void launch(QProcess& process, bool ignoreTerminate = false)
    {
        process.start(QCoreApplication::applicationFilePath(),
                      {"--child", ignoreTerminate ? "ignore" : "normal"});
        QVERIFY(process.waitForStarted(2000));
        QVERIFY(process.waitForReadyRead(2000));
        QCOMPARE(process.readAllStandardOutput().trimmed(), QByteArray("ready"));
    }
private slots:
    void alreadyStoppedCompletesAsynchronously()
    {
        AsyncProcessStopper stopper;
        QProcess process;
        int completed = 0;
        stopper.stop(&process, [&](bool ok) { QVERIFY(ok); ++completed; });
        QCOMPARE(completed, 0);
        QTRY_COMPARE(completed, 1);
        QVERIFY(!stopper.busy());
    }
    void stopsOnlyOwnedChild()
    {
        QProcess child, unrelated;
        launch(child);
        launch(unrelated);
        AsyncProcessStopper stopper(100, 500);
        int completed = 0;
        stopper.stop(&child, [&](bool ok) { QVERIFY(ok); ++completed; });
        QTRY_COMPARE_WITH_TIMEOUT(completed, 1, 2000);
        QCOMPARE(child.state(), QProcess::NotRunning);
        QCOMPARE(unrelated.state(), QProcess::Running);
        unrelated.kill();
        QVERIFY(unrelated.waitForFinished(2000));
    }
    void remainsResponsiveWhileChildIgnoresTerminate()
    {
        QProcess child;
        launch(child, true);
        AsyncProcessStopper stopper(150, 500);
        int ticks = 0, completed = 0;
        QTimer heartbeat;
        heartbeat.setInterval(10);
        connect(&heartbeat, &QTimer::timeout, this, [&] { ++ticks; });
        heartbeat.start();
        stopper.stop(&child, [&](bool ok) { QVERIFY(ok); ++completed; });
        QCOMPARE(completed, 0);
        QTRY_COMPARE_WITH_TIMEOUT(completed, 1, 2000);
        QVERIFY(ticks >= 3);
        QCOMPARE(child.state(), QProcess::NotRunning);
    }
    void repeatedStopDoesNotDuplicateCompletion()
    {
        QProcess child;
        launch(child, true);
        AsyncProcessStopper stopper(100, 500);
        int completed = 0;
        stopper.stop(&child, [&](bool ok) { QVERIFY(ok); ++completed; });
        stopper.stop(&child, [&](bool) { QFAIL("Duplicate stop must not replace the first callback"); });
        QTRY_COMPARE_WITH_TIMEOUT(completed, 1, 2000);
        QTest::qWait(150);
        QCOMPARE(completed, 1);
    }
    void replacementStartsOnlyAfterOldProcessExits()
    {
        QProcess child, replacement;
        launch(child, true);
        AsyncProcessStopper stopper(100, 500);
        bool completed = false;
        stopper.stop(&child, [&](bool ok) {
            QVERIFY(ok);
            QCOMPARE(child.state(), QProcess::NotRunning);
            QVERIFY(!stopper.busy());
            replacement.start(QCoreApplication::applicationFilePath(), {"--child", "normal"});
            completed = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 2000);
        QVERIFY(replacement.waitForStarted(2000) || replacement.state() == QProcess::Running);
        replacement.kill();
        QVERIFY(replacement.waitForFinished(2000));
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains("--child")) {
        if (app.arguments().contains("ignore")) std::signal(SIGTERM, SIG_IGN);
        std::cout << "ready" << std::endl;
        return app.exec();
    }
    ProcessStopTest tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "ProcessStopTest.moc"
