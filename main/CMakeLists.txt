idf_component_register(
    SRCS "main.c" "app.c"
    INCLUDE_DIRS "."
    REQUIRES board st7305 gfx usb_kbd storage ui notes editor pcf85063
    PRIV_REQUIRES esp_timer
)
