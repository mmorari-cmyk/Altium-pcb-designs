/**
 * ============================================================================
 * ARCHIVO: servidor_web.c (ESP32-S3)
 * ----------------------------------------------------------------------------
 * FUNCIÓN PRINCIPAL:
 * Panel de Telemetría Web y Diagnóstico en Vivo:
 * 1. Sirve la interfaz gráfica en tiempo real (192.168.4.1) con relojes a 100 Hz.
 * 2. Muestra alertas de seguridad vehicular y estado del búfer de RAM.
 * 3. Permite la descarga directa del archivo autolog.csv.
 * 4. Analizador Post-Viaje: evalúa cualquier archivo CSV subido (Verde, Amarillo o Rojo).
 * ============================================================================
 */
#include "servidor_web.h"
#include "telemetria_tipos.h"
#include "almacen_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SERVIDOR_WEB";

static const char PAGINA_HTML[] = 
"<!DOCTYPE html><html lang='es'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>AUTOlog - Diagnóstico y Telemetría Vehicular</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0;}"
"body{background:#090d16;color:#f8fafc;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;padding:16px;display:flex;justify-content:center;min-height:100vh;}"
".app{max-width:920px;width:100%;display:flex;flex-direction:column;gap:16px;}"
".header{background:#111827;border:1px solid #1f2937;border-radius:14px;padding:18px 24px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:12px;box-shadow:0 4px 20px rgba(0,0,0,0.4);}"
".brand{display:flex;align-items:center;gap:14px;}"
".brand-icon{width:42px;height:42px;background:linear-gradient(135deg,#0284c7,#38bdf8);border-radius:10px;display:flex;align-items:center;justify-content:center;font-size:1.4rem;box-shadow:0 2px 10px rgba(56,189,248,0.3);}"
".brand h1{font-size:1.45rem;font-weight:900;letter-spacing:-0.03em;color:#f8fafc;margin:0;line-height:1.1;}"
".brand p{font-size:0.75rem;color:#94a3b8;font-weight:600;letter-spacing:0.04em;text-transform:uppercase;margin-top:2px;}"
".live-badge{background:rgba(16,185,129,0.15);border:1px solid rgba(16,185,129,0.3);color:#34d399;padding:6px 14px;border-radius:20px;font-size:0.75rem;font-weight:800;letter-spacing:0.06em;display:flex;align-items:center;gap:8px;text-transform:uppercase;}"
".live-dot{width:8px;height:8px;background:#10b981;border-radius:50%;box-shadow:0 0 8px #10b981;animation:pulse 1.8s infinite;}"
"@keyframes pulse{0%,100%{opacity:1;transform:scale(1);}50%{opacity:0.4;transform:scale(0.8);}}"
".banner-alerta{border-radius:14px;padding:20px 24px;display:flex;align-items:center;gap:18px;border:1px solid transparent;transition:all 0.3s ease;}"
".banner-ok{background:rgba(16,185,129,0.08);border-color:rgba(16,185,129,0.3);}"
".banner-warn{background:rgba(245,158,11,0.1);border-color:rgba(245,158,11,0.4);}"
".banner-danger{background:rgba(239,68,68,0.12);border-color:rgba(239,68,68,0.5);animation:glow 1.5s infinite;}"
"@keyframes glow{0%,100%{box-shadow:0 0 15px rgba(239,68,68,0.2);}50%{box-shadow:0 0 25px rgba(239,68,68,0.4);}}"
".banner-ico{font-size:2.4rem;line-height:1;}"
".banner-txt h2{font-size:1.15rem;font-weight:900;letter-spacing:-0.02em;margin:0 0 4px 0;}"
".banner-txt p{font-size:0.88rem;color:#cbd5e1;line-height:1.4;margin:0;}"
".grid-relojes{display:grid;grid-template-columns:repeat(auto-fit,minmax(270px,1fr));gap:16px;}"
".card{background:#111827;border:1px solid #1f2937;border-radius:14px;padding:22px;display:flex;flex-direction:column;position:relative;box-shadow:0 4px 15px rgba(0,0,0,0.3);}"
".reloj-card{align-items:center;}"
".reloj-svg{width:220px;height:125px;overflow:visible;}"
".reloj-fondo{fill:none;stroke:#1e293b;stroke-width:14;stroke-linecap:round;}"
".reloj-progreso{fill:none;stroke-width:14;stroke-linecap:round;transition:stroke-dashoffset 0.15s ease-out;}"
".reloj-centro{position:absolute;top:76px;text-align:center;}"
".reloj-num{font-size:2.6rem;font-weight:900;line-height:1;letter-spacing:-0.04em;}"
".reloj-lbl{font-size:0.8rem;font-weight:700;color:#94a3b8;margin-top:2px;letter-spacing:0.05em;}"
".card-titulo{font-size:0.85rem;font-weight:800;color:#cbd5e1;text-transform:uppercase;letter-spacing:0.06em;margin-top:10px;text-align:center;}"
".grid-indicadores{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;}"
".param-card{gap:10px;}"
".param-header{display:flex;justify-content:space-between;align-items:center;}"
".param-nombre{font-size:0.85rem;font-weight:700;color:#94a3b8;text-transform:uppercase;letter-spacing:0.05em;}"
".param-valor{font-size:1.35rem;font-weight:900;}"
".barra-fondo{background:#1e293b;border-radius:6px;height:10px;overflow:hidden;position:relative;}"
".barra-progreso{height:100%;border-radius:6px;transition:width 0.2s ease-out;}"
".param-estado{font-size:0.75rem;font-weight:700;color:#64748b;display:flex;align-items:center;gap:6px;}"
".card-resumen{background:#111827;border:1px solid #1f2937;border-radius:14px;padding:22px;display:flex;flex-direction:column;gap:16px;}"
".resumen-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:14px;}"
".resumen-item{background:#0d131f;border:1px solid #1e293b;border-radius:10px;padding:14px;display:flex;flex-direction:column;gap:4px;}"
".resumen-sub{font-size:0.72rem;font-weight:700;color:#94a3b8;text-transform:uppercase;letter-spacing:0.05em;}"
".resumen-val{font-size:1.4rem;font-weight:900;color:#38bdf8;}"
".panel-descarga{background:linear-gradient(135deg,rgba(2,132,199,0.12),rgba(14,165,233,0.05));border:1px solid rgba(56,189,248,0.3);border-radius:14px;padding:22px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:16px;}"
".descarga-info h3{font-size:1.15rem;font-weight:900;color:#f8fafc;margin:0 0 4px 0;}"
".descarga-info p{font-size:0.85rem;color:#94a3b8;margin:0;}"
".btn-dl{background:linear-gradient(135deg,#0284c7,#0ea5e9);color:#fff;font-weight:800;font-size:0.95rem;padding:14px 28px;border-radius:10px;text-decoration:none;display:inline-flex;align-items:center;gap:10px;box-shadow:0 4px 15px rgba(2,132,199,0.4);transition:all 0.2s ease;}"
".btn-dl:hover{transform:translateY(-1px);box-shadow:0 6px 20px rgba(2,132,199,0.6);}"
".btn-poc{background:rgba(245,158,11,0.12);border:1px solid rgba(245,158,11,0.3);color:#fbbf24;font-weight:700;font-size:0.82rem;padding:8px 14px;border-radius:8px;cursor:pointer;display:inline-flex;align-items:center;gap:6px;transition:all 0.15s;}"
".btn-poc:hover{background:rgba(245,158,11,0.25);}"
"</style></head><body>"
"<div class='app'>"
"  <header class='header'>"
"    <div class='brand'>"
"      <div class='brand-icon'>🏎️</div>"
"      <div>"
"        <h1>AUTOlog Diagnostic</h1>"
"        <p>Monitor de Salud Vehicular &amp; Datalogger</p>"
"      </div>"
"    </div>"
"    <div class='live-badge'>"
"      <span class='live-dot'></span>"
"      <span>Enlace Activo 100 Hz</span>"
"    </div>"
"  </header>"
"  <section id='banner_salud' class='banner-alerta banner-ok'>"
"    <div id='banner_ico' class='banner-ico'>✅</div>"
"    <div class='banner-txt'>"
"      <h2 id='banner_titulo' style='color:#34d399;'>SISTEMA EN CONDICIÓN NORMAL</h2>"
"      <p id='banner_desc'>Todos los parámetros mecánicos y térmicos se encuentran dentro del rango de operación seguro.</p>"
"    </div>"
"  </section>"
"  <section class='grid-relojes'>"
"    <div class='card reloj-card'>"
"      <svg class='reloj-svg' viewBox='0 0 220 125'>"
"        <path class='reloj-fondo' d='M 20 115 A 90 90 0 0 1 200 115'></path>"
"        <path id='rpm_arc' class='reloj-progreso' d='M 20 115 A 90 90 0 0 1 200 115' stroke='#38bdf8' stroke-dasharray='282.7' stroke-dashoffset='282.7'></path>"
"      </svg>"
"      <div class='reloj-centro'>"
"        <div id='rpm_val' class='reloj-num' style='color:#38bdf8;'>0</div>"
"        <div class='reloj-lbl'>RPM MOTOR</div>"
"      </div>"
"      <div class='card-titulo'>Régimen de Giro</div>"
"    </div>"
"    <div class='card reloj-card'>"
"      <svg class='reloj-svg' viewBox='0 0 220 125'>"
"        <path class='reloj-fondo' d='M 20 115 A 90 90 0 0 1 200 115'></path>"
"        <path id='vel_arc' class='reloj-progreso' d='M 20 115 A 90 90 0 0 1 200 115' stroke='#34d399' stroke-dasharray='282.7' stroke-dashoffset='282.7'></path>"
"      </svg>"
"      <div class='reloj-centro'>"
"        <div id='vel_val' class='reloj-num' style='color:#34d399;'>0</div>"
"        <div class='reloj-lbl'>KM / H</div>"
"      </div>"
"      <div class='card-titulo'>Velocidad Vehículo</div>"
"    </div>"
"  </section>"
"  <section class='grid-indicadores'>"
"    <div class='card param-card'>"
"      <div class='param-header'>"
"        <span class='param-nombre'>Refrigerante</span>"
"        <span id='temp_val' class='param-valor' style='color:#38bdf8;'>-- °C</span>"
"      </div>"
"      <div class='barra-fondo'><div id='temp_bar' class='barra-progreso' style='width:0%;background:#38bdf8;'></div></div>"
"      <div id='temp_status' class='param-estado'>● Temperatura Óptima (&lt;95°C)</div>"
"    </div>"
"    <div class='card param-card'>"
"      <div class='param-header'>"
"        <span class='param-nombre'>Acelerador (TPS)</span>"
"        <span id='tps_val' class='param-valor' style='color:#a855f7;'>0%</span>"
"      </div>"
"      <div class='barra-fondo'><div id='tps_bar' class='barra-progreso' style='width:0%;background:#a855f7;'></div></div>"
"      <div id='tps_status' class='param-estado'>● Mariposa Cerrada (Ralentí)</div>"
"    </div>"
"    <div class='card param-card'>"
"      <div class='param-header'>"
"        <span class='param-nombre'>Modo de Manejo</span>"
"        <span id='fsm_val' class='param-valor' style='color:#38bdf8;font-size:1.1rem;'>INICIANDO</span>"
"      </div>"
"      <div class='barra-fondo'><div id='fsm_bar' class='barra-progreso' style='width:100%;background:#38bdf8;'></div></div>"
"      <div id='fsm_sub' class='param-estado'>● Computadora de a bordo activa</div>"
"    </div>"
"  </section>"
"  <section class='card-resumen'>"
"    <div style='display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;'>"
"      <h3 style='font-size:1rem;font-weight:900;color:#f8fafc;letter-spacing:-0.02em;'>Estado del Registrador y Caja Negra</h3>"
"    </div>"
"    <div class='resumen-grid'>"
"      <div class='resumen-item'>"
"        <span class='resumen-sub'>Muestras en Memoria</span>"
"        <span id='rx_count' class='resumen-val'>0</span>"
"      </div>"
"      <div class='resumen-item'>"
"        <span class='resumen-sub'>Integridad del Viaje</span>"
"        <span id='perdidas_txt' class='resumen-val' style='color:#10b981;'>100% (0 pérdidas)</span>"
"      </div>"
"      <div class='resumen-item'>"
"        <span class='resumen-sub'>Búfer Anti-Caída FreeRTOS</span>"
"        <span id='buffer_txt' class='resumen-val' style='color:#38bdf8;'>0 / 250</span>"
"      </div>"
"      <div class='resumen-item'>"
"        <span class='resumen-sub'>Frecuencia de Registro</span>"
"        <span class='resumen-val' style='color:#34d399;'>100 Hz (10 ms)</span>"
"      </div>"
"    </div>"
"  </section>"
"  <section class='panel-descarga'>"
"    <div class='descarga-info'>"
"      <h3>Auditoría y Análisis de Telemetría</h3>"
"      <p>Descargá el registro completo del recorrido en formato CSV para graficar o evaluar en Excel.</p>"
"    </div>"
"    <a href='/descargar' class='btn-dl'>📥 Descargar Registro del Viaje (CSV)</a>"
"  </section>"
"  <section class='card' style='border:1px solid #38bdf8;padding:20px;gap:12px;'>"
"    <div>"
"      <h3 style='font-size:1.05rem;font-weight:900;color:#38bdf8;'>🧪 Análisis Post-Viaje: Cargar archivo CSV</h3>"
"      <p style='font-size:0.8rem;color:#94a3b8;margin-top:2px;'>Arrastrá o seleccioná un archivo .csv para evaluar el estado del motor (Verde, Amarillo o Rojo).</p>"
"    </div>"
"    <input type='file' id='input_csv' accept='.csv' style='display:none;' onchange='procesarCSV(event)'>"
"    <div id='drop_zone' ondragover='event.preventDefault();this.style.borderColor=\"#38bdf8\";' ondragleave='this.style.borderColor=\"#334155\";' ondrop='event.preventDefault();this.style.borderColor=\"#334155\";procesarArchivo(event.dataTransfer.files[0]);' onclick=\"document.getElementById('input_csv').click()\" style='border:2px dashed #334155;border-radius:10px;padding:20px;text-align:center;cursor:pointer;background:#0d131f;transition:border-color 0.2s;'>"
"      <div style='font-size:1.8rem;margin-bottom:4px;'>📂</div>"
"      <div style='font-size:0.9rem;font-weight:800;color:#f8fafc;'>Arrastrá tu archivo .csv acá o hacé clic para buscarlo</div>"
"      <div style='font-size:0.74rem;color:#94a3b8;margin-top:2px;'>Evalúa automáticamente RPM ≥ 5000 / Temp ≥ 95°C (Amarillo) y RPM ≥ 6200 / Temp ≥ 105°C (Rojo)</div>"
"    </div>"
"    <div id='resultado_csv' style='display:none;background:#0d131f;border:1px solid #1e293b;border-radius:10px;padding:16px;margin-top:4px;'>"
"      <div id='res_banner' style='border-radius:8px;padding:12px 16px;font-weight:900;font-size:0.95rem;display:flex;align-items:center;gap:10px;'></div>"
"      <div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:10px;margin-top:12px;' id='res_stats'></div>"
"    </div>"
"  </section>"
"</div>"
"<script>"
"function procesarCSV(e){ if(e.target.files && e.target.files[0]) procesarArchivo(e.target.files[0]); }"
"function procesarArchivo(file){"
"  if(!file)return;"
"  let reader=new FileReader();"
"  reader.onload=function(evt){"
"    let lines=evt.target.result.split('\\n');"
"    let total=0,rojas=0,amarillas=0,verdes=0;"
"    let maxRpm=0,maxTemp=0,maxVel=0;"
"    for(let i=1;i<lines.length;i++){"
"      let line=lines[i].trim();"
"      if(!line)continue;"
"      let p=line.split(',');"
"      if(p.length<5)continue;"
"      let rpm=parseInt(p[2])||0;"
"      let vel=parseInt(p[3])||0;"
"      let temp=parseInt(p[4])||0;"
"      total++;"
"      if(rpm>maxRpm)maxRpm=rpm;"
"      if(temp>maxTemp)maxTemp=temp;"
"      if(vel>maxVel)maxVel=vel;"
"      if(rpm>=6200||temp>=105)rojas++;"
"      else if(rpm>=5000||temp>=95)amarillas++;"
"      else verdes++;"
"    }"
"    if(total===0)return;"
"    let resDiv=document.getElementById('resultado_csv');"
"    let resBanner=document.getElementById('res_banner');"
"    let resStats=document.getElementById('res_stats');"
"    resDiv.style.display='block';"
"    if(rojas>0){"
"      resBanner.style.background='rgba(239,68,68,0.2)';"
"      resBanner.style.border='1px solid #ef4444';"
"      resBanner.style.color='#f87171';"
"      resBanner.innerHTML='🚨 RESULTADO: ROJO (CRÍTICO) — Detectadas ' + rojas + ' tramas en zona crítica (RPM ≥ 6200 o Temp ≥ 105°C).';"
"    }else if(amarillas>0){"
"      resBanner.style.background='rgba(245,158,11,0.2)';"
"      resBanner.style.border='1px solid #f59e0b';"
"      resBanner.style.color='#fbbf24';"
"      resBanner.innerHTML='⚠️ RESULTADO: AMARILLO (ALTA EXIGENCIA) — Detectadas ' + amarillas + ' tramas de alta carga (RPM ≥ 5000 o Temp ≥ 95°C).';"
"    }else{"
"      resBanner.style.background='rgba(16,185,129,0.2)';"
"      resBanner.style.border='1px solid #10b981';"
"      resBanner.style.color='#34d399';"
"      resBanner.innerHTML='✅ RESULTADO: VERDE (NORMAL) — Todas las ' + total.toLocaleString() + ' tramas operaron dentro de rangos seguros.';"
"    }"
"    resStats.innerHTML="
"      '<div class=\"resumen-item\"><span class=\"resumen-sub\">Total Muestras</span><span class=\"resumen-val\" style=\"font-size:1.1rem;\">'+total.toLocaleString()+'</span></div>'+"
"      '<div class=\"resumen-item\"><span class=\"resumen-sub\">Pico RPM</span><span class=\"resumen-val\" style=\"font-size:1.1rem;color:'+(maxRpm>=6200?'#ef4444':(maxRpm>=5000?'#fbbf24':'#38bdf8'))+'\">'+maxRpm+'</span></div>'+"
"      '<div class=\"resumen-item\"><span class=\"resumen-sub\">Pico Temp</span><span class=\"resumen-val\" style=\"font-size:1.1rem;color:'+(maxTemp>=105?'#ef4444':(maxTemp>=95?'#fbbf24':'#34d399'))+'\">'+maxTemp+' °C</span></div>'+"
"      '<div class=\"resumen-item\"><span class=\"resumen-sub\">Velocidad Máx</span><span class=\"resumen-val\" style=\"font-size:1.1rem;color:#38bdf8;\">'+maxVel+' km/h</span></div>';"
"  };"
"  reader.readAsText(file);"
"}"
"const FSM_NOMBRES = ['INICIALIZANDO', 'RALENTÍ', 'ACELERANDO', 'VELOCIDAD CRUCERO', 'FRENADA REGENERATIVA', 'CONTROL MANUAL', 'ALTA EXIGENCIA', 'LÍMITE CRÍTICO'];"
"const LONG_ARCO = 282.7;"
"async function actualizar(){"
"  try {"
"    let r = await fetch('/api/estadisticas');"
"    let d = await r.json();"
"    document.getElementById('rpm_val').innerText = d.rpm;"
"    let rpmPct = Math.min(1, d.rpm / 7500);"
"    let rpmArc = document.getElementById('rpm_arc');"
"    rpmArc.style.strokeDashoffset = LONG_ARCO * (1 - rpmPct);"
"    rpmArc.style.stroke = d.rpm > 6000 ? '#ef4444' : (d.rpm > 4500 ? '#f59e0b' : '#38bdf8');"
"    document.getElementById('rpm_val').style.color = rpmArc.style.stroke;"
"    document.getElementById('vel_val').innerText = d.velocidad;"
"    let velPct = Math.min(1, d.velocidad / 160);"
"    document.getElementById('vel_arc').style.strokeDashoffset = LONG_ARCO * (1 - velPct);"
"    document.getElementById('tps_val').innerText = d.tps + '%';"
"    document.getElementById('tps_bar').style.width = d.tps + '%';"
"    let tpsStatus = document.getElementById('tps_status');"
"    if (d.tps == 0) tpsStatus.innerText = '● Mariposa Cerrada (Ralentí)';"
"    else if (d.tps < 40) tpsStatus.innerText = '● Carga Parcial (Crucero/Ciudad)';"
"    else if (d.tps < 80) tpsStatus.innerText = '● Fuerte Demanda de Potencia';"
"    else tpsStatus.innerText = '● WOT: Acelerador al 100%';"
"    document.getElementById('temp_val').innerText = d.temp + ' °C';"
"    let tempPct = Math.min(100, (d.temp / 125) * 100);"
"    let tbar = document.getElementById('temp_bar');"
"    tbar.style.width = tempPct + '%';"
"    let tstatus = document.getElementById('temp_status');"
"    if (d.temp > 102) {"
"      tbar.style.background = '#ef4444';"
"      document.getElementById('temp_val').style.color = '#ef4444';"
"      tstatus.style.color = '#ef4444';"
"      tstatus.innerText = '● ¡PELIGRO! Sobrecalentamiento Crítico';"
"    } else if (d.temp > 92) {"
"      tbar.style.background = '#f59e0b';"
"      document.getElementById('temp_val').style.color = '#f59e0b';"
"      tstatus.style.color = '#f59e0b';"
"      tstatus.innerText = '● Advertencia: Temperatura Elevada';"
"    } else {"
"      tbar.style.background = '#38bdf8';"
"      document.getElementById('temp_val').style.color = '#38bdf8';"
"      tstatus.style.color = '#94a3b8';"
"      tstatus.innerText = '● Temperatura Normal de Operación';"
"    }"
"    let fsmNom = FSM_NOMBRES[d.fsm] || ('MODO ' + d.fsm);"
"    let fsmVal = document.getElementById('fsm_val');"
"    let fsmBar = document.getElementById('fsm_bar');"
"    fsmVal.innerText = fsmNom;"
"    if (d.fsm == 7) { fsmVal.style.color = '#ef4444'; fsmBar.style.background = '#ef4444'; }"
"    else if (d.fsm == 6) { fsmVal.style.color = '#fbbf24'; fsmBar.style.background = '#fbbf24'; }"
"    else { fsmVal.style.color = '#38bdf8'; fsmBar.style.background = '#38bdf8'; }"
"    let banner = document.getElementById('banner_salud');"
"    let bIco = document.getElementById('banner_ico');"
"    let bTit = document.getElementById('banner_titulo');"
"    let bDesc = document.getElementById('banner_desc');"
"    if (d.temp > 102 || d.rpm > 6200) {"
"      banner.className = 'banner-alerta banner-danger';"
"      bIco.innerText = '🚨';"
"      bTit.style.color = '#f87171';"
"      bTit.innerText = 'ALERTA CRÍTICA: PARÁMETROS SUPERADOS';"
"      bDesc.innerText = d.temp > 102 ? 'Temperatura excesiva de refrigerante. Riesgo de deformación y fatiga mecánica.' : 'Régimen de motor en zona roja de sobrerrégimen. Reduzca la marcha o suelte acelerador.';"
"    } else if (d.temp > 92 || d.rpm > 5000) {"
"      banner.className = 'banner-alerta banner-warn';"
"      bIco.innerText = '⚠️';"
"      bTit.style.color = '#fbbf24';"
"      bTit.innerText = 'ADVERTENCIA: ALTA EXIGENCIA';"
"      bDesc.innerText = 'El motor está trabajando bajo condiciones de alta carga térmica o régimen elevado.';"
"    } else {"
"      banner.className = 'banner-alerta banner-ok';"
"      bIco.innerText = '✅';"
"      bTit.style.color = '#34d399';"
"      bTit.innerText = 'SISTEMA EN CONDICIÓN NORMAL';"
"      bDesc.innerText = 'Todos los parámetros mecánicos y térmicos se encuentran dentro del rango de operación seguro.';"
"    }"
"    document.getElementById('rx_count').innerText = d.guardadas.toLocaleString() + ' tramas';"
"    let perTxt = document.getElementById('perdidas_txt');"
"    if (d.perdidas > 0) {"
"      perTxt.style.color = '#ef4444';"
"      perTxt.innerText = 'Alerta: ' + d.perdidas + ' tramas perdidas';"
"    } else {"
"      perTxt.style.color = '#10b981';"
"      perTxt.innerText = '100% (0 pérdidas)';"
"    }"
"    let pctBuf = ((d.cola_actual / 250) * 100).toFixed(0);"
"    document.getElementById('buffer_txt').innerText = d.cola_actual + ' / 250 (' + pctBuf + '%)';"
"  } catch(e) {}"
"}"
"setInterval(actualizar, 200);"
"actualizar();"
"</script></body></html>";

