Big thanks to previous work done by:  [@MariusSchiffer](https://github.com/MariusSchiffer/esp32-keyble), [@tc-maxx](https://github.com/tc-maxx/esp32-keyble), [@lumokitho](https://github.com/lumokitho/esp32-keyble) and the original creator [@oyooyo](https://github.com/oyooyo/keyble)

# Eqiva eQ-3 Bluetooth Smart Lock for ESPHome

This ESPHome custom component allows full control of Eqiva eQ-3 BLE smart locks.

---

## 1. Production Config (Recommended: C++ Flow Control)

The built-in C++ flow control completely manages connection lifecycle, commands, and status updates directly on the ESP32. You don't need any complex YAML scripts or UI inputs.

```yaml
esphome:
  name: esphome-eqiva-lock
  friendly_name: Eqiva Lock

esp32:
  board: esp32dev
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_BOOTLOADER_WDT_TIME_MS: "60000"
      CONFIG_BT_GATTC_CACHE_NVS_FLASH: "y"

logger:

api:
  encryption:
    key: !secret api_key

ota:
  - platform: esphome
    password: !secret ota_password

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

external_components:
  - source: github://digaus/esphome-components-eqiva
    refresh: 0s

esp32_ble_tracker:
  scan_parameters:
    interval: 320ms
    window: 200ms
    active: true

# BLE discovery
eqiva_ble:

# Lock Component Configuration
eqiva_key_ble:
  - id: my_lock
    mac_address: !secret eqiva_mac_address
    user_id: !secret eqiva_user_id
    user_key: !secret eqiva_user_key
    # disconnect_timeout: 0s   # Default: 0s (Permanent connection, fastest response)
    # disconnect_timeout: 10s  # Optional: Connect-on-demand (disconnects after 10s idle)
    # status_update_interval: 2h

text_sensor:
  - platform: eqiva_key_ble
    eqiva_key_ble_id: my_lock
    lock_status:
      id: lock_state
      name: "Lock State"
    low_battery:
      name: "Low Battery"
    lock_ble_state:
      name: "Lock BLE State"

lock:
  - platform: eqiva_key_ble
    name: "Front Door Lock"
    eqiva_key_ble_id: my_lock
    # Optional: Sensor fusion with an external door contact / latch reed sensor (e.g. KNX, Zigbee)
    # latch_sensor: door_reed_sensor
    # invert_latch_sensor: true # Set to true if sensor reports OFF when bolt is in latch
```

---

## 2. Connection Modes

* **Permanent Connection (`disconnect_timeout: 0s`, Default):**
  * The ESP32 maintains a persistent BLE connection with the lock.
  * Commands execute with near-zero latency.
  * Recommended for standard setups and continuous operation.

* **Connect on Demand (`disconnect_timeout: >0s`, e.g. `10s`):**
  * The ESP32 connects only when a command is triggered or the `status_update_interval` (default `2h`) expires.
  * Disconnects automatically after `disconnect_timeout` of inactivity.

---

## 3. Performance & GATT Caching (`CONFIG_BT_GATTC_CACHE_NVS_FLASH`)

By default in ESP-IDF, GATT service discovery results are only stored in volatile RAM and lost on ESP32 reboot.

To enable instantaneous reconnects immediately after an ESP32 restart, opt-in to persistent flash caching by adding this to your `esp32:` block:

```yaml
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_BT_GATTC_CACHE_NVS_FLASH: "y"
```

---

## 4. Alternative / Flexible Setup (Dynamic UI Inputs & Pairing)

If you prefer to configure your lock credentials dynamically from Home Assistant (without hardcoding them or editing `secrets.yaml`), or need to perform initial pairing via the Web UI:

See [`example.yaml`](example.yaml) in this repository for the full configuration containing:
* Text fields for `mac_address`, `user_key`, and `card_key`
* Number field for `user_id`
* Dropdown selects for `direction` (Left/Right), `position` (Vertical/Horizontal), and `turns` (1-4)
* Action buttons for **Pair**, **Connect**, **Disconnect**, and **Apply Settings**
