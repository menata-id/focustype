idf_component_register(
    SRCS "kbd_hid.c" "usb_kbd.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos
    PRIV_REQUIRES usb esp_timer
)
