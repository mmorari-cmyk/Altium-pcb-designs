/**
 * ============================================================================
 * ARCHIVO: fsm_ecu.c (ESP32-C6)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Simulador de física del vehículo. En cada paso de tiempo calcula la evolución
 * dinámica del motor: subida y bajada de RPM, inercia de la velocidad y
 * comportamiento térmico del refrigerante según la exigencia del acelerador.
 * ============================================================================
 */
#include "fsm_ecu.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "FSM_ECU";

static variables_motor_t s_motor;
static uint32_t s_ciclos_en_estado = 0;
static uint8_t s_acelerador_manual_target = 0;
static bool s_freno_manual_activo = false;
static SemaphoreHandle_t s_mutex_motor = NULL;

void fsm_ecu_init(void)
{
    if (s_mutex_motor == NULL) {
        s_mutex_motor = xSemaphoreCreateMutex();
    }

    if (xSemaphoreTake(s_mutex_motor, portMAX_DELAY) == pdTRUE) {
        s_motor.rpm = 800;
        s_motor.velocidad_kmh = 0;
        s_motor.temperatura_c = 85;
        s_motor.acelerador_pct = 0;
        s_motor.estado_actual = ESTADO_ECU_RALENTI;
        s_motor.modo = MODO_SIM_AUTOMATICO;
        s_ciclos_en_estado = 0;
        s_acelerador_manual_target = 0;
        s_freno_manual_activo = false;
        xSemaphoreGive(s_mutex_motor);
    }

    ESP_LOGI(TAG, "FSM inicializada en ESTADO_ECU_RALENTI (Modo Automatico).");
}

