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

#include "UdpDiscovery.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QSet>

namespace {

const char kAppTag[] = "InputServer";
const char kTypeQuery[] = "query";
const char kTypeAnnounce[] = "announce";
const char kRoleServer[] = "server";
const char kRoleClient[] = "client";

constexpr int kQueryIntervalMs = 2000;
constexpr int kPeerTimeoutMs = 8000;

QList<QHostAddress> broadcastAddresses()
{
    QList<QHostAddress> result{QHostAddress::Broadcast};
    QSet<QString> seen{QHostAddress(QHostAddress::Broadcast).toString()};

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            !flags.testFlag(QNetworkInterface::CanBroadcast) ||
            flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const auto entries = iface.addressEntries();
        for (const auto& entry : entries) {
            const QHostAddress broadcast = entry.broadcast();
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol || broadcast.isNull()) {
                continue;
            }
            if (!seen.contains(broadcast.toString())) {
                seen.insert(broadcast.toString());
                result.append(broadcast);
            }
        }
    }
    return result;
}

} // namespace

UdpDiscovery::UdpDiscovery(Role role, QString screen_name, quint16 tcp_port,
                           quint16 discovery_port, QObject* parent) :
    QObject(parent),
    role_(role),
    screen_name_(std::move(screen_name)),
    tcp_port_(tcp_port),
    discovery_port_(discovery_port)
{
    query_timer_.setInterval(kQueryIntervalMs);
    expire_timer_.setInterval(kQueryIntervalMs);
    connect(&query_timer_, &QTimer::timeout, this, &UdpDiscovery::sendQuery);
    connect(&expire_timer_, &QTimer::timeout, this, &UdpDiscovery::expirePeers);
    connect(&socket_, &QUdpSocket::readyRead, this, &UdpDiscovery::readPending);
}

UdpDiscovery::~UdpDiscovery()
{
    // No serverLost signals here: receivers may already be tearing down.
    query_timer_.stop();
    expire_timer_.stop();
    socket_.close();
}

bool UdpDiscovery::start()
{
    stop();

    // The server listens on the well-known port; a client only needs some
    // port for the replies to come back to.
    const quint16 bind_port = role_ == Role::Server ? discovery_port_ : 0;
    if (!socket_.bind(QHostAddress::AnyIPv4, bind_port,
                      QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        error_ = socket_.errorString();
        return false;
    }

    if (role_ == Role::Client) {
        sendQuery();
        query_timer_.start();
        expire_timer_.start();
    }
    return true;
}

void UdpDiscovery::stop()
{
    query_timer_.stop();
    expire_timer_.stop();
    socket_.close();

    const auto servers = servers_;
    servers_.clear();
    for (const auto& peer : servers) {
        emit serverLost(peer.name, peer.address);
    }
}

QByteArray UdpDiscovery::encode(const Message& message)
{
    QJsonObject json{
        {"app", kAppTag},
        {"v", kProtocolVersion},
        {"type", message.type},
        {"role", message.role == Role::Server ? kRoleServer : kRoleClient},
        {"name", message.name},
    };
    if (message.port != 0) {
        json.insert("port", message.port);
    }
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

std::optional<UdpDiscovery::Message> UdpDiscovery::decode(const QByteArray& datagram)
{
    if (datagram.isEmpty() || datagram.size() > kMaxDatagramSize) {
        return std::nullopt;
    }

    QJsonParseError parse_error;
    const auto doc = QJsonDocument::fromJson(datagram, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }

    const auto json = doc.object();
    if (json.value("app").toString() != QLatin1String(kAppTag) ||
        json.value("v").toInt() != kProtocolVersion) {
        return std::nullopt;
    }

    Message message;
    message.type = json.value("type").toString();
    if (message.type != QLatin1String(kTypeQuery) &&
        message.type != QLatin1String(kTypeAnnounce)) {
        return std::nullopt;
    }

    const QString role = json.value("role").toString();
    if (role == QLatin1String(kRoleServer)) {
        message.role = Role::Server;
    } else if (role == QLatin1String(kRoleClient)) {
        message.role = Role::Client;
    } else {
        return std::nullopt;
    }

    message.name = json.value("name").toString().trimmed();
    if (message.name.isEmpty() || message.name.size() > kMaxNameLength) {
        return std::nullopt;
    }

    if (message.type == QLatin1String(kTypeAnnounce)) {
        const int port = json.value("port").toInt();
        if (port <= 0 || port > 65535) {
            return std::nullopt;
        }
        message.port = static_cast<quint16>(port);
    }
    return message;
}

void UdpDiscovery::sendQuery()
{
    Message query;
    query.type = kTypeQuery;
    query.role = Role::Client;
    query.name = screen_name_;
    const QByteArray datagram = encode(query);

    const auto targets = broadcastAddresses();
    for (const auto& target : targets) {
        socket_.writeDatagram(datagram, target, discovery_port_);
    }
}

void UdpDiscovery::readPending()
{
    while (socket_.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = socket_.receiveDatagram(kMaxDatagramSize + 1);
        if (!datagram.isValid()) {
            continue;
        }
        if (auto message = decode(datagram.data())) {
            QHostAddress sender = datagram.senderAddress();
            // Replies to IPv4 broadcasts may arrive as IPv4-mapped IPv6.
            bool is_ipv4 = false;
            const quint32 ipv4 = sender.toIPv4Address(&is_ipv4);
            if (is_ipv4) {
                sender = QHostAddress(ipv4);
            }
            handle(*message, sender, static_cast<quint16>(datagram.senderPort()));
        }
    }
}

void UdpDiscovery::handle(const Message& message, const QHostAddress& sender,
                          quint16 sender_port)
{
    if (role_ == Role::Server && message.type == QLatin1String(kTypeQuery) &&
        message.role == Role::Client) {
        emit clientFound(message.name);

        Message reply;
        reply.type = kTypeAnnounce;
        reply.role = Role::Server;
        reply.name = screen_name_;
        reply.port = tcp_port_;
        socket_.writeDatagram(encode(reply), sender, sender_port);
        return;
    }

    if (role_ == Role::Client && message.type == QLatin1String(kTypeAnnounce) &&
        message.role == Role::Server) {
        const QString key = peerKey(sender, message.name);
        const bool is_new = !servers_.contains(key);
        Peer& peer = servers_[key];
        peer.name = message.name;
        peer.address = sender;
        peer.port = message.port;
        peer.last_seen = QDateTime::currentDateTimeUtc();
        if (is_new) {
            emit serverFound(peer.name, peer.address, peer.port);
        }
    }
}

void UdpDiscovery::expirePeers()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it = servers_.begin(); it != servers_.end();) {
        if (it->last_seen.msecsTo(now) > kPeerTimeoutMs) {
            const Peer peer = *it;
            it = servers_.erase(it);
            emit serverLost(peer.name, peer.address);
        } else {
            ++it;
        }
    }
}

QString UdpDiscovery::peerKey(const QHostAddress& address, const QString& name)
{
    return address.toString() + QLatin1Char('|') + name;
}
