#pragma once

#include "esphome/components/esp32_ble_client/ble_client_base.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/text_sensor/text_sensor.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/application.h"

#include <queue>
#include "eQ3_constants.h"
#include "eQ3_message.h"
#include "eQ3_util.h"
#include <esp_log.h>

#ifdef USE_ESP32


namespace esphome {
namespace eqiva_key_ble {


using namespace esp32_ble_client;

class EqivaKeyBle;

class EqivaKeyBle : public BLEClientBase {
    bool sendMessage(eQ3Message::Message *msg, bool nonce);
    void sendFragment();
    void sendNonce();
    void init();
    void finishPair();

    std::queue<eQ3Message::MessageFragment> sendQueue;
    BLECharacteristic *write{nullptr};
    BLECharacteristic *read{nullptr};
    bool sendingNonce{false};
    uint32_t sending_time_ms_{0};
    eQ3Message::Message *currentMsg{nullptr};
    bool requestPair{false};
    uint16_t cached_write_handle_{0};
    uint16_t cached_read_handle_{0};
    BLEService cached_service_{};
    BLECharacteristic cached_write_char_{};
    BLECharacteristic cached_read_char_{};
    uint32_t last_activity_time_{0};
    bool handshake_completed_{false};
    uint32_t disconnect_timeout_{0};
    uint32_t status_update_interval_{7200000};
    uint32_t last_status_update_time_{0};
    std::string pending_mac_address_{""};
    bool pending_connect_{false};
    uint32_t pending_connect_start_time_{0};
    CommandType last_command_sent_{REQUEST_STATUS};
    std::string previous_lock_state_{"UNKNOWN"};
    uint32_t max_connect_failures_{4};
    uint32_t consecutive_connect_failures_{0};
    bool connect_in_progress_{false};
    bool connection_succeeded_this_cycle_{false};
    uint32_t last_contact_time_{0};
    uint32_t last_scanner_restart_time_{0};
    uint32_t watchdog_scanner_timeout_ms_{1800000}; // 30 min, 0 = disabled
    uint32_t watchdog_reboot_timeout_ms_{3600000};  // 60 min, 0 = disabled

    std::string getClientState() {
        std::string client_state;
        switch(this->state()) {
            case espbt::ClientState::INIT: {
                client_state = "INIT";
                break;
            }
            case espbt::ClientState::DISCONNECTING: {
                client_state = "DISCONNECTING";
                break;
            }
            case espbt::ClientState::IDLE: {
                client_state = "IDLE";
                break;
            }
            case espbt::ClientState::DISCOVERED: {
                client_state = "DISCOVERED";
                break;
            }
            case espbt::ClientState::CONNECTING: {
                client_state = "CONNECTING";
                break;
            }
            case espbt::ClientState::CONNECTED: {
                client_state = "CONNECTED";
                break;
            }
            case espbt::ClientState::ESTABLISHED: {
                client_state = "ESTABLISHED";
                break;
            }
        }
        return client_state;
    }
    public:
        ClientState clientState;
        void setup() override;
#ifdef USE_ESP32_BLE_DEVICE
        bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
#endif
        void set_disconnect_timeout(uint32_t ms) {
            this->disconnect_timeout_ = ms;
            if (this->disconnect_timeout_ > 0 && this->status_update_interval_ > 0) {
                this->enable_loop();
            }
        }
        void set_status_update_interval(uint32_t ms) {
            this->status_update_interval_ = ms;
            if (this->disconnect_timeout_ > 0 && this->status_update_interval_ > 0) {
                this->enable_loop();
            }
        }
        void set_max_connect_failures(uint32_t count) {
            this->max_connect_failures_ = count;
        }
        uint32_t get_consecutive_connect_failures() const {
            return this->consecutive_connect_failures_;
        }
        void set_watchdog_scanner_timeout(uint32_t ms) {
            this->watchdog_scanner_timeout_ms_ = ms;
        }
        uint32_t get_watchdog_scanner_timeout() const {
            return this->watchdog_scanner_timeout_ms_;
        }
        void set_watchdog_reboot_timeout(uint32_t ms) {
            this->watchdog_reboot_timeout_ms_ = ms;
        }
        uint32_t get_watchdog_reboot_timeout() const {
            return this->watchdog_reboot_timeout_ms_;
        }
        void connect();
        void disconnect();
        void loop() override;
        void startPair();
        void applySettings();
        void sendCommand(CommandType command);
        void set_user_id(int user_id) {
            clientState.user_id = user_id;
        }
        void set_user_key(std::string user_key) {
            if (user_key.length() > 0) {
                clientState.user_key = hexstring_to_string(user_key);
            }
        }
        void set_card_key(std::string card_key) {
            if (card_key.length() > 0) {
                for(char &c : card_key)
                    c = tolower(c);
                clientState.card_key = card_key.substr(14, 32);
            }
        }
        void set_turn_left(bool turn_left) { clientState.turn_left = turn_left; }
        void set_key_horizontal(bool key_horizontal) { clientState.key_horizontal = key_horizontal; }
        void set_lock_turns(int lock_turns) { clientState.lock_turns = lock_turns; }


