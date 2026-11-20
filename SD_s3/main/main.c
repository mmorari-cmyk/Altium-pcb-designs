/**
 * ============================================================================
 * PROYECTO: AUTOlog - Datalogger Automotriz Dual-Core (ESP32-S3)
 * ARCHIVO:  main.c
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Orquestador principal de la arquitectura Dual-Core en FreeRTOS:
 * 1. Inicializa el sistema de persistencia en Flash (SPIFFS).
 * 2. Crea el búfer elástico en RAM (Queue de 250 elementos).
 * 3. Asigna la Ingestión de Tiempo Real al NÚCLEO 0 (Prioridad 5).
 * 4. Asigna el Almacenamiento Continuo al NÚCLEO 1 (Prioridad 4).
 * 5. Levanta el servidor web con telemetría en vivo en 192.168.4.1.
 * ============================================================================
 */

#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "telemetria_tipos.h"
#include "can_receptor.h"
#include "almacen_flash.h"
#include "servidor_web.h"

static const char *TAG = "MAIN_S3";

// Definicion de las variables globales compartidas
QueueHandle_t g_cola_telemetria = NULL;
metricas_sistema_t g_metricas = {0};

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   AUTOlog PoC - Datalogger Automotriz Dual-Core         ");
    ESP_LOGI(TAG, "   Nucleo 0: CAN Ingestion (GPIO 13)                     ");
    ESP_LOGI(TAG, "   Nucleo 1: Flash Persistence (/spiffs/autolog.csv)     ");
    ESP_LOGI(TAG, "==========================================================");

    // 1. Inicializar memoria no volatil (NVS) requerida por el stack Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Montar almacenamiento Flash SPIFFS y crear el archivo CSV
    ESP_ERROR_CHECK(almacen_flash_init());

    // 3. Crear el búfer elastico en RAM (Cola FreeRTOS para 250 tramas)
    g_cola_telemetria = xQueueCreate(CAPACIDAD_COLA_TRAMAS, sizeof(paquete_telemetria_t));
    if (!g_cola_telemetria) {
        ESP_LOGE(TAG, "Error critico creando la cola de telemetria!");
        return;
    }

    // 4. Inicializar receptor de hardware CAN a 500 kbps
    ESP_ERROR_CHECK(can_receptor_init());

    // 5. Iniciar punto de acceso Wi-Fi y servidor web HTTP
    ESP_ERROR_CHECK(servidor_web_init());

    // 6. Asignar la FSM de Persistencia al Nucleo 1 (prioridad 4)
    xTaskCreatePinnedToCore(tarea_fsm_almacen, "fsm_almacen", 4096, NULL, 4, NULL, 1);

    // 7. Asignar la FSM de Ingestion CAN al Nucleo 0 (prioridad 5 - tiempo real)
    xTaskCreatePinnedToCore(tarea_fsm_ingestion, "fsm_ingestion", 4096, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "Sistema AUTOlog operativo en ambos nucleos.");
}
