# Shared protocol headers (ESP-IDF component)
# Provides: uart_frame.h, p4_protocol.h
# Note: data_structures.h lives in ../include/ and is added via INCLUDE_DIRS

idf_component_register(
    INCLUDE_DIRS "." "../include"
)
