/**
 * ============================================================================
 * ARCHIVO: can_driver.h (ESP32-C6)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Define la estructura de la trama de datos del vehículo (ID y payload),
 * el pin de salida física hacia el S3 (GPIO 10) y las funciones de envío.
 * ============================================================================
 */
#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdint.h>
#include "esp_err.h"
#include "soc/gpio_num.h"

// Pin fisico de emision hacia el ESP32-S3 (GPIO 10 puro en ESP32-C6)
#define PIN_CAN_TX GPIO_NUM_10

// Estructura de trama automotriz OBD-II emulada sobre enlace fisico
typedef struct {
    uint32_t identifier;
    uint8_t  data_length_code;
    uint8_t  data[8];
} trama_automotriz_t;

/**
 * @brief Inicializa el puerto serie UART hacia el cable fisico
 */
esp_err_t can_driver_init(void);

/**
 * @brief Envia una trama automotriz completa de 8 bytes por el cable
 */
esp_err_t can_driver_transmit(const trama_automotriz_t *trama);

#endif // CAN_DRIVER_H
