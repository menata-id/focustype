idf_component_register(
    SRCS "pcf_regs.c" "pcf85063.c"
    INCLUDE_DIRS "include"
    REQUIRES board
    PRIV_REQUIRES esp_driver_i2c
)
