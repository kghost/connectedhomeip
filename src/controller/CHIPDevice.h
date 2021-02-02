/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    All rights reserved.
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

/**
 *  @file
 *    This file contains definitions for Device class. The objects of this
 *    class will be used by Controller applications to interact with CHIP
 *    devices. The class provides mechanism to construct, send and receive
 *    messages to and from the corresponding CHIP devices.
 */

#pragma once

#include <app/CommandSender.h>
#include <app/InteractionModelEngine.h>
#include <app/util/basic-types.h>
#include <core/CHIPCallback.h>
#include <core/CHIPCore.h>
#include <support/Base64.h>
#include <support/DLLUtil.h>
#include <transport/PASESession.h>
#include <transport/SecureSessionMgr.h>
#include <transport/TransportMgr.h>
#include <transport/raw/MessageHeader.h>
#include <transport/raw/UDP.h>

namespace chip {
namespace Controller {

class DeviceController;
class DeviceStatusDelegate;
struct SerializedDevice;

using DeviceTransportMgr = TransportMgr<Transport::UDP /* IPv6 */
#if INET_CONFIG_ENABLE_IPV4
                                        ,
                                        Transport::UDP /* IPv4 */
#endif
                                        >;

class DLL_EXPORT DevicePairingDelegate
{
public:
    virtual ~DevicePairingDelegate() {}

    /**
     * @brief
     *   Called when the pairing reaches a certain stage.
     *
     * @param status Current status of pairing
     */
    virtual void OnStatusUpdate() {}

    /**
     * @brief
     *   Called when the pairing is complete (with success or error)
     *
     * @param error Error cause, if any
     */
    virtual void OnPairingComplete(CHIP_ERROR error) {}

    /**
     * @brief
     *   Called when the pairing is deleted (with success or error)
     *
     * @param error Error cause, if any
     */
    virtual void OnPairingDeleted(CHIP_ERROR error) {}
};

class DLL_EXPORT Device : public SessionEstablishmentDelegate
{
public:
    Device() : mInterface(INET_NULL_INTERFACEID), mActive(false), mState(DeviceState::NotConnected) {}
    ~Device()
    {
        if (mCommandSender != nullptr)
        {
            mCommandSender->Shutdown();
            mCommandSender = nullptr;
        }
    }

    /**
     * @brief
     *   Set the delegate object which will be called when a message is received.
     *   The user of this Device object must reset the delegate (by calling
     *   SetDelegate(nullptr)) before releasing their delegate object.
     *
     * @param[in] delegate   The pointer to the delegate object.
     */
    void SetDelegate(DeviceStatusDelegate * delegate) { mStatusDelegate = delegate; }

    /**
     * @brief
     *   Set the delegate object which will be called when pairing state is changes
     *   The user of this Device object must reset the delegate (by calling
     *   SetDelegate(nullptr)) before releasing their delegate object.
     *
     * @param[in] delegate   The pointer to the delegate object.
     */
    void SetPairingDelegate(DevicePairingDelegate * delegate) { mPairingDelegate = delegate; }

    // ----- Messaging -----
    /**
     * @brief
     *   Send the provided message to the device
     *
     * @param[in] message   The message to be sent.
     *
     * @return CHIP_ERROR   CHIP_NO_ERROR on success, or corresponding error
     */
    CHIP_ERROR SendMessage(System::PacketBufferHandle message);

    /**
     * @brief
     *   Send the command in internal command sender.
     */
    CHIP_ERROR SendCommands();

    /**
     * @brief
     *   Initialize internal command sender, required for sending commands over interaction model.
     */
    void InitCommandSender()
    {
        if (mCommandSender != nullptr)
        {
            mCommandSender->Shutdown();
            mCommandSender = nullptr;
        }
#ifdef CHIP_APP_USE_INTERACTION_MODEL
        chip::app::InteractionModelEngine::GetInstance()->NewCommandSender(&mCommandSender);
#endif
    }
    app::CommandSender * GetCommandSender() { return mCommandSender; }

    /**
     * @brief
     *   Get the IP address assigned to the device.
     *
     * @param[out] addr   The reference to the IP address.
     *
     * @return true, if the IP address was filled in the out parameter, false otherwise
     */
    bool GetIpAddress(Inet::IPAddress & addr) const;

