/* InputServer -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include <QByteArray>
#include <QRegularExpression>
#include <QString>
#include <optional>

struct TrustedClientRequest {
    QString name;
    QByteArray fingerprint;
};

// The engine emits this only after mutual TLS has authenticated the client.
// Both fields are hex encoded so a device name cannot create log/config lines.
inline std::optional<TrustedClientRequest> parseTrustedClientRequest(const QString& line)
{
    static const QRegularExpression request(QStringLiteral(
        "^\\[[^\\]\\r\\n]+\\] NOTE: trusted client registration: ([0-9a-fA-F]{2,510}) ([0-9a-fA-F]{64})$"));
    const auto match = request.match(line);
    if (!match.hasMatch() || match.captured(1).size() % 2 != 0) return std::nullopt;
    const auto bytes = QByteArray::fromHex(match.captured(1).toLatin1());
    const QString name = QString::fromUtf8(bytes);
    static const QRegularExpression validName(QStringLiteral(
        "^[A-Za-z0-9_](?:[A-Za-z0-9_.-]{0,253}[A-Za-z0-9_.])?$"));
    if (!validName.match(name).hasMatch() || name.contains("..")) return std::nullopt;
    return TrustedClientRequest{name, QByteArray::fromHex(match.captured(2).toLatin1())};
}
