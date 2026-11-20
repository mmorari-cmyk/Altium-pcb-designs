/**
 * ============================================================================
 * PROYECTO: AUTOlog - Emulador de ECU y Cockpit Interactivo (ESP32-C6)
 * ARCHIVO:  main.c
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Punto de entrada del emulador del vehículo. Configura el microcontrolador,
 * inicializa el enlace físico serial (GPIO 10), arranca el simulador del motor
 * y ejecuta la tarea de transmisión a cadencia fija de 100 Hz (cada 10 ms).
 * ============================================================================
 */

#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "can_driver.h"
#include "fsm_ecu.h"
#include "servidor_web_c6.h"

static const char *TAG = "MAIN_C6";
#define PERIODO_MUESTREO_MS  10 // 10 ms = 100 Hz

static void tarea_ecu(void *pvParameters)
{
    TickType_t ultima_marca = xTaskGetTickCount();
    const TickType_t periodo = pdMS_TO_TICKS(PERIODO_MUESTREO_MS);
    uint32_t contador_tramas = 0;

    fsm_ecu_init();

    while (1) {
        vTaskDelayUntil(&ultima_marca, periodo);

        // 1. Actualizar la maquina de estados o fisica manual del motor
        variables_motor_t motor = fsm_ecu_actualizar();

        // 2. Empaquetar en trama estandar OBD-II (ID 0x7E8)
        trama_automotriz_t trama = {
            .identifier = 0x7E8,
            .data_length_code = 8,
            .data = {
                (uint8_t)(motor.rpm >> 8),   // Byte 0: RPM alta
                (uint8_t)(motor.rpm & 0xFF), // Byte 1: RPM baja
                motor.velocidad_kmh,         // Byte 2: Velocidad
                motor.temperatura_c,         // Byte 3: Temperatura
                motor.acelerador_pct,        // Byte 4: Acelerador
                (uint8_t)motor.estado_actual,// Byte 5: Estado FSM
                0x00,                        // Byte 6: Reservado
                0x00                         // Byte 7: Reservado
            }
        };

        // 3. Enviar por el cable fisico (GPIO 4) hacia el ESP32-S3
        if (can_driver_transmit(&trama) == ESP_OK) {
            contador_tramas++;
        }

        // Actualizar telemetria del servidor web
        servidor_web_c6_actualizar_tx(contador_tramas);

        // Log informativo cada 2 segundos
        if (contador_tramas > 0 && (contador_tramas % 200 == 0)) {
            ESP_LOGI(TAG, "[100Hz] TX hacia S3: %lu tramas | Modo: %s | Estado: %d | RPM: %u | Vel: %u km/h",
                     contador_tramas,
                     motor.modo == MODO_SIM_MANUAL ? "MANUAL" : "AUTO",
                     motor.estado_actual, motor.rpm, motor.velocidad_kmh);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   AUTOlog - Simulador ECU y Cockpit Web (ESP32-C6)      ");
    ESP_LOGI(TAG, "   TX CAN/UART: GPIO 18 a 100 Hz                         ");
    ESP_LOGI(TAG, "   Wi-Fi STA: 'AUTOlog_PoC' -> IP 192.168.4.2            ");
    ESP_LOGI(TAG, "==========================================================");

    // 1. Inicializar almacenamiento no volatil para Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Inicializar enlace fisico automotriz (GPIO 4 a 115200 baud)
    ESP_ERROR_CHECK(can_driver_init());

    // 3. Inicializar conexion Wi-Fi y servidor HTTP del Cockpit
    ESP_ERROR_CHECK(servidor_web_c6_init());

    // 4. Iniciar tarea de alta prioridad (5) para emision precisa a 100 Hz
    xTaskCreate(tarea_ecu, "tarea_ecu", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Simulador ECU operativo.");
}
