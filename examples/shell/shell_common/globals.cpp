/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include <Globals.h>

#include <protocols/secure_channel/PASESession.h>
#include <stack/Stack.h>

#if INET_CONFIG_ENABLE_TCP_ENDPOINT
chip::Stack<TransportConfigurationWithTcp> gStack(chip::kTestControllerNodeId);
#else
chip::Stack<> gStack(chip::kTestControllerNodeId);
#endif
