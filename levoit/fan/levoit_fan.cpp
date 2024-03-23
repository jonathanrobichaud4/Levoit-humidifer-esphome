#include "esphome/core/log.h"
#include "levoit_fan.h"

namespace esphome {
namespace levoit {

static const char *const TAG = "levoit.fan";

void LevoitFan::setup() {
  this->parent_->register_listener(LevoitPayloadType::STATUS_RESPONSE, [this](uint8_t *payloadData, size_t payloadLen) {
    switch (this->parent_->device_model_) {
      case LevoitDeviceModel::CORE_400S:
        // in the 400s, there is no way to disable the fan, and leave the device on
        // so the power state will represent the main power

        power = payloadData[4];
        fanMode = payloadData[5];
        reportedManualFanSpeed = payloadData[6];
        currentFanSpeed = payloadData[7];

        this->state = power == 0x01;
        this->speed = reportedManualFanSpeed;

        break;

      default:
        // original behavior for 300s; fan switch is being repurposed
        // to represent auto mode on/off

        power = payloadData[4] == 0x01;
        fanMode = payloadData[5];
        reportedManualFanSpeed = payloadData[6];
        currentFanSpeed = payloadData[9];

        this->state = (fanMode == 0x00);
        this->speed = reportedManualFanSpeed;
    }

    this->publish_state();
  });
}

void LevoitFan::dump_config() { LOG_FAN("", "Levoit Fan", this); }

fan::FanTraits LevoitFan::get_traits() {
  switch (this->parent_->device_model_) {
    case LevoitDeviceModel::CORE_400S:
      // 400s has 4 speeds
      return fan::FanTraits(false, true, false, 4);
    default:
      // 300s has 3 speeds
      return fan::FanTraits(false, true, false, 3);
  }
}

void LevoitFan::control(const fan::FanCall &call) {
  bool newPowerState = this->state;

  if (call.get_state().has_value()) {
    newPowerState = *call.get_state();

    switch (this->parent_->device_model_) {
      case LevoitDeviceModel::CORE_400S:
        // fan switch controls main power state
        this->parent_->send_command(LevoitCommand{.payloadType = LevoitPayloadType::SET_POWER_STATE,
                                                  .packetType = LevoitPacketType::SEND_MESSAGE,
                                                  .payload = {0x00, newPowerState}});
        break;
      default:
        // fan switch controls auto mode
        if (newPowerState == true) {
          this->parent_->send_command(LevoitCommand{.payloadType = LevoitPayloadType::SET_FAN_MODE,
                                                    .packetType = LevoitPacketType::SEND_MESSAGE,
                                                    .payload = {0x00, 0x00}});
        } else {
          this->parent_->send_command(LevoitCommand{.payloadType = LevoitPayloadType::SET_FAN_MODE,
                                                    .packetType = LevoitPacketType::SEND_MESSAGE,
                                                    .payload = {0x00, 0x02}});
        }
    }
  }

  if (call.get_speed().has_value()) {
    uint8_t targetSpeed = *call.get_speed();

    // 400s-specific behavior
    if (this->parent_->device_model_ == LevoitDeviceModel::CORE_400S) {
      // if fan is off, we don't set speed
      if (newPowerState == false) {
        return;
      }

      if (targetSpeed == 0) {
        // fan speed can report as 0-speed (auto mode), but setting to 0-speed results in error
        // set to 1 instead
        targetSpeed = 1;
      }
    }

    this->parent_->send_command(LevoitCommand{.payloadType = LevoitPayloadType::SET_FAN_MANUAL,
                                              .packetType = LevoitPacketType::SEND_MESSAGE,
                                              .payload = {0x00, 0x00, 0x01, targetSpeed}});
  }
}

}  // namespace levoit
}  // namespace esphome
