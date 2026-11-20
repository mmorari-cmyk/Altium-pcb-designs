/**
 * ============================================================================
 * ARCHIVO: can_driver.c (ESP32-C6)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Enlace de transmisión hacia el datalogger. Construye el paquete de 12 bytes
 * con cabecera de sincronización (0xAA 0x55), calcula el código de seguridad
 * (Checksum XOR sobre los datos), añade el fin de trama (0xED) y transmite.
 * ============================================================================
 */
#include "can_driver.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"

static const char *TAG = "CAN_DRIVER_C6";
#define UART_NUM_C6 UART_NUM_1

esp_err_t can_driver_init(void)
{
    ESP_LOGI(TAG, "Iniciando enlace fisico automotriz en GPIO %d a 115200 baud...", PIN_CAN_TX);

    // 1. Resetear y configurar GPIO 10 como salida digital pura
    gpio_reset_pin(PIN_CAN_TX);
    gpio_set_direction(PIN_CAN_TX, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(PIN_CAN_TX, GPIO_PULLUP_ONLY);

    // 2. Instalar driver UART1
    esp_err_t ret = uart_driver_install(UART_NUM_C6, 1024, 1024, 0, NULL, 0);
    if (ret != ESP_OK) return ret;

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ret = uart_param_config(UART_NUM_C6, &uart_config);
    if (ret != ESP_OK) return ret;

    ret = uart_set_pin(UART_NUM_C6, PIN_CAN_TX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "Enlace fisico C6 listo emitiendo en GPIO %d.", PIN_CAN_TX);
    return ESP_OK;
}

esp_err_t can_driver_transmit(const trama_automotriz_t *trama)
{
    // Cabecera magica de sincronizacion de trama: 0xAA, 0x55
    uint8_t paquete[12];
    paquete[0] = 0xAA;
    paquete[1] = 0x55;
    paquete[2] = (uint8_t)(trama->identifier >> 8);
    paquete[3] = (uint8_t)(trama->identifier & 0xFF);
    paquete[4] = trama->data[0]; // RPM alta
    paquete[5] = trama->data[1]; // RPM baja
    paquete[6] = trama->data[2]; // Velocidad km/h
    paquete[7] = trama->data[3]; // Temperatura C
    paquete[8] = trama->data[4]; // Acelerador TPS pct
    paquete[9] = trama->data[5]; // Estado FSM

    // Checksum XOR sobre los 8 bytes utiles (ID + payload)
    uint8_t chk = 0;
    for (int i = 2; i <= 9; i++) {
        chk ^= paquete[i];
    }
    paquete[10] = chk;
    paquete[11] = 0xED; // Delimitador fin de paquete

    int enviados = uart_write_bytes(UART_NUM_C6, paquete, sizeof(paquete));
    return (enviados == sizeof(paquete)) ? ESP_OK : ESP_FAIL;
}
