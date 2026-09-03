import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import lock, binary_sensor
from esphome.const import CONF_ID
from . import eqiva_key_ble_ns, EqivaKeyBle, CONF_EQIVA_KEY_BLE_ID

DEPENDENCIES = ["eqiva_key_ble"]

EqivaLockEntity = eqiva_key_ble_ns.class_("EqivaLockEntity", lock.Lock, cg.Component)

CONF_LATCH_SENSOR = "latch_sensor"
CONF_INVERT_LATCH_SENSOR = "invert_latch_sensor"

CONFIG_SCHEMA = lock.lock_schema(
    EqivaLockEntity,
).extend(
    {
        cv.GenerateID(CONF_EQIVA_KEY_BLE_ID): cv.use_id(EqivaKeyBle),
        cv.Optional(CONF_LATCH_SENSOR): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_INVERT_LATCH_SENSOR, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await lock.register_lock(var, config)

    parent = await cg.get_variable(config[CONF_EQIVA_KEY_BLE_ID])
    cg.add(var.set_parent(parent))

    if CONF_LATCH_SENSOR in config:
        sens = await cg.get_variable(config[CONF_LATCH_SENSOR])
        cg.add(var.set_latch_sensor(sens))
        cg.add(var.set_invert_latch_sensor(config[CONF_INVERT_LATCH_SENSOR]))
        cg.add_define("USE_BINARY_SENSOR")
