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
#pragma once

#include <memory>
#include <pthread.h>

#include <jni.h>

#include <controller/CHIPDeviceController.h>
#include <controller/java/AndroidBleManager.h>
#include <platform/internal/DeviceNetworkInfo.h>
#include <stack/ControllerStackImpl.h>

// Choose an approximation of PTHREAD_NULL if pthread.h doesn't define one.
#ifndef PTHREAD_NULL
#define PTHREAD_NULL 0
#endif // PTHREAD_NULL

/**
 * This class contains all relevant information for the JNI view of CHIPDeviceController
 * to handle all controller-related processing.
 *
 * Generally it contains the DeviceController class itself, plus any related delegates/callbacks.
 */
class AndroidChipControllerStackWrapper : public chip::Controller::DevicePairingDelegate,
                                       public chip::Controller::DeviceStatusDelegate,
                                       public chip::PersistentStorageDelegate
{
public:
    AndroidChipControllerStackWrapper(chip::NodeId nodeId) : mChipStack(nodeId) {};
    ~AndroidChipControllerStackWrapper();

    CHIP_ERROR Init(int listeningPort);

    chip::System::Layer & GetSystemLayer() { return mChipStack.GetSystemLayer(); }
    chip::Inet::InetLayer & GetInetLayer() { return mChipStack.GetInetLayer(); }
    chip::Ble::BleLayer * GetBleLayer() { return mChipStack.GetBleLayer(); }
    chip::Controller::DeviceCommissioner * Controller() { return &mChipStack.GetDeviceCommissioner(); }

    void SetJavaObjectRef(JavaVM * vm, jobject obj);

    void SendNetworkCredentials(const char * ssid, const char * password);
    void SendThreadCredentials(const chip::DeviceLayer::Internal::DeviceNetworkInfo & threadData);

    // DevicePairingDelegate implementation
    void OnNetworkCredentialsRequested(chip::RendezvousDeviceCredentialsDelegate * callback) override;
    void OnOperationalCredentialsRequested(const char * csr, size_t csr_length,
                                           chip::RendezvousDeviceCredentialsDelegate * callback) override;
    void OnStatusUpdate(chip::RendezvousSessionDelegate::Status status) override;
    void OnPairingComplete(CHIP_ERROR error) override;
    void OnPairingDeleted(CHIP_ERROR error) override;

    // DeviceStatusDelegate implementation
    void OnMessage(chip::System::PacketBufferHandle msg) override;
    void OnStatusChange(void) override;

    // PersistentStorageDelegate implementation
    void SetStorageDelegate(chip::PersistentStorageResultDelegate * delegate) override;
    CHIP_ERROR SyncGetKeyValue(const char * key, char * value, uint16_t & size) override;
    void AsyncSetKeyValue(const char * key, const char * value) override;
    void AsyncDeleteKeyValue(const char * key) override;

    jlong ToJNIHandle()
    {
        static_assert(sizeof(jlong) >= sizeof(void *), "Need to store a pointer in a java handle");
        return reinterpret_cast<jlong>(this);
    }

    jobject JavaObjectRef() { return mJavaObjectRef; }

    static AndroidChipControllerStackWrapper * FromJNIHandle(jlong handle)
    {
        return reinterpret_cast<AndroidChipControllerStackWrapper *>(handle);
    }

    static AndroidChipControllerStackWrapper * AllocateNew(JavaVM * vm, jobject deviceControllerObj, chip::NodeId nodeId, CHIP_ERROR * errInfoOnFailure);

private:
    chip::ControllerStackImpl<AndroidBleManager> mChipStack;
    pthread_t sIOThread        = PTHREAD_NULL;
    bool sShutdown             = false;

    chip::RendezvousDeviceCredentialsDelegate * mCredentialsDelegate = nullptr;
    chip::PersistentStorageResultDelegate * mStorageResultDelegate   = nullptr;

    JavaVM * mJavaVM       = nullptr;
    jobject mJavaObjectRef = nullptr;

    JNIEnv * GetJavaEnv();

    jclass GetPersistentStorageClass() { return GetJavaEnv()->FindClass("chip/devicecontroller/PersistentStorage"); }
};
