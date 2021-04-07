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
#include "AndroidChipControllerStackWrapper.h"
#include "CHIPJNIError.h"
#include <stack/ControllerStackImpl.h>
#include <controller/java/Global.h>

#include <memory>

using chip::PersistentStorageResultDelegate;
using chip::Controller::DeviceCommissioner;

namespace {

bool FindMethod(JNIEnv * env, jobject object, const char * methodName, const char * methodSignature, jmethodID * methodId)
{
    if ((env == nullptr) || (object == nullptr))
    {
        ChipLogError(Controller, "Missing java object for %s", methodName);
        return false;
    }

    jclass javaClass = env->GetObjectClass(object);
    if (javaClass == NULL)
    {
        ChipLogError(Controller, "Failed to get class for %s", methodName);
        return false;
    }

    *methodId = env->GetMethodID(javaClass, methodName, methodSignature);
    if (*methodId == NULL)
    {
        ChipLogError(Controller, "Failed to find method %s", methodName);
        return false;
    }

    return true;
}

void CallVoidInt(JNIEnv * env, jobject object, const char * methodName, jint argument)
{
    jmethodID method;

    if (!FindMethod(env, object, methodName, "(I)V", &method))
    {
        return;
    }

    env->ExceptionClear();
    env->CallVoidMethod(object, method, argument);
}

void * IOThreadMain(void * arg)
{
    JNIEnv * env;
    JavaVMAttachArgs attachArgs;
    struct timeval sleepTime;
    fd_set readFDs, writeFDs, exceptFDs;
    int numFDs = 0;

    AndroidChipControllerStackWrapper * wrapper = static_cast<AndroidChipControllerStackWrapper*>(arg);

    // Attach the IO thread to the JVM as a daemon thread.
    // This allows the JVM to shutdown without waiting for this thread to exit.
    attachArgs.version = JNI_VERSION_1_6;
    attachArgs.name    = (char *) "CHIP Device Controller IO Thread";
    attachArgs.group   = NULL;
#ifdef __ANDROID__
    sJVM->AttachCurrentThreadAsDaemon(&env, (void *) &attachArgs);
#else
    sJVM->AttachCurrentThreadAsDaemon((void **) &env, (void *) &attachArgs);
#endif

    // Set to true to quit the loop. This is currently unused.
    std::atomic<bool> quit;

    ChipLogProgress(Controller, "IO thread starting");

    // Lock the stack to prevent collisions with Java threads.
    pthread_mutex_lock(&sStackLock);

    // Loop until we are told to exit.
    while (!quit.load(std::memory_order_relaxed))
    {
        numFDs = 0;
        FD_ZERO(&readFDs);
        FD_ZERO(&writeFDs);
        FD_ZERO(&exceptFDs);

        sleepTime.tv_sec  = 10;
        sleepTime.tv_usec = 0;

        // Collect the currently active file descriptors.
        wrapper->GetSystemLayer().PrepareSelect(numFDs, &readFDs, &writeFDs, &exceptFDs, sleepTime);
        wrapper->GetInetLayer().PrepareSelect(numFDs, &readFDs, &writeFDs, &exceptFDs, sleepTime);

        // Unlock the stack so that Java threads can make API calls.
        pthread_mutex_unlock(&sStackLock);

        // Wait for for I/O or for the next timer to expire.
        int selectRes = select(numFDs, &readFDs, &writeFDs, &exceptFDs, &sleepTime);

        // Break the loop if requested to shutdown.
        // if (sShutdown)
        // break;

        // Re-lock the stack.
        pthread_mutex_lock(&sStackLock);

        // Perform I/O and/or dispatch timers.
        wrapper->GetSystemLayer().HandleSelectResult(selectRes, &readFDs, &writeFDs, &exceptFDs);
        wrapper->GetInetLayer().HandleSelectResult(selectRes, &readFDs, &writeFDs, &exceptFDs);
    }

    // Detach the thread from the JVM.
    sJVM->DetachCurrentThread();

    return NULL;
}

} // namespace

AndroidChipControllerStackWrapper::~AndroidChipControllerStackWrapper()
{
    // If the IO thread has been started, shut it down and wait for it to exit.
    if (sIOThread != PTHREAD_NULL)
    {
        sShutdown = true;
        mChipStack.GetSystemLayer().WakeSelect();
        pthread_join(sIOThread, NULL);
    }

    if ((mJavaVM != nullptr) && (mJavaObjectRef != nullptr))
    {
        GetJavaEnv()->DeleteGlobalRef(mJavaObjectRef);
    }
    mChipStack.Shutdown();
}

