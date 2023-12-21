#define LOG_TAG "Google_SE-service"
#include "SecureElement.h"
#include <aidl/android/hardware/secure_element/BnSecureElement.h>
#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    auto se = ndk::SharedRefBase::make<aidl::android::hardware::secure_element::SecureElement>();
    const std::string name = std::string() +
                             aidl::android::hardware::secure_element::BnSecureElement::descriptor +
                             "/eSE1";
    binder_status_t status = AServiceManager_addService(se->asBinder().get(), name.c_str());
    CHECK_EQ(status, STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}