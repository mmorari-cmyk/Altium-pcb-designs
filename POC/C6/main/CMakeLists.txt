idf_component_register(SRCS "main.c" "can_driver.c" "fsm_ecu.c" "servidor_web_c6.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_server driver esp_driver_uart esp_driver_gpio)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=cpp -Wno-cpp)

