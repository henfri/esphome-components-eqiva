#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/lock/lock.h"
#include "eQ3_constants.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#else
namespace esphome {
namespace binary_sensor {
class BinarySensor;
}
}
#endif

namespace esphome {
namespace eqiva_key_ble {

class EqivaKeyBle;

class EqivaLockEntity : public lock::Lock, public Component {
 public:
  void set_parent(EqivaKeyBle *parent) { this->parent_ = parent; }
  void set_latch_sensor(binary_sensor::BinarySensor *latch_sensor) { this->latch_sensor_ = latch_sensor; }
  void set_invert_latch_sensor(bool invert) { this->invert_latch_sensor_ = invert; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(const lock::LockCall &call) override;
  void open_latch() override;
  void update_state_(LockStatus status);

  EqivaKeyBle *parent_{nullptr};
  binary_sensor::BinarySensor *latch_sensor_{nullptr};
  bool invert_latch_sensor_{false};
  uint32_t last_open_time_{0};
};

}  // namespace eqiva_key_ble
}  // namespace esphome
