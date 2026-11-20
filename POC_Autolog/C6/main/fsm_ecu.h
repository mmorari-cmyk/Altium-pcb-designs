/**
 * ============================================================================
 * ARCHIVO: fsm_ecu.h (ESP32-C6)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Define los estados de la máquina finita del motor (Ralentí, Acelerando,
 * Crucero, Frenando, Exigencia, Crítico) y la estructura de variables físicas.
 * ============================================================================
 */
#ifndef FSM_ECU_H
#define FSM_ECU_H

#include <stdint.h>
#include <stdbool.h>

/* Modos de operacion de la ECU simulada */
typedef enum {
    MODO_SIM_AUTOMATICO = 0, // Ciclo autonomo de estados (Ralenti -> Acelera -> Crucero -> Freno)
    MODO_SIM_MANUAL    = 1  // Control manual interactivo desde la interfaz web
} modo_simulacion_t;

/* Estados de la maquina de estados finitos del motor */
typedef enum {
    ESTADO_ECU_INICIALIZACION = 0,
    ESTADO_ECU_RALENTI        = 1, // 800 RPM, 0 km/h (Normal)
    ESTADO_ECU_ACELERANDO     = 2, // Sube RPM y velocidad (Normal)
    ESTADO_ECU_CRUCERO        = 3, // Velocidad constante en ruta (Normal)
    ESTADO_ECU_FRENANDO       = 4, // Desaceleracion hasta detenerse
    ESTADO_ECU_MANUAL         = 5, // Control manual interactivo
    ESTADO_ECU_ALTA_EXIGENCIA = 6, // >5000 RPM, >92 C (Alerta Amarilla)
    ESTADO_ECU_CRITICO        = 7  // >6200 RPM, >102 C (Alerta Roja Critica)
} estado_ecu_t;

/* Variables fisicas del motor simulado */
typedef struct {
    uint16_t          rpm;
    uint8_t           velocidad_kmh;
    uint8_t           temperatura_c;
    uint8_t           acelerador_pct;
    estado_ecu_t      estado_actual;
    modo_simulacion_t modo;
} variables_motor_t;

/**
 * @brief Inicializa las variables del motor y el estado inicial
 */
void fsm_ecu_init(void);

/**
 * @brief Ejecuta un paso de fisica/transicion de estado (llamar a 100 Hz / cada 10 ms)
 * @return Estructura actualizada con las variables del motor
 */
variables_motor_t fsm_ecu_actualizar(void);

/**
 * @brief Obtiene una copia segura de las variables actuales del motor
 */
variables_motor_t fsm_ecu_obtener_variables(void);

/**
 * @brief Cambia el modo de operacion (Automatico o Manual)
 */
void fsm_ecu_set_modo(modo_simulacion_t modo);

/**
 * @brief Actualiza los controles manuales desde la interfaz web
 * @param acelerador_pct Nivel de acelerador deseado (0 a 100)
 * @param freno_activo True si el pedal de freno esta presionado
 */
void fsm_ecu_set_control_manual(uint8_t acelerador_pct, bool freno_activo);

/**
 * @brief Aplica un preset de conduccion
 * @param preset_id 1=Ralenti, 2=Ciudad (40 km/h), 3=Crucero (95 km/h), 4=Pista (140 km/h)
 */
void fsm_ecu_set_preset(int preset_id);

#endif // FSM_ECU_H
