#pragma once

#include "esphome/core/component.h"
#include "esphome/components/levoit/levoit.h"
#include "esphome/components/select/select.h"

#include <vector>

namespace esphome {
namespace levoit {

enum LevoitSelectPurpose: uint8_t {
  PURIFIER_FAN_MODE,
  PURIFIER_AUTO_MODE
};

class LevoitSelect : public select::Select, public Component {
 public:
  LevoitSelect(Levoit *parent, LevoitSelectPurpose purpose) : parent_(parent), purpose_(purpose) {};
  void setup() override;
  void dump_config() override;

 protected:
  void control(const std::string &value) override;

  Levoit *parent_;
  LevoitSelectPurpose purpose_;
};

}  // namespace levoit
}  // namespace esphome
