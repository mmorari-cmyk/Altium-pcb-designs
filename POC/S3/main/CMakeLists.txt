idf_component_register(SRCS "main.c" "can_receptor.c" "almacen_flash.c" "servidor_web.c"
                       INCLUDE_DIRS "."
                       REQUIRES spiffs esp_wifi esp_event esp_netif nvs_flash esp_http_server driver esp_driver_uart esp_driver_gpio)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=cpp -Wno-cpp)
