/**
 * ============================================================================
 * ARCHIVO: servidor_web_c6.c (ESP32-C6)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Panel interactivo del conductor (Cockpit Web en 192.168.4.2). Ofrece pedales
 * virtuales y presets de manejo para controlar interactivamente el motor simulado.
 * ============================================================================
 */
#include "servidor_web_c6.h"
#include "fsm_ecu.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WEB_C6";
static uint32_t s_contador_tx = 0;

static const char HTML_COCKPIT[] =
"<!DOCTYPE html><html lang='es'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>AUTOlog - Simulador del Auto (ESP32-C6)</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0;}"
"body{background:#0b0f19;color:#f1f5f9;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;padding:16px;display:flex;justify-content:center;}"
".contenedor{max-width:880px;width:100%;display:flex;flex-direction:column;gap:16px;}"
".cabecera{background:#111827;border:1px solid #1f2937;border-radius:12px;padding:16px 20px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;}"
".titulo{display:flex;align-items:center;gap:12px;}"
".titulo h1{font-size:1.4rem;font-weight:800;color:#a855f7;letter-spacing:-0.02em;margin:0;}"
".badge{font-size:0.75rem;font-weight:700;padding:4px 10px;border-radius:6px;text-transform:uppercase;letter-spacing:0.05em;}"
".badge-c6{background:#7e22ce;color:#f3e8ff;}"
".badge-tx{background:#15803d;color:#dcfce7;animation:pulso 2s infinite;}"
"@keyframes pulso{0%,100%{opacity:1;}50%{opacity:0.6;}}"
".seccion-relojes{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px;}"
".reloj-card{background:#111827;border:1px solid #1f2937;border-radius:12px;padding:20px;display:flex;flex-direction:column;align-items:center;position:relative;}"
".reloj-svg{width:200px;height:120px;overflow:visible;}"
".reloj-fondo{fill:none;stroke:#1f2937;stroke-width:14;stroke-linecap:round;}"
".reloj-progreso{fill:none;stroke-width:14;stroke-linecap:round;transition:stroke-dashoffset 0.1s ease-out;}"
".rpm-prog{stroke:#a855f7;}"
".vel-prog{stroke:#38bdf8;}"
".reloj-centro{position:absolute;top:70px;text-align:center;}"
".reloj-valor{font-size:2.2rem;font-weight:900;line-height:1;letter-spacing:-0.03em;}"
".reloj-unidad{font-size:0.85rem;font-weight:600;color:#94a3b8;margin-top:2px;}"
".reloj-subtitulo{margin-top:8px;font-size:0.85rem;color:#cbd5e1;font-weight:600;text-transform:uppercase;letter-spacing:0.05em;}"
".card{background:#111827;border:1px solid #1f2937;border-radius:12px;padding:20px;display:flex;flex-direction:column;gap:14px;}"
".barra-item{display:flex;flex-direction:column;gap:6px;}"
".barra-encabezado{display:flex;justify-content:space-between;font-size:0.85rem;font-weight:600;color:#cbd5e1;}"
".barra-fondo{background:#1f2937;border-radius:8px;height:14px;overflow:hidden;position:relative;}"
".barra-relleno{height:100%;border-radius:8px;transition:width 0.1s ease-out;}"
".grid-botones{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:10px;}"
".btn{padding:12px 18px;border-radius:8px;font-weight:700;font-size:0.88rem;cursor:pointer;border:none;text-decoration:none;display:inline-flex;align-items:center;justify-content:center;gap:8px;transition:all 0.15s;text-align:center;}"
".btn-modo{background:#1e293b;color:#cbd5e1;border:2px solid transparent;}"
".btn-modo.activo{background:#7e22ce;color:#fff;border-color:#c084fc;}"
".btn-preset{background:#1f2937;color:#e2e8f0;}"
".btn-preset:hover{background:#374151;}"
".btn-freno{background:#b91c1c;color:#fff;font-size:1.05rem;padding:16px;user-select:none;}"
".btn-freno:active,.btn-freno.presionado{background:#ef4444;box-shadow:0 0 15px #ef4444;}"
".btn-link{background:#0284c7;color:#fff;}"
".btn-link:hover{background:#0369a1;}"
".slider-contenedor{display:flex;flex-direction:column;gap:8px;padding:10px 0;}"
"input[type=range]{-webkit-appearance:none;width:100%;height:12px;border-radius:6px;background:#1f2937;outline:none;cursor:pointer;}"
"input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:26px;height:26px;border-radius:50%;background:#a855f7;cursor:pointer;box-shadow:0 0 8px #a855f7;}"
".fsm-badge{display:inline-block;padding:4px 10px;border-radius:6px;font-size:0.8rem;font-weight:800;}"
"</style></head><body>"
"<div class='contenedor'>"
"  <div class='cabecera'>"
"    <div class='titulo'>"
"      <h1>AUTOlog Simulador</h1>"
"      <span class='badge badge-c6'>ESP32-C6 ECU</span>"
"      <span class='badge badge-tx'>100 Hz EN VIVO</span>"
"    </div>"
"    <div style='font-size:0.85rem;color:#94a3b8;'>"
"      TX GPIO 18 &rarr; S3 GPIO 13: <span id='tx_count' style='color:#a855f7;font-weight:700;'>0</span> tramas"
"    </div>"
"  </div>"
"  <div class='seccion-relojes'>"
"    <div class='reloj-card'>"
"      <svg class='reloj-svg' viewBox='0 0 200 120'>"
"        <path class='reloj-fondo' d='M 20 110 A 80 80 0 0 1 180 110'></path>"
"        <path id='rpm_arc' class='reloj-progreso rpm-prog' d='M 20 110 A 80 80 0 0 1 180 110' stroke-dasharray='251.3' stroke-dashoffset='251.3'></path>"
"      </svg>"
"      <div class='reloj-centro'>"
"        <div class='reloj-valor' id='rpm_val' style='color:#c084fc;'>800</div>"
"        <div class='reloj-unidad'>RPM</div>"
"      </div>"
"      <div class='reloj-subtitulo'>Tacómetro (RPM)</div>"
"    </div>"
"    <div class='reloj-card'>"
"      <svg class='reloj-svg' viewBox='0 0 200 120'>"
"        <path class='reloj-fondo' d='M 20 110 A 80 80 0 0 1 180 110'></path>"
"        <path id='vel_arc' class='reloj-progreso vel-prog' d='M 20 110 A 80 80 0 0 1 180 110' stroke-dasharray='251.3' stroke-dashoffset='251.3'></path>"
"      </svg>"
"      <div class='reloj-centro'>"
"        <div class='reloj-valor' id='vel_val' style='color:#38bdf8;'>0</div>"
"        <div class='reloj-unidad'>KM/H</div>"
"      </div>"
"      <div class='reloj-subtitulo'>Velocidad (KM/H)</div>"
"    </div>"
"  </div>"
"  <div class='card'>"
"    <div style='display:flex;justify-content:space-between;align-items:center;'>"
"      <span style='font-size:0.9rem;font-weight:700;color:#cbd5e1;'>MODO DE OPERACIÓN</span>"
"      <span id='fsm_badge' class='fsm-badge' style='background:#7e22ce;color:#fff;'>AUTOMÁTICO</span>"
"    </div>"
"    <div class='grid-botones'>"
"      <button id='btn_modo_auto' class='btn btn-modo activo' onclick='setModo(0)'>🔄 Automático (FSM)</button>"
"      <button id='btn_modo_manual' class='btn btn-modo' onclick='setModo(1)'>🕹️ Control Manual</button>"
"    </div>"
"  </div>"
"  <div class='card' id='panel_manual'>"
"    <span style='font-size:0.9rem;font-weight:700;color:#cbd5e1;'>CONTROLES DEL VEHÍCULO</span>"
"    <div class='slider-contenedor'>"
"      <div class='barra-encabezado'><span>Pedal Acelerador (TPS)</span><span id='slider_txt'>0%</span></div>"
"      <input type='range' id='tps_slider' min='0' max='100' value='0' oninput='onSliderChange(this.value)' onchange='onSliderChange(this.value)'>"
"    </div>"
"    <div class='grid-botones'>"
"      <button class='btn btn-preset' onclick='setPreset(1)'>🅿️ Ralentí</button>"
"      <button class='btn btn-preset' onclick='setPreset(2)'>🏙️ Ciudad (40 km/h)</button>"
"      <button class='btn btn-preset' onclick='setPreset(3)'>🛣️ Crucero (95 km/h)</button>"
"      <button class='btn btn-preset' onclick='setPreset(4)'>🏎️ Fondo (140+ km/h)</button>"
"    </div>"
"    <button id='btn_freno' class='btn btn-freno' onmousedown='frenar(true)' onmouseup='frenar(false)' ontouchstart='frenar(true)' ontouchend='frenar(false)'>🛑 PISAR PEDAL DE FRENO</button>"
"  </div>"
"  <div class='card'>"
"    <div class='barra-item'>"
"      <div class='barra-encabezado'><span>Apertura Mariposa (TPS Real)</span><span id='tps_txt'>0%</span></div>"
"      <div class='barra-fondo'><div id='tps_bar' class='barra-relleno' style='width:0%;background:#c084fc;'></div></div>"
"    </div>"
"    <div class='barra-item'>"
"      <div class='barra-encabezado'><span>Temperatura Refrigerante</span><span id='temp_txt'>85 °C</span></div>"
"      <div class='barra-fondo'><div id='temp_bar' class='barra-relleno' style='width:30%;background:#f59e0b;'></div></div>"
"    </div>"
"  </div>"
"  <div class='card' style='display:flex;flex-direction:row;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;'>"
"    <div>"
"      <div style='font-size:0.85rem;color:#94a3b8;'>Destino CAN: ESP32-S3 Datalogger (192.168.4.1)</div>"
"      <div style='font-size:0.75rem;color:#64748b;'>Persistencia SPIFFS CSV + Telemetría Dual-Core</div>"
"    </div>"
"    <a href='http://192.168.4.1' target='_blank' class='btn btn-link'>📊 Abrir Telemetr&iacute;a (ESP32-S3) &rarr;</a>"
"  </div>"
"</div>"
"<script>"
"const FSM_NOMBRES = ['INICIALIZANDO', 'RALENTÍ', 'ACELERANDO', 'CRUCERO', 'FRENANDO', 'MANUAL'];"
"const FSM_COLORES = ['#475569', '#64748b', '#0284c7', '#10b981', '#f59e0b', '#7e22ce'];"
"const LONG_ARCO = 251.3;"
"let modoActual = 0;"
"let frenoActivo = false;"
"async function actualizar(){"
"  try {"
"    let r = await fetch('/api/estado');"
"    let d = await r.json();"
"    document.getElementById('rpm_val').innerText = d.rpm;"
"    let rpmPct = Math.min(1, d.rpm / 7000);"
"    document.getElementById('rpm_arc').style.strokeDashoffset = LONG_ARCO * (1 - rpmPct);"
"    document.getElementById('vel_val').innerText = d.velocidad;"
"    let velPct = Math.min(1, d.velocidad / 160);"
"    document.getElementById('vel_arc').style.strokeDashoffset = LONG_ARCO * (1 - velPct);"
"    document.getElementById('tps_txt').innerText = d.tps + '%';"
"    document.getElementById('tps_bar').style.width = d.tps + '%';"
"    document.getElementById('temp_txt').innerText = d.temp + ' °C';"
"    let tempPct = Math.min(100, (d.temp / 120) * 100);"
"    document.getElementById('temp_bar').style.width = tempPct + '%';"
"    document.getElementById('tx_count').innerText = d.tx_count;"
"    let fsmNom = FSM_NOMBRES[d.fsm] || ('MODO ' + d.fsm);"
"    let fsmCol = FSM_COLORES[d.fsm] || '#7e22ce';"
"    let badge = document.getElementById('fsm_badge');"
"    badge.innerText = fsmNom;"
"    badge.style.background = fsmCol;"
"    if (d.modo !== modoActual) {"
"      modoActual = d.modo;"
"      document.getElementById('btn_modo_auto').className = 'btn btn-modo' + (modoActual === 0 ? ' activo' : '');"
"      document.getElementById('btn_modo_manual').className = 'btn btn-modo' + (modoActual === 1 ? ' activo' : '');"
"    }"
"  } catch(e) {}"
"}"
"async function setModo(m){"
"  modoActual = m;"
"  document.getElementById('btn_modo_auto').className = 'btn btn-modo' + (m === 0 ? ' activo' : '');"
"  document.getElementById('btn_modo_manual').className = 'btn btn-modo' + (m === 1 ? ' activo' : '');"
"  await fetch('/api/control?modo=' + m);"
"}"
"async function onSliderChange(v){"
"  document.getElementById('slider_txt').innerText = v + '%';"
"  await fetch('/api/control?modo=1&tps=' + v + '&freno=0');"
"}"
"async function setPreset(p){"
"  await fetch('/api/control?preset=' + p);"
"  const val = [0, 0, 25, 50, 100][p] || 0;"
"  document.getElementById('tps_slider').value = val;"
"  document.getElementById('slider_txt').innerText = val + '%';"
"}"
"async function frenar(activo){"
"  frenoActivo = activo;"
"  let btn = document.getElementById('btn_freno');"
"  if (activo) {"
"    btn.classList.add('presionado');"
"    document.getElementById('tps_slider').value = 0;"
"    document.getElementById('slider_txt').innerText = '0%';"
"    await fetch('/api/control?modo=1&tps=0&freno=1');"
"  } else {"
"    btn.classList.remove('presionado');"
"    await fetch('/api/control?modo=1&tps=0&freno=0');"
"  }"
"}"
"setInterval(actualizar, 150);"
"actualizar();"
"</script></body></html>";