    /**
     * @brief
     *   Initialize the device object with secure session manager and inet layer object
     *   references. This variant of function is typically used when the device object
     *   is created from a serialized device information. The other parameters (address, port,
     *   interface etc) are part of the serialized device, so those are not required to be
     *   initialized.
     *
     *   Note: The lifetime of session manager and inet layer objects must be longer than
     *   that of this device object. If these objects are freed, while the device object is
     *   still using them, it can lead to unknown behavior and crashes.
     *
     * @param[in] transportMgr Transport manager object pointer
     * @param[in] sessionMgr   Secure session manager object pointer
     * @param[in] inetLayer    InetLayer object pointer
     * @param[in] listenPort   Port on which controller is listening (typically CHIP_PORT)
     */
    void Init(DeviceController * deviceController, DeviceTransportMgr * transportMgr, SecureSessionMgr * sessionMgr,
              Inet::InetLayer * inetLayer, uint16_t listenPort, NodeId peerNodeId = kUndefinedNodeId)
    {
        mDeviceController = deviceController;
        mTransportMgr     = transportMgr;
        mSessionManager   = sessionMgr;
        mInetLayer        = inetLayer;
        mListenPort       = listenPort;
        mDeviceId         = peerNodeId;
    }

    /** @brief Serialize the Pairing Session to a string. It's guaranteed that the string
     *         will be null terminated, and there won't be any embedded null characters.
     *
     * @return Returns a CHIP_ERROR on error, CHIP_NO_ERROR otherwise
     **/
    CHIP_ERROR Serialize(SerializedDevice & output);

    /** @brief Deserialize the Pairing Session from the string. It's expected that the string
     *         will be null terminated, and there won't be any embedded null characters.
     *
     * @return Returns a CHIP_ERROR on error, CHIP_NO_ERROR otherwise
     **/
    CHIP_ERROR Deserialize(const SerializedDevice & input);

    // ----- Connection Management -----
    /**
     * @brief
     *   Pair a CHIP device with the provided Rendezvous connection parameters.
     *   Use registered DevicePairingDelegate object to receive notifications on
     *   pairing status updates.
     *
     *   Note: Pairing process requires that the caller has registered PersistentStorageDelegate
     *         in the Init() call.
     *
     * @param[in] setupPINCode  The PIN code of the peer device
     *
     * @return CHIP_ERROR       The connection status
     */
    CHIP_ERROR PairDeviceOverIP(Transport::PeerAddress address, uint32_t setupPINCode);

    /**
     * @brief
     *   Pair a CHIP device with the provided Rendezvous connection parameters.
     *   Use registered DevicePairingDelegate object to receive notifications on
     *   pairing status updates.
     *
     *   Note: Pairing process requires that the caller has registered PersistentStorageDelegate
     *         in the Init() call.
     *
     * @param[in] setupPINCode  The PIN code of the peer device
     *
     * @return CHIP_ERROR       The connection status
     */
    CHIP_ERROR PairDeviceOverBle(uint16_t mDiscriminator, uint32_t setupPINCode);

    /**
     * @brief
     *   This function stops a pairing process that's in progress. It does not delete the pairing of a previously
     *   paired device.
     *
     * @param[in] error                 Reason to stop the pairing procedure
     *
     * @return CHIP_ERROR               CHIP_NO_ERROR on success, or corresponding error
     */
    CHIP_ERROR StopPairing(CHIP_ERROR error);

    //////////// SessionEstablishmentDelegate Implementation ///////////////
    CHIP_ERROR SendSessionEstablishmentMessage(const PacketHeader & header, const Transport::PeerAddress & peerAddress,
                                               System::PacketBufferHandle msgBuf) override;
    void OnSessionEstablishmentError(CHIP_ERROR error) override;
    void OnSessionEstablished() override;

    // Handle unsure pairing message
    void OnMessageReceived(const PacketHeader & packetHeader, const Transport::PeerAddress & peerAddress,
                           System::PacketBufferHandle msg);

    /**
     * @brief
     *   Called when a new pairing is being established
     *
     * @param session A handle to the secure session
     * @param mgr     A pointer to the SecureSessionMgr
     */
    void OnNewConnection(SecureSessionHandle session, SecureSessionMgr * mgr);

    /**
     * @brief
     *   Called when a connection is closing.
     *
     *   The receiver should release all resources associated with the connection.
     *
     * @param session A handle to the secure session
     * @param mgr     A pointer to the SecureSessionMgr
     */
    void OnConnectionExpired(SecureSessionHandle session, SecureSessionMgr * mgr);

    /**
     * @brief
     *   This function is called when a message is received from the corresponding CHIP
     *   device. The message ownership is transferred to the function, and it is expected
     *   to release the message buffer before returning.
     *
     * @param[in] header        Reference to common packet header of the received message
     * @param[in] payloadHeader Reference to payload header in the message
     * @param[in] session       A handle to the secure session
     * @param[in] msgBuf        The message buffer
     * @param[in] mgr           Pointer to secure session manager which received the message
     */
    void OnMessageReceived(const PacketHeader & header, const PayloadHeader & payloadHeader, SecureSessionHandle session,
                           System::PacketBufferHandle msgBuf, SecureSessionMgr * mgr);

