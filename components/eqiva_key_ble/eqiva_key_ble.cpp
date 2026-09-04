#include "eqiva_key_ble.h"
#include "esphome/core/log.h"
#include <cinttypes>
#include "eQ3_util.h"
#include "eQ3_message.h"
#include <sstream>
#include "esp_random.h"
#include "esphome/components/esp32_ble_client/ble_characteristic.h"
#include "esphome/components/esp32_ble_client/ble_service.h"

#ifdef USE_ESP32
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

namespace esphome {
namespace eqiva_key_ble {


static const char *const TAG = "eqiva_key_ble";

void EqivaKeyBle::dump_config() {
  ESP_LOGCONFIG(TAG, "Eqiva Key-BLE:");
  ESP_LOGCONFIG(TAG, "  Address: %s", this->address_str());
  ESP_LOGCONFIG(TAG, "  UserKey: %s", clientState.user_key.length() > 0 ? string_to_hex(clientState.user_key).c_str() : "");
  ESP_LOGCONFIG(TAG, "  UserId: %d", clientState.user_id);
  ESP_LOGCONFIG(TAG, "  CardKey: %s", clientState.card_key.c_str());

}

bool EqivaKeyBle::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t esp_gattc_if,
                                    esp_ble_gattc_cb_param_t *param) {
  if (this->mac_address_sensor_ != nullptr) {
    this->mac_address_sensor_->publish_state(this->address_str());
  }

  // Bypassing MTU negotiation for Eqiva Lock on CONNECT event
  if (event == ESP_GATTC_CONNECT_EVT) {
    if (this->check_addr(param->connect.remote_bda)) {
      this->conn_id_ = param->connect.conn_id;
      this->set_state(espbt::ClientState::CONNECTING);
      ESP_LOGD(TAG, "Bypassing MTU request for Eqiva Lock on CONNECT event.");
      return true; // Prevents BLEClientBase from sending MTU request
    }
  }

  // Intercept ESP_GATTC_OPEN_EVT to read GATT services and characteristics from local cache
  if (event == ESP_GATTC_OPEN_EVT) {
    if (this->check_addr(param->open.remote_bda)) {
      if (this->cached_write_handle_ != 0 && this->cached_read_handle_ != 0 && 
          (param->open.status == ESP_GATT_OK || param->open.status == ESP_GATT_ALREADY_OPEN)) {
        ESP_LOGI(TAG, "Found Eqiva Lock characteristics in class cache (Write: 0x%04x, Read: 0x%04x). Bypassing OTA service search!", 
                 this->cached_write_handle_, this->cached_read_handle_);
        
        // Cache hit! Update our conn_id_ and state to match a successful connection open
        this->conn_id_ = param->open.conn_id;
        this->set_state(espbt::ClientState::CONNECTED);

        this->cached_service_.client = this;

        this->cached_write_char_.handle = this->cached_write_handle_;
        this->cached_write_char_.uuid = esp32_ble_tracker::ESPBTUUID::from_raw("3141dd40-15db-11e6-a24b-0002a5d5c51b");
        this->cached_write_char_.service = &this->cached_service_;

        this->cached_read_char_.handle = this->cached_read_handle_;
        this->cached_read_char_.uuid = esp32_ble_tracker::ESPBTUUID::from_raw("359d4820-15db-11e6-82bd-0002a5d5c51b");
        this->cached_read_char_.service = &this->cached_service_;

        this->write = &this->cached_write_char_;
        this->read = &this->cached_read_char_;

        esp_err_t errRc = ::esp_ble_gattc_register_for_notify(
            this->gattc_if_,
            this->remote_bda_,
            this->read->handle
        );

        if (errRc != ESP_OK) {
          ESP_LOGW(TAG, "GATT notify registration failed (err=0x%x). Invalidating cached handles, falling back to full discovery.", errRc);
          this->cached_write_handle_ = 0;
          this->cached_read_handle_ = 0;
          this->write = nullptr;
          this->read = nullptr;
        } else {
          this->set_state(espbt::ClientState::ESTABLISHED);
          clientState.remote_session_nonce.clear();
          clientState.local_session_nonce.clear();

          init();
          if (currentMsg == NULL && requestPair == false && clientState.user_key.length() > 0 && clientState.user_id < 255) {
            auto * msg = new eQ3Message::StatusRequestMessage;
            sendMessage(msg, false);
          }
          return true; // Bypasses BLEClientBase::gattc_event_handler for OPEN_EVT on cache hit
        }
      }

      if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) {
        ESP_LOGW(TAG, "Connection open failed (status=%d).", param->open.status);
        return false;
      }

      ESP_LOGD(TAG, "Connection open. Querying local GATT cache for Eqiva Lock...");

      // 1. Query Service UUID "58e06900-15d8-11e6-b737-0002a5d5c51b"
      esp_bt_uuid_t svc_uuid;
      svc_uuid.len = ESP_UUID_LEN_128;
      uint8_t svc_uuid_bytes[16] = {0x1b, 0xc5, 0xd5, 0xa5, 0x02, 0x00, 0x37, 0xb7, 0xe6, 0x11, 0xd8, 0x15, 0x00, 0x69, 0xe0, 0x58};
      memcpy(svc_uuid.uuid.uuid128, svc_uuid_bytes, 16);

      uint16_t count = 0;
      esp_gatt_status_t status = esp_ble_gattc_get_service(
          this->gattc_if_,
          param->open.conn_id,
          &svc_uuid,
          nullptr,
          &count,
          0
      );

      uint16_t write_handle = 0;
      uint16_t read_handle = 0;

      if (status == ESP_GATT_OK && count > 0) {
        esp_gattc_service_elem_t *result = (esp_gattc_service_elem_t *) malloc(sizeof(esp_gattc_service_elem_t) * count);
        if (result != nullptr) {
          status = esp_ble_gattc_get_service(
              this->gattc_if_,
              param->open.conn_id,
              &svc_uuid,
              result,
              &count,
              0
          );
          if (status == ESP_GATT_OK) {
            uint16_t start_handle = result[0].start_handle;
            uint16_t end_handle = result[0].end_handle;

            // 2. Query Write Characteristic UUID "3141dd40-15db-11e6-a24b-0002a5d5c51b"
            esp_bt_uuid_t write_uuid;
            write_uuid.len = ESP_UUID_LEN_128;
            uint8_t write_uuid_bytes[16] = {0x1b, 0xc5, 0xd5, 0xa5, 0x02, 0x00, 0x4b, 0xa2, 0xe6, 0x11, 0xdb, 0x15, 0x40, 0xdd, 0x41, 0x31};
            memcpy(write_uuid.uuid.uuid128, write_uuid_bytes, 16);

            uint16_t char_count = 0;
            status = esp_ble_gattc_get_char_by_uuid(
                this->gattc_if_,
                param->open.conn_id,
                start_handle,
                end_handle,
                write_uuid,
                nullptr,
                &char_count
            );
            if (status == ESP_GATT_OK && char_count > 0) {
              esp_gattc_char_elem_t *char_result = (esp_gattc_char_elem_t *) malloc(sizeof(esp_gattc_char_elem_t) * char_count);
              if (char_result != nullptr) {
                status = esp_ble_gattc_get_char_by_uuid(
                    this->gattc_if_,
                    param->open.conn_id,
                    start_handle,
                    end_handle,
                    write_uuid,
                    char_result,
                    &char_count
                );
                if (status == ESP_GATT_OK) {
                  write_handle = char_result[0].char_handle;
                }
                free(char_result);
              }
            }

            // 3. Query Read Characteristic UUID "359d4820-15db-11e6-82bd-0002a5d5c51b"
            esp_bt_uuid_t read_uuid;
            read_uuid.len = ESP_UUID_LEN_128;
            uint8_t read_uuid_bytes[16] = {0x1b, 0xc5, 0xd5, 0xa5, 0x02, 0x00, 0xbd, 0x82, 0xe6, 0x11, 0xdb, 0x15, 0x20, 0x48, 0x9d, 0x35};
            memcpy(read_uuid.uuid.uuid128, read_uuid_bytes, 16);

            char_count = 0;
            status = esp_ble_gattc_get_char_by_uuid(
                this->gattc_if_,
                param->open.conn_id,
                start_handle,
                end_handle,
                read_uuid,
                nullptr,
                &char_count
            );
            if (status == ESP_GATT_OK && char_count > 0) {
              esp_gattc_char_elem_t *char_result = (esp_gattc_char_elem_t *) malloc(sizeof(esp_gattc_char_elem_t) * char_count);
              if (char_result != nullptr) {
                status = esp_ble_gattc_get_char_by_uuid(
                    this->gattc_if_,
                    param->open.conn_id,
                    start_handle,
                    end_handle,
                    read_uuid,
                    char_result,
                    &char_count
                );
                if (status == ESP_GATT_OK) {
                  read_handle = char_result[0].char_handle;
                }
                free(char_result);
              }
            }
          }
          free(result);
        }
      }

      if (write_handle != 0 && read_handle != 0) {
        ESP_LOGI(TAG, "Found Eqiva Lock characteristics in local cache (Write: 0x%04x, Read: 0x%04x). Bypassing OTA service search!", write_handle, read_handle);
        
        // Cache hit! Update our conn_id_ and state to match a successful connection open
        this->conn_id_ = param->open.conn_id;
        this->set_state(espbt::ClientState::CONNECTED);

        this->cached_service_.client = this;

        this->cached_write_char_.handle = write_handle;
        this->cached_write_char_.uuid = esp32_ble_tracker::ESPBTUUID::from_raw("3141dd40-15db-11e6-a24b-0002a5d5c51b");
        this->cached_write_char_.service = &this->cached_service_;

        this->cached_read_char_.handle = read_handle;
        this->cached_read_char_.uuid = esp32_ble_tracker::ESPBTUUID::from_raw("359d4820-15db-11e6-82bd-0002a5d5c51b");
        this->cached_read_char_.service = &this->cached_service_;

        this->cached_write_handle_ = write_handle;
        this->cached_read_handle_ = read_handle;

        this->write = &this->cached_write_char_;
        this->read = &this->cached_read_char_;

        esp_err_t errRc = ::esp_ble_gattc_register_for_notify(
            this->gattc_if_,
            this->remote_bda_,
            this->read->handle
        );

        if (errRc != ESP_OK) {
          ESP_LOGW(TAG, "GATT notify registration failed (err=0x%x). Invalidating cached handles, falling back to full discovery.", errRc);
          this->cached_write_handle_ = 0;
          this->cached_read_handle_ = 0;
          this->write = nullptr;
          this->read = nullptr;
        } else {
          this->set_state(espbt::ClientState::ESTABLISHED);
          clientState.remote_session_nonce.clear();
          clientState.local_session_nonce.clear();

          init();
          if (currentMsg == NULL && requestPair == false && clientState.user_key.length() > 0 && clientState.user_id < 255) {
            auto * msg = new eQ3Message::StatusRequestMessage;
            sendMessage(msg, false);
          }
          return true; // Bypasses BLEClientBase::gattc_event_handler for OPEN_EVT on cache hit
        }
      } else {
        ESP_LOGD(TAG, "Eqiva Lock characteristics not found in local cache. Running standard service search...");
        
        // Debug local cache services
        uint16_t all_count = 0;
        esp_gatt_status_t all_status = esp_ble_gattc_get_service(
            this->gattc_if_,
            param->open.conn_id,
            nullptr,
            nullptr,
            &all_count,
            0
        );
        ESP_LOGD(TAG, "GATT Cache all services count: %d, status: %d", all_count, all_status);
        if (all_status == ESP_GATT_OK && all_count > 0) {
          esp_gattc_service_elem_t *all_result = (esp_gattc_service_elem_t *) malloc(sizeof(esp_gattc_service_elem_t) * all_count);
          if (all_result != nullptr) {
            esp_ble_gattc_get_service(
                this->gattc_if_,
                param->open.conn_id,
                nullptr,
                all_result,
                &all_count,
                0
            );
            for (int i = 0; i < all_count; i++) {
              auto uuid = esp32_ble_tracker::ESPBTUUID::from_uuid(all_result[i].uuid);
              char uuid_str[esp32_ble_tracker::UUID_STR_LEN];
              uuid.to_str(uuid_str);
              ESP_LOGD(TAG, "  Cached service [%d]: %s (start: 0x%04x, end: 0x%04x)", 
                       i, uuid_str, all_result[i].start_handle, all_result[i].end_handle);
            }
            free(all_result);
          }
        }
      }
    }
  }