        void set_lock_ble_state_sensor(text_sensor::TextSensor *lock_ble_state_sensor) { this->lock_ble_state_sensor_ = lock_ble_state_sensor; }
        void set_low_battery_sensor(text_sensor::TextSensor *low_battery_sensor) { this->low_battery_sensor_ = low_battery_sensor; }
        void set_lock_status_sensor(text_sensor::TextSensor *lock_status_sensor) { this->lock_status_sensor_ = lock_status_sensor; }
        void set_user_key_sensor(text_sensor::TextSensor *user_key_sensor) { this->user_key_sensor_ = user_key_sensor; }
        void set_user_id_sensor(text_sensor::TextSensor *user_id_sensor) { this->user_id_sensor_ = user_id_sensor; }
        void set_mac_address_sensor(text_sensor::TextSensor *mac_address_sensor) { this->mac_address_sensor_ = mac_address_sensor; }
#ifdef USE_SENSOR
        void set_consecutive_connect_failures_sensor(sensor::Sensor *s) { this->consecutive_connect_failures_sensor_ = s; }
        void set_last_contact_duration_sensor(sensor::Sensor *s) { this->last_contact_duration_sensor_ = s; }
#endif

        void set_state(esphome::esp32_ble_tracker::ClientState st) {
            BLEClientBase::set_state(st);
            if (this->lock_ble_state_sensor_ != nullptr) {
                this->lock_ble_state_sensor_->publish_state(getClientState());
            }
        };
        void set_pending_connection(const std::string &mac) {
            this->pending_mac_address_ = mac;
            this->pending_connect_ = true;
            this->pending_connect_start_time_ = millis();
        }
        bool has_pending_connection() const { return this->pending_connect_; }
        std::string get_pending_mac_address() const { return this->pending_mac_address_; }
        void clear_pending_connection() { this->pending_connect_ = false; }
        
        uint64_t get_configured_mac_address() const { return this->configured_mac_address_; }
        void set_configured_mac_address(uint64_t mac) { this->configured_mac_address_ = mac; }
        
        void register_status_callback(std::function<void(LockStatus, bool)> &&callback) {
            this->status_callbacks_.push_back(std::move(callback));
        }
        void register_connection_state_callback(std::function<void(espbt::ClientState)> &&callback) {
            this->connection_state_callbacks_.push_back(std::move(callback));
        }
        LockStatus get_current_lock_status() const { return this->current_lock_status_; }

        void clear_bonds_and_cache(const std::string &mac);
        void dump_config() override;
        bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                esp_ble_gattc_cb_param_t *param) override;

