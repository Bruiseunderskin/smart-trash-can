# Edge Impulse TFLite invoke probe

Measures 20 `MicroInterpreter::Invoke()` calls on an ESP32-S3 using the user's `Waste-Rec` 160×160 int8 TFLite model. Input is zero-filled. The measurement excludes capture, RGB preprocessing, display and lid movement, and does not measure classification quality. Do not infer end-to-end acceptance from this probe.

Provide the Edge Impulse model locally at `main/models/waste4_int8.tflite`; do not commit the export. The observed export contains six operators: ADD, CONV_2D, DEPTHWISE_CONV_2D, FULLY_CONNECTED, RESHAPE and SOFTMAX. The resolver also registers the person-probe pooling operators. Review the generated export's terms and applicable permissions before use.

```sh
source /path/to/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodemXXX -b 460800 flash
idf.py -p /dev/cu.usbmodemXXX -b 115200 monitor
```

The PSRAM defaults are for the measured board; verify the actual board and PSRAM mode. Capture the complete boot and RESULT logs. Model input type, scale, zero point and image preprocessing must be validated before using predictions from real images.
