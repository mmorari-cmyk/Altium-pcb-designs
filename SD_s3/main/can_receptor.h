/**
 * ============================================================================
 * ARCHIVO: can_receptor.h (ESP32-S3)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Declara las funciones para configurar el hardware receptor en GPIO 13
 * y la tarea de ingestión que corre en el Núcleo 0.
 * ============================================================================
 */
#ifndef CAN_RECEPTOR_H
#define CAN_RECEPTOR_H

#include "esp_err.h"

/**
 * @brief Inicializa el controlador CAN (TWAI) en modo LISTEN_ONLY a 500 kbps
 */
esp_err_t can_receptor_init(void);

/**
 * @brief Tarea de la FSM de Ingestion CAN (debe correr en el Nucleo 0)
 */
void tarea_fsm_ingestion(void *pvParameters);

#endif // CAN_RECEPTOR_H
