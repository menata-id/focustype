idf_component_register(
    SRCS "st7305.c" "st7305_pack.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_spi esp_driver_gpio
    PRIV_REQUIRES heap freertos
)
