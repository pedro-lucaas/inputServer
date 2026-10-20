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

#include "NetworkProfile.h"

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <netlistmgr.h>
#endif

namespace network_profile {

#if defined(Q_OS_WIN)

namespace {

template <typename T>
struct ComPtr {
    T* ptr = nullptr;
    ~ComPtr() { if (ptr) ptr->Release(); }
    T** out() { return &ptr; }
    T* operator->() const { return ptr; }
    explicit operator bool() const { return ptr != nullptr; }
};

} // namespace

QStringList publicNetworkNames()
{
    QStringList names;

    // Qt has already initialized COM on the GUI thread; this is a no-op
    // there and a proper initialization when called from elsewhere.
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool must_uninit = SUCCEEDED(init);

    {
        ComPtr<INetworkListManager> manager;
        if (FAILED(CoCreateInstance(__uuidof(NetworkListManager), nullptr, CLSCTX_ALL,
                                    __uuidof(INetworkListManager),
                                    reinterpret_cast<void**>(manager.out())))) {
            if (must_uninit) CoUninitialize();
            return names;
        }

        ComPtr<IEnumNetworks> networks;
        if (SUCCEEDED(manager->GetNetworks(NLM_ENUM_NETWORK_CONNECTED, networks.out()))) {
            for (;;) {
                ComPtr<INetwork> network;
                ULONG fetched = 0;
                if (networks->Next(1, network.out(), &fetched) != S_OK || fetched == 0) {
                    break;
                }

                NLM_NETWORK_CATEGORY category = NLM_NETWORK_CATEGORY_PRIVATE;
                if (FAILED(network->GetCategory(&category)) ||
                    category != NLM_NETWORK_CATEGORY_PUBLIC) {
                    continue;
                }

                // Only the network that reaches the internet is the LAN the
                // other computers are on; VPN tunnels and virtual adapters
                // (local connectivity only) would just be noise.
                NLM_CONNECTIVITY connectivity = NLM_CONNECTIVITY_DISCONNECTED;
                const int internet = NLM_CONNECTIVITY_IPV4_INTERNET | NLM_CONNECTIVITY_IPV6_INTERNET;
                if (FAILED(network->GetConnectivity(&connectivity)) ||
                    (connectivity & internet) == 0) {
                    continue;
                }

                BSTR name = nullptr;
                if (SUCCEEDED(network->GetName(&name)) && name) {
                    names.append(QString::fromWCharArray(name));
                    SysFreeString(name);
                }
            }
        }
    }

    if (must_uninit) {
        CoUninitialize();
    }
    return names;
}

#else

QStringList publicNetworkNames()
{
    return {};
}

#endif

} // namespace network_profile
