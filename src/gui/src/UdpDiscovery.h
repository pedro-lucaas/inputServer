/*
 * InputServer -- mouse and keyboard sharing utility
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUdpSocket>

#include <optional>

// Finds InputServer servers on the local network without Bonjour/mDNS.
//
// Clients broadcast a small JSON query to the discovery port every couple of
// seconds; servers listen on that port and answer the sender directly. Only
// the server needs an inbound firewall rule: the reply is unicast traffic to
// the port the query came from. Discovery is unauthenticated on purpose; the
// TLS fingerprint check on connect is what proves a server's identity.
class UdpDiscovery : public QObject
{
    Q_OBJECT

public:
    enum class Role { Server, Client };

    struct Peer {
        QString name;
        QHostAddress address;
        quint16 port = 0;
        QDateTime last_seen;
    };

    struct Message {
        QString type;
        Role role = Role::Client;
        QString name;
        quint16 port = 0;
    };

    static constexpr int kProtocolVersion = 1;
    static constexpr int kMaxDatagramSize = 512;
    static constexpr int kMaxNameLength = 255;

    UdpDiscovery(Role role, QString screen_name, quint16 tcp_port, quint16 discovery_port,
                 QObject* parent = nullptr);
    ~UdpDiscovery() override;

    bool start();
    void stop();
    QString errorString() const { return error_; }

    static QByteArray encode(const Message& message);
    static std::optional<Message> decode(const QByteArray& datagram);

    // Lower is better: 192.168/16, 10/8, 172.16/12, then anything else
    // (VPN, CGNAT, virtual adapters).
    static int addressRank(const QHostAddress& address);

signals:
    // Client side: a server answered (again with a better address for the same
    // name, e.g. LAN instead of VPN), or was not heard from for a while.
    void serverFound(const QString& name, const QHostAddress& address, quint16 port);
    void serverLost(const QString& name, const QHostAddress& address);
    // Server side: a client is looking for servers.
    void clientFound(const QString& name);

private:
    void sendQuery();
    void readPending();
    void handle(const Message& message, const QHostAddress& sender, quint16 sender_port);
    void expirePeers();

    Role role_;
    QString screen_name_;
    quint16 tcp_port_;
    quint16 discovery_port_;
    QUdpSocket socket_;
    QTimer query_timer_;
    QTimer expire_timer_;
    QHash<QString, Peer> servers_;
    QString error_;
};
