#include "eqiva_lock.h"
#include "eqiva_key_ble.h"

namespace esphome {
namespace eqiva_key_ble {

static const char *const TAG = "eqiva_lock";

void EqivaLockEntity::setup() {
  this->traits.set_supports_open(true);

  if (this->parent_ != nullptr) {
    this->parent_->register_status_callback([this](LockStatus status, bool low_battery) {
      this->update_state_(status);
    });
  }

#ifdef USE_BINARY_SENSOR
  if (this->latch_sensor_ != nullptr) {
    this->latch_sensor_->add_on_state_callback([this](bool state) {
      if (this->parent_ != nullptr) {
        this->update_state_(this->parent_->get_current_lock_status());
      } else {
        this->update_state_(UNKNOWN);
      }
    });
  }
#endif
}

void EqivaLockEntity::dump_config() {
  LOG_LOCK(TAG, "Eqiva Lock", this);
#ifdef USE_BINARY_SENSOR
  if (this->latch_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Latch Sensor: configured (Invert: %s)", YESNO(this->invert_latch_sensor_));
  } else {
    ESP_LOGCONFIG(TAG, "  Latch Sensor: not configured");
  }
#endif
}

void EqivaLockEntity::control(const lock::LockCall &call) {
  if (!call.get_state().has_value() || this->parent_ == nullptr) return;
  auto state = *call.get_state();

  if (state == lock::LOCK_STATE_LOCKED) {
    this->publish_state(lock::LOCK_STATE_LOCKING);
    this->parent_->sendCommand(LOCK);
  } else if (state == lock::LOCK_STATE_UNLOCKED) {
    this->publish_state(lock::LOCK_STATE_UNLOCKING);
    this->parent_->sendCommand(UNLOCK);
  }
}

void EqivaLockEntity::open_latch() {
  if (this->parent_ != nullptr) {
    this->publish_state(lock::LOCK_STATE_UNLOCKING);
    this->parent_->sendCommand(OPEN);
  }
}

void EqivaLockEntity::update_state_(LockStatus status) {
  bool bolt_in_latch = false;
  bool has_latch_sensor = false;

#ifdef USE_BINARY_SENSOR
  if (this->latch_sensor_ != nullptr && this->latch_sensor_->has_state()) {
    has_latch_sensor = true;
    bolt_in_latch = this->latch_sensor_->state;
    if (this->invert_latch_sensor_) {
      bolt_in_latch = !bolt_in_latch;
    }
  }
#endif

  // 1. Ultimative Wahrheit: Wenn Riegelkontakt vorhanden und Riegel in der Falle -> LOCKED!
  if (has_latch_sensor && bolt_in_latch) {
    this->publish_state(lock::LOCK_STATE_LOCKED);
    return;
  }

  // 2. Schloss meldet MOVING
  if (status == MOVING) {
    if (this->state == lock::LOCK_STATE_UNLOCKING || this->state == lock::LOCK_STATE_LOCKING) {
      return; // Transient moving state already active, keep it
    }
    if (this->state == lock::LOCK_STATE_LOCKED) {
      this->publish_state(lock::LOCK_STATE_UNLOCKING);
    } else {
      this->publish_state(lock::LOCK_STATE_LOCKING);
    }
    return;
  }

  // 3. Schloss meldet UNKNOWN (Positionsverlust / Jammed)
  if (status == UNKNOWN) {
    this->publish_state(lock::LOCK_STATE_JAMMED);
    return;
  }

  // 4. Schloss meldet LOCKED, aber Riegelkontakt meldet NICHT in der Falle
  if (status == LOCKED) {
    if (has_latch_sensor && !bolt_in_latch) {
      // Tür steht offen, obwohl Riegel ausgefahren ist
      this->publish_state(lock::LOCK_STATE_UNLOCKED);
    } else {
      this->publish_state(lock::LOCK_STATE_LOCKED);
    }
    return;
  }

  // 5. UNLOCKED oder OPENED
  if (status == UNLOCKED || status == OPENED) {
    this->publish_state(lock::LOCK_STATE_UNLOCKED);
    return;
  }

  this->publish_state(lock::LOCK_STATE_UNLOCKED);
}

}  // namespace eqiva_key_ble
}  // namespace esphome
