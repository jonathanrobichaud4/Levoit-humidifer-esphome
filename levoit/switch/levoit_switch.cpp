#include "esphome/core/log.h"
#include "levoit_switch.h"

namespace esphome {
namespace levoit {

static const char *const TAG = "levoit.switch";

void LevoitSwitch::setup() {
  this->parent_->register_listener(LevoitPayloadType::STATUS_RESPONSE, [this](uint8_t *buf, size_t len) {
    if (this->purpose_ == DISPLAY_LOCK) {
      bool currentDisplayLockState = buf[14];
      this->publish_state(currentDisplayLockState);
    }
    if (this->purpose_ == MASTER_POWER) {
      bool currentMasterPowerState = buf[4];
      this->publish_state(currentMasterPowerState);
    }
    if (this->purpose_ == DISPLAY_ON) {
      bool currentDisplayPowerState;
      switch (this->parent_->device_model_) {
        case LevoitDeviceModel::CORE_400S:
          // also could be 8, which is 0 when off and 64 when on
          currentDisplayPowerState = buf[9] != 0x00;
          break;
        default:
          currentDisplayPowerState = buf[7] != 0x00;
      }
      this->publish_state(currentDisplayPowerState);
    }
  });
}

void LevoitSwitch::write_state(bool state) {
  ESP_LOGV(TAG, "Setting switch purposecode %u: %s", (uint8_t) this->purpose_, ONOFF(state));
  if (this->purpose_ == DISPLAY_LOCK) {
    this->parent_->send_command(LevoitCommand{.payloadType = LevoitPayloadType::SET_DISPLAY_LOCK,
                                              .packetType = LevoitPacketType::SEND_MESSAGE,
                                              .payload = {0x00, state}});
  }
  if (this->purpose_ == MASTER_POWER) {
    this->parent_->send_command(LevoitCommand{.payloadType = LevoitPayloadType::SET_POWER_STATE,
                                              .packetType = LevoitPacketType::SEND_MESSAGE,
                                              .payload = {0x00, state}});
  }
  if (this->purpose_ == DISPLAY_ON) {
    this->parent_->send_command(LevoitCommand{.payloadType = LevoitPayloadType::SET_SCREEN_BRIGHTNESS,
                                              .packetType = LevoitPacketType::SEND_MESSAGE,
                                              .payload = {0x00, state == true ? (uint8_t) 0x64 : (uint8_t) 0x00}});
  }
}

void LevoitSwitch::dump_config() {
  LOG_SWITCH("", "Levoit Switch", this);
  ESP_LOGCONFIG(TAG, "  Switch purposecode: %u", (uint8_t) this->purpose_);
}

}  // namespace levoit
}  // namespace esphome
