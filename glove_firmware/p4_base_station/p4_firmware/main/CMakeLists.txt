idf_component_register(
    SRCS "main.cpp" "uart_receiver.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_event
    PRIV_REQUIRES esp_timer shared
)

# FramePairer.h lives in receiver/lib — not in the shared component
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../receiver/lib"
)
