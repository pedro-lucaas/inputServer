/* InputServer -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../src/ServerBrowserDialog.h"
#include <QApplication>
#include <QListWidget>
#include <QPushButton>
#include <gtest/gtest.h>

TEST(ServerBrowserTests, emptySearchCannotBeAccepted)
{
    ServerBrowserDialog dialog;
    EXPECT_TRUE(dialog.selectedName().isEmpty());
    EXPECT_FALSE(dialog.findChild<QPushButton*>("selectServer")->isEnabled());
}

TEST(ServerBrowserTests, discoveryDoesNotChooseServerAndBetterAddressUpdatesEntry)
{
    ServerBrowserDialog dialog;
    dialog.serverFound("desktop", QHostAddress("100.64.0.1"), 24800);
    EXPECT_TRUE(dialog.selectedName().isEmpty());
    dialog.serverFound("desktop", QHostAddress("192.168.0.128"), 24800);
    auto* list = dialog.findChild<QListWidget*>("availableServers");
    ASSERT_EQ(list->count(), 1);
    EXPECT_TRUE(list->item(0)->text().contains("192.168.0.128:24800"));
}

TEST(ServerBrowserTests, selectionCanBeConfirmed)
{
    ServerBrowserDialog dialog;
    dialog.serverFound("desktop", QHostAddress("192.168.0.128"), 24800);
    dialog.selectServer("desktop");
    auto* button = dialog.findChild<QPushButton*>("selectServer");
    ASSERT_TRUE(button->isEnabled());
    button->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.selectedName(), "desktop");
}

TEST(ServerBrowserTests, expiredSelectionDoesNotFallBackToOtherServer)
{
    ServerBrowserDialog dialog;
    dialog.serverFound("first", QHostAddress("192.168.0.1"), 24800);
    dialog.serverFound("second", QHostAddress("192.168.0.2"), 24800);
    dialog.selectServer("first");
    dialog.serverLost("first", QHostAddress("192.168.0.1"));
    EXPECT_TRUE(dialog.selectedName().isEmpty());
    EXPECT_FALSE(dialog.findChild<QPushButton*>("selectServer")->isEnabled());
}

TEST(ServerBrowserTests, expiryOfAnotherServerPreservesChoice)
{
    ServerBrowserDialog dialog;
    dialog.serverFound("first", QHostAddress("192.168.0.1"), 24800);
    dialog.serverFound("second", QHostAddress("192.168.0.2"), 24800);
    dialog.selectServer("second");
    dialog.serverLost("first", QHostAddress("192.168.0.1"));
    EXPECT_EQ(dialog.selectedName(), "second");
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