void servidor_web_c6_actualizar_tx(uint32_t contador_tx)
{
    s_contador_tx = contador_tx;
}

static esp_err_t manejador_raiz(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, HTML_COCKPIT, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t manejador_api_estado(httpd_req_t *req)
{
    variables_motor_t m = fsm_ecu_obtener_variables();
    char resp[256];
    snprintf(resp, sizeof(resp),
             "{\"rpm\":%u,\"velocidad\":%u,\"temp\":%u,\"tps\":%u,\"fsm\":%u,\"modo\":%u,\"tx_count\":%lu}",
             m.rpm, m.velocidad_kmh, m.temperatura_c, m.acelerador_pct,
             m.estado_actual, m.modo, s_contador_tx);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t manejador_api_control(httpd_req_t *req)
{
    char query[128];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char param[32];

        // 1. Parametro preset
        if (httpd_query_key_value(query, "preset", param, sizeof(param)) == ESP_OK) {
            int preset = atoi(param);
            fsm_ecu_set_preset(preset);
        }

        // 2. Parametro modo
        if (httpd_query_key_value(query, "modo", param, sizeof(param)) == ESP_OK) {
            int modo = atoi(param);
            fsm_ecu_set_modo((modo_simulacion_t)modo);
        }

        // 3. Parametro tps y freno
        if (httpd_query_key_value(query, "tps", param, sizeof(param)) == ESP_OK) {
            uint8_t tps = (uint8_t)atoi(param);
            bool freno = false;
            char freno_str[16];
            if (httpd_query_key_value(query, "freno", freno_str, sizeof(freno_str)) == ESP_OK) {
                freno = (atoi(freno_str) == 1);
            }
            fsm_ecu_set_control_manual(tps, freno);
        }
    }

    const char *resp = "{\"estado\":\"ok\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Conectando al AP 'AUTOlog_PoC' del S3...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Desconectado de AUTOlog_PoC. Reintentando conexion...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "ESP32-C6 conectado con IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

esp_err_t servidor_web_c6_init(void)
{
    // 1. Iniciar subsistema de red TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *netif_sta = esp_netif_create_default_wifi_sta();

    // 2. Asignar IP fija 192.168.4.2 para que coincida exactamente con el boton del S3
    esp_netif_dhcpc_stop(netif_sta);
    esp_netif_ip_info_t ip_info;
    esp_netif_set_ip4_addr(&ip_info.ip, 192, 168, 4, 2);
    esp_netif_set_ip4_addr(&ip_info.gw, 192, 168, 4, 1);
    esp_netif_set_ip4_addr(&ip_info.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif_sta, &ip_info));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "AUTOlog_PoC",
            .password = "",
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 3. Iniciar servidor HTTP en el C6
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_raiz = {.uri = "/", .method = HTTP_GET, .handler = manejador_raiz};
        httpd_register_uri_handler(server, &uri_raiz);

        httpd_uri_t uri_estado = {.uri = "/api/estado", .method = HTTP_GET, .handler = manejador_api_estado};
        httpd_register_uri_handler(server, &uri_estado);

        httpd_uri_t uri_control = {.uri = "/api/control", .method = HTTP_GET, .handler = manejador_api_control};
        httpd_register_uri_handler(server, &uri_control);

        ESP_LOGI(TAG, "Servidor Cockpit HTTP operativo en puerto 80 (IP 192.168.4.2).");
    }

    return ESP_OK;
}
