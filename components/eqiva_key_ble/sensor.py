import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    DEVICE_CLASS_DURATION,
    STATE_CLASS_MEASUREMENT,
    UNIT_SECOND,
    UNIT_EMPTY,
)
from . import CONF_EQIVA_KEY_BLE_ID, EqivaKeyBle

DEPENDENCIES = ["eqiva_key_ble"]

CONF_CONSECUTIVE_CONNECT_FAILURES = "consecutive_connect_failures"
CONF_LAST_CONTACT_DURATION = "last_contact_duration"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_EQIVA_KEY_BLE_ID): cv.use_id(EqivaKeyBle),
        cv.Optional(CONF_CONSECUTIVE_CONNECT_FAILURES): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:alert-circle-outline",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LAST_CONTACT_DURATION): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:timer-outline",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_EQIVA_KEY_BLE_ID])

    for key in [
        CONF_CONSECUTIVE_CONNECT_FAILURES,
        CONF_LAST_CONTACT_DURATION,
    ]:
        if key not in config:
            continue
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))
