idf_component_register(
    SRCS "node_main.c" "pca9685.c" "tca9548a.c" "tof050c.c" "cdc_dev.c"
         "../../../common/stc_lidmsg.c"
    INCLUDE_DIRS "." "../../../common"
    REQUIRES driver esp_driver_i2c esp_driver_usb_serial_jtag)
