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
#include "AndroidBleManager.h"

#include <controller/java/AndroidChipControllerStackWrapper.h>
#include <controller/java/Global.h>
#include <controller/java/CHIPJNIError.h>

#if CONFIG_NETWORK_LAYER_BLE
// Use StackUnlockGuard to temporarily unlock the CHIP BLE stack, e.g. when calling application
// or Android BLE code as a result of a BLE event.
struct StackUnlockGuard
{
    StackUnlockGuard() { pthread_mutex_unlock(&sStackLock); }
    ~StackUnlockGuard() { pthread_mutex_lock(&sStackLock); }
};

void ReportError(JNIEnv * env, CHIP_ERROR cbErr, const char * functName)
{
    if (cbErr == CHIP_JNI_ERROR_EXCEPTION_THROWN)
    {
        ChipLogError(Controller, "Java exception thrown in %s", functName);
        env->ExceptionDescribe();
    }
    else
    {
        const char * errStr;
        switch (cbErr)
        {
        case CHIP_JNI_ERROR_TYPE_NOT_FOUND:
            errStr = "JNI type not found";
            break;
        case CHIP_JNI_ERROR_METHOD_NOT_FOUND:
            errStr = "JNI method not found";
            break;
        case CHIP_JNI_ERROR_FIELD_NOT_FOUND:
            errStr = "JNI field not found";
            break;
        default:
            errStr = chip::ErrorStr(cbErr);
            break;
        }
        ChipLogError(Controller, "Error in %s : %s", functName, errStr);
    }
}

CHIP_ERROR AndroidBleManager::Init(chip::ControllerStackImpl<AndroidBleManager>* stack) {
    ChipLogProgress(Controller, "BLE Layer being configured.");

    // Initialize the BleApplicationDelegate
    sBleApplicationDelegate.SetNotifyChipConnectionClosedCallback(HandleNotifyChipConnectionClosed);
    // Initialize the BlePlatformDelegate
    sBlePlatformDelegate.SetSendWriteRequestCallback(HandleSendCharacteristic);
    sBlePlatformDelegate.SetSubscribeCharacteristicCallback(HandleSubscribeCharacteristic);
    sBlePlatformDelegate.SetUnsubscribeCharacteristicCallback(HandleUnsubscribeCharacteristic);
    sBlePlatformDelegate.SetCloseConnectionCallback(HandleCloseConnection);
    sBlePlatformDelegate.SetGetMTUCallback(HandleGetMTU);
    // Initialize the BleConnectionDelegate
    sBleConnectionDelegate.SetNewConnectionCallback(HandleNewConnection);

    ChipLogProgress(Controller, "Asking for BLE Layer initialization.");
    ReturnErrorOnFailure(sBleLayer.Init(&sBlePlatformDelegate, &sBleConnectionDelegate, &sBleApplicationDelegate, &stack->GetSystemLayer()));

    ChipLogProgress(Controller, "BLE was initialized.");
    return CHIP_NO_ERROR;
}

void AndroidBleManager::HandleNotifyChipConnectionClosed(BLE_CONNECTION_OBJECT connObj)
{
    StackUnlockGuard unlockGuard;
    CHIP_ERROR err = CHIP_NO_ERROR;
    JNIEnv * env;
    jmethodID method;
    intptr_t tmpConnObj;

    ChipLogProgress(Controller, "Received NotifyChipConnectionClosed");

    sJVM->GetEnv((void **) &env, JNI_VERSION_1_6);

    method = env->GetStaticMethodID(sAndroidChipStackCls, "onNotifyChipConnectionClosed", "(I)V");
    VerifyOrExit(method != NULL, err = CHIP_JNI_ERROR_METHOD_NOT_FOUND);

    ChipLogProgress(Controller, "Calling Java NotifyChipConnectionClosed");

    env->ExceptionClear();
    tmpConnObj = reinterpret_cast<intptr_t>(connObj);
    env->CallStaticVoidMethod(sAndroidChipStackCls, method, static_cast<jint>(tmpConnObj));
    VerifyOrExit(!env->ExceptionCheck(), err = CHIP_JNI_ERROR_EXCEPTION_THROWN);

exit:
    if (err != CHIP_NO_ERROR)
    {
        ReportError(env, err, __FUNCTION__);
    }
    env->ExceptionClear();
}

