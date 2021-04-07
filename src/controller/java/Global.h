/*
 *   Copyright (c) 2021 Project CHIP Authors
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

#include <jni.h>
#include <pthread.h>

#include <support/CodeUtils.h>

extern JavaVM * sJVM;
extern pthread_mutex_t sStackLock;

extern jclass sAndroidChipStackCls;
extern jclass sChipDeviceControllerExceptionCls;

CHIP_ERROR N2J_ByteArray(JNIEnv * env, const uint8_t * inArray, uint32_t inArrayLen, jbyteArray & outArray);
CHIP_ERROR N2J_Error(JNIEnv * env, CHIP_ERROR inErr, jthrowable & outEx);
CHIP_ERROR N2J_NewStringUTF(JNIEnv * env, const char * inStr, jstring & outString);
CHIP_ERROR GetClassRef(JNIEnv * env, const char * clsType, jclass & outCls);
void ThrowError(JNIEnv * env, CHIP_ERROR errToThrow);
