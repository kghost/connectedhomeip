/**
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


#pragma once

#include "config.h"

#include <app/util/types_stub.h>
#include <messaging/ExchangeDelegate.h>

/**
 *  @brief
 *    The ember session which is associated with a Exchange. The lifespan of the object is same as the ExchangeContext or slight
 *    longer than the ExchangeContext. The initiator side should manually allocate the object, and bind it to a ExchangeContext
 *    using ExchangeManager::NewContext. The acceptor side should manually allocate the object when first message received in the
 *    registered UnsolicitedMessageHandler.
 */
class EmberSession : public chip::ExchangeDelegate
{
public:
    EmberSession() {}
    virtual ~EmberSession() {}

    // Not copyable or movable
    EmberSession(const EmberSession &) = delete;
    EmberSession & operator=(const EmberSession &) = delete;
    EmberSession(EmberSession &&) = delete;
    EmberSession& operator=(EmberSession &&) = delete;

    EmberBindingTableEntry & GetBinding() { return mBinding; }

    void OnMessageReceived(chip::ExchangeContext * ec, const chip::PacketHeader & packetHeader, uint32_t protocolId, uint8_t msgType, chip::System::PacketBuffer * payload) override;
    void OnResponseTimeout(chip::ExchangeContext * ec) override;
    void OnExchangeClosing(chip::ExchangeContext * ec) override;

private:
    EmberBindingTableEntry mBinding;
};

/**
 *  @brief
 *    Short live class for storing variables to be used to send or handle a message.  The object of this class will be created on
 *    the stack when start processing a message, or handle a timeout event. The object will be passed all way along the processing
 *    procedure. Almost all functions will take an argument of the context.
 */
class EmberContext
{
public:
    EmberContext(EmberSession & session, chip::ExchangeContext & context);

    // Not copyable or movable
    EmberContext(const EmberContext &) = delete;
    EmberContext & operator=(const EmberContext &) = delete;
    EmberContext(EmberContext &&) = delete;
    EmberContext& operator=(EmberContext &&) = delete;

    EmberSession & GetSession() { return mSession; }
    chip::ExchangeContext & GetExchangeContext() { return mExchangeContext; }

    EmberApsFrame mResponseApsFrame;
    uint8_t mAppResponseData[EMBER_AF_MAXIMUM_SEND_PAYLOAD_LENGTH];
    uint16_t mAppResponseLength;

private:
    EmberSession & mSession;
    chip::ExchangeContext & mExchangeContext;
};

