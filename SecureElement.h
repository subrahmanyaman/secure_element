#pragma once
#include <aidl/android/hardware/secure_element/BnSecureElement.h>
// #include <android-base/logging.h>
#include "Transport.h"
#include <android/binder_manager.h>
#include <android/binder_process.h>

namespace aidl::android::hardware::secure_element {

using aidl::android::hardware::secure_element::BnSecureElement;
using aidl::android::hardware::secure_element::ISecureElementCallback;
using aidl::android::hardware::secure_element::LogicalChannelResponse;
using ndk::ScopedAStatus;

#ifndef MIN_APDU_LENGTH
#define MIN_APDU_LENGTH 0x04
#endif
#ifndef DEFAULT_BASIC_CHANNEL
#define DEFAULT_BASIC_CHANNEL 0x00
#endif

class SecureElement : public BnSecureElement {
  public:
    SecureElement();
    ScopedAStatus init(const std::shared_ptr<ISecureElementCallback>& client_callback) override;
    ScopedAStatus getAtr(std::vector<uint8_t>* aidl_return) override;
    ScopedAStatus reset() override;
    ScopedAStatus isCardPresent(bool* aidl_return) override;
    ScopedAStatus openBasicChannel(const std::vector<uint8_t>& aid, int8_t p2,
                                   std::vector<uint8_t>* aidl_return) override;
    ScopedAStatus openLogicalChannel(
        const std::vector<uint8_t>& aid, int8_t p2,
        ::aidl::android::hardware::secure_element::LogicalChannelResponse* aidl_return) override;
    ScopedAStatus closeChannel(int8_t channel_number) override;
    ScopedAStatus transmit(const std::vector<uint8_t>& data,
                           std::vector<uint8_t>* aidl_return) override;

  private:
    static std::vector<bool> mOpenedChannels;
    std::shared_ptr<ISecureElementCallback> mCallback;
    std::shared_ptr<SocketTransport> mSocketTransport;
};

}  // namespace aidl::android::hardware::secure_element