import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart
from esphome.const import CONF_ID

DEPENDENCIES = ['uart']
AUTO_LOAD = ['climate']

tcl_climate_ns = cg.esphome_ns.namespace('tcl_climate')
TCLClimate = tcl_climate_ns.class_('TCLClimate', climate.Climate, uart.UARTDevice, cg.PollingComponent)

# === NEW: Add these two lines ===
CONF_BEEP = "beep"
CONF_DISPLAY = "display"
# ===============================

CONFIG_SCHEMA = climate.climate_schema(TCLClimate).extend({
    cv.GenerateID(): cv.declare_id(TCLClimate),
    # === NEW: Add these two optional config options ===
    cv.Optional(CONF_BEEP, default=True): cv.boolean,
    cv.Optional(CONF_DISPLAY, default=True): cv.boolean,
    # ==================================================
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.polling_component_schema('450ms'))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await uart.register_uart_device(var, config)

    # === NEW: Apply beep and display settings ===
    cg.add(var.set_beep(config[CONF_BEEP]))
    cg.add(var.set_display(config[CONF_DISPLAY]))
    # ============================================