static esp_err_t manejador_raiz(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PAGINA_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t manejador_api_stats(httpd_req_t *req)
{
    char resp[300];
    snprintf(resp, sizeof(resp),
             "{\"rx\":%lu,\"guardadas\":%lu,\"perdidas\":%lu,\"pico_cola\":%lu,\"cola_actual\":%lu,"
             "\"rpm\":%u,\"velocidad\":%u,\"temp\":%u,\"tps\":%u,\"fsm\":%u}",
             g_metricas.tramas_recibidas, g_metricas.tramas_guardadas,
             g_metricas.tramas_perdidas, g_metricas.maximo_uso_cola, g_metricas.uso_actual_cola,
             g_metricas.rpm_actual, g_metricas.velocidad_actual,
             g_metricas.temperatura_actual, g_metricas.acelerador_actual,
             g_metricas.estado_fsm_actual);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t manejador_descarga(httpd_req_t *req)
{
    FILE *f_csv = almacen_flash_obtener_csv();
    if (f_csv) fflush(f_csv);

    FILE *f = fopen(RUTA_ARCHIVO_CSV, "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"autolog.csv\"");

    char buf[512];
    size_t leidos = 0;
    while ((leidos = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, leidos) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }
    fclose(f);
    return httpd_resp_send_chunk(req, NULL, 0);
}

esp_err_t servidor_web_init(void)
{
    // 1. Iniciar Wi-Fi SoftAP
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t config_wifi = {
        .ap = {
            .ssid = "AUTOlog_PoC",
            .ssid_len = strlen("AUTOlog_PoC"),
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config_wifi));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi 'AUTOlog_PoC' listo en 192.168.4.1");

    // 2. Iniciar Servidor HTTP
    httpd_handle_t servidor = NULL;
    httpd_config_t config_http = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&servidor, &config_http) == ESP_OK) {
        httpd_uri_t uri_raiz = {.uri = "/", .method = HTTP_GET, .handler = manejador_raiz};
        httpd_register_uri_handler(servidor, &uri_raiz);

        httpd_uri_t uri_stats = {.uri = "/api/estadisticas", .method = HTTP_GET, .handler = manejador_api_stats};
        httpd_register_uri_handler(servidor, &uri_stats);

        httpd_uri_t uri_dl = {.uri = "/descargar", .method = HTTP_GET, .handler = manejador_descarga};
        httpd_register_uri_handler(servidor, &uri_dl);

        ESP_LOGI(TAG, "Servidor HTTP escuchando en puerto 80.");
    }

    return ESP_OK;
}
