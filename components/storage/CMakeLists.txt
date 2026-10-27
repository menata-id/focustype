idf_component_register(
    SRCS "storage.c"
    INCLUDE_DIRS "include"
    REQUIRES board
    PRIV_REQUIRES fatfs sdmmc vfs wear_levelling
)
