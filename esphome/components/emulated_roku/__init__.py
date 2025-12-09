import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome import automation

DEPENDENCIES = ["network"]

CONF_DEVICE_NAME = "device_name"
CONF_PORT = "port"
CONF_ON_KEY_PRESS = "on_key_press"

emulated_roku_ns = cg.esphome_ns.namespace("emulated_roku")
EmulatedRokuComponent = emulated_roku_ns.class_("EmulatedRokuComponent", cg.Component)
KeyPressTrigger = emulated_roku_ns.class_(
    "KeyPressTrigger", automation.Trigger.template(cg.std_string, cg.std_string)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EmulatedRokuComponent),
        cv.Optional(CONF_DEVICE_NAME, default="ESPHome Roku"): cv.string,
        cv.Optional(CONF_PORT, default=8060): cv.port,
        cv.Optional(CONF_ON_KEY_PRESS): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(KeyPressTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
    cg.add(var.set_port(config[CONF_PORT]))

    for conf in config.get(CONF_ON_KEY_PRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "type"), (cg.std_string, "key")], conf
        )
