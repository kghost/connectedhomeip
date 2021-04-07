/*
 *   Copyright (c) 2020 Project CHIP Authors
 *   All rights reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

#include "ReportingCommand.h"

#include "../common/Commands.h"
#include "gen/CHIPClientCallbacks.h"
#include <controller/CHIPClusters.h>

using namespace ::chip;

namespace {
constexpr uint16_t kWaitDurationInSeconds = UINT16_MAX;
} // namespace

CHIP_ERROR ReportingCommand::Run(chip::ControllerStack * stack, NodeId remoteId)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    chip::Controller::BasicCluster cluster;

    err = stack->GetDeviceCommissioner().ServiceEvents();
    VerifyOrExit(err == CHIP_NO_ERROR, ChipLogError(Controller, "Init failure! Run Loop: %s", ErrorStr(err)));

    err = stack->GetDeviceCommissioner().GetDevice(remoteId, &mDevice);
    VerifyOrExit(err == CHIP_NO_ERROR, ChipLogError(chipTool, "Init failure! No pairing for device: %" PRIu64, stack->GetLocalNodeId()));

    mDevice->SetDelegate(this);

    AddReportCallbacks(mEndPointId);

    cluster.Associate(mDevice, mEndPointId);
    err = cluster.MfgSpecificPing(nullptr, nullptr);
    VerifyOrExit(err == CHIP_NO_ERROR, ChipLogError(Controller, "Init failure! Ping failure: %s", ErrorStr(err)));

    UpdateWaitForResponse(true);
    WaitForResponse(kWaitDurationInSeconds);

exit:
    stack->GetDeviceCommissioner().ServiceEventSignal();
    return err;
}

void ReportingCommand::OnMessage(PacketBufferHandle buffer)
{
    ChipLogDetail(chipTool, "%" PRIu64 ": Received %zu bytes", mDevice->GetDeviceId(), buffer->DataLength());
    HandleDataModelMessage(mDevice->GetDeviceId(), std::move(buffer));
}

void ReportingCommand::OnStatusChange(void)
{
    ChipLogDetail(chipTool, "DeviceStatusDelegate::OnStatusChange");
}
