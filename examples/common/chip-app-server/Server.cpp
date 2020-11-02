/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Server.h"

#include "DataModelHandler.h"
#include "RendezvousServer.h"
#include "SessionManager.h"

#include <ble/BLEEndPoint.h>
#include <inet/IPAddress.h>
#include <inet/InetError.h>
#include <inet/InetLayer.h>
#include <messaging/ExchangeContext.h>
#include <messaging/ExchangeMgr.h>
#include <platform/CHIPDeviceLayer.h>
#include <support/CodeUtils.h>
#include <support/ErrorStr.h>
#include <support/logging/CHIPLogging.h>
#include <sys/param.h>
#include <system/SystemPacketBuffer.h>

using namespace ::chip;
using namespace ::chip::Inet;
using namespace ::chip::Transport;
using namespace ::chip::DeviceLayer;

#ifndef EXAMPLE_SERVER_NODEID
#define EXAMPLE_SERVER_NODEID 12344321
#endif // EXAMPLE_SERVER_NODEID

namespace {

class ServerCallback : public ExchangeContextDelegate
{
public:
    void OnMessageReceived(ExchangeContext * ec, const PacketHeader & packetHeader, uint32_t protocolId, uint8_t msgType, System::PacketBuffer * buffer) override
    {
        const size_t data_len = buffer->DataLength();

        // as soon as a client connects, assume it is connected
        VerifyOrExit(buffer != NULL, ChipLogProgress(AppServer, "Received data but couldn't process it..."));

        VerifyOrExit(ec->GetPeerNodeId() != kUndefinedNodeId, ChipLogProgress(AppServer, "Unknown source for received message"));

        ChipLogProgress(AppServer, "Packet received from exchange %p: %zu bytes", ec, static_cast<size_t>(data_len));

        HandleDataModelMessage(ec, packetHeader, buffer);
        buffer = NULL;

    exit:
        // HandleDataModelMessage calls Free on the buffer without an AddRef, if HandleDataModelMessage was not called, free the
        // buffer.
        if (buffer != NULL)
        {
            System::PacketBuffer::Free(buffer);
        }
    }

    void OnResponseTimeout(ExchangeContext * ec) override
    {
        ChipLogProgress(AppServer, "Exchange %p timeout.", ec);
    }
};

DemoSessionManager gSessions;
ExchangeManager gExchangeManager;
ServerCallback gCallbacks;
SecurePairingUsingTestSecret gTestPairing;
RendezvousServer gRendezvousServer;

} // namespace

SecureSessionMgrBase & chip::SessionManager()
{
    return gSessions;
}

// The function will initialize datamodel handler and then start the server
// The server assumes the platform's networking has been setup already
void InitServer()
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    Optional<Transport::PeerAddress> peer(Transport::Type::kUndefined);

    InitDataModelHandler();

    err = gSessions.Init(EXAMPLE_SERVER_NODEID, &DeviceLayer::SystemLayer,
                         UdpListenParameters(&DeviceLayer::InetLayer).SetAddressType(kIPAddressType_IPv6));
    SuccessOrExit(err);

    // This flag is used to bypass BLE in the cirque test
    // Only in the cirque test this is enabled with --args='bypass_rendezvous=true'
#ifndef BYPASS_RENDEZVOUS
    {
        RendezvousParameters params;
        uint32_t pinCode;

        SuccessOrExit(err = DeviceLayer::ConfigurationMgr().GetSetupPinCode(pinCode));
        params.SetSetupPINCode(pinCode)
            .SetLocalNodeId(EXAMPLE_SERVER_NODEID)
            .SetBleLayer(DeviceLayer::ConnectivityMgr().GetBleLayer());
        SuccessOrExit(err = gRendezvousServer.Init(params));
    }
#endif

    err = gSessions.NewPairing(peer, &gTestPairing);
    SuccessOrExit(err);

    gExchangeManager.Init(&gSessions);
    gSessions.SetDelegate(&gExchangeManager);
    gExchangeManager.RegisterUnsolicitedMessageHandler(Protocols::kProtocol_InteractionModel, 0, &gCallbacks);

exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "ERROR setting up transport: %s", ErrorStr(err));
    }
    else
    {
        ChipLogProgress(AppServer, "Server Listening...");
    }
}