bool AndroidBleManager::HandleSendCharacteristic(BLE_CONNECTION_OBJECT connObj, const uint8_t * svcId, const uint8_t * charId,
                              const uint8_t * characteristicData, uint32_t characteristicDataLen)
{
    StackUnlockGuard unlockGuard;
    CHIP_ERROR err = CHIP_NO_ERROR;
    JNIEnv * env;
    jbyteArray svcIdObj;
    jbyteArray charIdObj;
    jbyteArray characteristicDataObj;
    jmethodID method;
    intptr_t tmpConnObj;
    bool rc = false;

    ChipLogProgress(Controller, "Received SendCharacteristic");

    sJVM->GetEnv((void **) &env, JNI_VERSION_1_6);

    err = N2J_ByteArray(env, svcId, 16, svcIdObj);
    SuccessOrExit(err);

    err = N2J_ByteArray(env, charId, 16, charIdObj);
    SuccessOrExit(err);

    err = N2J_ByteArray(env, characteristicData, characteristicDataLen, characteristicDataObj);
    SuccessOrExit(err);

    method = env->GetStaticMethodID(sAndroidChipStackCls, "onSendCharacteristic", "(I[B[B[B)Z");
    VerifyOrExit(method != NULL, err = CHIP_JNI_ERROR_METHOD_NOT_FOUND);

    ChipLogProgress(Controller, "Calling Java SendCharacteristic");

    env->ExceptionClear();
    tmpConnObj = reinterpret_cast<intptr_t>(connObj);
    rc = (bool) env->CallStaticBooleanMethod(sAndroidChipStackCls, method, static_cast<jint>(tmpConnObj), svcIdObj, charIdObj,
                                             characteristicDataObj);
    VerifyOrExit(!env->ExceptionCheck(), err = CHIP_JNI_ERROR_EXCEPTION_THROWN);

exit:
    if (err != CHIP_NO_ERROR)
    {
        ReportError(env, err, __FUNCTION__);
        rc = false;
    }
    env->ExceptionClear();

    return rc;
}

bool AndroidBleManager::HandleSubscribeCharacteristic(BLE_CONNECTION_OBJECT connObj, const uint8_t * svcId, const uint8_t * charId)
{
    StackUnlockGuard unlockGuard;
    CHIP_ERROR err = CHIP_NO_ERROR;
    JNIEnv * env;
    jbyteArray svcIdObj;
    jbyteArray charIdObj;
    jmethodID method;
    intptr_t tmpConnObj;
    bool rc = false;

    ChipLogProgress(Controller, "Received SubscribeCharacteristic");

    sJVM->GetEnv((void **) &env, JNI_VERSION_1_6);

    err = N2J_ByteArray(env, svcId, 16, svcIdObj);
    SuccessOrExit(err);

    err = N2J_ByteArray(env, charId, 16, charIdObj);
    SuccessOrExit(err);

    method = env->GetStaticMethodID(sAndroidChipStackCls, "onSubscribeCharacteristic", "(I[B[B)Z");
    VerifyOrExit(method != NULL, err = CHIP_JNI_ERROR_METHOD_NOT_FOUND);

    ChipLogProgress(Controller, "Calling Java SubscribeCharacteristic");

    env->ExceptionClear();
    tmpConnObj = reinterpret_cast<intptr_t>(connObj);
    rc = (bool) env->CallStaticBooleanMethod(sAndroidChipStackCls, method, static_cast<jint>(tmpConnObj), svcIdObj, charIdObj);
    VerifyOrExit(!env->ExceptionCheck(), err = CHIP_JNI_ERROR_EXCEPTION_THROWN);

exit:
    if (err != CHIP_NO_ERROR)
    {
        ReportError(env, err, __FUNCTION__);
        rc = false;
    }
    env->ExceptionClear();

    return rc;
}

bool AndroidBleManager::HandleUnsubscribeCharacteristic(BLE_CONNECTION_OBJECT connObj, const uint8_t * svcId, const uint8_t * charId)
{
    StackUnlockGuard unlockGuard;
    CHIP_ERROR err = CHIP_NO_ERROR;
    JNIEnv * env;
    jbyteArray svcIdObj;
    jbyteArray charIdObj;
    jmethodID method;
    intptr_t tmpConnObj;
    bool rc = false;

    ChipLogProgress(Controller, "Received UnsubscribeCharacteristic");

    sJVM->GetEnv((void **) &env, JNI_VERSION_1_6);

    err = N2J_ByteArray(env, svcId, 16, svcIdObj);
    SuccessOrExit(err);

    err = N2J_ByteArray(env, charId, 16, charIdObj);
    SuccessOrExit(err);

    method = env->GetStaticMethodID(sAndroidChipStackCls, "onUnsubscribeCharacteristic", "(I[B[B)Z");
    VerifyOrExit(method != NULL, err = CHIP_JNI_ERROR_METHOD_NOT_FOUND);

    ChipLogProgress(Controller, "Calling Java UnsubscribeCharacteristic");

    env->ExceptionClear();
    tmpConnObj = reinterpret_cast<intptr_t>(connObj);
    rc = (bool) env->CallStaticBooleanMethod(sAndroidChipStackCls, method, static_cast<jint>(tmpConnObj), svcIdObj, charIdObj);
    VerifyOrExit(!env->ExceptionCheck(), err = CHIP_JNI_ERROR_EXCEPTION_THROWN);

exit:
    if (err != CHIP_NO_ERROR)
    {
        ReportError(env, err, __FUNCTION__);
        rc = false;
    }
    env->ExceptionClear();

    return rc;
}

