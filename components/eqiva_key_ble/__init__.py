import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import esp32_ble_tracker, esp32_ble_client
from esphome.const import (
    CONF_ID,
    CONF_MAC_ADDRESS,
)

CONF_EQIVA_KEY_BLE_ID = "eqiva_key_ble_id"

CONF_USER_ID = "user_id"
CONF_USER_KEY = "user_key"
CONF_CARD_KEY = "card_key"
CONF_TURN_LEFT = "turn_left"
CONF_KEY_HORIZONTAL = "key_horizontal"
CONF_LOCK_TURNS = "lock_turns"
CONF_DISCONNECT_TIMEOUT = "disconnect_timeout"
CONF_STATUS_UPDATE_INTERVAL = "status_update_interval"
CONF_MAX_CONNECT_FAILURES = "max_connect_failures"

AUTO_LOAD = ["esp32_ble_client", "text_sensor"]
DEPENDENCIES = ["esp32_ble_tracker"]
CODEOWNERS = ["@digaus"]

eqiva_key_ble_ns = cg.esphome_ns.namespace("eqiva_key_ble")
EqivaKeyBle = eqiva_key_ble_ns.class_("EqivaKeyBle", esp32_ble_client.BLEClientBase)

EqivaLock = eqiva_key_ble_ns.class_("EqivaLock", automation.Action)
EqivaUnlock = eqiva_key_ble_ns.class_("EqivaUnlock", automation.Action)
EqivaOpen = eqiva_key_ble_ns.class_("EqivaOpen", automation.Action)
EqivaStatus = eqiva_key_ble_ns.class_("EqivaStatus", automation.Action)
EqivaPair = eqiva_key_ble_ns.class_("EqivaPair", automation.Action)
EqivaConnect = eqiva_key_ble_ns.class_("EqivaConnect", automation.Action)
EqivaDisconnect = eqiva_key_ble_ns.class_("EqivaDisconnect", automation.Action)
EqivaSettings = eqiva_key_ble_ns.class_("EqivaSettings", automation.Action)
EqivaSetDisconnectTimeout = eqiva_key_ble_ns.class_("EqivaSetDisconnectTimeout", automation.Action)
EqivaSetStatusUpdateInterval = eqiva_key_ble_ns.class_("EqivaSetStatusUpdateInterval", automation.Action)

CONFIG_SCHEMA = cv.ensure_list(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EqivaKeyBle),
            cv.Optional(CONF_MAC_ADDRESS): cv.templatable(cv.mac_address),
            cv.Optional(CONF_USER_ID, default=255): cv.one_of(0, 1, 2, 3, 4, 5, 6, 7, 255),
            cv.Optional(CONF_USER_KEY, default=""): cv.string,
            cv.Optional(CONF_DISCONNECT_TIMEOUT, default="0s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_STATUS_UPDATE_INTERVAL, default="2h"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MAX_CONNECT_FAILURES, default=4): cv.positive_int,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
)


async def to_code(configs):
    for config in configs:
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await esp32_ble_tracker.register_client(var, config)
        if mac_address := config.get(CONF_MAC_ADDRESS):
            cg.add(var.set_address(mac_address.as_hex))
        cg.add(var.set_user_id(config[CONF_USER_ID]))
        cg.add(var.set_user_key(config[CONF_USER_KEY]))
        cg.add(var.set_disconnect_timeout(config[CONF_DISCONNECT_TIMEOUT]))
        cg.add(var.set_status_update_interval(config[CONF_STATUS_UPDATE_INTERVAL]))
        cg.add(var.set_max_connect_failures(config[CONF_MAX_CONNECT_FAILURES]))


