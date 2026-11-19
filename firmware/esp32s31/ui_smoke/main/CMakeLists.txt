set(UI_ROOT "C:/smart_trash_can-ui-main/ui/lvgl")
file(GLOB UI_GENERATED
     "${UI_ROOT}/generated/stc_lp_*.c"
     "${UI_ROOT}/generated/stc_lvglpro_assets.c")

idf_component_register(
    SRCS
        "main.c"
        "${UI_ROOT}/src/stc_ui.c"
        "${UI_ROOT}/src/stc_chat_ui.c"
        ${UI_GENERATED}
    INCLUDE_DIRS
        "."
        "${UI_ROOT}/include"
        "${UI_ROOT}/src"
        "${UI_ROOT}/generated"
    REQUIRES
        esp32_s31_korvo_1
        lvgl
        esp_lvgl_port)
