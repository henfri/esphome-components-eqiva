Big thanks to previous work done by:  [@MariusSchiffer](https://github.com/MariusSchiffer/esp32-keyble), [@tc-maxx](https://github.com/tc-maxx/esp32-keyble), [@lumokitho](https://github.com/lumokitho/esp32-keyble) and the original creator [@oyooyo](https://github.com/oyooyo/keyble)

# Eqiva BLE Lock ESPHome Component

This custom ESPHome component controls the **Eqiva Bluetooth Smart Lock**. It features optimized connection logic including local GATT handle caching, direct-connect, and configurable auto-disconnect parameters for ultra-low latencies and power efficiency.

---

## ⚡ Recommended Static Configuration (Single Lock)

In ESPHome, it is standard practice to hardcode connection details (`mac_address`, `user_id`, `user_key`) directly in the YAML after pairing. This removes all unnecessary sliders, text inputs, and helper buttons, reducing your YAML configuration to under 90 lines:

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
      CONFIG_BT_GATTC_CACHE_NVS_FLASH: "y" # Enables flash caching of handles

# Enable logging
logger:

# Enable Home Assistant API
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
    ssid: "Esphome-Eqiva-Lock"
    password: "12345678"
  
  on_connect:
    - esp32_ble_tracker.start_scan:
        continuous: true
  on_disconnect:
    - esp32_ble_tracker.stop_scan:

external_components:
  - source: github://henfri/esphome-components-eqiva

esp32_ble_tracker:
  scan_parameters:
    interval: 320ms
    window: 200ms
    active: true

eqiva_ble:

eqiva_key_ble:
  id: key_ble
  mac_address: "00:1A:22:18:A6:96" # <-- Set your lock's MAC address
  user_id: 1                       # <-- Set your paired User ID
  user_key: "931390fd4aa373bc827798aaeb035638" # <-- Set your paired User Key
  auto_disconnect: true
  idle_timeout: 10s
  status_update_interval: 2h

text_sensor:
  - platform: eqiva_key_ble
    lock_status:
      id: lock_state
      name: "Lock State"
    lock_ble_state:
      name: "Lock BLE State"
    low_battery:
      name: "Low Battery"

lock:
  - platform: template
    name: "Lock"
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
    unlock_action:
      - eqiva_key_ble.unlock:
    open_action:
      - eqiva_key_ble.open:
```

---

## 👥 Recommended Multi-Lock Configuration (Two Locks)

Since connections and background updates are handled natively in C++, you can easily control multiple locks on a single ESP32 without complex script runners or timers:

```yaml
esphome:
  name: esphome-eqiva-twolocks
  friendly_name: esphome-eqiva-twolocks

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
    ssid: "Esphome-Eqiva-TwoLocks"
    password: "12345678"
  
  on_connect:
    - esp32_ble_tracker.start_scan:
        continuous: true
  on_disconnect:
    - esp32_ble_tracker.stop_scan:

external_components:
  - source: github://henfri/esphome-components-eqiva

esp32_ble_tracker:
  id: ble_tracker
  max_connections: 2          # One BLE connection slot per lock
  scan_parameters:
    window: 200ms
    interval: 320ms
    active: true

eqiva_ble:

eqiva_key_ble:
  - id: lock_front
    mac_address: "00:1A:22:18:A6:96"
    user_id: 1
    user_key: "931390fd4aa373bc827798aaeb035638"
    auto_disconnect: true
    idle_timeout: 10s
    status_update_interval: 2h

  - id: lock_back
    mac_address: "00:1A:22:18:CE:94"
    user_id: 2
    user_key: "827798aaeb035638931390fd4aa373bc"
    auto_disconnect: true
    idle_timeout: 10s
    status_update_interval: 2h

text_sensor:
  - platform: eqiva_key_ble
    eqiva_key_ble_id: lock_front
    lock_status:
      id: lock_front_state
      name: "Front - Lock State"
    lock_ble_state:
      name: "Front - BLE State"
    low_battery:
      name: "Front - Low Battery"

  - platform: eqiva_key_ble
    eqiva_key_ble_id: lock_back
    lock_status:
      id: lock_back_state
      name: "Back - Lock State"
    lock_ble_state:
      name: "Back - BLE State"
    low_battery:
      name: "Back - Low Battery"

lock:
  - platform: template
    name: "Front Lock"
    icon: "mdi:lock"
    lambda: |-
      std::string state = id(lock_front_state).state;
      if (state == "LOCKED") return LOCK_STATE_LOCKED;
      if (state == "UNLOCKED" || state == "OPENED") return LOCK_STATE_UNLOCKED;
      if (state == "LOCKING") return LOCK_STATE_LOCKING;
      if (state == "UNLOCKING") return LOCK_STATE_UNLOCKING;
      if (state == "UNKNOWN") return LOCK_STATE_JAMMED;
      return {};
    lock_action:
      - eqiva_key_ble.lock:
          id: lock_front
    unlock_action:
      - eqiva_key_ble.unlock:
          id: lock_front
    open_action:
      - eqiva_key_ble.open:
          id: lock_front

  - platform: template
    name: "Back Lock"
    icon: "mdi:lock"
    lambda: |-
      std::string state = id(lock_back_state).state;
      if (state == "LOCKED") return LOCK_STATE_LOCKED;
      if (state == "UNLOCKED" || state == "OPENED") return LOCK_STATE_UNLOCKED;
      if (state == "LOCKING") return LOCK_STATE_LOCKING;
      if (state == "UNLOCKING") return LOCK_STATE_UNLOCKING;
      if (state == "UNKNOWN") return LOCK_STATE_JAMMED;
      return {};
    lock_action:
      - eqiva_key_ble.lock:
          id: lock_back
    unlock_action:
      - eqiva_key_ble.unlock:
          id: lock_back
    open_action:
      - eqiva_key_ble.open:
          id: lock_back
```

---

## 🛠️ Dynamic UI Configuration (For Pairing & Setup)

If you want to pair a new lock or test connections interactively, you can use input fields and buttons in the ESPHome web interface.

See [example.yaml](example.yaml) for a complete UI configuration containing:
*   User Key, MAC Address, and Card Key text fields
*   User ID number sliders
*   Connect, Disconnect, Pair, and Settings buttons

---

## 🔑 Initial Pairing Process

For connecting and controlling a lock, you need the `mac_address`, `user_id`, and `user_key`. The `user_id` and `user_key` can only be retrieved during pairing, for which you need the `card_key` (obtained from the pairing card QR code).

1.  Flash a firmware containing the **Dynamic UI Configuration** (see [example.yaml](example.yaml)).
2.  Power on your ESP32. Check the logs under `eqiva_ble` to discover the lock's MAC address.
3.  Enter the discovered MAC Address in the UI text field.
4.  Enter the lock's pairing `card_key` in the UI text field.
5.  Hold the lock's physical **open button** for 5 seconds until the lock's display shows pairing mode.
6.  Click **Pair** in the ESPHome UI.
7.  The logs will print your assigned `user_id` and `user_key`. Copy these values into your production YAML configuration (Static Configuration) to make the setup permanent and clean!