    /**
     * @brief
     *   Return whether the current device object is actively associated with a paired CHIP
     *   device. An active object can be used to communicate with the corresponding device.
     */
    bool IsActive() const { return mActive; }

    void SetActive(bool active) { mActive = active; }

    bool IsSecureConnected() const { return IsActive() && mState == DeviceState::SecureConnected; }

    void Reset()
    {
        SetActive(false);
        mState          = DeviceState::NotConnected;
        mSessionManager = nullptr;
        mStatusDelegate = nullptr;
        mInetLayer      = nullptr;
    }

    NodeId GetDeviceId() const { return mDeviceId; }

    bool MatchesSession(SecureSessionHandle session) const { return mSecureSession == session; }

    void AddResponseHandler(EndpointId endpoint, ClusterId cluster, Callback::Callback<> * onResponse);
    void AddReportHandler(EndpointId endpoint, ClusterId cluster, Callback::Callback<> * onReport);

private:
    enum class DeviceState
    {
        Loaded,
        NotConnected,
        Connecting,
        SecureConnected,
    };

    struct CallbackInfo
    {
        EndpointId endpoint;
        ClusterId cluster;
    };

    enum class ResetTransport
    {
        kYes,
        kNo,
    };
    /* Node ID assigned to the CHIP device */
    NodeId mDeviceId;

    /* IP Address of the CHIP device */
    Inet::IPAddress mDeviceAddr;

    /* Port on which the CHIP device is receiving messages. Typically it is CHIP_PORT */
    uint16_t mDevicePort;

    /* Local network interface that should be used to communicate with the device */
    Inet::InterfaceId mInterface;

    Inet::InetLayer * mInetLayer;

    bool mActive;
    DeviceState mState;

    PASESessionSerializable mPairing;

    DeviceStatusDelegate * mStatusDelegate;
    DevicePairingDelegate * mPairingDelegate;

    DeviceController * mDeviceController;
    SecureSessionMgr * mSessionManager;

    DeviceTransportMgr * mTransportMgr;

    app::CommandSender * mCommandSender;

    SecureSessionHandle mSecureSession = {};

    PASESession * mPASESession;

    /* Track all outstanding response callbacks for this device. The callbacks are
       registered when a command is sent to the device, to get notified with the results. */
    Callback::CallbackDeque mResponses;

    /* Track all outstanding callbacks for attribute reports from this device. The callbacks are
       registered when the user requests attribute reporting for device attributes. */
    Callback::CallbackDeque mReports;

    /**
     * @brief
     *   This function loads the secure session object from the serialized operational
     *   credentials corresponding to the device. This is typically done when the device
     *   does not have an active secure channel.
     *
     * @param[in] resetNeeded   Does the underlying network socket require a reset
     */
    CHIP_ERROR LoadSecureSessionParameters(ResetTransport resetNeeded);

    uint16_t mListenPort;
};

/**
 * This class defines an interface for an object that the user of Device
 * can register as a delegate. The delegate object will be called by the
 * Device when a new message or status update is received from the corresponding
 * CHIP device.
 */
class DLL_EXPORT DeviceStatusDelegate
{
public:
    virtual ~DeviceStatusDelegate() {}

    /**
     * @brief
     *   Called when a message is received from the device.
     *
     * @param[in] msg Received message buffer.
     */
    virtual void OnMessage(System::PacketBufferHandle msg) = 0;

    /**
     * @brief
     *   Called when device status is updated.
     *
     */
    virtual void OnStatusChange(void){};
};

#ifdef IFNAMSIZ
constexpr uint16_t kMaxInterfaceName = IFNAMSIZ;
#else
constexpr uint16_t kMaxInterfaceName = 32;
#endif

typedef struct SerializableDevice
{
    PASESessionSerializable mOpsCreds;
    uint64_t mDeviceId; /* This field is serialized in LittleEndian byte order */
    uint8_t mDeviceAddr[INET6_ADDRSTRLEN];
    uint16_t mDevicePort; /* This field is serealized in LittelEndian byte order */
    uint8_t mInterfaceName[kMaxInterfaceName];
} SerializableDevice;

typedef struct SerializedDevice
{
    // Extra uint64_t to account for padding bytes (NULL termination, and some decoding overheads)
    // The encoder may not include a NULL character, and there are maximum 2 bytes of padding.
    // So extra 8 bytes should be sufficient to absorb this overhead.
    uint8_t inner[BASE64_ENCODED_LEN(sizeof(SerializableDevice) + sizeof(uint64_t))];
} SerializedDevice;

} // namespace Controller
} // namespace chip
