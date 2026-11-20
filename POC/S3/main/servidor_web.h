/**
 * ============================================================================
 * ARCHIVO: servidor_web.h (ESP32-S3)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Declara la función de inicialización del punto de acceso Wi-Fi autónomo
 * ("AUTOlog_PoC" en 192.168.4.1) y el servidor HTTP embebido.
 * ============================================================================
 */
#ifndef SERVIDOR_WEB_H
#define SERVIDOR_WEB_H

#include "esp_err.h"

/**
 * @brief Configura la red Wi-Fi SoftAP "AUTOlog_PoC" e inicia el servidor HTTP
 */
esp_err_t servidor_web_init(void);

#endif // SERVIDOR_WEB_H
