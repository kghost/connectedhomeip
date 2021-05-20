/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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
 *    @file
 *      This file implements unit tests for CHIP Interaction Model Engine
 *
 */

#include <app/InteractionModelEngine.h>
#include <core/CHIPCore.h>
#include <core/CHIPTLV.h>
#include <core/CHIPTLVDebug.hpp>
#include <core/CHIPTLVUtilities.hpp>
#include <messaging/ExchangeContext.h>
#include <messaging/ExchangeMgr.h>
#include <messaging/Flags.h>
#include <platform/CHIPDeviceLayer.h>
#include <protocols/secure_channel/MessageCounterManager.h>
#include <protocols/secure_channel/PASESession.h>
#include <stack/Stack.h>
#include <support/ErrorStr.h>
#include <support/UnitTestRegistration.h>
#include <system/SystemPacketBuffer.h>
#include <system/TLVPacketBufferBackingStore.h>
#include <transport/SecureSessionMgr.h>
#include <transport/raw/UDP.h>

#include <nlunit-test.h>

class NoopTransportConfig : chip::TransportConfiguration
{
public:
    using transport = chip::TransportMgr<>;

    CHIP_ERROR Init(const chip::StackParameters & parameters, chip::Inet::InetLayer & inetLayer, chip::Ble::BleLayer * bleLayer)
    {
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR Shutdown() { return CHIP_NO_ERROR; }

    chip::TransportMgrBase & Get() { return mTransportManager; }

private:
    transport mTransportManager;
};

static chip::Stack<NoopTransportConfig> gStack(chip::kTestDeviceNodeId);

namespace chip {
namespace app {
class TestInteractionModelEngine
{
public:
    static void TestClusterInfoPushRelease(nlTestSuite * apSuite, void * apContext);
    static int GetClusterInfoListLength(ClusterInfo * apClusterInfoList);
};

int TestInteractionModelEngine::GetClusterInfoListLength(ClusterInfo * apClusterInfoList)
{
    int length           = 0;
    ClusterInfo * runner = apClusterInfoList;
    while (runner != nullptr)
    {
        runner = runner->mpNext;
        length++;
    }
    return length;
}

void TestInteractionModelEngine::TestClusterInfoPushRelease(nlTestSuite * apSuite, void * apContext)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    err            = InteractionModelEngine::GetInstance()->Init(&gStack.GetExchangeManager(), nullptr);
    NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);
    ClusterInfo * clusterInfoList = nullptr;
    ClusterInfo clusterInfo1;
    ClusterInfo clusterInfo2;
    ClusterInfo clusterInfo3;

    clusterInfo1.mEndpointId = 1;
    clusterInfo2.mEndpointId = 2;
    clusterInfo3.mEndpointId = 3;

    InteractionModelEngine::GetInstance()->PushFront(clusterInfoList, clusterInfo1);
    NL_TEST_ASSERT(apSuite, clusterInfoList != nullptr && clusterInfo1.mEndpointId == clusterInfoList->mEndpointId);
    NL_TEST_ASSERT(apSuite, GetClusterInfoListLength(clusterInfoList) == 1);

    InteractionModelEngine::GetInstance()->PushFront(clusterInfoList, clusterInfo2);
    NL_TEST_ASSERT(apSuite, clusterInfoList != nullptr && clusterInfo2.mEndpointId == clusterInfoList->mEndpointId);
    NL_TEST_ASSERT(apSuite, GetClusterInfoListLength(clusterInfoList) == 2);

    InteractionModelEngine::GetInstance()->PushFront(clusterInfoList, clusterInfo3);
    NL_TEST_ASSERT(apSuite, clusterInfoList != nullptr && clusterInfo3.mEndpointId == clusterInfoList->mEndpointId);
    NL_TEST_ASSERT(apSuite, GetClusterInfoListLength(clusterInfoList) == 3);

    InteractionModelEngine::GetInstance()->ReleaseClusterInfoList(clusterInfoList);
    NL_TEST_ASSERT(apSuite, GetClusterInfoListLength(clusterInfoList) == 0);
}
} // namespace app
} // namespace chip

namespace {

// clang-format off
const nlTest sTests[] =
        {
                NL_TEST_DEF("TestClusterInfoPushRelease", chip::app::TestInteractionModelEngine::TestClusterInfoPushRelease),
                NL_TEST_SENTINEL()
        };
// clang-format on
} // namespace

int TestInteractionModelEngine()
{
    // clang-format off
    nlTestSuite theSuite =
	{
        "TestInteractionModelEngine",
        &sTests[0],
        nullptr,
        nullptr
    };
    // clang-format on

    NL_TEST_ASSERT(&theSuite, gStack.Init(chip::StackParameters()) == CHIP_NO_ERROR);
    nlTestRunner(&theSuite, nullptr);
    int result = nlTestRunnerStats(&theSuite);
    NL_TEST_ASSERT(&theSuite, gStack.Shutdown() == CHIP_NO_ERROR);

    return result;
}

CHIP_REGISTER_TEST_SUITE(TestInteractionModelEngine)
