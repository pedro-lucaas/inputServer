/* InputServer -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "../src/ServerConfig.h"
#include "../src/TrustedClientRequest.h"
#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <gtest/gtest.h>
#include <utility>

TEST(ClientEnrollmentTests, approvedClientIsAddedWithoutManualNameOrDialog)
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    ServerConfig config(&settings, 5, 3, "desktop", nullptr);
    ASSERT_TRUE(config.addTrustedClient("MacBook-Air-de-Pedro.local"));
    EXPECT_EQ(std::as_const(config).screens()[7].name(), "desktop");
    EXPECT_EQ(std::as_const(config).screens()[8].name(), "MacBook-Air-de-Pedro.local");
    ServerConfig reloaded(&settings, 5, 3, "desktop", nullptr);
    EXPECT_EQ(std::as_const(reloaded).screens()[8].name(), "MacBook-Air-de-Pedro.local");
}

TEST(ClientEnrollmentTests, reconnectDoesNotDuplicateOrMoveExistingScreen)
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    ServerConfig config(&settings, 5, 3, "desktop", nullptr);
    ASSERT_TRUE(config.addTrustedClient("mac"));
    EXPECT_FALSE(config.addTrustedClient("mac"));
    EXPECT_EQ(config.numScreens(), 2);
    EXPECT_EQ(std::as_const(config).screens()[8].name(), "mac");
}

TEST(ClientEnrollmentTests, fullRightHandSideUsesAnotherReachablePosition)
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    ServerConfig config(&settings, 5, 3, "desktop", nullptr);
    ASSERT_TRUE(config.addTrustedClient("one"));
    ASSERT_TRUE(config.addTrustedClient("two"));
    ASSERT_TRUE(config.addTrustedClient("three"));
    EXPECT_EQ(std::as_const(config).screens()[8].name(), "one");
    EXPECT_EQ(std::as_const(config).screens()[9].name(), "two");
    EXPECT_EQ(config.numScreens(), 4);
}

TEST(ClientEnrollmentTests, fullGridDoesNotOverwriteExistingDevices)
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    ServerConfig config(&settings, 5, 3, "desktop", nullptr);
    for (int i = 0; i < 14; ++i) ASSERT_TRUE(config.addTrustedClient(QString("client%1").arg(i)));
    EXPECT_FALSE(config.addTrustedClient("extra"));
    EXPECT_EQ(config.numScreens(), 15);
}

TEST(ClientEnrollmentTests, malformedNamesCannotInjectConfiguration)
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
    ServerConfig config(&settings, 5, 3, "desktop", nullptr);
    for (const auto& name : {QString(), QString("client\nend"), QString("<b>client</b>"), QString(".."), QString(256, 'a')}) {
        EXPECT_FALSE(config.addTrustedClient(name));
    }
}

TEST(ClientEnrollmentTests, registrationRequiresStructuredCertificateBoundRequest)
{
    const QString prefix = "[2026-10-20T11:00:00] NOTE: trusted client registration: ";
    const QString name = QString::fromLatin1(QByteArray("MacBook-Air-de-Pedro.local").toHex());
    const QString fingerprint(64, 'a');
    const auto request = parseTrustedClientRequest(prefix + name + " " + fingerprint);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->name, "MacBook-Air-de-Pedro.local");
    EXPECT_EQ(request->fingerprint.size(), 32);
    EXPECT_FALSE(parseTrustedClientRequest("client found on the network: mac").has_value());
    EXPECT_FALSE(parseTrustedClientRequest(prefix + name + " " + QString(63, 'a')).has_value());
    EXPECT_FALSE(parseTrustedClientRequest(prefix + "0" + name + " " + fingerprint).has_value());
    EXPECT_FALSE(parseTrustedClientRequest(prefix + QString::fromLatin1(QByteArray("client\nend").toHex()) + " " + fingerprint).has_value());
    EXPECT_FALSE(parseTrustedClientRequest(prefix + name + " " + fingerprint + "\nspoof").has_value());
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
