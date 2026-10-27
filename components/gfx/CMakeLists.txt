idf_component_register(
    SRCS "gfx.c" "gfx_text.c" "fonts_generated.c"
    INCLUDE_DIRS "include"
    REQUIRES st7305
    PRIV_REQUIRES heap util
)
