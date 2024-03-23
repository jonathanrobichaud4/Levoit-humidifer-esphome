from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import ICON_SECURITY, DEVICE_CLASS_LOCK, CONF_POWER, ICON_POWER, ICON_BRIGHTNESS_5
from .. import levoit_ns, CONF_LEVOIT_ID, Levoit

DEPENDENCIES = ["levoit"]
CODEOWNERS = ["@acvigue"]

CONF_DISPLAY_LOCK = "display_lock"
CONF_DISPLAY_ON = "display_on"

LevoitSwitch = levoit_ns.class_("LevoitSwitch", switch.Switch, cg.Component)
LevoitSwitchPurpose = levoit_ns.enum("LevoitSwitchPurpose")

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(CONF_LEVOIT_ID): cv.use_id(Levoit),
        cv.Optional(CONF_DISPLAY_LOCK): switch.switch_schema(LevoitSwitch, icon=ICON_SECURITY).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_DISPLAY_ON): switch.switch_schema(LevoitSwitch, icon=ICON_BRIGHTNESS_5).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_POWER): switch.switch_schema(LevoitSwitch, icon=ICON_POWER).extend(cv.COMPONENT_SCHEMA),
    })
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LEVOIT_ID])
    if config_display_lock := config.get(CONF_DISPLAY_LOCK):
        var = await switch.new_switch(config_display_lock, parent, LevoitSwitchPurpose.DISPLAY_LOCK)
        await cg.register_component(var, config_display_lock)

    if config_display_on := config.get(CONF_DISPLAY_ON):
        var = await switch.new_switch(config_display_on, parent, LevoitSwitchPurpose.DISPLAY_ON)
        await cg.register_component(var, config_display_on)
    
    if config_power := config.get(CONF_POWER):
        var = await switch.new_switch(config_power, parent, LevoitSwitchPurpose.MASTER_POWER)
        await cg.register_component(var, config_power)
