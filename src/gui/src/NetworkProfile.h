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

#include <QStringList>

namespace network_profile {

// Names of the connected, internet-facing networks that Windows classifies
// as "Public" (VPN tunnels and virtual adapters are ignored).
// The installer only opens the firewall for private networks, so other
// computers cannot reach a server on these. Always empty on other systems.
QStringList publicNetworkNames();

} // namespace network_profile
