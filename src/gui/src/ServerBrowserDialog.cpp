/* InputServer -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ServerBrowserDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ServerBrowserDialog::ServerBrowserDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Available servers"));
    resize(520, 340);
    auto* layout = new QVBoxLayout(this);
    auto* hint = new QLabel(tr("Choose the computer whose keyboard and mouse will control this Mac."), this);
#ifndef Q_OS_MAC
    hint->setText(tr("Choose the computer whose keyboard and mouse will control this computer."));
#endif
    hint->setWordWrap(true);
    layout->addWidget(hint);
    servers_ = new QListWidget(this);
    servers_->setObjectName("availableServers");
    servers_->setAccessibleName(tr("Available servers"));
    layout->addWidget(servers_);
    status_ = new QLabel(this);
    status_->setWordWrap(true);
    layout->addWidget(status_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    select_ = buttons->addButton(tr("Select server"), QDialogButtonBox::AcceptRole);
    select_->setObjectName("selectServer");
    select_->setProperty("primary", true);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(servers_, &QListWidget::currentRowChanged, this, [this] { updateState(); });
    connect(servers_, &QListWidget::itemDoubleClicked, this, [this] {
        if (!selectedName().isEmpty()) accept();
    });
    updateState();
}

void ServerBrowserDialog::serverFound(const QString& name, const QHostAddress& address, quint16 port)
{
    QListWidgetItem* item = nullptr;
    for (int i = 0; i < servers_->count(); ++i) {
        if (servers_->item(i)->data(Qt::UserRole).toString() == name) {
            item = servers_->item(i);
            break;
        }
    }
    if (!item) {
        item = new QListWidgetItem(servers_);
        item->setData(Qt::UserRole, name);
    }
    item->setText(QStringLiteral("%1\n%2:%3").arg(name, address.toString()).arg(port));
    updateState();
}

void ServerBrowserDialog::serverLost(const QString& name, const QHostAddress&)
{
    for (int i = 0; i < servers_->count(); ++i) {
        if (servers_->item(i)->data(Qt::UserRole).toString() == name) {
            const bool selected = servers_->item(i) == servers_->currentItem();
            delete servers_->takeItem(i);
            // Never silently select a different server after one disappears.
            if (selected) servers_->setCurrentRow(-1);
            break;
        }
    }
    updateState();
}

void ServerBrowserDialog::selectServer(const QString& name)
{
    for (int i = 0; i < servers_->count(); ++i) {
        if (servers_->item(i)->data(Qt::UserRole).toString() == name) {
            servers_->setCurrentRow(i);
            return;
        }
    }
}

QString ServerBrowserDialog::selectedName() const
{
    const auto* item = servers_->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void ServerBrowserDialog::updateState()
{
    select_->setEnabled(!selectedName().isEmpty());
    status_->setText(servers_->count() == 0
        ? tr("Searching the local network... No servers found yet. Keep the server running on the same network, or cancel and enter its IP manually.")
        : tr("The list updates automatically. Select a server to continue."));
}