    protected: 
        LockStatus current_lock_status_{UNKNOWN};
        std::vector<std::function<void(LockStatus, bool)>> status_callbacks_;
        std::vector<std::function<void(espbt::ClientState)>> connection_state_callbacks_;
        espbt::ClientState previous_client_state_{espbt::ClientState::INIT};
        uint64_t configured_mac_address_{0};
        text_sensor::TextSensor *lock_ble_state_sensor_{nullptr};                
        text_sensor::TextSensor *low_battery_sensor_{nullptr};
        text_sensor::TextSensor *lock_status_sensor_{nullptr};
        text_sensor::TextSensor *user_key_sensor_{nullptr};
        text_sensor::TextSensor *user_id_sensor_{nullptr};
        text_sensor::TextSensor *mac_address_sensor_{nullptr};
#ifdef USE_SENSOR
        sensor::Sensor *consecutive_connect_failures_sensor_{nullptr};
        sensor::Sensor *last_contact_duration_sensor_{nullptr};
#endif
        uint32_t last_telemetry_publish_time_{0};

};


template<typename... Ts>
class EqivaSettings : public Action<Ts...>, public Parented<EqivaKeyBle> {
    TEMPLATABLE_VALUE(bool, turn_left)
    TEMPLATABLE_VALUE(bool, key_horizontal)
    TEMPLATABLE_VALUE(int, lock_turns)
    public:
        void play(const Ts &...x) override { 
            auto turn_left = this->turn_left_.value(x...);
            auto key_horizontal = this->key_horizontal_.value(x...);
            auto lock_turns = this->lock_turns_.value(x...);
            this->parent_->set_turn_left(turn_left);
            this->parent_->set_key_horizontal(key_horizontal);
            this->parent_->set_lock_turns(lock_turns);
            this->parent_->applySettings();
        }
};

template<typename... Ts>
class EqivaConnect : public Action<Ts...>, public Parented<EqivaKeyBle> {
    TEMPLATABLE_VALUE(std::string, mac_address)
    TEMPLATABLE_VALUE(int, user_id)
    TEMPLATABLE_VALUE(std::string, user_key)
    public:
        void play(const Ts &...x) override {

            auto mac_address = this->mac_address_.value(x...);
            uint64_t target_mac = string_to_mac(mac_address);
            auto user_id = this->user_id_.value(x...);
            auto user_key = this->user_key_.value(x...);
            this->parent_->set_user_id(user_id);
            this->parent_->set_user_key(user_key);

            if (this->parent_->get_configured_mac_address() != target_mac) {
                this->parent_->clear_bonds_and_cache(mac_address);
                this->parent_->set_configured_mac_address(target_mac);
                
                auto current_state = this->parent_->state();
                if (current_state != espbt::ClientState::IDLE && current_state != espbt::ClientState::INIT) {
                    this->parent_->set_pending_connection(mac_address);
                    this->parent_->disconnect();
                } else {
                    this->parent_->set_address(target_mac);
                    this->parent_->connect();
                }
            } else {
                this->parent_->set_address(target_mac);
                this->parent_->connect();
            }
            ESP_LOGD("ESP Eqiva", " Address: %s, %s", this->parent_->address_str(), mac_address.c_str());
        }
};

template<typename... Ts>
class EqivaDisconnect : public Action<Ts...>, public Parented<EqivaKeyBle> {
    public:
        void play(const Ts &...x) {
            // this->parent_->set_user_id(255);
            // this->parent_->set_user_key("");
            this->parent_->disconnect();
        }
};

template<typename... Ts>
class EqivaPair : public Action<Ts...>, public Parented<EqivaKeyBle> {
    TEMPLATABLE_VALUE(std::string, card_key)
    TEMPLATABLE_VALUE(std::string, mac_address)
    public:
        void play(const Ts &...x) { 
            auto card_key = this->card_key_.value(x...);
            auto mac_address = this->mac_address_.value(x...);
            this->parent_->set_card_key(card_key);
            this->parent_->set_address(string_to_mac(mac_address));
            this->parent_->startPair();
        }
};

template<typename... Ts>
class EqivaLock : public Action<Ts...>, public Parented<EqivaKeyBle> {
 public:
  void play(const Ts &...x) { this->parent_->sendCommand(LOCK); }
};

template<typename... Ts>
class EqivaUnlock : public Action<Ts...>, public Parented<EqivaKeyBle> {
 public:
  void play(const Ts &...x) { this->parent_->sendCommand(UNLOCK); }
};

template<typename... Ts>
class EqivaOpen : public Action<Ts...>, public Parented<EqivaKeyBle> {
 public:
  void play(const Ts &...x) { this->parent_->sendCommand(OPEN); }
};

template<typename... Ts>
class EqivaStatus : public Action<Ts...>, public Parented<EqivaKeyBle> {
 public:
  void play(const Ts &...x) { this->parent_->sendCommand(REQUEST_STATUS); }
};




template<typename... Ts>
class EqivaSetDisconnectTimeout : public Action<Ts...>, public Parented<EqivaKeyBle> {
    TEMPLATABLE_VALUE(uint32_t, timeout)
    public:
        void play(const Ts &...x) override {
            auto timeout = this->timeout_.value(x...);
            this->parent_->set_disconnect_timeout(timeout);
            this->parent_->set_auto_connect(timeout == 0);
            ESP_LOGI("eqiva_key_ble", "Disconnect timeout updated to %" PRIu32 " ms", timeout);
        }
};

template<typename... Ts>
class EqivaSetStatusUpdateInterval : public Action<Ts...>, public Parented<EqivaKeyBle> {
    TEMPLATABLE_VALUE(uint32_t, interval)
    public:
        void play(const Ts &...x) override {
            auto interval = this->interval_.value(x...);
            this->parent_->set_status_update_interval(interval);
            ESP_LOGI("eqiva_key_ble", "Status update interval updated to %" PRIu32 " ms", interval);
        }
};

template<typename... Ts>
class EqivaSetWatchdogScannerTimeout : public Action<Ts...>, public Parented<EqivaKeyBle> {
    TEMPLATABLE_VALUE(uint32_t, timeout)
    public:
        void play(const Ts &...x) override {
            auto timeout = this->timeout_.value(x...);
            this->parent_->set_watchdog_scanner_timeout(timeout);
            ESP_LOGI("eqiva_key_ble", "Watchdog scanner timeout updated to %" PRIu32 " ms", timeout);
        }
};

template<typename... Ts>
class EqivaSetWatchdogRebootTimeout : public Action<Ts...>, public Parented<EqivaKeyBle> {
    TEMPLATABLE_VALUE(uint32_t, timeout)
    public:
        void play(const Ts &...x) override {
            auto timeout = this->timeout_.value(x...);
            this->parent_->set_watchdog_reboot_timeout(timeout);
            ESP_LOGI("eqiva_key_ble", "Watchdog reboot timeout updated to %" PRIu32 " ms", timeout);
        }
};

}  // namespace eqiva_key_ble
}  // namespace esphome

#endif
