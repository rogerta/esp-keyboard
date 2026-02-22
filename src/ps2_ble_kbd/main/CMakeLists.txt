idf_component_register(
    SRCS
        "main.c"
        "hid_dev.c"
        "ps2_proto.c"
        "ps2_kbd.c"
        "ps2_mgr.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash driver esp_rom
)