variables_motor_t fsm_ecu_actualizar(void)
{
    variables_motor_t copia;

    if (s_mutex_motor == NULL) {
        return s_motor;
    }

    if (xSemaphoreTake(s_mutex_motor, pdMS_TO_TICKS(5)) != pdTRUE) {
        // En caso de contencion minima, devolvemos el ultimo valor sin bloquear la tarea de 100 Hz
        return s_motor;
    }

    s_ciclos_en_estado++;

    if (s_motor.modo == MODO_SIM_MANUAL) {
        s_motor.estado_actual = ESTADO_ECU_MANUAL;

        if (s_freno_manual_activo) {
            // Frenado brusco
            s_motor.acelerador_pct = 0;
            if (s_motor.rpm > 820) {
                s_motor.rpm = (s_motor.rpm > 830) ? s_motor.rpm - 30 : 800;
            } else {
                s_motor.rpm = 800;
            }
            if (s_motor.velocidad_kmh > 0) {
                s_motor.velocidad_kmh = (s_motor.velocidad_kmh >= 2) ? s_motor.velocidad_kmh - 2 : 0;
            }
        } else {
            // Respuesta al acelerador manual (TPS)
            s_motor.acelerador_pct = s_acelerador_manual_target;
            uint16_t rpm_objetivo = 800 + ((uint32_t)s_acelerador_manual_target * 57); // 800 a 6500 RPM

            if (s_motor.rpm < rpm_objetivo) {
                uint16_t delta = (rpm_objetivo - s_motor.rpm > 25) ? 25 : (rpm_objetivo - s_motor.rpm);
                s_motor.rpm += delta;
            } else if (s_motor.rpm > rpm_objetivo) {
                uint16_t delta = (s_motor.rpm - rpm_objetivo > 15) ? 15 : (s_motor.rpm - rpm_objetivo);
                s_motor.rpm -= delta;
            }

            // Calculo de velocidad segun inercia mecanica simulada
            uint8_t vel_objetivo = (s_motor.rpm > 800) ? (uint8_t)((s_motor.rpm - 800) / 36) : 0;
            if (s_ciclos_en_estado % 3 == 0) { // Actualiza velocidad cada 30 ms para inercia realista
                if (s_motor.velocidad_kmh < vel_objetivo) {
                    s_motor.velocidad_kmh++;
                } else if (s_motor.velocidad_kmh > vel_objetivo) {
                    s_motor.velocidad_kmh--;
                }
            }
        }

        // Temperatura motor dependiente de RPM y carga
        uint8_t temp_objetivo = 85 + (uint8_t)(s_motor.rpm / 200); // 85C a 117C
        if (s_ciclos_en_estado % 50 == 0) { // Sube/baja lentamente
            if (s_motor.temperatura_c < temp_objetivo) s_motor.temperatura_c++;
            if (s_motor.temperatura_c > temp_objetivo) s_motor.temperatura_c--;
        }

    } else {
        // MODO AUTOMATICO: FSM ciclica original
        switch (s_motor.estado_actual) {

        case ESTADO_ECU_RALENTI:
            s_motor.rpm = 800 + (s_ciclos_en_estado % 20);
            s_motor.velocidad_kmh = 0;
            s_motor.acelerador_pct = 0;
            s_motor.temperatura_c = 86;

            if (s_ciclos_en_estado >= 300) {
                s_ciclos_en_estado = 0;
                s_motor.estado_actual = ESTADO_ECU_ACELERANDO;
                ESP_LOGI(TAG, "[FSM Auto] Transicion -> ESTADO_ECU_ACELERANDO (Condicion Normal)");
            }
            break;

        case ESTADO_ECU_ACELERANDO:
            s_motor.acelerador_pct = 45;
            if (s_motor.rpm < 3500) s_motor.rpm += 10;
            if (s_motor.velocidad_kmh < 75) s_motor.velocidad_kmh = (s_motor.rpm > 800) ? (s_motor.rpm - 800) / 36 : 0;
            s_motor.temperatura_c = 89;

            if (s_ciclos_en_estado >= 350) {
                s_ciclos_en_estado = 0;
                s_motor.estado_actual = ESTADO_ECU_CRUCERO;
                ESP_LOGI(TAG, "[FSM Auto] Transicion -> ESTADO_ECU_CRUCERO (Condicion Normal)");
            }
            break;

        case ESTADO_ECU_CRUCERO:
            s_motor.rpm = 2800 + (s_ciclos_en_estado % 30);
            s_motor.velocidad_kmh = 90;
            s_motor.acelerador_pct = 30;
            s_motor.temperatura_c = 90;

            if (s_ciclos_en_estado >= 350) {
                s_ciclos_en_estado = 0;
                s_motor.estado_actual = ESTADO_ECU_ALTA_EXIGENCIA;
                ESP_LOGI(TAG, "[FSM Auto] Transicion -> ESTADO_ECU_ALTA_EXIGENCIA (>5000 RPM / >92 C)");
            }
            break;

        case ESTADO_ECU_ALTA_EXIGENCIA:
            s_motor.acelerador_pct = 80;
            // Acelerar fuertemente superando los 5000 RPM (hasta 5400 RPM)
            if (s_motor.rpm < 5400) s_motor.rpm += 12;
            if (s_motor.velocidad_kmh < 130) s_motor.velocidad_kmh = (s_motor.rpm > 800) ? (s_motor.rpm - 800) / 36 : 0;
            // Elevar temperatura superando los 92 C (hasta 96 C)
            if (s_motor.temperatura_c < 96 && (s_ciclos_en_estado % 50 == 0)) {
                s_motor.temperatura_c++;
            }

            if (s_ciclos_en_estado >= 400) {
                s_ciclos_en_estado = 0;
                s_motor.estado_actual = ESTADO_ECU_CRITICO;
                ESP_LOGI(TAG, "[FSM Auto] Transicion -> ESTADO_ECU_CRITICO (>6200 RPM / >102 C)");
            }
            break;

        case ESTADO_ECU_CRITICO:
            s_motor.acelerador_pct = 100; // Acelerador a fondo
            // Régimen crítico en zona roja superando 6200 RPM (hasta 6450 RPM)
            if (s_motor.rpm < 6450) s_motor.rpm += 15;
            if (s_motor.velocidad_kmh < 150) s_motor.velocidad_kmh = (s_motor.rpm > 800) ? (s_motor.rpm - 800) / 36 : 0;
            // Temperatura crítica superando 102 C (hasta 106 C)
            if (s_motor.temperatura_c < 106 && (s_ciclos_en_estado % 40 == 0)) {
                s_motor.temperatura_c++;
            }

            if (s_ciclos_en_estado >= 350) {
                s_ciclos_en_estado = 0;
                s_motor.estado_actual = ESTADO_ECU_FRENANDO;
                ESP_LOGI(TAG, "[FSM Auto] Transicion -> ESTADO_ECU_FRENANDO (Enfriamiento)");
            }
            break;

        case ESTADO_ECU_FRENANDO:
            s_motor.acelerador_pct = 0;
            if (s_motor.rpm > 820) s_motor.rpm = (s_motor.rpm > 850) ? s_motor.rpm - 30 : 800;
            if (s_motor.velocidad_kmh > 0) s_motor.velocidad_kmh = (s_motor.velocidad_kmh > 2) ? s_motor.velocidad_kmh - 2 : 0;
            // Enfriar el motor gradualmente
            if (s_motor.temperatura_c > 86 && (s_ciclos_en_estado % 40 == 0)) {
                s_motor.temperatura_c--;
            }

            if (s_ciclos_en_estado >= 400) {
                s_ciclos_en_estado = 0;
                s_motor.estado_actual = ESTADO_ECU_RALENTI;
                ESP_LOGI(TAG, "[FSM Auto] Transicion -> ESTADO_ECU_RALENTI (Ciclo completado)");
            }
            break;

        default:
            s_motor.estado_actual = ESTADO_ECU_RALENTI;
            s_ciclos_en_estado = 0;
            break;
        }
    }

    copia = s_motor;
    xSemaphoreGive(s_mutex_motor);
    return copia;
}

