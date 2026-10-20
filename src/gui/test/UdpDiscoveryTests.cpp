/*  InputServer -- mouse and keyboard sharing utility

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#include "../src/UdpDiscovery.h"
#include <gtest/gtest.h>

using Message = UdpDiscovery::Message;
using Role = UdpDiscovery::Role;

TEST(UdpDiscoveryTests, announce_round_trip)
{
    Message announce;
    announce.type = "announce";
    announce.role = Role::Server;
    announce.name = QString::fromUtf8("escritório");
    announce.port = 24800;

    auto decoded = UdpDiscovery::decode(UdpDiscovery::encode(announce));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->type, "announce");
    EXPECT_EQ(decoded->role, Role::Server);
    EXPECT_EQ(decoded->name, announce.name);
    EXPECT_EQ(decoded->port, 24800);
}

TEST(UdpDiscoveryTests, query_round_trip_has_no_port)
{
    Message query;
    query.type = "query";
    query.role = Role::Client;
    query.name = "notebook";

    auto decoded = UdpDiscovery::decode(UdpDiscovery::encode(query));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->type, "query");
    EXPECT_EQ(decoded->role, Role::Client);
    EXPECT_EQ(decoded->port, 0);
}

TEST(UdpDiscoveryTests, rejects_foreign_or_malformed_datagrams)
{
    const char* invalid[] = {
        "",
        "not json",
        "[]",
        R"({"app":"Other","v":1,"type":"query","role":"client","name":"a"})",
        R"({"app":"InputServer","v":2,"type":"query","role":"client","name":"a"})",
        R"({"app":"InputServer","v":1,"type":"hello","role":"client","name":"a"})",
        R"({"app":"InputServer","v":1,"type":"query","role":"peer","name":"a"})",
        R"({"app":"InputServer","v":1,"type":"query","role":"client","name":"  "})",
        R"({"app":"InputServer","v":1,"type":"announce","role":"server","name":"a"})",
        R"({"app":"InputServer","v":1,"type":"announce","role":"server","name":"a","port":70000})",
    };
    for (const char* datagram : invalid) {
        EXPECT_FALSE(UdpDiscovery::decode(QByteArray(datagram)).has_value()) << datagram;
    }
}

TEST(UdpDiscoveryTests, rejects_oversized_datagram_and_name)
{
    QByteArray big(UdpDiscovery::kMaxDatagramSize + 1, ' ');
    EXPECT_FALSE(UdpDiscovery::decode(big).has_value());

    Message query;
    query.type = "query";
    query.role = Role::Client;
    query.name = QString(UdpDiscovery::kMaxNameLength + 1, QLatin1Char('x'));
    EXPECT_FALSE(UdpDiscovery::decode(UdpDiscovery::encode(query)).has_value());
}
