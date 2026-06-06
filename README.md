Big thanks to previous work done by:  [@MariusSchiffer](https://github.com/MariusSchiffer/esp32-keyble), [@tc-maxx](https://github.com/tc-maxx/esp32-keyble), [@lumokitho](https://github.com/lumokitho/esp32-keyble) and the original creator [@oyooyo](https://github.com/oyooyo/keyble)


# Example yaml:

The new C++ flow control completely manages connections, status polling, and disconnection logic. You do not need any UI inputs (like text fields or buttons) for normal operation. Just enter your lock credentials directly in the `eqiva_key_ble` component block:

```yaml
esphome:
  name: esphome-eqiva-lock
  friendly_name: esphome-eqiva-lock

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
  ap:
    ssid: "Eqiva-Lock-Fallback"
    password: !secret ap_password

external_components:
  - source: github://digaus/esphome-components-eqiva
    refresh: 0s

esp32_ble_tracker:
  scan_parameters:
    interval: 320ms
    window: 200ms
    active: true

# Used for discovering the lock's MAC address (check ESPHome logs)
eqiva_ble:

# Configure your lock credentials here (no UI inputs needed!)
eqiva_key_ble:
  - id: my_lock
    mac_address: !secret eqiva_mac_address
    user_id: !secret eqiva_user_id
    user_key: !secret eqiva_user_key
    disconnect_timeout: 10s
    status_update_interval: 2h

# Read-only state sensors
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
      icon: mdi:bluetooth-settings

# Home Assistant Lock Entity
lock:
  - platform: template
    name: "Main Door Lock"
    icon: "mdi:lock"
    lambda: |-
      std::string state = id(lock_state).state;
      if (state == "LOCKED") return LOCK_STATE_LOCKED;
      if (state == "UNLOCKED" || state == "OPENED") return LOCK_STATE_UNLOCKED;
      if (state == "LOCKING") return LOCK_STATE_LOCKING;
      if (state == "UNLOCKING") return LOCK_STATE_UNLOCKING;
      if (state == "UNKNOWN") return LOCK_STATE_JAMMED;
      return {};
    lock_action:
      - eqiva_key_ble.lock:
          id: my_lock
    unlock_action:
      - eqiva_key_ble.unlock:
          id: my_lock
    open_action:
      - eqiva_key_ble.open:
          id: my_lock
```


# Performance Optimizations

This component includes optimizations for fast connection times and low latency:

## Direct Connect and `esp32_ble_tracker`
The component uses "Direct Connect" (bypassing the active scan window) to achieve fast connection times. However, if the `esp32_ble_tracker` is actively scanning when `connect()` is called, the Bluedroid stack may silently fail or produce long connection times. 

To prevent this, you should ensure that `esp32_ble_tracker` is stopped before connecting to the lock, or that scanning is disabled by default. The provided `example.yaml` demonstrates this by using `continuous: false` and starting/stopping the scan based on wifi connectivity.

## Persistent GATT Caching (NVS)
The component caches the GATT characteristics to bypass service discovery on subsequent connections. By default, this cache is stored in RAM and is lost upon rebooting the ESP32.

To persist the GATT cache across reboots and ensure fast connections immediately after startup, you must opt-in to NVS caching in your `esp-idf` framework configuration:
```yaml
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_BT_GATTC_CACHE_NVS_FLASH: "y"
```

# Initial Pairing

Before you can use the lock, you need to pair the ESP32 to it to obtain the `user_id` and `user_key`. Since the new YAML example above has no web UI inputs, you will need a temporary "Pairing Config" (or use ESPHome `secrets.yaml` and modify it later).

1. Flash your ESP32 with an ESPHome config that contains the UI inputs (see the `example.yaml` file in this repository for the full pairing config).
2. Look at the ESPHome logs to find the lock's `mac_address` (logged by `eqiva_ble`).
3. Enter the `mac_address` in the Web UI.
4. Scan the QR code of the pairing card included with your lock and paste the result into the `card_key` input on the Web UI.
5. Put the lock into pairing mode (hold the open button for 5 seconds).
6. Press the **Pair** Button on the Web UI.
7. The ESP will pair and print the generated `user_key` and `user_id` in the ESPHome log (and Web UI).
8. Copy the `mac_address`, `user_id`, and `user_key` to your `secrets.yaml`.
9. You can now switch to the ultra-clean "Production Config" shown at the top of this README!
