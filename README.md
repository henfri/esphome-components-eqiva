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
    active: false  # Passive scanning prevents WiFi coexistence collisions and scanner lockups

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
    # watchdog_scanner_timeout: 30min # Default: 30min. Restarts GAP scanner if no packets seen (0 = disabled)
    # watchdog_reboot_timeout: 60min  # Default: 60min. Reboots ESP32 if lock remains unreachable (0 = disabled)

sensor:
  - platform: eqiva_key_ble
    eqiva_key_ble_id: my_lock
    last_contact_duration:
      name: "BLE Last Contact Duration"
    consecutive_connect_failures:
      name: "BLE Connect Failures"

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

## 4. Self-Healing Watchdogs & Fault Recovery

Single-radio ESP32 boards sharing 2.4 GHz WiFi and Bluetooth (coexistence) can occasionally experience BLE GAP scanner lockups under heavy radio traffic or prolonged runtime. This component features an automatic 2-stage recovery watchdog:

* **Staged Liveness Tracking:** Tracks contact with the lock across both passive BLE advertisements (`parse_device`) and active GATT connections (`ClientState::ESTABLISHED`).
* **Stage 1 – Scanner Reset (`watchdog_scanner_timeout`, Default: 30 min):**
  If no advertisement or connection has succeeded for the configured duration, the component transparently stops and restarts the background BLE continuous scan (`esp32_ble_tracker`). This revives stalled Bluedroid GAP scanners without resetting the ESP32 or dropping WiFi.
* **Stage 2 – Safe ESP32 Reboot (`watchdog_reboot_timeout`, Default: 60 min):**
  If the radio driver or transceiver hardware remains in an unrecoverable lockup beyond this threshold, a clean `App.safe_reboot()` is triggered.
* **Runtime Configurable:** Timeouts can be set to `0` to disable, or dynamically adjusted via Home Assistant / ESPHome actions (`eqiva_key_ble.set_watchdog_scanner_timeout` and `eqiva_key_ble.set_watchdog_reboot_timeout`).
* **State Recovery & Delayed Command Prevention:**
  If a connection fails or times out while the lock was requested to unlock/lock, the native lock entity immediately rolls back from transient `UNLOCKING`/`LOCKING` states to the actual door state as soon as BLE returns to `IDLE`. Any stale unexecuted commands in the send queue are safely discarded to prevent delayed "ghost" unlocks.
* **Command Debouncing & Spam Protection:**
  Duplicate lock/unlock requests sent while the motor is already in motion (`LOCKING` or `UNLOCKING`) are safely ignored to protect against repeated user clicks, race conditions, or automation bursts.
* **Diagnostic Telemetry Sensors (`sensor.eqiva_key_ble`):**
  Provides `last_contact_duration` (seconds elapsed since last beacon or connection) and `consecutive_connect_failures` (count of failed attempts) directly into Home Assistant for proactive signal-quality monitoring.

---

## 5. Alternative / Flexible Setup (Dynamic UI Inputs & Pairing)

If you prefer to configure your lock credentials dynamically from Home Assistant (without hardcoding them or editing `secrets.yaml`), or need to perform initial pairing via the Web UI:

See [`example.yaml`](example.yaml) in this repository for the full configuration containing:
* Text fields for `mac_address`, `user_key`, and `card_key`
* Number field for `user_id`, `disconnect_timeout`, `status_update_interval`, and watchdogs
* Dropdown selects for `direction` (Left/Right), `position` (Vertical/Horizontal), and `turns` (1-4)
* Action buttons for **Pair**, **Connect**, **Disconnect**, and **Apply Settings**
