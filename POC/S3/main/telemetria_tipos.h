/**
 * ============================================================================
 * ARCHIVO: telemetria_tipos.h (ESP32-S3)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Estructuras globales del sistema: define el formato de cada muestra de
 * telemetría (paquete_telemetria_t), las métricas del registrador (tramas
 * recibidas, guardadas, perdidas, ocupación de cola) y el pin CAN_RX (GPIO 13).
 * ============================================================================
 */
#ifndef TELEMETRIA_TIPOS_H
#define TELEMETRIA_TIPOS_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "soc/gpio_num.h"

// Pin CAN_RX conectado fisicamente al C6 (GPIO 13 segun esquematico Altium)
#define PIN_CAN_RX               GPIO_NUM_13

// Tamano del búfer elastico en RAM: 250 tramas (2.5 segundos de absorcion)
#define CAPACIDAD_COLA_TRAMAS    250

// Ubicacion del archivo en la memoria Flash interna
#define RUTA_ARCHIVO_CSV         "/spiffs/autolog.csv"

/* Estructura de cada muestra de telemetria */
typedef struct {
    uint32_t tiempo_ms;
    uint32_t id_can;
    uint16_t rpm;
    uint8_t  velocidad_kmh;
    uint8_t  temperatura_c;
    uint8_t  acelerador_pct;
    uint8_t  estado_fsm_ecu;
} paquete_telemetria_t;

/* Metricas globales para evaluacion de rendimiento */
typedef struct {
    uint32_t tramas_recibidas;
    uint32_t tramas_guardadas;
    uint32_t tramas_perdidas;
    uint32_t maximo_uso_cola;     // Marca de agua maxima (Watermark)
    uint32_t uso_actual_cola;
    // Telemetria automotriz en tiempo real
    uint16_t rpm_actual;
    uint8_t  velocidad_actual;
    uint8_t  temperatura_actual;
    uint8_t  acelerador_actual;
    uint8_t  estado_fsm_actual;
} metricas_sistema_t;

// Variables compartidas entre modulos
extern QueueHandle_t g_cola_telemetria;
extern metricas_sistema_t g_metricas;

#endif // TELEMETRIA_TIPOS_H
