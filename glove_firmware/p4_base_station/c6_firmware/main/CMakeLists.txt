idf_component_register(
    SRCS "main.cpp" "espnow_handler.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_now driver esp_event nvs_flash shared
)