  if (!BLEClientBase::gattc_event_handler(event, esp_gattc_if, param))
    return false;

  if (this->lock_ble_state_sensor_ != nullptr) {
    this->lock_ble_state_sensor_->publish_state(getClientState());
  }

  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (this->state() == espbt::ClientState::ESTABLISHED) {
        clientState.remote_session_nonce.clear();
        clientState.local_session_nonce.clear();
        write = this->get_characteristic(esp32_ble_tracker::ESPBTUUID::from_raw("58e06900-15d8-11e6-b737-0002a5d5c51b"), esp32_ble_tracker::ESPBTUUID::from_raw("3141dd40-15db-11e6-a24b-0002a5d5c51b"));
        read = this->get_characteristic(esp32_ble_tracker::ESPBTUUID::from_raw("58e06900-15d8-11e6-b737-0002a5d5c51b"), esp32_ble_tracker::ESPBTUUID::from_raw("359d4820-15db-11e6-82bd-0002a5d5c51b"));
        
        if (write != nullptr && read != nullptr) {
          this->cached_write_handle_ = write->handle;
          this->cached_read_handle_ = read->handle;
          ESP_LOGI(TAG, "Cached handles from discovery: Write: 0x%04x, Read: 0x%04x", this->cached_write_handle_, this->cached_read_handle_);

          esp_err_t errRc = ::esp_ble_gattc_register_for_notify(
            this->gattc_if_,
            this->remote_bda_,
            read->handle
          );

          if (errRc != ESP_OK) {
            ESP_LOGW(TAG, "GATT notify registration failed during service discovery (err=0x%x)", errRc);
            this->cached_write_handle_ = 0;
            this->cached_read_handle_ = 0;
            this->write = nullptr;
            this->read = nullptr;
            this->disconnect();
            break;
          }

          init();
          if (currentMsg == NULL && requestPair == false && clientState.user_key.length() > 0 && clientState.user_id < 255) {
            auto * msg = new eQ3Message::StatusRequestMessage;
            sendMessage(msg, false);
          }
        } else {
          ESP_LOGE(TAG, "Failed to find write or read characteristic during service discovery!");
        }
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
    case ESP_GATTC_CLOSE_EVT: {
      ESP_LOGD(TAG, "ESP_GATTC_DISCONNECT_EVT / ESP_GATTC_CLOSE_EVT");
      while (!this->sendQueue.empty()) {
        this->sendQueue.pop();
      }
      this->sending_time_ms_ = 0;
      this->sendingNonce = false;
      this->clientState.remote_session_nonce.clear();
      this->clientState.local_session_nonce.clear();
#ifdef USE_ESP32
      if (!this->handshake_completed_ && this->cached_write_handle_ != 0) {
        ESP_LOGW(TAG, "Connection lost before handshake completed. Invalidating GATT cache handles!");
        this->cached_write_handle_ = 0;
        this->cached_read_handle_ = 0;
      }
#endif
      this->write = nullptr;
      this->read = nullptr;
      if (this->pending_connect_ && this->state() == espbt::ClientState::IDLE) {
        std::string pending_mac = this->pending_mac_address_;
        this->pending_connect_ = false;
        ESP_LOGI(TAG, "Executing deferred connection to MAC: %s", pending_mac.c_str());
        this->set_address(string_to_mac(pending_mac));
        this->handshake_completed_ = false;
        this->connect();
      }
      break;
    }
    case ESP_GATTC_SEARCH_RES_EVT: {
      ESP_LOGD(TAG, "ESP_GATTC_SEARCH_RES_EVT");
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      ESP_LOGD(TAG, "ESP_GATTC_WRITE_CHAR_EVT");
      uint32_t now = millis();
      ESP_LOGI(TAG, "Send successfull: %u | %u | %u", this->sending_time_ms_, now, now - this->sending_time_ms_);
      this->sending_time_ms_ = 0;
      sendFragment();
      break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT: {
      ESP_LOGD(TAG, "ESP_GATTC_WRITE_DESCR_EVT");
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT");
      this->sending_time_ms_ = 0;
      if (param != NULL) {

        eQ3Message::MessageFragment frag;

        frag.data = std::string((char *) param->notify.value, param->notify.value_len);
        auto msgtype = frag.getType();
        if (frag.isLast()) {
          ESP_LOGD(TAG, "LAST");
        } else {
          ESP_LOGD(TAG, "NOT_LAST, TODO");
          /*eQ3Message::FragmentAckMessage ack(frag.getStatusByte());
          ESP_LOGD(TAG, "Send message: %s ", string_to_hex(ack.data).c_str());
          auto *write = this->get_characteristic(esp32_ble_tracker::ESPBTUUID::from_raw("58e06900-15d8-11e6-b737-0002a5d5c51b"), esp32_ble_tracker::ESPBTUUID::from_raw("3141dd40-15db-11e6-a24b-0002a5d5c51b"));
          write->write_value((uint8_t *) (ack.data.c_str()), 16, ESP_GATT_WRITE_TYPE_RSP);*/
        }
        std::stringstream ss;
        ss << frag.getData();
        std::string msgdata = ss.str();
        if (eQ3Message::Message::isTypeSecure(msgtype)) {
          if (msgdata.length() < 7) {
            ESP_LOGW(TAG, "Malformed secure message: length %zu < 7", msgdata.length());
            return true;
          }
          auto msg_security_counter = static_cast<uint16_t>(static_cast<uint8_t>(msgdata[msgdata.length() - 6]));
          msg_security_counter <<= 8;
          msg_security_counter += static_cast<uint8_t>(msgdata[msgdata.length() - 5]);
          if (msg_security_counter <= clientState.remote_security_counter) {
              ESP_LOGD(TAG,"Remote security counter missmatch");
              return true;
          }
          clientState.remote_security_counter = msg_security_counter;
          string msg_auth_value = msgdata.substr(msgdata.length() - 4, 4);
          ESP_LOGD(TAG, "# Auth value: ");
          ESP_LOGD(TAG, string_to_hex(msg_auth_value).c_str());
          //std::string decrypted = crypt_data(msgdata.substr(0, msgdata.length() - 6), msgtype,
          std::string decrypted = crypt_data(msgdata.substr(1, msgdata.length() - 7), msgtype, clientState.local_session_nonce, clientState.remote_security_counter, clientState.user_key);
          ESP_LOGD(TAG, "# Crypted data: ");
          ESP_LOGD(TAG, string_to_hex(msgdata.substr(1, msgdata.length() - 7)).c_str());
          std::string computed_auth_value = compute_auth_value(decrypted, msgtype, clientState.local_session_nonce, clientState.remote_security_counter, clientState.user_key);
          if (msg_auth_value != computed_auth_value) {
              ESP_LOGE(TAG,"# Auth value mismatch");
              clientState.remote_session_nonce.clear();
              clientState.remote_security_counter = 0;
              return true;
          }
          if(clientState.card_key.length() > 0 && clientState.user_id != 255) {
            ESP_LOGI(TAG, "Pairing successfull, please copy user_key: %s and user_id: %d into your yaml", string_to_hex(clientState.user_key).c_str(), clientState.user_id);
            clientState.card_key.clear();
          }
          msgdata = decrypted;
          ESP_LOGD(TAG, "# Decrypted: ");
          ESP_LOGD(TAG, string_to_hex(msgdata).c_str());

        }
        switch (msgtype) {
          case 0: {
              ESP_LOGD(TAG, "Case 0");
              break;
          }

          case 0x81: // answer with security
              // TODO call callback to user that pairing succeeded
              ESP_LOGD(TAG, "Case 0x81");
              break;

          case 0x01: // answer without security
              // TODO report error
              ESP_LOGD(TAG, "Case 0x01");
              break;
  
          case 0x05: {
              ESP_LOGD(TAG, "Case 0x05");
              auto * message = new eQ3Message::StatusRequestMessage;
              sendMessage(message, false);
              break;
          }

          case 0x03: {
              // Nonce success
              ESP_LOGD(TAG, "Case 0x03");
              if (msgdata.length() < 10) {
                ESP_LOGW(TAG, "Connection info message too short (len: %zu)", msgdata.length());
                break;
              }
              eQ3Message::Connection_Info_Message message;
              message.data = msgdata;
              clientState.user_id = message.getUserId();
              clientState.remote_session_nonce = message.getRemoteSessionNonce();
              clientState.local_security_counter = 1;
              clientState.remote_security_counter = 0;
              if (clientState.remote_session_nonce.length() == 8) {
                ESP_LOGD(TAG,"Nonce exchanged: %s",  string_to_hex(clientState.remote_session_nonce).c_str());
                ESP_LOGD(TAG,"Remote user_id: %d",  clientState.user_id);

              } else {
                ESP_LOGE(TAG,"error Nonce exchanged: %s",  string_to_hex(clientState.remote_session_nonce).c_str());
              }

              int user_id = message.getUserId();
              if (this->user_key_sensor_ != nullptr) {
                this->user_key_sensor_->publish_state(string_to_hex(clientState.user_key).c_str());
              }
              if (this->user_id_sensor_ != nullptr) {
                this->user_id_sensor_->publish_state(std::to_string(user_id));
              }
  
              this->handshake_completed_ = true;
              this->last_activity_time_ = millis();

              sendingNonce = false;
              if (currentMsg != NULL) {
                if (sendMessage(currentMsg, false)) {
                  currentMsg = NULL;
                }
              } else if (requestPair) {
                finishPair();
                requestPair = false;
              }
              break;
          }

          case 0x83: {
              ESP_LOGD(TAG, "Case 0x83");
              if (msgdata.length() < 3) {
                ESP_LOGW(TAG, "Status info message too short (len: %zu)", msgdata.length());
                break;
              }
              // status info
              eQ3Message::Status_Info_Message message;
              message.data = msgdata;
              std::string lockStatus;
              switch(message.getLockStatus()) {
                case 0: {
                  lockStatus = "UNKNOWN";
                  this->previous_lock_state_ = lockStatus;
                  break;
                }
                case 1: {
                  if (this->last_command_sent_ == LOCK) {
                    lockStatus = "LOCKING";
                  } else if (this->last_command_sent_ == UNLOCK || this->last_command_sent_ == OPEN) {
                    lockStatus = "UNLOCKING";
                  } else {
                    if (this->previous_lock_state_ == "LOCKED") {
                      lockStatus = "UNLOCKING";
                    } else if (this->previous_lock_state_ == "UNLOCKED" || this->previous_lock_state_ == "OPENED") {
                      lockStatus = "LOCKING";
                    } else {
                      lockStatus = "MOVING";
                    }
                  }
                  break;
                }
                case 2: {
                  lockStatus = "UNLOCKED";
                  this->previous_lock_state_ = lockStatus;
                  break;
                }
                case 3: {
                  lockStatus = "LOCKED";
                  this->previous_lock_state_ = lockStatus;
                  break;
                }
                case 4: {
                  lockStatus = "OPENED";
                  this->previous_lock_state_ = lockStatus;
                  break;
                }
              }
              if (lockStatus == "LOCKED" || lockStatus == "UNLOCKED" || lockStatus == "OPENED") {
                this->last_command_sent_ = REQUEST_STATUS;
              }
              this->last_status_update_time_ = millis();
              this->current_lock_status_ = (LockStatus) message.getLockStatus();
              for (auto &cb : this->status_callbacks_) {
                cb(this->current_lock_status_, message.isBatteryLow());
              }
              if (this->lock_status_sensor_ != nullptr) {
                this->lock_status_sensor_->publish_state(lockStatus);
              }
              if (this->low_battery_sensor_ != nullptr) {
                this->low_battery_sensor_->publish_state(message.isBatteryLow() ? "true" : "false");
              }

              ESP_LOGD(TAG, "# Lock state: %d", message.getLockStatus());
              ESP_LOGD(TAG, "# Battery low: %s", message.isBatteryLow() ? "true" : "false");
              break;
          }

          default: { // user info
              ESP_LOGD(TAG, "Case default");
              break;
          }
        }
        sendFragment();
      }
      break;
    }
    default: {
      ESP_LOGD(TAG, "OTHER EVENT %d", static_cast<int>(event));
      break;
    }
  }
  return true;
}
void EqivaKeyBle::init() {
    if(clientState.user_key.length() == 16) {
      sendNonce();
    } else {
      ESP_LOGE(TAG, "User Error: (Key: %s, ID:  %d)", clientState.user_key.c_str(), clientState.user_id);
    }
}
void EqivaKeyBle::sendCommand(CommandType command) {
  this->last_command_sent_ = command;
  if (command == REQUEST_STATUS) {
      auto * msg = new eQ3Message::StatusRequestMessage;
      sendMessage(msg, false);
  } else {
      auto msg = new eQ3Message::CommandMessage((char) command);
      sendMessage(msg, false);
  }
}
void EqivaKeyBle::applySettings() {
    auto * msg = new eQ3Message::Mount_Options_Set_Message;
    sendMessage(msg, false);
}
void EqivaKeyBle::startPair() {
    if (clientState.card_key.length() > 0) {
      clientState.user_id = 255;
      clientState.user_key.clear();
      clientState.remote_session_nonce.clear();
      const char charset[] = "0123456789abcdefghijklmnopqrstuv";
      auto randchar = [&charset]() -> char {
          return charset[esp_random() & 0x1F];
      };
      std::string str(16, 0);
      std::generate_n(str.begin(), 16, randchar);
      clientState.user_key = str;
      ESP_LOGI(TAG, "CardKey: %s", clientState.card_key.c_str());
      ESP_LOGI(TAG, "Please press and hold open button for 5 seconds to enter pairing mode");
      ESP_LOGI(TAG, "Trying to pair...");
      this->requestPair = true;
      if (this->state() == espbt::ClientState::ESTABLISHED) {
        init();
      } else {
        this->connect();
      }
    } else {
      ESP_LOGI(TAG, "Card key missing!");
    }

}

void EqivaKeyBle::finishPair() {
    if (sendingNonce == false && clientState.remote_session_nonce.length() > 0) {
      auto *message = new eQ3Message::PairingRequestMessage();
      message->data.append(1, clientState.user_id);   
      std::string cardKey = hexstring_to_string(clientState.card_key);
      std::string encrypted_pair_key = crypt_data(clientState.user_key, 0x04, clientState.remote_session_nonce, clientState.local_security_counter, cardKey);
      if (encrypted_pair_key.length() < 22)
          encrypted_pair_key.append(22 - encrypted_pair_key.length(), 0);
      message->data.append(encrypted_pair_key);

      // counter
      message->data.append(1, (char) (clientState.local_security_counter >> 8));
      message->data.append(1, (char) (clientState.local_security_counter));

      // auth value
      std::string  extra;
      extra.append(1, clientState.user_id);
      extra.append(clientState.user_key);
      if (extra.length() < 23)
          extra.append(23 - extra.length(), 0);
      std::string auth_value = compute_auth_value(extra, 0x04, clientState.remote_session_nonce, clientState.local_security_counter, cardKey);
      message->data.append(auth_value);
      sendMessage(message, false);
    } else {
      requestPair = true;
    }
  }

void EqivaKeyBle::sendNonce() {
    sendingNonce = true;
    clientState.local_session_nonce.clear();
    for (int i = 0; i < 8; i++)
        clientState.local_session_nonce.append(1,esp_random());
  
    auto *noncemsg = new eQ3Message::Connection_Request_Message;
    sendMessage(noncemsg, true);
}

bool EqivaKeyBle::sendMessage(eQ3Message::Message *msg, bool nonce) {
    this->last_activity_time_ = millis();
    if (((sendingNonce == false && clientState.remote_session_nonce.length() > 0) || nonce) && this->state() == espbt::ClientState::ESTABLISHED) {
      ESP_LOGD(TAG, "Start send message");
      std::string data;
      if (msg->isSecure()) {
          std::string padded_data;
          padded_data.append(msg->encode(&clientState));
          int pad_to = generic_ceil(padded_data.length(), 15, 8);
          if (pad_to > padded_data.length())
              padded_data.append(pad_to - padded_data.length(), 0);
          // crypt_data(padded_data, msg->id, clientState.remote_session_nonce, clientState.local_security_counter, clientState.user_key);
          data.append(1, msg->id);
          data.append(crypt_data(padded_data, msg->id, clientState.remote_session_nonce, clientState.local_security_counter, clientState.user_key));
          data.append(1, (char) (clientState.local_security_counter >> 8));
          data.append(1, (char) clientState.local_security_counter);
          data.append(compute_auth_value(padded_data, msg->id, clientState.remote_session_nonce, clientState.local_security_counter, clientState.user_key));
          clientState.local_security_counter++;
      } else {
          data.append(1, msg->id);
          data.append(msg->encode(&clientState));
      }
    
      // fragment
      int chunks = data.length() / 15;
      if (data.length() % 15 > 0)
          chunks += 1;

      ESP_LOGD(TAG, "Start create fragments");
      for (int i = 0; i < chunks; i++) {
          eQ3Message::MessageFragment frag;
          frag.data.append(1, (i ? 0 : 0x80) + (chunks - 1 - i)); // fragment status byte
          frag.data.append(data.substr(i * 15, 15));
          if (frag.data.length() < 16)
              frag.data.append(16 - (frag.data.length() % 16), 0);  // padding
          sendQueue.push(frag);
          sendFragment();
      }
      delete msg;
      return true;
    } else {
      ESP_LOGI(TAG, "Retaining message...");
      uint32_t now = millis();
      auto retain_msg = [this](eQ3Message::Message *new_msg) {
        if (this->currentMsg != nullptr && this->currentMsg != new_msg) {
          delete this->currentMsg;
        }
        this->currentMsg = new_msg;
      };
      // ESP_LOGE(TAG, "Millis: %u | %u", this->sending_time_ms_, now);
      if (this->sending_time_ms_ > 0 && (now - this->sending_time_ms_ > 3000)) {
        this->sending_time_ms_ = 0;
        if (sendingNonce) {
          ESP_LOGI(TAG, "Nonce timeout, sending again...");
          sendNonce();
          retain_msg(msg);
        } else {
          ESP_LOGI(TAG, "Message timeout, sending again...");
          retain_msg(msg);
          if (sendMessage(this->currentMsg, false)) {
            this->currentMsg = nullptr;
          }
        }
      } else {
        if (sendingNonce) {
          ESP_LOGI(TAG, "Reason: exchanging nonce");
        }
        if (clientState.remote_session_nonce.length() == 0) {
          ESP_LOGI(TAG, "Reason: no remote session");
        }
        if (this->state() != espbt::ClientState::ESTABLISHED) {
          ESP_LOGI(TAG, "Reason: lock not connected");
        }
        retain_msg(msg);
      }
      if (this->state() == espbt::ClientState::IDLE) {
        if (this->address_ == 0 || this->address_ == 1) {
          ESP_LOGE(TAG, "Cannot connect: No valid MAC address configured! Please connect first.");
          if (this->currentMsg != nullptr) {
            delete this->currentMsg;
            this->currentMsg = nullptr;
          }
        } else {
          ESP_LOGI(TAG, "Triggering connection to send message.");
          this->connect();
        }
      }
      return false;
    }
}

void EqivaKeyBle::sendFragment() {
    uint32_t now = millis();
    ESP_LOGD(TAG, "Check send frag: %s, %s", sendQueue.empty() ? "empty" : "not-empty", this->sending_time_ms_ > 0 ? "sending" : "not-sending");
    if (sendQueue.empty() || (this->sending_time_ms_ > 0 && (now - this->sending_time_ms_ <= 3000)) || this->state() != espbt::ClientState::ESTABLISHED) {
      return;
    }
      
    this->sending_time_ms_ = now;
    std::string data = sendQueue.front().data;
    sendQueue.pop();
    ESP_LOGI(TAG, "Sending %zu bytes", data.size());
    write->write_value((uint8_t *) (data.c_str()), 16, ESP_GATT_WRITE_TYPE_RSP);
}



void EqivaKeyBle::setup() {
  BLEClientBase::setup();
  this->set_auto_connect(this->disconnect_timeout_ == 0);
  this->last_status_update_time_ = millis();
  this->last_contact_time_ = millis();
#ifdef USE_SENSOR
  if (this->consecutive_connect_failures_sensor_ != nullptr) {
    this->consecutive_connect_failures_sensor_->publish_state(0);
  }
  if (this->last_contact_duration_sensor_ != nullptr) {
    this->last_contact_duration_sensor_->publish_state(0);
  }
#endif
}

#ifdef USE_ESP32_BLE_DEVICE
bool EqivaKeyBle::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() == this->address_) {
    this->remote_addr_type_ = device.get_address_type();
    this->last_contact_time_ = millis();
  }
  return BLEClientBase::parse_device(device);
}
#endif

