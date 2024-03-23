from esphome.components import time
from esphome import automation
from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@acvigue"]

CONF_LEVOIT_ID = "levoit"
CONF_LEVOIT_MODEL = "model"

VALID_MODELS = ["core300s", "core400s"]

levoit_ns = cg.esphome_ns.namespace("levoit")
Levoit = levoit_ns.class_("Levoit", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Levoit),
    cv.Optional(CONF_LEVOIT_MODEL, default="core300s"):  cv.All(cv.string, cv.one_of(*VALID_MODELS)),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_LEVOIT_MODEL in config:
        cg.add(var.set_device_model(config[CONF_LEVOIT_MODEL]))
