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

/**
 *    @file
 *      This file defines the classes corresponding to CHIP Exchange Context Delegate.
 *
 */

#pragma once

#include <messaging/ApplicationExchangeDispatch.h>
#include <messaging/ExchangeMessageDispatch.h>
#include <support/CHIPMem.h>
#include <system/SystemPacketBuffer.h>
#include <transport/SecureSessionMgr.h>
#include <transport/raw/MessageHeader.h>

namespace chip {
namespace Messaging {

class ExchangeContext;

/**
 * @brief
 *   This class provides a skeleton for the callback functions. The functions will be
 *   called by ExchangeContext object on specific events. If the user of ExchangeContext
 *   is interested in receiving these callbacks, they can specialize this class and handle
 *   each trigger in their implementation of this class.
 */
class DLL_EXPORT ExchangeDelegateBase
{
public:
    virtual ~ExchangeDelegateBase() {}

    /**
     * @brief
     *   This function is the protocol callback for handling a received CHIP message.
     *
     *  @param[in]    ec            A pointer to the ExchangeContext object.
     *  @param[in]    packetHeader  A reference to the PacketHeader object.
     *  @param[in]    payloadHeader A reference to the PayloadHeader object.
     *  @param[in]    payload       A handle to the PacketBuffer object holding the message payload.
     */
    virtual void OnMessageReceived(ExchangeContext * ec, const PacketHeader & packetHeader, const PayloadHeader & payloadHeader,
                                   System::PacketBufferHandle && payload) = 0;

    /**
     * @brief
     *   This function is the protocol callback to invoke when the timeout for the receipt
     *   of a response message has expired.
     *
     *  @param[in]    ec            A pointer to the ExchangeContext object.
     */
    virtual void OnResponseTimeout(ExchangeContext * ec) = 0;

    /**
     * @brief
     *   This function is the protocol callback to invoke when the associated
     *   exchange context is being closed
     *
     *  @param[in]    ec            A pointer to the ExchangeContext object.
     */
    virtual void OnExchangeClosing(ExchangeContext * ec) {}

    virtual ExchangeMessageDispatch * GetMessageDispatch(ReliableMessageMgr * rmMgr, SecureSessionMgr * sessionMgr) = 0;
};

class DLL_EXPORT ExchangeDelegate : public ExchangeDelegateBase
{
public:
    virtual ~ExchangeDelegate() {}

    virtual ExchangeMessageDispatch * GetMessageDispatch(ReliableMessageMgr * rmMgr, SecureSessionMgr * sessionMgr)
    {
        mMessageDispatch.Init(rmMgr, sessionMgr);
        return &mMessageDispatch;
    }

private:
    ApplicationExchangeDispatch mMessageDispatch;
};

} // namespace Messaging
} // namespace chip
