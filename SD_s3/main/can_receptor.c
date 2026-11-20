/**
 * ============================================================================
 * ARCHIVO: can_receptor.c (ESP32-S3 · NÚCLEO 0)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Captura en Tiempo Real y Filtro de Integridad:
 * 1. Lee los bytes entrantes por el cable físico (GPIO 13).
 * 2. Sincroniza el inicio del mensaje buscando la cabecera (0xAA 0x55).
 * 3. Valida el código de control (Checksum XOR); descarta si hay ruido.
 * 4. Si el dato es válido, lo deposita en la cola de RAM sin esperas.
 * ============================================================================
 */
#include "can_receptor.h"
#include "telemetria_tipos.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

static const char *TAG = "RECEPTOR_S3";
#define UART_NUM_S3 UART_NUM_1

esp_err_t can_receptor_init(void)
{
    ESP_LOGI(TAG, "Iniciando receptor automotriz en GPIO %d a 115200 baud...", PIN_CAN_RX);

    // 1. Configurar GPIO 13 explícitamente como entrada con pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_CAN_RX),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // 2. Instalar driver UART
    esp_err_t ret = uart_driver_install(UART_NUM_S3, 1024, 0, 0, NULL, 0);
    if (ret != ESP_OK) return ret;

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ret = uart_param_config(UART_NUM_S3, &uart_config);
    if (ret != ESP_OK) return ret;

    // 3. Conectar GPIO 13 como RX desde el C6
    ret = uart_set_pin(UART_NUM_S3, UART_PIN_NO_CHANGE, PIN_CAN_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "Receptor S3 listo en GPIO 13 (Pull-up activo).");
    return ESP_OK;
}

void tarea_fsm_ingestion(void *pvParameters)
{
    ESP_LOGI(TAG, "FSM Ingestion en Nucleo %d escuchando al ESP32-C6...", xPortGetCoreID());

    uint8_t rx_buffer[128];
    uint8_t payload[10];
    int payload_idx = 0;

    typedef enum {
        PARSER_BUSCAR_AA = 0,
        PARSER_BUSCAR_55,
        PARSER_LEER_PAYLOAD
    } parser_estado_t;

    parser_estado_t estado_parser = PARSER_BUSCAR_AA;
    paquete_telemetria_t paquete;
    TickType_t ultima_marca_diag = xTaskGetTickCount();

    while (1) {
        // Diagnóstico cada 1 segundo mostrando estado fisico de GPIO 13
        if ((xTaskGetTickCount() - ultima_marca_diag) > pdMS_TO_TICKS(1000)) {
            ultima_marca_diag = xTaskGetTickCount();
            int nivel_13 = gpio_get_level(PIN_CAN_RX);
            size_t bytes_en_cola = 0;
            uart_get_buffered_data_len(UART_NUM_S3, &bytes_en_cola);
            ESP_LOGI(TAG, "Diag RX [GPIO %d]: Nivel fisico=%d | Bytes en buffer UART=%d | Tramas RX=%lu | Perdidas=%lu",
                     PIN_CAN_RX, nivel_13, (int)bytes_en_cola, g_metricas.tramas_recibidas, g_metricas.tramas_perdidas);
        }

        int bytes_leidos = uart_read_bytes(UART_NUM_S3, rx_buffer, sizeof(rx_buffer), pdMS_TO_TICKS(20));
        for (int b = 0; b < bytes_leidos; b++) {
            uint8_t byte_in = rx_buffer[b];
            switch (estado_parser) {
            case PARSER_BUSCAR_AA:
                if (byte_in == 0xAA) {
                    estado_parser = PARSER_BUSCAR_55;
                }
                break;

            case PARSER_BUSCAR_55:
                if (byte_in == 0x55) {
                    estado_parser = PARSER_LEER_PAYLOAD;
                    payload_idx = 0;
                } else if (byte_in != 0xAA) {
                    estado_parser = PARSER_BUSCAR_AA;
                }
                break;

            case PARSER_LEER_PAYLOAD:
                payload[payload_idx++] = byte_in;
                if (payload_idx >= 10) {
                    // payload[0..1] = ID CAN (0x07E8)
                    // payload[2..3] = RPM (16 bits)
                    // payload[4]    = Velocidad (km/h)
                    // payload[5]    = Temperatura (C)
                    // payload[6]    = Acelerador (TPS %)
                    // payload[7]    = Estado FSM (0 a 5)
                    // payload[8]    = Checksum XOR
                    // payload[9]    = Delimitador fin de paquete (0xED)

                    uint8_t chk_calculado = 0;
                    for (int i = 0; i < 8; i++) {
                        chk_calculado ^= payload[i];
                    }

                    // Verificacion de integridad y delimitador
                    if (payload[9] == 0xED && payload[8] == chk_calculado) {
                        uint32_t can_id  = ((uint32_t)payload[0] << 8) | payload[1];
                        uint16_t rpm     = ((uint16_t)payload[2] << 8) | payload[3];
                        uint8_t  vel     = payload[4];
                        uint8_t  temp    = payload[5];
                        uint8_t  tps     = payload[6];
                        uint8_t  fsm     = payload[7];

                        g_metricas.tramas_recibidas++;

                        // Actualizar telemetria para el dashboard web en tiempo real
                        g_metricas.rpm_actual         = rpm;
                        g_metricas.velocidad_actual   = vel;
                        g_metricas.temperatura_actual = temp;
                        g_metricas.acelerador_actual  = tps;
                        g_metricas.estado_fsm_actual  = fsm;

                        // Empaquetar para el búfer elastico de persistencia en Flash
                        paquete.tiempo_ms      = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
                        paquete.id_can         = can_id;
                        paquete.rpm            = rpm;
                        paquete.velocidad_kmh  = vel;
                        paquete.temperatura_c  = temp;
                        paquete.acelerador_pct = tps;
                        paquete.estado_fsm_ecu = fsm;

                        UBaseType_t ocupacion = uxQueueMessagesWaiting(g_cola_telemetria);
                        g_metricas.uso_actual_cola = (uint32_t)ocupacion;
                        if (ocupacion > g_metricas.maximo_uso_cola) {
                            g_metricas.maximo_uso_cola = (uint32_t)ocupacion;
                        }

                        if (xQueueSend(g_cola_telemetria, &paquete, 0) != pdPASS) {
                            g_metricas.tramas_perdidas++;
                        }
                    } else {
                        // Trama rechazada por ruido / corrupcion en el cable
                        g_metricas.tramas_perdidas++;
                    }

                    estado_parser = PARSER_BUSCAR_AA;
                }
                break;
            }
        }
    }
}