variables_motor_t fsm_ecu_obtener_variables(void)
{
    variables_motor_t copia = s_motor;
    if (s_mutex_motor && xSemaphoreTake(s_mutex_motor, pdMS_TO_TICKS(10)) == pdTRUE) {
        copia = s_motor;
        xSemaphoreGive(s_mutex_motor);
    }
    return copia;
}

void fsm_ecu_set_modo(modo_simulacion_t modo)
{
    if (s_mutex_motor && xSemaphoreTake(s_mutex_motor, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_motor.modo = modo;
        s_ciclos_en_estado = 0;
        if (modo == MODO_SIM_MANUAL) {
            s_motor.estado_actual = ESTADO_ECU_MANUAL;
            ESP_LOGI(TAG, "Cambiado a MODO_SIM_MANUAL (Cockpit interactivo).");
        } else {
            s_motor.estado_actual = ESTADO_ECU_RALENTI;
            s_freno_manual_activo = false;
            ESP_LOGI(TAG, "Cambiado a MODO_SIM_AUTOMATICO (FSM Ciclica).");
        }
        xSemaphoreGive(s_mutex_motor);
    }
}

void fsm_ecu_set_control_manual(uint8_t acelerador_pct, bool freno_activo)
{
    if (s_mutex_motor && xSemaphoreTake(s_mutex_motor, pdMS_TO_TICKS(20)) == pdTRUE) {
        s_motor.modo = MODO_SIM_MANUAL;
        s_motor.estado_actual = ESTADO_ECU_MANUAL;
        s_acelerador_manual_target = (acelerador_pct > 100) ? 100 : acelerador_pct;
        s_freno_manual_activo = freno_activo;
        xSemaphoreGive(s_mutex_motor);
    }
}

void fsm_ecu_set_preset(int preset_id)
{
    if (s_mutex_motor && xSemaphoreTake(s_mutex_motor, pdMS_TO_TICKS(20)) == pdTRUE) {
        s_motor.modo = MODO_SIM_MANUAL;
        s_motor.estado_actual = ESTADO_ECU_MANUAL;
        s_freno_manual_activo = false;

        switch (preset_id) {
        case 1: // Ralenti
            s_acelerador_manual_target = 0;
            break;
        case 2: // Ciudad (40 km/h)
            s_acelerador_manual_target = 25;
            break;
        case 3: // Crucero (95 km/h)
            s_acelerador_manual_target = 50;
            break;
        case 4: // Pista / Maxima aceleracion
            s_acelerador_manual_target = 100;
            break;
        default:
            break;
        }
        xSemaphoreGive(s_mutex_motor);
    }
}