void EqivaKeyBle::connect() {
  this->connect_in_progress_ = true;
  this->connection_succeeded_this_cycle_ = false;
  this->handshake_completed_ = false;
  this->last_activity_time_ = millis();
  BLEClientBase::connect();
}

void EqivaKeyBle::disconnect() {
  BLEClientBase::disconnect();
}


void EqivaKeyBle::clear_bonds_and_cache(const std::string &mac_str) {
  uint64_t mac = string_to_mac(mac_str);
  if (mac == 0) return;
  
  // Reset in-memory cached GATT handles — they belong to the previous lock
  // and would cause the OPEN_EVT handler to skip service discovery
  this->cached_write_handle_ = 0;
  this->cached_read_handle_ = 0;
  this->consecutive_connect_failures_ = 0;
  this->last_contact_time_ = millis();
  ESP_LOGI(TAG, "Invalidated cached GATT handles and reset watchdog for MAC swap");

  
  esp_bd_addr_t bda;
  bda[0] = (uint8_t) (mac >> 40);
  bda[1] = (uint8_t) (mac >> 32);
  bda[2] = (uint8_t) (mac >> 24);
  bda[3] = (uint8_t) (mac >> 16);
  bda[4] = (uint8_t) (mac >> 8);
  bda[5] = (uint8_t) (mac);

  esp_err_t err = esp_ble_gattc_cache_clean(bda);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_cache_clean failed for %s, err=%d", mac_str.c_str(), err);
  } else {
    ESP_LOGI(TAG, "Cleaned GATTC cache for MAC: %s", mac_str.c_str());
  }

  err = esp_ble_remove_bond_device(bda);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_remove_bond_device failed for %s, err=%d", mac_str.c_str(), err);
  } else {
    ESP_LOGI(TAG, "Removed BLE bonding keys for MAC: %s", mac_str.c_str());
  }

  uint64_t current_mac = this->address_;
  if (current_mac != 0 && current_mac != mac) {
    esp_bd_addr_t old_bda;
    old_bda[0] = (uint8_t) (current_mac >> 40);
    old_bda[1] = (uint8_t) (current_mac >> 32);
    old_bda[2] = (uint8_t) (current_mac >> 24);
    old_bda[3] = (uint8_t) (current_mac >> 16);
    old_bda[4] = (uint8_t) (current_mac >> 8);
    old_bda[5] = (uint8_t) (current_mac);
    
    esp_ble_gattc_cache_clean(old_bda);
    esp_ble_remove_bond_device(old_bda);
  }
}

