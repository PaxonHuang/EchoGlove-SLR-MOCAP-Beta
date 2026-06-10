idf_component_register(
    SRCS "tflite_infer.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_psram esp_timer
)
