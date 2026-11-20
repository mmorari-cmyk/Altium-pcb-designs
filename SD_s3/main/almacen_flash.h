/**
 * ============================================================================
 * ARCHIVO: almacen_flash.h (ESP32-S3)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Declara las funciones para montar la partición SPIFFS ('storage'),
 * abrir el archivo autolog.csv y ejecutar la tarea de persistencia en Núcleo 1.
 * ============================================================================
 */
#ifndef ALMACEN_FLASH_H
#define ALMACEN_FLASH_H

#include <stdio.h>
#include "esp_err.h"

/**
 * @brief Monta el sistema de archivos SPIFFS y crea el archivo autolog.csv
 */
esp_err_t almacen_flash_init(void);

/**
 * @brief Retorna el descriptor de archivo del CSV abierto
 */
FILE *almacen_flash_obtener_csv(void);

/**
 * @brief Tarea de la FSM de Persistencia (debe correr en el Nucleo 1)
 */
void tarea_fsm_almacen(void *pvParameters);

#endif // ALMACEN_FLASH_H
