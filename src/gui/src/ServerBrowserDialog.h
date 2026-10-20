/* InputServer -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDialog>
#include <QHostAddress>

class QLabel;
class QListWidget;
class QPushButton;

class ServerBrowserDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ServerBrowserDialog(QWidget* parent = nullptr);
    void serverFound(const QString& name, const QHostAddress& address, quint16 port);
    void serverLost(const QString& name, const QHostAddress& address);
    void selectServer(const QString& name);
    QString selectedName() const;

private:
    void updateState();
    QListWidget* servers_;
    QLabel* status_;
    QPushButton* select_;
};
