# Add this folder to EXTRA_COMPONENT_DIRS in an ESP-IDF project using LVGL 9.5.
file(GLOB STC_GENERATED CONFIGURE_DEPENDS "generated/*.c")
idf_component_register(SRCS "src/stc_ui.c" ${STC_GENERATED}
    INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "src" "generated" REQUIRES lvgl)
