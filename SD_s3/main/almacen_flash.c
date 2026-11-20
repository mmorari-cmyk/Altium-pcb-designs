/**
 * ============================================================================
 * ARCHIVO: almacen_flash.c (ESP32-S3 · NÚCLEO 1)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Almacenamiento Seguro y Persistencia en Memoria:
 * 1. Monta la partición de datos SPIFFS e inicializa el archivo CSV.
 * 2. Extrae las muestras validadas acumuladas en la cola de RAM.
 * 3. Escribe las filas en formato tabular con un búfer I/O de 4 KB.
 * 4. Sincroniza con memoria física periódicamente para evitar pérdidas.
 * ============================================================================
 */
#include "almacen_flash.h"
#include "telemetria_tipos.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <unistd.h>

static const char *TAG = "ALMACEN_FLASH";

typedef enum {
    ESTADO_ALMACEN_ESPERANDO_DATO = 0,
    ESTADO_ALMACEN_ESCRIBIENDO_REGISTRO,
    ESTADO_ALMACEN_SINCRONIZANDO_DISCO
} estado_almacen_t;

static estado_almacen_t s_estado = ESTADO_ALMACEN_ESPERANDO_DATO;
static FILE *s_archivo_csv = NULL;
static char s_buffer_io_flash[4096];

esp_err_t almacen_flash_init(void)
{
    ESP_LOGI(TAG, "Montando particion de datos SPIFFS 'storage'...");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al montar SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, usado = 0;
    ret = esp_spiffs_info("storage", &total, &usado);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Espacio en Flash: %d KB total, %d KB usado", (int)(total / 1024), (int)(usado / 1024));
    }

    // Si la memoria quedo llena de pruebas anteriores, borramos el archivo previo o formateamos
    unlink(RUTA_ARCHIVO_CSV);

    s_archivo_csv = fopen(RUTA_ARCHIVO_CSV, "w");
    if (!s_archivo_csv) {
        ESP_LOGW(TAG, "Espacio insuficiente. Formateando particion SPIFFS...");
        esp_spiffs_format("storage");
        s_archivo_csv = fopen(RUTA_ARCHIVO_CSV, "w");
    }

    if (!s_archivo_csv) {
        ESP_LOGE(TAG, "Error critico: No se pudo abrir %s", RUTA_ARCHIVO_CSV);
        return ESP_FAIL;
    }

    // Configurar búfer de 4 KB para acelerar escrituras por bloque en Flash
    setvbuf(s_archivo_csv, s_buffer_io_flash, _IOFBF, sizeof(s_buffer_io_flash));

    // Encabezado de columnas
    fprintf(s_archivo_csv, "tiempo_ms,id_can,rpm,velocidad_kmh,temperatura_c,acelerador_pct,estado_ecu\n");
    fflush(s_archivo_csv);

    ESP_LOGI(TAG, "Archivo CSV inicializado exitosamente: %s", RUTA_ARCHIVO_CSV);
    return ESP_OK;
}

FILE *almacen_flash_obtener_csv(void)
{
    return s_archivo_csv;
}

void tarea_fsm_almacen(void *pvParameters)
{
    ESP_LOGI(TAG, "FSM Persistencia iniciada en Nucleo %d", xPortGetCoreID());

    paquete_telemetria_t paquete;
    uint32_t contador_filas = 0;

    while (1) {
        switch (s_estado) {

        case ESTADO_ALMACEN_ESPERANDO_DATO:
            // Espera dato de la cola de FreeRTOS
            if (xQueueReceive(g_cola_telemetria, &paquete, portMAX_DELAY) == pdPASS) {
                s_estado = ESTADO_ALMACEN_ESCRIBIENDO_REGISTRO;
            }
            break;

        case ESTADO_ALMACEN_ESCRIBIENDO_REGISTRO:
            if (s_archivo_csv) {
                fprintf(s_archivo_csv, "%lu,0x%03lX,%u,%u,%u,%u,%u\n",
                        paquete.tiempo_ms, paquete.id_can, paquete.rpm,
                        paquete.velocidad_kmh, paquete.temperatura_c,
                        paquete.acelerador_pct, paquete.estado_fsm_ecu);
                g_metricas.tramas_guardadas++;
                contador_filas++;

                if (g_metricas.tramas_guardadas % 100 == 0) {
                    ESP_LOGI(TAG, "[FLASH 100Hz] Guardadas: %lu tramas | RPM: %u | Vel: %u km/h",
                             g_metricas.tramas_guardadas, paquete.rpm, paquete.velocidad_kmh);
                }
            }

            // Cada 50 registros sincroniza con la memoria Flash física
            if (contador_filas >= 50) {
                s_estado = ESTADO_ALMACEN_SINCRONIZANDO_DISCO;
            } else {
                s_estado = ESTADO_ALMACEN_ESPERANDO_DATO;
            }
            break;

        case ESTADO_ALMACEN_SINCRONIZANDO_DISCO:
            if (s_archivo_csv) {
                fflush(s_archivo_csv);
            }
            contador_filas = 0;
            s_estado = ESTADO_ALMACEN_ESPERANDO_DATO;
            break;
        }
    }
}
