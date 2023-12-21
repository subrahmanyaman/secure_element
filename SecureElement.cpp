#define LOG_TAG "Google_SE-service:SecureElement"

#include <aidl/android/hardware/secure_element/BnSecureElement.h>
#include <android-base/hex.h>
#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "SecureElement.h"
#include "Transport.h"

#include <android-base/stringprintf.h>

namespace aidl::android::hardware::secure_element {
using ::android::base::HexString;
#define UNUSED(expr)                                                                               \
    do {                                                                                           \
        (void)(expr);                                                                              \
    } while (0)

SecureElement::SecureElement() {
    mSocketTransport = std::make_shared<SocketTransport>();
}

ScopedAStatus SecureElement::init(const std::shared_ptr<ISecureElementCallback>& client_callback) {
    LOG(INFO) << __func__ << " callback: " << client_callback.get();
    if (client_callback == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }
    mCallback = client_callback;
    mCallback->onStateChange(true, "init");
    return ScopedAStatus::ok();
}

ScopedAStatus SecureElement::getAtr(std::vector<uint8_t>* aidl_return) {
    LOG(INFO) << __func__;
    if (mCallback == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    std::vector<uint8_t> const atr{};
    *aidl_return = atr;
    return ScopedAStatus::ok();
}
ScopedAStatus SecureElement::isCardPresent(bool* aidl_return) {
    LOG(INFO) << __func__;
    if (mCallback == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    *aidl_return = true;
    return ScopedAStatus::ok();
}

ScopedAStatus SecureElement::reset() {
    LOG(INFO) << __func__;
    if (mCallback == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    mCallback->onStateChange(false, "reset");
    mCallback->onStateChange(true, "reset");
    return ScopedAStatus::ok();
}

ScopedAStatus SecureElement::transmit(const std::vector<uint8_t>& data,
                                      std::vector<uint8_t>* aidl_return) {
    LOG(INFO) << __func__ << " data: " << HexString(data.data(), data.size()) << " (" << data.size()
              << ")";
    if (mCallback == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    std::vector<uint8_t> output;
    if (!mSocketTransport->sendData(data, output)) {
        return ScopedAStatus::fromServiceSpecificError(IOERROR);
    }
    *aidl_return = output;
    return ScopedAStatus::ok();
}

ScopedAStatus SecureElement::openLogicalChannel(
    const std::vector<uint8_t>& aid, int8_t p2,
    ::aidl::android::hardware::secure_element::LogicalChannelResponse* aidl_return) {
    LOG(INFO) << __func__ << " aid: " << ::android::base::HexString(aid.data(), aid.size()) << " ("
              << aid.size() << ") p2 " << p2;
    if (mCallback == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    std::vector<uint8_t> resApduBuff;
    std::vector<uint8_t> manageChannelCommand = {0x00, 0x70, 0x00, 0x00, 0x01};
    uint16_t outsize;

    LOG(INFO) << "Start openLogicalChannel";

    if (!mSocketTransport->isConnected()) {
        if (!mSocketTransport->openConnection()) {
            LOG(ERROR) << "error while open Correction";
            return ScopedAStatus::fromServiceSpecificError(IOERROR);
        }
    }

    LOG(INFO) << "Socket is Connected, sending manage channel command";
    // send manage command (optional) but will need in FiRa multi-channel implementation
    if (!mSocketTransport->sendData(manageChannelCommand, resApduBuff)) {
        return ScopedAStatus::fromServiceSpecificError(IOERROR);
    }
    outsize = resApduBuff.size();
    if (!(resApduBuff[outsize - 2] == 0x90 && resApduBuff[outsize - 1] == 0x00)) {
        return ScopedAStatus::fromServiceSpecificError(
            IOERROR);  // TODO: return should be as per Status
    }

    std::vector<uint8_t> selectCmd;
    if ((resApduBuff[0] > 0x03) && (resApduBuff[0] < 0x14)) {
        /* update CLA byte according to GP spec Table 11-12*/
        selectCmd.push_back(0x40 + (resApduBuff[0] - 4)); /* Class of instruction */
    } else if ((resApduBuff[0] > 0x00) && (resApduBuff[0] < 0x04)) {
        /* update CLA byte according to GP spec Table 11-11*/
        selectCmd.push_back((uint8_t)resApduBuff[0]); /* Class of instruction */
    } else {
        LOG(ERROR) << "Invalid Channel " << resApduBuff[0];
        resApduBuff[0] = 0xff;
        return ScopedAStatus::fromServiceSpecificError(IOERROR);
    }
    int8_t channelNumber = resApduBuff[0];
    LOG(INFO) << "manage channel command is done";

    // send select command
    LOG(INFO) << "sending Select command";
    selectCmd.push_back((uint8_t)0xA4);        /* Instruction code */
    selectCmd.push_back((uint8_t)0x04);        /* Instruction parameter 1 */
    selectCmd.push_back(p2);                   /* Instruction parameter 2 */
    selectCmd.push_back((uint8_t)aid.size());  // should be fine as AID is always less than 128
    selectCmd.insert(selectCmd.end(), aid.begin(), aid.end());
    selectCmd.push_back((uint8_t)256);

    resApduBuff.clear();
    if (!mSocketTransport->sendData(selectCmd, resApduBuff)) {
        return ScopedAStatus::fromServiceSpecificError(IOERROR);
    }
    outsize = resApduBuff.size();

    if (!((resApduBuff[outsize - 2] == 0x90 && resApduBuff[outsize - 1] == 0x00) ||
          (resApduBuff[outsize - 2] == 0x62) || (resApduBuff[outsize - 2] == 0x63))) {
        // sendData response failed
        if (outsize > 0 && (resApduBuff[outsize - 2] == 0x64 && resApduBuff[outsize - 1] == 0xFF)) {
            return ScopedAStatus::fromServiceSpecificError(IOERROR);
        } else {
            return ScopedAStatus::fromServiceSpecificError(FAILED);
        }
    }

    *aidl_return = LogicalChannelResponse{
        .channelNumber = channelNumber,
        .selectResponse = resApduBuff,
    };
    return ScopedAStatus::ok();
}

ScopedAStatus SecureElement::openBasicChannel(const std::vector<uint8_t>& aid, int8_t p2,
                                              std::vector<uint8_t>* aidl_return) {
    // send select command
    LOG(INFO) << "sending Select command";
    std::vector<uint8_t> selectCmd;
    std::vector<uint8_t> resApduBuff;
    uint16_t outsize;

    selectCmd.push_back((uint8_t)0x00);        /* CLA - Basic Channel 0 */
    selectCmd.push_back((uint8_t)0xA4);        /* Instruction code */
    selectCmd.push_back((uint8_t)0xA4);        /* Instruction code */
    selectCmd.push_back((uint8_t)0x04);        /* Instruction parameter 1 */
    selectCmd.push_back(p2);                   /* Instruction parameter 2 */
    selectCmd.push_back((uint8_t)aid.size());  // should be fine as AID is always less than 128
    selectCmd.insert(selectCmd.end(), aid.begin(), aid.end());
    selectCmd.push_back((uint8_t)256);

    if (!mSocketTransport->sendData(selectCmd, resApduBuff)) {
        return ScopedAStatus::fromServiceSpecificError(IOERROR);
    }
    outsize = resApduBuff.size();

    if (!((resApduBuff[outsize - 2] == 0x90 && resApduBuff[outsize - 1] == 0x00) ||
          (resApduBuff[outsize - 2] == 0x62) || (resApduBuff[outsize - 2] == 0x63))) {
        // sendData response failed
        if (outsize > 0 && (resApduBuff[outsize - 2] == 0x64 && resApduBuff[outsize - 1] == 0xFF)) {
            return ScopedAStatus::fromServiceSpecificError(IOERROR);
        } else {
            return ScopedAStatus::fromServiceSpecificError(FAILED);
        }
    }

    *aidl_return = resApduBuff;
    return ScopedAStatus::ok();
}

ScopedAStatus SecureElement::closeChannel(int8_t channelNumber) {
    LOG(INFO) << __func__ << " channel number: " << static_cast<int>(channelNumber);
    if (mCallback == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    LOG(INFO) << "internalCloseChannel";
    std::vector<uint8_t> manageChannelCommand = {0x00, 0x70, 0x80, 0x00, 0x00};
    std::vector<uint8_t> resApduBuff;

    // change class of instruction & p2 parameter
    manageChannelCommand[0] = channelNumber;
    // For Supplementary Channel update CLA byte according to GP
    if ((channelNumber > 0x03) && (channelNumber < 0x14)) {
        /* update CLA byte according to GP spec Table 11-12*/
        manageChannelCommand[0] = 0x40 + (channelNumber - 4);
    }
    manageChannelCommand[3] = channelNumber; /* Instruction parameter 2 */

    if (!mSocketTransport->sendData(manageChannelCommand, resApduBuff)) {
        return ScopedAStatus::fromServiceSpecificError(IOERROR);
    }
    if (!(resApduBuff[0] == 0x90 && resApduBuff[1] == 0x00)) {
        LOG(ERROR) << "internalCloseChannel failed";
        return ScopedAStatus::fromServiceSpecificError(FAILED);
    }

    return ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::secure_element