void EqivaKeyBle::loop() {
  // BLEClientBase::loop() handles critical state transitions:
  // - INIT → IDLE (registers GATT app via esp_ble_gattc_app_register)
  // - DISCONNECTING timeout → forces IDLE if CLOSE_EVT never arrives
  BLEClientBase::loop();

  if (this->pending_connect_ && (millis() - this->pending_connect_start_time_ > 15000)) {
    ESP_LOGW(TAG, "pending_connect_ timed out after 15s (DISCONNECTING stuck). Clearing flag — next HA trigger will initiate a fresh connect.");
    this->pending_connect_ = false;
  }

  espbt::ClientState current_state = this->state();
  if (current_state != this->previous_client_state_) {
    this->previous_client_state_ = current_state;
    for (auto &cb : this->connection_state_callbacks_) {
      cb(current_state);
    }
    if (current_state == espbt::ClientState::ESTABLISHED) {
      this->connection_succeeded_this_cycle_ = true;
      this->last_contact_time_ = millis();
      if (this->consecutive_connect_failures_ > 0) {
        ESP_LOGI(TAG, "BLE connection established! Resetting failure counter (was %u)", this->consecutive_connect_failures_);
        this->consecutive_connect_failures_ = 0;
#ifdef USE_SENSOR
        if (this->consecutive_connect_failures_sensor_ != nullptr) {
          this->consecutive_connect_failures_sensor_->publish_state(0);
        }
#endif
      }
    }
    if (current_state == espbt::ClientState::IDLE) {
      if (this->currentMsg != nullptr) {
        ESP_LOGD(TAG, "Cleaning up undelivered message on transition to IDLE");
        delete this->currentMsg;
        this->currentMsg = nullptr;
      }
      while (!this->sendQueue.empty()) {
        this->sendQueue.pop();
      }
      this->sending_time_ms_ = 0;
      this->sendingNonce = false;

      if (this->connect_in_progress_) {
        this->connect_in_progress_ = false;
        if (!this->connection_succeeded_this_cycle_) {
          this->consecutive_connect_failures_++;
          ESP_LOGW(TAG, "BLE connection attempt failed! Consecutive failures: %u",
                   this->consecutive_connect_failures_);
#ifdef USE_SENSOR
          if (this->consecutive_connect_failures_sensor_ != nullptr) {
            this->consecutive_connect_failures_sensor_->publish_state(this->consecutive_connect_failures_);
          }
#endif
        }
      }
    }
  }

  uint32_t now = millis();
#ifdef USE_SENSOR
  if (now - this->last_telemetry_publish_time_ >= 30000) {
    this->last_telemetry_publish_time_ = now;
    if (this->last_contact_duration_sensor_ != nullptr) {
      float dur_s = (now - this->last_contact_time_) / 1000.0f;
      this->last_contact_duration_sensor_->publish_state(dur_s);
    }
  }
#endif
  if (this->state() == espbt::ClientState::IDLE && this->address_ != 0 && this->address_ != 1) {
    uint32_t time_since_contact = now - this->last_contact_time_;

    // Level 2: Auto Reboot if silence exceeds configured timeout (0 = disabled)
    if (this->watchdog_reboot_timeout_ms_ > 0 && time_since_contact > this->watchdog_reboot_timeout_ms_) {
      ESP_LOGE(TAG, "Watchdog Level 2: No contact from lock for %u min (limit: %u min)! Rebooting ESP32 to recover radio...",
               time_since_contact / 60000, this->watchdog_reboot_timeout_ms_ / 60000);
      this->last_contact_time_ = now;
      App.safe_reboot();
    }
    // Level 1: Scanner Reset if silence exceeds configured timeout (0 = disabled)
    else if (this->watchdog_scanner_timeout_ms_ > 0 && time_since_contact > this->watchdog_scanner_timeout_ms_) {
      if (now - this->last_scanner_restart_time_ > 600000) { // throttle scanner restart to once every 10 min
        this->last_scanner_restart_time_ = now;
        if (esp32_ble_tracker::global_esp32_ble_tracker != nullptr) {
          ESP_LOGW(TAG, "Watchdog Level 1: No contact from lock for %u min (limit: %u min). Restarting BLE scanner to clear GAP state...",
                   time_since_contact / 60000, this->watchdog_scanner_timeout_ms_ / 60000);
          esp32_ble_tracker::global_esp32_ble_tracker->stop_scan();
          esp32_ble_tracker::global_esp32_ble_tracker->set_scan_continuous(true);
          esp32_ble_tracker::global_esp32_ble_tracker->start_scan();
        }
      }
    }
  }

  if (this->state() == espbt::ClientState::ESTABLISHED) {
    if (this->disconnect_timeout_ > 0) {
      if (now - this->last_activity_time_ > this->disconnect_timeout_) {
        ESP_LOGI(TAG, "Connection idle for %u ms. Disconnecting to save battery.", this->disconnect_timeout_);
        this->disconnect();
      }
    } else {
      // Continuous connection mode: refresh status periodically using configured interval (default 15 min / 900000 ms)
      uint32_t interval = this->status_update_interval_ > 0 ? this->status_update_interval_ : 900000;
      if (now - this->last_status_update_time_ > interval) {
        ESP_LOGI(TAG, "Continuous connection status update interval reached (%u ms). Requesting status...", interval);
        this->last_status_update_time_ = now;
        this->sendCommand(REQUEST_STATUS);
      }
    }
  } else if (this->state() == espbt::ClientState::IDLE) {
    if (this->disconnect_timeout_ > 0 && this->status_update_interval_ > 0) {
      // BLEClientBase::loop() disables the component loop when in IDLE.
      // Re-enable it so our periodic polling timer continues to run!
      this->enable_loop();
      if (now - this->last_status_update_time_ > this->status_update_interval_) {
        ESP_LOGI(TAG, "Periodic status update interval reached. Connecting to retrieve status...");
        this->last_status_update_time_ = now;
        this->sendCommand(REQUEST_STATUS);
      }
    }
  }
}

}  // namespace eqiva_key_ble
}  // namespace esphome

#endif

