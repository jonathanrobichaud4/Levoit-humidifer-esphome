#include "esphome/core/log.h"
#include "levoit_sensor.h"

namespace esphome {
namespace levoit {

static const char *const TAG = "levoit.sensor";

void LevoitSensor::setup() {
  this->parent_->register_listener(LevoitPayloadType::STATUS_RESPONSE, [this](uint8_t *payloadData, size_t payloadLen) {
    if (this->purpose_ == LevoitSensorPurpose::PM25) {
      if (payloadData[12] == 0xFF && payloadData[13] == 0xFF) {
        this->publish_state(NAN);
      } else {
        this->publish_state((float) ((payloadData[13] << 8) + payloadData[12]) / 1.0);
      }
    } else if (this->purpose_ == LevoitSensorPurpose::AIR_QUALITY) {
      this->publish_state(payloadData[11]);
    }
  });
}

void LevoitSensor::dump_config() { LOG_SENSOR("", "Levoit Sensor", this); }

}  // namespace levoit
}  // namespace esphome
