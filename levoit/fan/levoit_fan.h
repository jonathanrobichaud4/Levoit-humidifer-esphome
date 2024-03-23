#pragma once

#include "esphome/core/component.h"
#include "esphome/components/levoit/levoit.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace levoit {

class LevoitFan : public Component, public fan::Fan {
 public:
  LevoitFan(Levoit *parent) : parent_(parent) {}
  void setup() override;
  void dump_config() override;

  fan::FanTraits get_traits() override;

 protected:
  void control(const fan::FanCall &call) override;
  Levoit *parent_;

 private:
  bool power;
  uint8_t fanMode;
  uint8_t reportedManualFanSpeed;
  uint8_t currentFanSpeed;
};

}  // namespace levoit
}  // namespace esphome