CHIP_ERROR AndroidChipControllerStackWrapper::Init(int listeningPort) {
    mChipStack.GetTransportConfig().SetListenPort(listeningPort);
    mChipStack.GetBleLayer()->mAppState = this;
    ReturnErrorOnFailure(mChipStack.Init());

    int pthreadErr = pthread_create(&sIOThread, NULL, IOThreadMain, this);
    VerifyOrReturnError(pthreadErr == 0, chip::System::MapErrorPOSIX(pthreadErr));

    return CHIP_NO_ERROR;
}

void AndroidChipControllerStackWrapper::SetJavaObjectRef(JavaVM * vm, jobject obj)
{
    mJavaVM        = vm;
    mJavaObjectRef = GetJavaEnv()->NewGlobalRef(obj);
}

JNIEnv * AndroidChipControllerStackWrapper::GetJavaEnv()
{
    if (mJavaVM == nullptr)
    {
        return nullptr;
    }

    JNIEnv * env = nullptr;
    mJavaVM->GetEnv((void **) &env, JNI_VERSION_1_6);

    return env;
}

AndroidChipControllerStackWrapper * AndroidChipControllerStackWrapper::AllocateNew(JavaVM * vm, jobject deviceControllerObj, chip::NodeId nodeId, CHIP_ERROR * errInfoOnFailure)
{
    if (errInfoOnFailure == nullptr)
    {
        ChipLogError(Controller, "Missing error info");
        return nullptr;
    }

    *errInfoOnFailure = CHIP_NO_ERROR;

    std::unique_ptr<AndroidChipControllerStackWrapper> wrapper(new AndroidChipControllerStackWrapper(nodeId));
    wrapper->Init(CHIP_PORT + 1);

    wrapper->SetJavaObjectRef(vm, deviceControllerObj);

    *errInfoOnFailure = wrapper->Controller()->ServiceEvents();

    if (*errInfoOnFailure != CHIP_NO_ERROR)
    {
        return nullptr;
    }

    return wrapper.release();
}

void AndroidChipControllerStackWrapper::SendNetworkCredentials(const char * ssid, const char * password)
{
    if (mCredentialsDelegate == nullptr)
    {
        ChipLogError(Controller, "No credential callback available to send Wi-Fi credentials.");
        return;
    }

    ChipLogProgress(Controller, "Sending network credentials for %s...", ssid);
    mCredentialsDelegate->SendNetworkCredentials(ssid, password);
}

void AndroidChipControllerStackWrapper::SendThreadCredentials(const chip::DeviceLayer::Internal::DeviceNetworkInfo & threadData)
{
    if (mCredentialsDelegate == nullptr)
    {
        ChipLogError(Controller, "No credential callback available to send Thread credentials.");
        return;
    }

    ChipLogProgress(Controller, "Sending Thread credentials for channel %u, PAN ID %x...", threadData.ThreadChannel,
                    threadData.ThreadPANId);
    mCredentialsDelegate->SendThreadCredentials(threadData);
}

void AndroidChipControllerStackWrapper::OnNetworkCredentialsRequested(chip::RendezvousDeviceCredentialsDelegate * callback)
{
    mCredentialsDelegate = callback;

    JNIEnv * env = GetJavaEnv();

    jmethodID method;
    if (!FindMethod(env, mJavaObjectRef, "onNetworkCredentialsRequested", "()V", &method))
    {
        return;
    }

    env->ExceptionClear();
    env->CallVoidMethod(mJavaObjectRef, method);
}

void AndroidChipControllerStackWrapper::OnOperationalCredentialsRequested(const char * csr, size_t csr_length,
                                                                       chip::RendezvousDeviceCredentialsDelegate * callback)
{
    mCredentialsDelegate = callback;

    JNIEnv * env = GetJavaEnv();

    jbyteArray jCsr;
    if (!N2J_ByteArray(env, reinterpret_cast<const uint8_t *>(csr), csr_length, jCsr))
    {
        ChipLogError(Controller, "Failed to build byte array for operational credential request");
        return;
    }

    jmethodID method;
    if (!FindMethod(env, mJavaObjectRef, "onOperationalCredentialsRequested", "([B)V", &method))
    {
        return;
    }

    env->ExceptionClear();
    env->CallVoidMethod(mJavaObjectRef, method, jCsr);
}

void AndroidChipControllerStackWrapper::OnStatusUpdate(chip::RendezvousSessionDelegate::Status status)
{
    CallVoidInt(GetJavaEnv(), mJavaObjectRef, "onStatusUpdate", static_cast<jint>(status));
}

void AndroidChipControllerStackWrapper::OnPairingComplete(CHIP_ERROR error)
{
    CallVoidInt(GetJavaEnv(), mJavaObjectRef, "onPairingComplete", static_cast<jint>(error));
}

