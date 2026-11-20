/**
 * ============================================================================
 * ARCHIVO: servidor_web_c6.h (ESP32-C6)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Declara las funciones para conectar el ESP32-C6 a la red Wi-Fi y servir
 * el Cockpit de control manual del motor simulado.
 * ============================================================================
 */
#ifndef SERVIDOR_WEB_C6_H
#define SERVIDOR_WEB_C6_H

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief Inicializa Wi-Fi en modo Station (asociado al AP AUTOlog_PoC con IP fija 192.168.4.2)
 *        y levanta el servidor web con la interfaz del Cockpit/Simulador de la ECU.
 */
esp_err_t servidor_web_c6_init(void);

/**
 * @brief Actualiza la cuenta de tramas transmitidas mostrada en la telemetria web
 */
void servidor_web_c6_actualizar_tx(uint32_t contador_tx);

#endif // SERVIDOR_WEB_C6_H