bool AndroidBleManager::HandleCloseConnection(BLE_CONNECTION_OBJECT connObj)
{
    StackUnlockGuard unlockGuard;
    CHIP_ERROR err = CHIP_NO_ERROR;
    JNIEnv * env;
    jmethodID method;
    intptr_t tmpConnObj;
    bool rc = false;

    ChipLogProgress(Controller, "Received CloseConnection");

    sJVM->GetEnv((void **) &env, JNI_VERSION_1_6);

    method = env->GetStaticMethodID(sAndroidChipStackCls, "onCloseConnection", "(I)Z");
    VerifyOrExit(method != NULL, err = CHIP_JNI_ERROR_METHOD_NOT_FOUND);

    ChipLogProgress(Controller, "Calling Java CloseConnection");

    env->ExceptionClear();
    tmpConnObj = reinterpret_cast<intptr_t>(connObj);
    rc         = (bool) env->CallStaticBooleanMethod(sAndroidChipStackCls, method, static_cast<jint>(tmpConnObj));
    VerifyOrExit(!env->ExceptionCheck(), err = CHIP_JNI_ERROR_EXCEPTION_THROWN);

exit:
    if (err != CHIP_NO_ERROR)
    {
        ReportError(env, err, __FUNCTION__);
        rc = false;
    }
    env->ExceptionClear();
    return rc;
}

uint16_t AndroidBleManager::HandleGetMTU(BLE_CONNECTION_OBJECT connObj)
{
    StackUnlockGuard unlockGuard;
    CHIP_ERROR err = CHIP_NO_ERROR;
    JNIEnv * env;
    jmethodID method;
    intptr_t tmpConnObj;
    uint16_t mtu = 0;

    ChipLogProgress(Controller, "Received GetMTU");

    sJVM->GetEnv((void **) &env, JNI_VERSION_1_6);

    method = env->GetStaticMethodID(sAndroidChipStackCls, "onGetMTU", "(I)I");
    VerifyOrExit(method != NULL, err = CHIP_JNI_ERROR_METHOD_NOT_FOUND);

    ChipLogProgress(Controller, "Calling Java onGetMTU");

    env->ExceptionClear();
    tmpConnObj = reinterpret_cast<intptr_t>(connObj);
    mtu        = (int16_t) env->CallStaticIntMethod(sAndroidChipStackCls, method, static_cast<jint>(tmpConnObj));
    VerifyOrExit(!env->ExceptionCheck(), err = CHIP_JNI_ERROR_EXCEPTION_THROWN);

exit:
    if (err != CHIP_NO_ERROR)
    {
        ReportError(env, err, __FUNCTION__);
        mtu = 0;
    }
    env->ExceptionClear();

    return mtu;
}

void AndroidBleManager::HandleNewConnection(void * appState, const uint16_t discriminator)
{
    StackUnlockGuard unlockGuard;
    CHIP_ERROR err = CHIP_NO_ERROR;
    JNIEnv * env;
    jmethodID method;
    jclass deviceControllerCls;
    AndroidChipControllerStackWrapper * wrapper = reinterpret_cast<AndroidChipControllerStackWrapper *>(appState);
    jobject self                             = wrapper->JavaObjectRef();

    ChipLogProgress(Controller, "Received New Connection");

    sJVM->GetEnv((void **) &env, JNI_VERSION_1_6);

    deviceControllerCls = env->GetObjectClass(self);
    VerifyOrExit(deviceControllerCls != NULL, err = CHIP_JNI_ERROR_TYPE_NOT_FOUND);

    method = env->GetMethodID(deviceControllerCls, "onConnectDeviceComplete", "()V");
    VerifyOrExit(method != NULL, err = CHIP_JNI_ERROR_METHOD_NOT_FOUND);

    ChipLogProgress(Controller, "Calling Java onConnectDeviceComplete");

    env->ExceptionClear();
    env->CallVoidMethod(self, method);
    VerifyOrExit(!env->ExceptionCheck(), err = CHIP_JNI_ERROR_EXCEPTION_THROWN);

exit:
    if (err != CHIP_NO_ERROR)
    {
        ReportError(env, err, __FUNCTION__);
    }
    env->ExceptionClear();
}
#endif