void AndroidChipControllerStackWrapper::OnPairingDeleted(CHIP_ERROR error)
{
    CallVoidInt(GetJavaEnv(), mJavaObjectRef, "onPairingDeleted", static_cast<jint>(error));
}

void AndroidChipControllerStackWrapper::OnMessage(chip::System::PacketBufferHandle msg) {}

void AndroidChipControllerStackWrapper::OnStatusChange(void) {}

void AndroidChipControllerStackWrapper::SetStorageDelegate(PersistentStorageResultDelegate * delegate)
{
    mStorageResultDelegate = delegate;
}

CHIP_ERROR AndroidChipControllerStackWrapper::SyncGetKeyValue(const char * key, char * value, uint16_t & size)
{
    jstring keyString       = NULL;
    jstring valueString     = NULL;
    const char * valueChars = nullptr;
    CHIP_ERROR err          = CHIP_NO_ERROR;
    jclass storageCls       = GetPersistentStorageClass();
    jmethodID method        = GetJavaEnv()->GetStaticMethodID(storageCls, "getKeyValue", "(Ljava/lang/String;)Ljava/lang/String;");

    GetJavaEnv()->ExceptionClear();

    err = N2J_NewStringUTF(GetJavaEnv(), key, keyString);
    SuccessOrExit(err);

    valueString = (jstring) GetJavaEnv()->CallStaticObjectMethod(storageCls, method, keyString);

    if (valueString != NULL)
    {
        size_t stringLength = GetJavaEnv()->GetStringUTFLength(valueString);
        if (stringLength > UINT16_MAX - 1)
        {
            err = CHIP_ERROR_BUFFER_TOO_SMALL;
        }
        else
        {
            if (value != nullptr)
            {
                valueChars = GetJavaEnv()->GetStringUTFChars(valueString, 0);
                size       = strlcpy(value, valueChars, size);
                if (size < stringLength)
                {
                    err = CHIP_ERROR_NO_MEMORY;
                }
            }
            else
            {
                size = stringLength;
                err  = CHIP_ERROR_NO_MEMORY;
            }
            // Increment size to account for null termination
            size += 1;
        }
    }
    else
    {
        err = CHIP_ERROR_INVALID_ARGUMENT;
    }

exit:
    GetJavaEnv()->ExceptionClear();
    if (valueChars != nullptr)
    {
        GetJavaEnv()->ReleaseStringUTFChars(valueString, valueChars);
    }
    GetJavaEnv()->DeleteLocalRef(keyString);
    GetJavaEnv()->DeleteLocalRef(valueString);
    return err;
}

void AndroidChipControllerStackWrapper::AsyncSetKeyValue(const char * key, const char * value)
{
    jclass storageCls = GetPersistentStorageClass();
    jmethodID method  = GetJavaEnv()->GetStaticMethodID(storageCls, "setKeyValue", "(Ljava/lang/String;Ljava/lang/String;)V");

    GetJavaEnv()->ExceptionClear();

    jstring keyString   = NULL;
    jstring valueString = NULL;
    CHIP_ERROR err      = CHIP_NO_ERROR;

    err = N2J_NewStringUTF(GetJavaEnv(), key, keyString);
    SuccessOrExit(err);
    err = N2J_NewStringUTF(GetJavaEnv(), value, valueString);
    SuccessOrExit(err);

    GetJavaEnv()->CallStaticVoidMethod(storageCls, method, keyString, valueString);

    if (mStorageResultDelegate)
    {
        mStorageResultDelegate->OnPersistentStorageStatus(key, PersistentStorageResultDelegate::Operation::kSET, CHIP_NO_ERROR);
    }

exit:
    GetJavaEnv()->ExceptionClear();
    GetJavaEnv()->DeleteLocalRef(keyString);
    GetJavaEnv()->DeleteLocalRef(valueString);
}

void AndroidChipControllerStackWrapper::AsyncDeleteKeyValue(const char * key)
{
    jclass storageCls = GetPersistentStorageClass();
    jmethodID method  = GetJavaEnv()->GetStaticMethodID(storageCls, "deleteKeyValue", "(Ljava/lang/String;)V");

    GetJavaEnv()->ExceptionClear();

    jstring keyString = NULL;
    CHIP_ERROR err    = CHIP_NO_ERROR;

    err = N2J_NewStringUTF(GetJavaEnv(), key, keyString);
    SuccessOrExit(err);

    GetJavaEnv()->CallStaticVoidMethod(storageCls, method, keyString);

    if (mStorageResultDelegate)
    {
        mStorageResultDelegate->OnPersistentStorageStatus(key, PersistentStorageResultDelegate::Operation::kDELETE, CHIP_NO_ERROR);
    }

exit:
    GetJavaEnv()->ExceptionClear();
    GetJavaEnv()->DeleteLocalRef(keyString);
}