@automation.register_action(
    "eqiva_key_ble.settings",
    EqivaSettings,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EqivaKeyBle),
            cv.Required(CONF_TURN_LEFT): cv.templatable(cv.boolean),
            cv.Required(CONF_KEY_HORIZONTAL): cv.templatable(cv.boolean),
            cv.Required(CONF_LOCK_TURNS): cv.templatable(cv.int_range(min=1, max=4)),
        }
    ),
    synchronous=False,
)
async def eqiva_key_ble_settings_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_TURN_LEFT], args, bool)
    cg.add(var.set_turn_left(template_))
    template_ = await cg.templatable(config[CONF_KEY_HORIZONTAL], args, bool)
    cg.add(var.set_key_horizontal(template_))
    template_ = await cg.templatable(config[CONF_LOCK_TURNS], args, cg.int_)
    cg.add(var.set_lock_turns(template_))
    return var

@automation.register_action(
    "eqiva_key_ble.connect",
    EqivaConnect,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EqivaKeyBle),
            cv.Required(CONF_MAC_ADDRESS): cv.templatable(cv.string),
            cv.Required(CONF_USER_ID): cv.templatable(cv.one_of(0, 1, 2, 3, 4, 5, 6, 7, int=True)),
            cv.Required(CONF_USER_KEY): cv.templatable(cv.string),
        }
    ),
    synchronous=False,
)
async def eqiva_key_ble_connect_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_MAC_ADDRESS], args, cg.std_string)
    cg.add(var.set_mac_address(template_))
    template_ = await cg.templatable(config[CONF_USER_ID], args, cg.int_)
    cg.add(var.set_user_id(template_))
    template_ = await cg.templatable(config[CONF_USER_KEY], args, cg.std_string)
    cg.add(var.set_user_key(template_))
    return var

@automation.register_action(
    "eqiva_key_ble.disconnect",
    EqivaDisconnect,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EqivaKeyBle),
        }
    ),
    synchronous=False,
)
async def eqiva_key_ble_disconnect_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

@automation.register_action(
    "eqiva_key_ble.pair",
    EqivaPair,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EqivaKeyBle),
            cv.Required(CONF_CARD_KEY): cv.templatable(cv.string),
            cv.Required(CONF_MAC_ADDRESS): cv.templatable(cv.string),
        }
    ),
    synchronous=False,
)
async def eqiva_key_ble_pair_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_CARD_KEY], args, cg.std_string)
    cg.add(var.set_card_key(template_))
    template_ = await cg.templatable(config[CONF_MAC_ADDRESS], args, cg.std_string)
    cg.add(var.set_mac_address(template_))
    return var

@automation.register_action(
    "eqiva_key_ble.lock",
    EqivaLock,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EqivaKeyBle),
        }
    ),
    synchronous=False,
)
async def eqiva_key_ble_lock_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "eqiva_key_ble.unlock",
    EqivaUnlock,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EqivaKeyBle),
        }
    ),
    synchronous=False,
)
async def eqiva_key_ble_unlock_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

@automation.register_action(
    "eqiva_key_ble.open",
    EqivaOpen,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EqivaKeyBle),
        }
    ),
    synchronous=False,
)
async def eqiva_key_ble_open_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

@automation.register_action(
    "eqiva_key_ble.status",
    EqivaStatus,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EqivaKeyBle),
        }
    ),
    synchronous=False,
)
async def eqiva_key_ble_status_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

@automation.register_action(
    "eqiva_key_ble.set_disconnect_timeout",
    EqivaSetDisconnectTimeout,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EqivaKeyBle),
            cv.Required("timeout"): cv.templatable(cv.positive_time_period_milliseconds),
        }
    ),
    synchronous=True,
)
async def eqiva_set_disconnect_timeout_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config["timeout"], args, cg.uint32)
    cg.add(var.set_timeout(template_))
    return var

@automation.register_action(
    "eqiva_key_ble.set_status_update_interval",
    EqivaSetStatusUpdateInterval,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EqivaKeyBle),
            cv.Required("interval"): cv.templatable(cv.positive_time_period_milliseconds),
        }
    ),
    synchronous=True,
)
async def eqiva_set_status_update_interval_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config["interval"], args, cg.uint32)
    cg.add(var.set_interval(template_))
    return var
