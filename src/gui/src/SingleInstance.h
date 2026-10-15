// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDir>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QVariant>
#include <functional>

// The lock owns the endpoint: only its owner may remove a stale socket.
// Use an app-specific directory in the current user's profile.
class SingleInstance : public QObject
{
public:
    explicit SingleInstance(const QString& directory) :
        lock_(directory + "/instance.lock"),
        endpoint_(directory + "/instance.socket")
    {
        QDir().mkpath(directory);
        lock_.setStaleLockTime(0); // Never expire the lock of a live process.
        server_.setSocketOptions(QLocalServer::UserAccessOption);
    }

    bool acquire(const QByteArray& command = "show")
    {
        if (!lock_.tryLock()) {
            QLocalSocket socket;
            socket.connectToServer(endpoint_);
            if (socket.waitForConnected(1500)) {
                socket.write(command + '\n');
                socket.waitForBytesWritten(1000);
                // Keep the named pipe alive until the owner has read the command.
                socket.waitForReadyRead(3000);
            }
            return false;
        }
        QLocalServer::removeServer(endpoint_);
        return server_.listen(endpoint_);
    }

    void onCommand(QObject* context, std::function<void(QByteArray)> handle)
    {
        QObject::connect(&server_, &QLocalServer::newConnection, context,
                         [this, handle] {
            while (auto* socket = server_.nextPendingConnection()) {
                auto receive = [socket, handle] {
                    if (socket->property("handled").toBool() || !socket->canReadLine()) {
                        return;
                    }
                    socket->setProperty("handled", true);
                    const QByteArray command = socket->readLine(32).trimmed();
                    socket->write("ok");
                    socket->flush();
                    socket->disconnectFromServer();
                    handle(command);
                };
                QObject::connect(socket, &QLocalSocket::readyRead, socket, receive);
                QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
                receive();
            }
        });
    }

    void onActivation(QObject* context, std::function<void()> activate)
    {
        onCommand(context, [activate](QByteArray) { activate(); });
    }

private:
    QLockFile lock_;
    QString endpoint_;
    QLocalServer server_;
};
