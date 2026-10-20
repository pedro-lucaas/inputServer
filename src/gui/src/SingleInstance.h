// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDir>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QVariant>
#include <functional>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <QCryptographicHash>
#endif

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

#if defined(Q_OS_WIN)
    ~SingleInstance() override
    {
        if (mutex_) {
            CloseHandle(mutex_);
        }
    }
#endif

    bool acquire(const QByteArray& command = "show")
    {
        if (anotherInstanceRunning() || !lock_.tryLock()) {
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
    // QLockFile cannot always tell that an elevated copy (e.g. started by an
    // installer run as administrator) is alive, and that let a second copy
    // start with a second tray icon. A named mutex in the session namespace
    // is seen by both: opening one created by an elevated process fails with
    // ERROR_ACCESS_DENIED, which still means "running".
    bool anotherInstanceRunning()
    {
#if defined(Q_OS_WIN)
        if (mutex_) {
            return false;
        }
        const QByteArray id = QCryptographicHash::hash(endpoint_.toUtf8(),
                                                       QCryptographicHash::Sha1).toHex();
        const std::wstring name = L"Local\\InputServer-GUI-" + QString::fromLatin1(id).toStdWString();
        mutex_ = CreateMutexW(nullptr, FALSE, name.c_str());
        const DWORD error = GetLastError();
        if (mutex_ && error == ERROR_ALREADY_EXISTS) {
            CloseHandle(mutex_);
            mutex_ = nullptr;
            return true;
        }
        return !mutex_ && error == ERROR_ACCESS_DENIED;
#else
        return false;
#endif
    }

    QLockFile lock_;
    QString endpoint_;
    QLocalServer server_;
#if defined(Q_OS_WIN)
    HANDLE mutex_ = nullptr;
#endif
};
