
// #include "web_server.h"
// #include "banchetto_manager.h"
// #include "settings_manager.h"
// #include "log_manager.h"
// #include "esp_log.h"
// #include "esp_http_server.h"
// #include "cJSON.h"
// #include <string.h>
// #include "audio_manager.h"
// #include "ota_manager.h"
// static const char *TAG = "WEB_SERVER";

// static httpd_handle_t server = NULL;

// // ─────────────────────────────────────────────────────────
// // HTML Dashboard
// // ─────────────────────────────────────────────────────────
// static const char dashboard_html[] =
// "<!DOCTYPE html>"
// "<html>"
// "<head>"
// "<meta charset='UTF-8'>"
// "<meta name='viewport' content='width=device-width,initial-scale=1'>"
// "<title>Banchetto Live</title>"
// "<style>"
// "*{margin:0;padding:0;box-sizing:border-box}"
// "body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;padding:20px}"
// ".container{max-width:800px;margin:0 auto;background:#16213e;border-radius:12px;padding:30px;box-shadow:0 8px 32px rgba(0,0,0,0.3)}"
// "h1{color:#0f3460;margin-bottom:20px;text-align:center;font-size:2em}"
// ".status{display:flex;align-items:center;gap:10px;margin:15px 0;padding:15px;background:#0f3460;border-radius:8px}"
// ".badge{display:inline-block;padding:5px 15px;border-radius:20px;font-size:0.9em;font-weight:bold}"
// ".badge.open{background:#2ecc71;color:#fff}"
// ".badge.closed{background:#e74c3c;color:#fff}"
// ".section{margin:25px 0;padding:20px;background:#0f3460;border-radius:8px}"
// ".section h2{color:#e94560;margin-bottom:15px;font-size:1.3em}"
// ".data-row{display:flex;justify-content:space-between;margin:10px 0;padding:8px 0;border-bottom:1px solid rgba(255,255,255,0.1)}"
// ".data-label{color:#aaa;min-width:120px}"
// ".data-value{color:#fff;font-weight:bold;font-size:1.1em;text-align:right;flex:1}"
// ".progress{background:#1a1a2e;border-radius:10px;overflow:hidden;height:40px;margin:10px 0;position:relative}"
// ".progress-bar{background:linear-gradient(90deg,#667eea,#764ba2);height:100%;transition:width 0.3s;display:flex;align-items:center;justify-content:center;color:#fff;font-weight:bold;font-size:1.2em}"
// ".timestamp{text-align:center;color:#888;font-size:0.85em;margin-top:20px}"
// ".links{display:flex;gap:10px;margin-top:20px}"
// ".settings-link{display:block;flex:1;text-align:center;padding:10px;background:#0f3460;border-radius:8px;color:#e94560;text-decoration:none;font-weight:bold;transition:background 0.3s}"
// ".settings-link:hover{background:#1a4d7a}"
// ".status-dot{width:12px;height:12px;border-radius:50%;animation:pulse 2s infinite}"
// ".status-dot.online{background:#2ecc71}"
// "@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}"
// ".order-selector{width:100%;padding:12px;background:#1a1a2e;border:2px solid #667eea;border-radius:8px;color:#fff;font-size:1.1em;margin-top:5px;cursor:pointer}"
// ".order-selector option{background:#1a1a2e;color:#fff}"
// "</style>"
// "</head>"
// "<body>"
// "<div class='container'>"
// "<h1 id='page-title'>Banchetto Live</h1>"
// "<div class='status'>"
// "<div class='status-dot online' id='status-dot'></div>"
// "<span id='connection-status'>Aggiornamento attivo</span>"
// "</div>"

// "<div class='section'>"
// "<h2>Operatore</h2>"
// "<div class='data-row'><span class='data-label'>Nome:</span><span class='data-value' id='operatore'>-</span></div>"
// "<div class='data-row'><span class='data-label'>Matricola:</span><span class='data-value' id='matricola'>-</span></div>"
// "<div class='data-row'><span class='data-label'>Sessione:</span><span class='badge' id='sessione'>-</span></div>"
// "</div>"

// "<div class='section'>"
// "<h2>Ordine</h2>"
// "<select class='order-selector' id='order-select' onchange='changeOrder()'></select>"
// "</div>"

// "<div class='section'>"
// "<h2>Articolo</h2>"
// "<div class='data-row'><span class='data-label'>Codice:</span><span class='data-value' id='codice'>-</span></div>"
// "<div class='data-row'><span class='data-label'>Descrizione:</span><span class='data-value' id='descrizione'>-</span></div>"
// "<div class='data-row'><span class='data-label'>OdP:</span><span class='data-value' id='odp'>-</span></div>"
// "<div class='data-row'><span class='data-label'>Ciclo:</span><span class='data-value' id='ciclo'>-</span></div>"
// "<div class='data-row'><span class='data-label'>Fase:</span><span class='data-value' id='fase'>-</span></div>"
// "</div>"

// "<div class='section'>"
// "<h2>Produzione Scatola</h2>"
// "<div class='data-row'><span class='data-label'>Scatola:</span><span class='data-value' id='scatola'>-</span></div>"
// "<div class='progress'><div class='progress-bar' id='progress-scatola' style='width:0%'><span style='position:absolute;right:10px'>0/0</span></div></div>"
// "</div>"

// "<div class='section'>"
// "<h2>Quantita</h2>"
// "<div class='data-row'><span class='data-label'>Totale OdP:</span><span class='data-value' id='qta-totale'>0</span></div>"
// "<div class='data-row'><span class='data-label'>Fase:</span><span class='data-value' id='qta-fase'>0</span></div>"
// "<div class='data-row'><span class='data-label'>Sessione:</span><span class='data-value' id='qta-sessione'>0</span></div>"
// "<div class='data-row'><span class='data-label'>Giornaliera:</span><span class='data-value' id='qta-giornaliera'>0</span></div>"
// "</div>"

// "<div class='timestamp' id='timestamp'>In attesa dati...</div>"
// "<div class='links'>"
// "<a href='#' id='settings-link' class='settings-link'>Impostazioni</a>"
// "<a href='/logs' class='settings-link'>Log Terminal</a>"
// "</div>"
// "</div>"

// "<script>"
// "let titleUpdated=false;"
// "let selectedIdx=0;"
// "let allItems=[];"

// "function changeOrder(){"
// "const sel=document.getElementById('order-select');"
// "selectedIdx=parseInt(sel.value);"
// "fetch('/api/set_index',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:selectedIdx})});"
// "updateDisplay();"
// "}"

// "function updateDropdown(){"
// "const sel=document.getElementById('order-select');"
// "const prev=sel.value;"
// "sel.innerHTML='';"
// "allItems.forEach((item,i)=>{"
// "const opt=document.createElement('option');"
// "opt.value=i;"
// "opt.textContent=(i+1)+' - '+item.codice_articolo+' (OdP: '+item.ord_prod+')';"
// "sel.appendChild(opt);"
// "});"
// "sel.value=selectedIdx;"
// "}"

// "function updateDisplay(){"
// "if(!allItems.length)return;"
// "const d=allItems[selectedIdx]||allItems[0];"

// "if(!titleUpdated&&d.banchetto){"
// "document.getElementById('page-title').textContent='Banchetto Live '+d.banchetto;"
// "document.title='Banchetto Live '+d.banchetto;"
// "titleUpdated=true;"
// "}"

// "document.getElementById('operatore').textContent=d.operatore||'-';"
// "document.getElementById('matricola').textContent=d.matricola||'-';"
// "const s=document.getElementById('sessione');"
// "s.textContent=d.sessione_aperta?'APERTA':'CHIUSA';"
// "s.className='badge '+(d.sessione_aperta?'open':'closed');"

// "document.getElementById('codice').textContent=d.codice_articolo||'-';"
// "document.getElementById('descrizione').textContent=d.descrizione_articolo||'-';"
// "document.getElementById('odp').textContent=d.ord_prod||'-';"
// "document.getElementById('ciclo').textContent=d.ciclo||'-';"
// "document.getElementById('fase').textContent=d.fase+' - '+d.descr_fase;"

// "document.getElementById('scatola').textContent=d.matr_scatola_corrente||'-';"
// "const p=(d.qta_scatola/d.qta_totale_scatola*100)||0;"
// "const pb=document.getElementById('progress-scatola');"
// "pb.style.width=Math.min(p,100)+'%';"
// "pb.querySelector('span').textContent=d.qta_scatola+'/'+d.qta_totale_scatola;"

// "document.getElementById('qta-totale').textContent=d.qta_totale;"
// "document.getElementById('qta-fase').textContent=d.qta_prod_fase;"
// "document.getElementById('qta-sessione').textContent=d.qta_prod_sessione;"
// "document.getElementById('qta-giornaliera').textContent=d.qta_totale_giornaliera;"

// "document.getElementById('timestamp').textContent='Aggiornato: '+new Date().toLocaleTimeString();"
// "document.getElementById('status-dot').className='status-dot online';"

// "document.getElementById('settings-link').href='/settings?idx='+selectedIdx;"
// "}"

// "function update(){"
// "fetch('/api/data_all').then(r=>r.json()).then(d=>{"
// "allItems=d.items||[];"
// "if(selectedIdx>=allItems.length)selectedIdx=0;"
// "updateDropdown();"
// "updateDisplay();"
// "}).catch(()=>{"
// "document.getElementById('status-dot').className='status-dot';"
// "});"
// "}"

// "update();"
// "setInterval(update,500);"
// "</script>"
// "</body>"
// "</html>";

// // ─────────────────────────────────────────────────────────
// // HTML Settings
// // ─────────────────────────────────────────────────────────
// static const char settings_html[] =
// "<!DOCTYPE html>"
// "<html>"
// "<head>"
// "<meta charset='UTF-8'>"
// "<meta name='viewport' content='width=device-width,initial-scale=1'>"
// "<title>Impostazioni</title>"
// "<style>"
// "*{margin:0;padding:0;box-sizing:border-box}"
// "body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;padding:20px}"
// ".container{max-width:600px;margin:0 auto;background:#16213e;border-radius:12px;padding:30px;box-shadow:0 8px 32px rgba(0,0,0,0.3)}"
// "h1{color:#e94560;margin-bottom:10px;text-align:center;font-size:2em}"
// ".subtitle{text-align:center;color:#667eea;font-size:1.1em;margin-bottom:25px}"
// ".setting-group{margin:25px 0;padding:20px;background:#0f3460;border-radius:8px}"
// ".setting-label{color:#aaa;font-size:1.1em;margin-bottom:10px;display:block}"
// ".slider-container{display:flex;align-items:center;gap:15px}"
// "input[type=range]{flex:1;height:8px;border-radius:5px;background:#1a1a2e;outline:none;-webkit-appearance:none}"
// "input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#667eea;cursor:pointer}"
// "input[type=range]::-moz-range-thumb{width:20px;height:20px;border-radius:50%;background:#667eea;cursor:pointer}"
// ".value-display{color:#fff;font-weight:bold;font-size:1.3em;min-width:50px;text-align:right}"
// ".btn{display:block;width:100%;padding:15px;margin-top:30px;background:linear-gradient(90deg,#667eea,#764ba2);border:none;border-radius:8px;color:#fff;font-size:1.1em;font-weight:bold;cursor:pointer;transition:transform 0.2s}"
// ".btn:hover{transform:scale(1.02)}"
// ".btn:active{transform:scale(0.98)}"
// ".back-link{display:block;text-align:center;margin-top:20px;color:#aaa;text-decoration:none}"
// ".back-link:hover{color:#fff}"
// ".status{text-align:center;margin-top:15px;padding:10px;border-radius:5px;display:none}"
// ".status.success{background:#2ecc71;color:#fff;display:block}"
// ".status.error{background:#e74c3c;color:#fff;display:block}"
// "</style>"
// "</head>"
// "<body>"
// "<div class='container'>"
// "<h1>Impostazioni</h1>"
// "<div class='subtitle' id='subtitle'>-</div>"

// "<div class='setting-group'>"
// "<label class='setting-label'>Volume Buzzer</label>"
// "<div class='slider-container'>"
// "<input type='range' id='volume' min='0' max='100' value='100'>"
// "<span class='value-display' id='volume-value'>100</span>"
// "</div>"
// "</div>"
// "<button class='btn' onclick='saveSettings()'>Applica Volume</button>"
// "<div class='status' id='status'></div>"

// "<div class='setting-group' style='margin-top:30px'>"
// "<label class='setting-label'>Registra Scatola Manualmente</label>"
// "<input type='text' id='barcode' placeholder='es: SB011196' style='width:100%;padding:12px;background:#1a1a2e;border:2px solid #667eea;border-radius:8px;color:#fff;font-size:1.1em;margin-top:10px'>"
// "<button class='btn' style='margin-top:15px' onclick='registerBox()'>Registra Scatola</button>"
// "</div>"
// "<div class='status' id='status-box'></div>"

// "<div class='setting-group' style='margin-top:30px'>"
// "<label class='setting-label'>Login Operatore (Matricola)</label>"
// "<input type='number' id='matricola' placeholder='es: 157' min='1' max='9999' style='width:100%;padding:12px;background:#1a1a2e;border:2px solid #2ecc71;border-radius:8px;color:#fff;font-size:1.1em;margin-top:10px'>"
// "<button class='btn' style='margin-top:15px;background:linear-gradient(90deg,#2ecc71,#27ae60)' onclick='loginOperator()'>Login Operatore</button>"
// "</div>"
// "<div class='status' id='status-operator'></div>"

// "<div class='setting-group' style='margin-top:30px'>"
// "<label class='setting-label'>Segnala Scarti</label>"
// "<input type='number' id='qta-scarti' placeholder='es: 3' min='1' max='9999' style='width:100%;padding:12px;background:#1a1a2e;border:2px solid #e74c3c;border-radius:8px;color:#fff;font-size:1.1em;margin-top:10px'>"
// "<button class='btn' style='margin-top:15px;background:linear-gradient(90deg,#e74c3c,#c0392b)' onclick='reportScrap()'>Segnala Scarti</button>"
// "</div>"
// "<div class='status' id='status-scrap'></div>"

// "<a href='/' class='back-link'>Torna alla Dashboard</a>"
// "</div>"

// "<script>"
// "const params=new URLSearchParams(window.location.search);"
// "const idx=parseInt(params.get('idx'))||0;"

// "fetch('/api/set_index',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:idx})});"

// "fetch('/api/data').then(r=>r.json()).then(d=>{"
// "document.getElementById('subtitle').textContent=d.codice_articolo+' - OdP: '+d.ord_prod;"
// "}).catch(()=>{});"

// "const volumeSlider=document.getElementById('volume');"
// "const volumeValue=document.getElementById('volume-value');"
// "const statusDiv=document.getElementById('status');"
// "volumeSlider.oninput=function(){volumeValue.textContent=this.value;};"

// "function loadSettings(){"
// "fetch('/api/settings').then(r=>r.json()).then(d=>{"
// "volumeSlider.value=d.volume||100;"
// "volumeValue.textContent=d.volume||100;"
// "}).catch(e=>console.error('Errore caricamento:',e));"
// "}"

// "function saveSettings(){"
// "const vol=parseInt(volumeSlider.value);"
// "fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({volume:vol})})"
// ".then(r=>r.json()).then(d=>{"
// "if(d.ok){statusDiv.className='status success';statusDiv.textContent='Volume salvato!';setTimeout(()=>statusDiv.style.display='none',3000);}"
// "else{statusDiv.className='status error';statusDiv.textContent='Errore salvataggio';}"
// "}).catch(e=>{statusDiv.className='status error';statusDiv.textContent='Errore comunicazione';});"
// "}"

// "function registerBox(){"
// "const barcode=document.getElementById('barcode').value.trim();"
// "const statusBox=document.getElementById('status-box');"
// "if(!barcode){statusBox.className='status error';statusBox.textContent='Inserisci codice scatola';return;}"
// "fetch('/api/scatola',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({barcode:barcode})})"
// ".then(r=>r.json()).then(d=>{"
// "if(d.ok){statusBox.className='status success';statusBox.textContent='Scatola '+barcode+' registrata!';document.getElementById('barcode').value='';setTimeout(()=>statusBox.style.display='none',3000);}"
// "else{statusBox.className='status error';statusBox.textContent=d.error||'Errore registrazione';}"
// "}).catch(e=>{statusBox.className='status error';statusBox.textContent='Errore comunicazione';});"
// "}"

// "function loginOperator(){"
// "const matricola=parseInt(document.getElementById('matricola').value);"
// "const statusOp=document.getElementById('status-operator');"
// "if(!matricola||matricola<1||matricola>9999){statusOp.className='status error';statusOp.textContent='Inserisci matricola valida (1-9999)';return;}"
// "fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({matricola:matricola})})"
// ".then(r=>r.json()).then(d=>{"
// "if(d.ok){statusOp.className='status success';statusOp.textContent='Operatore '+matricola+' loggato!';document.getElementById('matricola').value='';setTimeout(()=>statusOp.style.display='none',3000);}"
// "else{statusOp.className='status error';statusOp.textContent=d.error||'Login fallito';}"
// "}).catch(e=>{statusOp.className='status error';statusOp.textContent='Errore comunicazione';});"
// "}"

// "function reportScrap(){"
// "const qta=parseInt(document.getElementById('qta-scarti').value);"
// "const statusScrap=document.getElementById('status-scrap');"
// "if(!qta||qta<1||qta>9999){statusScrap.className='status error';statusScrap.textContent='Inserisci quantita valida (1-9999)';return;}"
// "fetch('/api/scarto',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({qta:qta})})"
// ".then(r=>r.json()).then(d=>{"
// "if(d.ok){statusScrap.className='status success';statusScrap.textContent=qta+' scarti registrati!';document.getElementById('qta-scarti').value='';setTimeout(()=>statusScrap.style.display='none',3000);}"
// "else{statusScrap.className='status error';statusScrap.textContent=d.error||'Errore registrazione scarti';}"
// "}).catch(e=>{statusScrap.className='status error';statusScrap.textContent='Errore comunicazione';});"
// "}"

// "loadSettings();"
// "</script>"
// "</body>"
// "</html>";

// // ─────────────────────────────────────────────────────────
// // HTML Log Terminal (invariato)
// // ─────────────────────────────────────────────────────────
// static const char logs_html[] =
// "<!DOCTYPE html>"
// "<html><head>"
// "<meta charset='UTF-8'>"
// "<meta name='viewport' content='width=device-width,initial-scale=1'>"
// "<title>Log Terminal</title>"
// "<style>"
// "*{margin:0;padding:0;box-sizing:border-box}"
// "body{background:#0d1117;color:#c9d1d9;font-family:'Courier New',monospace;display:flex;flex-direction:column;height:100vh}"
// ".toolbar{background:#161b22;padding:10px 16px;display:flex;gap:10px;align-items:center;border-bottom:1px solid #30363d;flex-shrink:0}"
// ".toolbar span{color:#8b949e;font-size:13px;flex:1}"
// "button{padding:6px 14px;border:1px solid #30363d;border-radius:6px;background:#21262d;color:#c9d1d9;cursor:pointer;font-size:12px}"
// "button:hover{background:#30363d}"
// "button.active{background:#1f6feb;border-color:#1f6feb;color:#fff}"
// "#terminal{flex:1;overflow-y:auto;padding:12px 16px;font-size:12px;line-height:1.6}"
// ".log-I{color:#c9d1d9}"
// ".log-W{color:#e3b341}"
// ".log-E{color:#f85149}"
// ".log-D{color:#7ee787}"
// ".log-V{color:#8b949e}"
// ".log-other{color:#79c0ff}"
// "#counter{color:#8b949e;font-size:12px}"
// "</style>"
// "</head><body>"
// "<div class='toolbar'>"
// "<span>ESP32 Log Terminal</span>"
// "<span id='counter'>righe: 0</span>"
// "<button id='btn-scroll' class='active' onclick='toggleScroll()'>Auto-scroll ON</button>"
// "<button onclick='clearTerminal()'>Clear</button>"
// "<a href='/' style='text-decoration:none'><button>Dashboard</button></a>"
// "</div>"
// "<div id='terminal'></div>"
// "<script>"
// "let from=0,autoScroll=true,total=0;"
// "const term=document.getElementById('terminal');"
// "const counter=document.getElementById('counter');"
// "function classForLine(l){"
// "if(l.match(/^[IWEDViwedv]\\s*\\(/)){const c=l[0].toUpperCase();return 'log-'+c;}"
// "if(l.startsWith('[INFO]'))return 'log-I';"
// "if(l.startsWith('[WARN]'))return 'log-W';"
// "if(l.startsWith('[ERR]')||l.startsWith('[ERROR]'))return 'log-E';"
// "return 'log-other';"
// "}"
// "function appendLines(lines){"
// "lines.forEach(l=>{"
// "if(!l.trim())return;"
// "const d=document.createElement('div');"
// "d.className=classForLine(l);"
// "d.textContent=l;"
// "term.appendChild(d);"
// "total++;"
// "});"
// "counter.textContent='righe: '+total;"
// "if(autoScroll)term.scrollTop=term.scrollHeight;"
// "}"
// "function toggleScroll(){"
// "autoScroll=!autoScroll;"
// "const b=document.getElementById('btn-scroll');"
// "b.textContent='Auto-scroll '+(autoScroll?'ON':'OFF');"
// "b.className=autoScroll?'active':'';"
// "}"
// "function clearTerminal(){term.innerHTML='';total=0;counter.textContent='righe: 0';}"
// "function poll(){"
// "fetch('/api/logs?from='+from)"
// ".then(r=>r.json())"
// ".then(d=>{if(d.lines&&d.lines.length)appendLines(d.lines);from=d.next;})"
// ".catch(()=>{})"
// ".finally(()=>setTimeout(poll,1000));"
// "}"
// "poll();"
// "</script>"
// "</body></html>";

// // ─────────────────────────────────────────────────────────
// // HANDLERS
// // ─────────────────────────────────────────────────────────
// static esp_err_t dashboard_handler(httpd_req_t *req)
// {
//     httpd_resp_set_type(req, "text/html");
//     httpd_resp_send(req, dashboard_html, HTTPD_RESP_USE_STRLEN);
//     return ESP_OK;
// }

// static esp_err_t settings_handler(httpd_req_t *req)
// {
//     httpd_resp_set_type(req, "text/html");
//     httpd_resp_send(req, settings_html, HTTPD_RESP_USE_STRLEN);
//     return ESP_OK;
// }

// static esp_err_t logs_page_handler(httpd_req_t *req)
// {
//     httpd_resp_set_type(req, "text/html");
//     httpd_resp_send(req, logs_html, HTTPD_RESP_USE_STRLEN);
//     return ESP_OK;
// }

// static esp_err_t api_logs_handler(httpd_req_t *req)
// {
//     char query[32] = {0};
//     uint32_t from = 0;
//     if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
//     {
//         char val[16] = {0};
//         if (httpd_query_key_value(query, "from", val, sizeof(val)) == ESP_OK)
//             from = (uint32_t)atoi(val);
//     }

//     const size_t buf_size = 16 * 1024;
//     char *buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
//     if (!buf)
//     {
//         httpd_resp_send_500(req);
//         return ESP_OK;
//     }

//     uint32_t last_idx = from;
//     uint32_t count = log_manager_get_lines(from, buf, buf_size, &last_idx);

//     cJSON *root = cJSON_CreateObject();
//     cJSON_AddNumberToObject(root, "next", last_idx);
//     cJSON *arr = cJSON_AddArrayToObject(root, "lines");

//     if (count > 0)
//     {
//         char *line = strtok(buf, "\n");
//         while (line)
//         {
//             if (strlen(line) > 0)
//                 cJSON_AddItemToArray(arr, cJSON_CreateString(line));
//             line = strtok(NULL, "\n");
//         }
//     }

//     char *json_str = cJSON_PrintUnformatted(root);
//     cJSON_Delete(root);
//     free(buf);

//     if (!json_str)
//     {
//         httpd_resp_send_500(req);
//         return ESP_OK;
//     }

//     httpd_resp_set_type(req, "application/json");
//     httpd_resp_send(req, json_str, strlen(json_str));
//     free(json_str);
//     return ESP_OK;
// }

// // GET /api/data — item corrente (retrocompatibilita)
// static esp_err_t api_data_handler(httpd_req_t *req)
// {
//     banchetto_data_t data;
//     if (!banchetto_manager_get_data(&data)) {
//         httpd_resp_set_status(req, "503 Service Unavailable");
//         httpd_resp_send(req, "{\"error\":\"busy\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }

//     cJSON *root = cJSON_CreateObject();
//     cJSON_AddStringToObject(root, "banchetto", data.banchetto);
//     cJSON_AddStringToObject(root, "operatore", data.operatore);
//     cJSON_AddStringToObject(root, "matricola", data.matricola);
//     cJSON_AddBoolToObject(root, "sessione_aperta", data.sessione_aperta);
//     cJSON_AddStringToObject(root, "codice_articolo", data.codice_articolo);
//     cJSON_AddStringToObject(root, "descrizione_articolo", data.descrizione_articolo);
//     cJSON_AddStringToObject(root, "fase", data.fase);
//     cJSON_AddStringToObject(root, "descr_fase", data.descr_fase);
//     cJSON_AddStringToObject(root, "ciclo", data.ciclo);
//     cJSON_AddStringToObject(root, "matr_scatola_corrente", data.matr_scatola_corrente);
//     cJSON_AddNumberToObject(root, "ord_prod", data.ord_prod);
//     cJSON_AddNumberToObject(root, "qta_totale", data.qta_totale);
//     cJSON_AddNumberToObject(root, "qta_scatola", data.qta_scatola);
//     cJSON_AddNumberToObject(root, "qta_totale_scatola", data.qta_totale_scatola);
//     cJSON_AddNumberToObject(root, "qta_prod_fase", data.qta_prod_fase);
//     cJSON_AddNumberToObject(root, "qta_prod_sessione", data.qta_prod_sessione);
//     cJSON_AddNumberToObject(root, "qta_totale_giornaliera", data.qta_totale_giornaliera);

//     char *json_str = cJSON_PrintUnformatted(root);
//     cJSON_Delete(root);

//     if (json_str == NULL) {
//         httpd_resp_set_status(req, "500 Internal Server Error");
//         httpd_resp_send(req, "{\"error\":\"json\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }

//     httpd_resp_set_type(req, "application/json");
//     httpd_resp_send(req, json_str, strlen(json_str));
//     free(json_str);
//     return ESP_OK;
// }

// // GET /api/data_all — tutti gli items
// static esp_err_t api_data_all_handler(httpd_req_t *req)
// {
//     uint8_t count = banchetto_manager_get_count();
//     uint8_t current = banchetto_manager_get_current_index();

//     cJSON *root = cJSON_CreateObject();
//     cJSON_AddNumberToObject(root, "count", count);
//     cJSON_AddNumberToObject(root, "current_index", current);
//     cJSON *arr = cJSON_AddArrayToObject(root, "items");

//     for (int i = 0; i < count; i++)
//     {
//         banchetto_data_t data;
//         if (!banchetto_manager_get_item(i, &data))
//             continue;

//         cJSON *item = cJSON_CreateObject();
//         cJSON_AddStringToObject(item, "banchetto", data.banchetto);
//         cJSON_AddStringToObject(item, "operatore", data.operatore);
//         cJSON_AddStringToObject(item, "matricola", data.matricola);
//         cJSON_AddBoolToObject(item, "sessione_aperta", data.sessione_aperta);
//         cJSON_AddStringToObject(item, "codice_articolo", data.codice_articolo);
//         cJSON_AddStringToObject(item, "descrizione_articolo", data.descrizione_articolo);
//         cJSON_AddStringToObject(item, "fase", data.fase);
//         cJSON_AddStringToObject(item, "descr_fase", data.descr_fase);
//         cJSON_AddStringToObject(item, "ciclo", data.ciclo);
//         cJSON_AddStringToObject(item, "matr_scatola_corrente", data.matr_scatola_corrente);
//         cJSON_AddNumberToObject(item, "ord_prod", data.ord_prod);
//         cJSON_AddNumberToObject(item, "qta_totale", data.qta_totale);
//         cJSON_AddNumberToObject(item, "qta_scatola", data.qta_scatola);
//         cJSON_AddNumberToObject(item, "qta_totale_scatola", data.qta_totale_scatola);
//         cJSON_AddNumberToObject(item, "qta_prod_fase", data.qta_prod_fase);
//         cJSON_AddNumberToObject(item, "qta_prod_sessione", data.qta_prod_sessione);
//         cJSON_AddNumberToObject(item, "qta_totale_giornaliera", data.qta_totale_giornaliera);
//         cJSON_AddItemToArray(arr, item);
//     }

//     char *json_str = cJSON_PrintUnformatted(root);
//     cJSON_Delete(root);

//     if (!json_str) {
//         httpd_resp_send_500(req);
//         return ESP_OK;
//     }

//     httpd_resp_set_type(req, "application/json");
//     httpd_resp_send(req, json_str, strlen(json_str));
//     free(json_str);
//     return ESP_OK;
// }

// // POST /api/set_index — cambia indice corrente
// static esp_err_t api_set_index_handler(httpd_req_t *req)
// {
//     char buf[64];
//     int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
//     if (ret <= 0) {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"no data\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }
//     buf[ret] = '\0';

//     cJSON *json = cJSON_Parse(buf);
//     if (!json) {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"invalid json\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }

//     cJSON *idx_item = cJSON_GetObjectItem(json, "index");
//     if (idx_item && cJSON_IsNumber(idx_item)) {
//         uint8_t idx = (uint8_t)idx_item->valueint;
//         banchetto_manager_set_current_index(idx);
//         ESP_LOGI(TAG, "Indice corrente impostato a %d via web", idx);
//         httpd_resp_set_type(req, "application/json");
//         httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
//     } else {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"missing index\"}", HTTPD_RESP_USE_STRLEN);
//     }

//     cJSON_Delete(json);
//     return ESP_OK;
// }

// static esp_err_t api_settings_get_handler(httpd_req_t *req)
// {
//     cJSON *root = cJSON_CreateObject();
//     cJSON_AddNumberToObject(root, "volume", settings_get_volume());

//     char *json_str = cJSON_PrintUnformatted(root);
//     cJSON_Delete(root);

//     if (json_str == NULL) {
//         httpd_resp_set_status(req, "500 Internal Server Error");
//         httpd_resp_send(req, "{\"error\":\"json\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }

//     httpd_resp_set_type(req, "application/json");
//     httpd_resp_send(req, json_str, strlen(json_str));
//     free(json_str);
//     return ESP_OK;
// }

// static esp_err_t api_settings_post_handler(httpd_req_t *req)
// {
//     char buf[100];
//     int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
//     if (ret <= 0) {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"no data\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }
//     buf[ret] = '\0';

//     cJSON *json = cJSON_Parse(buf);
//     if (json == NULL) {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"invalid json\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }

//     cJSON *volume_item = cJSON_GetObjectItem(json, "volume");
//     if (volume_item && cJSON_IsNumber(volume_item)) {
//         uint8_t vol = (uint8_t)volume_item->valueint;
//         if (settings_set_volume(vol) == ESP_OK) {
//             httpd_resp_set_type(req, "application/json");
//             httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
//             ESP_LOGI(TAG, "Volume aggiornato via Web: %d", vol);
//         } else {
//             httpd_resp_set_status(req, "400 Bad Request");
//             httpd_resp_send(req, "{\"ok\":false,\"error\":\"invalid volume\"}", HTTPD_RESP_USE_STRLEN);
//         }
//     } else {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"missing volume\"}", HTTPD_RESP_USE_STRLEN);
//     }

//     cJSON_Delete(json);
//     return ESP_OK;
// }

// static esp_err_t api_scatola_post_handler(httpd_req_t *req)
// {
//     char buf[100];
//     int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
//     if (ret <= 0) {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"no data\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }
//     buf[ret] = '\0';

//     cJSON *json = cJSON_Parse(buf);
//     if (json == NULL) {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"invalid json\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }

//     cJSON *barcode_item = cJSON_GetObjectItem(json, "barcode");
//     if (barcode_item && cJSON_IsString(barcode_item) && barcode_item->valuestring) {
//         banchetto_manager_set_barcode(barcode_item->valuestring);
//         httpd_resp_set_type(req, "application/json");
//         httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
//     } else {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"missing barcode\"}", HTTPD_RESP_USE_STRLEN);
//     }

//     cJSON_Delete(json);
//     return ESP_OK;
// }

// static esp_err_t api_login_post_handler(httpd_req_t *req)
// {
//     char buf[100];
//     int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
//     if (ret <= 0) {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"no data\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }
//     buf[ret] = '\0';

//     cJSON *json = cJSON_Parse(buf);
//     if (json == NULL) {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"invalid json\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }

//     cJSON *matricola_item = cJSON_GetObjectItem(json, "matricola");
//     if (matricola_item && cJSON_IsNumber(matricola_item)) {
//         uint16_t matr = (uint16_t)matricola_item->valueint;
//         esp_err_t result = banchetto_manager_login_by_matricola(matr);
//         if (result == ESP_OK) {
//             httpd_resp_set_type(req, "application/json");
//             httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
//         } else {
//             httpd_resp_set_status(req, "400 Bad Request");
//             httpd_resp_send(req, "{\"ok\":false,\"error\":\"login failed\"}", HTTPD_RESP_USE_STRLEN);
//         }
//     } else {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"missing matricola\"}", HTTPD_RESP_USE_STRLEN);
//     }

//     cJSON_Delete(json);
//     return ESP_OK;
// }

// static esp_err_t api_scarto_post_handler(httpd_req_t *req)
// {
//     char buf[100];
//     int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
//     if (ret <= 0) {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"no data\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }
//     buf[ret] = '\0';

//     cJSON *json = cJSON_Parse(buf);
//     if (json == NULL) {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"invalid json\"}", HTTPD_RESP_USE_STRLEN);
//         return ESP_OK;
//     }

//     cJSON *qta_item = cJSON_GetObjectItem(json, "qta");
//     if (qta_item && cJSON_IsNumber(qta_item)) {
//         uint32_t qta = (uint32_t)qta_item->valueint;
//         bool result = banchetto_manager_scarto(qta);
//         if (result) {
//             httpd_resp_set_type(req, "application/json");
//             httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
//         } else {
//             httpd_resp_set_status(req, "400 Bad Request");
//             httpd_resp_send(req, "{\"ok\":false,\"error\":\"scarto failed\"}", HTTPD_RESP_USE_STRLEN);
//         }
//     } else {
//         httpd_resp_set_status(req, "400 Bad Request");
//         httpd_resp_send(req, "{\"ok\":false,\"error\":\"missing qta\"}", HTTPD_RESP_USE_STRLEN);
//     }

//     cJSON_Delete(json);
//     return ESP_OK;
// }

// void web_server_broadcast_update(void)
// {
//     // Con polling non serve broadcast
// }

// // ─────────────────────────────────────────────────────────
// // INIT
// // ─────────────────────────────────────────────────────────
// esp_err_t web_server_init(void)
// {
//     ESP_LOGI(TAG, "Inizializzazione Web Server");

//     httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//     config.max_open_sockets = 7;
//     config.max_uri_handlers = 14;
//     config.task_priority = 3;
//     config.lru_purge_enable = true;
//     config.core_id = 1;

//     if (httpd_start(&server, &config) != ESP_OK) {
//         ESP_LOGE(TAG, "Errore avvio server HTTP");
//         return ESP_FAIL;
//     }

//     httpd_uri_t uris[] = {
//         { .uri = "/",             .method = HTTP_GET,  .handler = dashboard_handler,        .user_ctx = NULL },
//         { .uri = "/settings",     .method = HTTP_GET,  .handler = settings_handler,         .user_ctx = NULL },
//         { .uri = "/logs",         .method = HTTP_GET,  .handler = logs_page_handler,        .user_ctx = NULL },
//         { .uri = "/api/data",     .method = HTTP_GET,  .handler = api_data_handler,         .user_ctx = NULL },
//         { .uri = "/api/data_all", .method = HTTP_GET,  .handler = api_data_all_handler,     .user_ctx = NULL },
//         { .uri = "/api/logs",     .method = HTTP_GET,  .handler = api_logs_handler,         .user_ctx = NULL },
//         { .uri = "/api/settings", .method = HTTP_GET,  .handler = api_settings_get_handler, .user_ctx = NULL },
//         { .uri = "/api/settings", .method = HTTP_POST, .handler = api_settings_post_handler,.user_ctx = NULL },
//         { .uri = "/api/set_index",.method = HTTP_POST, .handler = api_set_index_handler,    .user_ctx = NULL },
//         { .uri = "/api/scatola",  .method = HTTP_POST, .handler = api_scatola_post_handler, .user_ctx = NULL },
//         { .uri = "/api/login",    .method = HTTP_POST, .handler = api_login_post_handler,   .user_ctx = NULL },
//         { .uri = "/api/scarto",   .method = HTTP_POST, .handler = api_scarto_post_handler,  .user_ctx = NULL },
//     };

//     for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++)
//         httpd_register_uri_handler(server, &uris[i]);

//     ESP_LOGI(TAG, "Server HTTP avviato su Core 1");
//     return ESP_OK;
// }
#include "web_server.h"
#include "banchetto_manager.h"
#include "settings_manager.h"
#include "log_manager.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <string.h>
#include "audio_manager.h"
#include "ota_manager.h"
#include "esp_app_desc.h"
#include "tagliatubi_manager.h"
#include "mode.h"
#include "collaudo_manager.h"
extern int wifi_get_rssi(void);

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

// ─────────────────────────────────────────────────────────
// HTML Dashboard
// ─────────────────────────────────────────────────────────
static const char dashboard_html[] =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Banchetto Live</title>"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;padding:20px}"
    ".container{max-width:800px;margin:0 auto;background:#16213e;border-radius:12px;padding:30px;box-shadow:0 8px 32px rgba(0,0,0,0.3)}"
    "h1{color:#0f3460;margin-bottom:20px;text-align:center;font-size:2em}"
    ".status{display:flex;align-items:center;justify-content:space-between;gap:10px;margin:15px 0;padding:15px;background:#0f3460;border-radius:8px}"
    ".badge{display:inline-block;padding:5px 15px;border-radius:20px;font-size:0.9em;font-weight:bold}"
    ".badge.open{background:#2ecc71;color:#fff}"
    ".badge.closed{background:#e74c3c;color:#fff}"
    ".section{margin:25px 0;padding:20px;background:#0f3460;border-radius:8px}"
    ".section h2{color:#e94560;margin-bottom:15px;font-size:1.3em}"
    ".data-row{display:flex;justify-content:space-between;margin:10px 0;padding:8px 0;border-bottom:1px solid rgba(255,255,255,0.1)}"
    ".data-label{color:#aaa;min-width:120px}"
    ".data-value{color:#fff;font-weight:bold;font-size:1.1em;text-align:right;flex:1}"
    ".progress{background:#1a1a2e;border-radius:10px;overflow:hidden;height:40px;margin:10px 0;position:relative}"
    ".progress-bar{background:linear-gradient(90deg,#667eea,#764ba2);height:100%;transition:width 0.3s;display:flex;align-items:center;justify-content:center;color:#fff;font-weight:bold;font-size:1.2em}"
    ".timestamp{text-align:center;color:#888;font-size:0.85em;margin-top:20px}"
    ".links{display:flex;gap:10px;margin-top:20px}"
    ".settings-link{display:block;flex:1;text-align:center;padding:10px;background:#0f3460;border-radius:8px;color:#e94560;text-decoration:none;font-weight:bold;transition:background 0.3s}"
    ".settings-link:hover{background:#1a4d7a}"
    ".status-dot{width:12px;height:12px;border-radius:50%;animation:pulse 2s infinite}"
    ".status-dot.online{background:#2ecc71}"
    "@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}"
    ".order-selector{width:100%;padding:12px;background:#1a1a2e;border:2px solid #667eea;border-radius:8px;color:#fff;font-size:1.1em;margin-top:5px;cursor:pointer}"
    ".order-selector option{background:#1a1a2e;color:#fff}"
    "</style>"
    "</head>"
    "<body>"
    "<div class='container'>"
    "<h1 id='page-title'>Banchetto Live</h1>"
    "<div class='status'>"
    "<div style='display:flex;align-items:center;gap:10px'>"
    "<div class='status-dot online' id='status-dot'></div>"
    "<span id='connection-status'>Aggiornamento attivo</span>"
    "</div>"
    "<div style='text-align:right;font-size:0.85em;color:#aaa'>"
    "<span id='fw-version'>v-</span>"
    "&nbsp;&nbsp;<span id='mat-dot' style='display:none;width:12px;height:12px;border-radius:50%;display:inline-block;background:#888;vertical-align:middle'></span>&nbsp;&nbsp;"
    "RSSI:&nbsp;<span id='rssi-value'>-</span>&nbsp;dBm"
    "</div>"
    "</div>"
    "<div class='section'>"
    "<h2>Operatore</h2>"
    "<div class='data-row'><span class='data-label'>Nome:</span><span class='data-value' id='operatore'>-</span></div>"
    "<div class='data-row'><span class='data-label'>Matricola:</span><span class='data-value' id='matricola'>-</span></div>"
    "<div class='data-row'><span class='data-label'>Sessione:</span><span class='badge' id='sessione'>-</span></div>"
    "</div>"
    "<div class='section'>"
    "<h2>Ordine</h2>"
    "<select class='order-selector' id='order-select' onchange='changeOrder()'></select>"
    "</div>"
    "<div class='section'>"
    "<h2>Articolo</h2>"
    "<div class='data-row'><span class='data-label'>Codice:</span><span class='data-value' id='codice'>-</span></div>"
    "<div class='data-row'><span class='data-label'>Descrizione:</span><span class='data-value' id='descrizione'>-</span></div>"
    "<div class='data-row'><span class='data-label'>OdP:</span><span class='data-value' id='odp'>-</span></div>"
    "<div class='data-row'><span class='data-label'>Ciclo:</span><span class='data-value' id='ciclo'>-</span></div>"
    "<div class='data-row'><span class='data-label'>Fase:</span><span class='data-value' id='fase'>-</span></div>"
    "</div>"
    "<div class='section'>"
    "<h2>Produzione Scatola</h2>"
    "<div class='data-row'><span class='data-label'>Scatola:</span><span class='data-value' id='scatola'>-</span></div>"
    "<div class='progress'><div class='progress-bar' id='progress-scatola' style='width:0%'><span style='position:absolute;right:10px'>0/0</span></div></div>"
    "</div>"
    "<div class='section'>"
    "<h2>Quantita</h2>"
    "<div class='data-row'><span class='data-label'>Totale OdP:</span><span class='data-value' id='qta-totale'>0</span></div>"
    "<div class='data-row'><span class='data-label'>Fase:</span><span class='data-value' id='qta-fase'>0</span></div>"
    "<div class='data-row'><span class='data-label'>Sessione:</span><span class='data-value' id='qta-sessione'>0</span></div>"
    "<div class='data-row'><span class='data-label'>Giornaliera:</span><span class='data-value' id='qta-giornaliera'>0</span></div>"
    "</div>"
    "<div class='timestamp' id='timestamp'>In attesa dati...</div>"
    "<div class='links'>"
    "<a href='#' id='settings-link' class='settings-link'>Impostazioni</a>"
    "<a href='/logs' class='settings-link'>Log Terminal</a>"
    "</div>"
    "</div>"
    "<script>"
    "let titleUpdated=false;"
    "let selectedIdx=0;"
    "let allItems=[];"
    "function changeOrder(){"
    "const sel=document.getElementById('order-select');"
    "selectedIdx=parseInt(sel.value);"
    "fetch('/api/set_index',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:selectedIdx})});"
    "updateDisplay();"
    "}"
    "function updateDropdown(){"
    "const sel=document.getElementById('order-select');"
    "if(!sel)return;"
    "sel.innerHTML='';"
    "allItems.forEach((item,i)=>{"
    "const opt=document.createElement('option');"
    "opt.value=i;"
    "opt.textContent=(i+1)+' - '+item.codice_articolo+' (OdP: '+item.ord_prod+')';"
    "sel.appendChild(opt);"
    "});"
    "sel.value=selectedIdx;"
    "}"
    "function updateDisplay(){"
    "if(!allItems.length)return;"
    "const d=allItems[selectedIdx]||allItems[0];"
    "if(!titleUpdated&&d.banchetto){"
    "document.getElementById('page-title').textContent='Banchetto Live '+d.banchetto;"
    "document.title='Banchetto Live '+d.banchetto;"
    "titleUpdated=true;"
    "}"
    "document.getElementById('operatore').textContent=d.operatore||'-';"
    "document.getElementById('matricola').textContent=d.matricola||'-';"
    "const s=document.getElementById('sessione');"
    "s.textContent=d.sessione_aperta?'APERTA':'CHIUSA';"
    "s.className='badge '+(d.sessione_aperta?'open':'closed');"
    "document.getElementById('codice').textContent=d.codice_articolo||'-';"
    "document.getElementById('descrizione').textContent=d.descrizione_articolo||'-';"
    "document.getElementById('odp').textContent=d.ord_prod||'-';"
    "document.getElementById('ciclo').textContent=d.ciclo||'-';"
    "document.getElementById('fase').textContent=d.fase+' - '+d.descr_fase;"
    "document.getElementById('scatola').textContent=d.matr_scatola_corrente||'-';"
    "const p=(d.qta_scatola/d.qta_totale_scatola*100)||0;"
    "const pb=document.getElementById('progress-scatola');"
    "pb.style.width=Math.min(p,100)+'%';"
    "pb.querySelector('span').textContent=d.qta_scatola+'/'+d.qta_totale_scatola;"
    "document.getElementById('qta-totale').textContent=d.qta_totale;"
    "document.getElementById('qta-fase').textContent=d.qta_prod_fase;"
    "document.getElementById('qta-sessione').textContent=d.qta_prod_sessione;"
    "document.getElementById('qta-giornaliera').textContent=d.qta_totale_giornaliera;"
    "document.getElementById('timestamp').textContent='Aggiornato: '+new Date().toLocaleTimeString();"
    "document.getElementById('status-dot').className='status-dot online';"
    "document.getElementById('settings-link').href='/settings?idx='+selectedIdx;"
    "}"
    "function update(){"
    "fetch('/api/data_all').then(r=>r.json()).then(d=>{"
    "allItems=d.items||[];"
    "if(selectedIdx>=allItems.length)selectedIdx=0;"
    "updateDropdown();"
    "updateDisplay();"
    "if(d.version)document.getElementById('fw-version').textContent='v'+d.version;"
    "if(d.rssi!==undefined)document.getElementById('rssi-value').textContent=d.rssi;"
    "var md=document.getElementById('mat-dot');"
    "if(d.mat_ok!==undefined){md.style.display='inline-block';md.style.background=d.mat_ok?'#2ecc71':'#e74c3c';}"
    "else{md.style.display='none';}"
    "}).catch(()=>{"
    "document.getElementById('status-dot').className='status-dot';"
    "});"
    "}"
    "update();"
    "setInterval(update,500);"
    "</script>"
    "</body>"
    "</html>";

// ─────────────────────────────────────────────────────────
// HTML Settings
// ─────────────────────────────────────────────────────────
static const char settings_html[] =
    "<!DOCTYPE html>"
    "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Impostazioni</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;padding:20px}.container{max-width:600px;margin:0 auto;background:#16213e;border-radius:12px;padding:30px;box-shadow:0 8px 32px rgba(0,0,0,0.3)}h1{color:#e94560;margin-bottom:10px;text-align:center;font-size:2em}.subtitle{text-align:center;color:#667eea;font-size:1.1em;margin-bottom:25px}.setting-group{margin:25px 0;padding:20px;background:#0f3460;border-radius:8px}.setting-label{color:#aaa;font-size:1.1em;margin-bottom:10px;display:block}.slider-container{display:flex;align-items:center;gap:15px}input[type=range]{flex:1;height:8px;border-radius:5px;background:#1a1a2e;outline:none;-webkit-appearance:none}input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#667eea;cursor:pointer}.value-display{color:#fff;font-weight:bold;font-size:1.3em;min-width:50px;text-align:right}.btn{display:block;width:100%;padding:15px;margin-top:30px;background:linear-gradient(90deg,#667eea,#764ba2);border:none;border-radius:8px;color:#fff;font-size:1.1em;font-weight:bold;cursor:pointer}.back-link{display:block;text-align:center;margin-top:20px;color:#aaa;text-decoration:none}.status{text-align:center;margin-top:15px;padding:10px;border-radius:5px;display:none}.status.success{background:#2ecc71;color:#fff;display:block}.status.error{background:#e74c3c;color:#fff;display:block}</style></head><body><div class='container'><h1>Impostazioni</h1><div class='subtitle' id='subtitle'>-</div><div class='setting-group'><label class='setting-label'>Volume Buzzer</label><div class='slider-container'><input type='range' id='volume' min='0' max='100' value='100'><span class='value-display' id='volume-value'>100</span></div></div><button class='btn' onclick='saveSettings()'>Applica Volume</button><div class='status' id='status'></div><div class='setting-group' style='margin-top:30px'><label class='setting-label'>Registra Scatola Manualmente</label><input type='text' id='barcode' placeholder='es: SB011196' style='width:100%;padding:12px;background:#1a1a2e;border:2px solid #667eea;border-radius:8px;color:#fff;font-size:1.1em;margin-top:10px'><button class='btn' style='margin-top:15px' onclick='registerBox()'>Registra Scatola</button></div><div class='status' id='status-box'></div><div class='setting-group' style='margin-top:30px'><label class='setting-label'>Login Operatore (Matricola)</label><input type='number' id='matricola' placeholder='es: 157' min='1' max='9999' style='width:100%;padding:12px;background:#1a1a2e;border:2px solid #2ecc71;border-radius:8px;color:#fff;font-size:1.1em;margin-top:10px'><button class='btn' style='margin-top:15px;background:linear-gradient(90deg,#2ecc71,#27ae60)' onclick='loginOperator()'>Login Operatore</button></div><div class='status' id='status-operator'></div><div class='setting-group' style='margin-top:30px'><label class='setting-label'>Segnala Scarti</label><input type='number' id='qta-scarti' placeholder='es: 3' min='1' max='9999' style='width:100%;padding:12px;background:#1a1a2e;border:2px solid #e74c3c;border-radius:8px;color:#fff;font-size:1.1em;margin-top:10px'><button class='btn' style='margin-top:15px;background:linear-gradient(90deg,#e74c3c,#c0392b)' onclick='reportScrap()'>Segnala Scarti</button></div><div class='status' id='status-scrap'></div><a href='/' class='back-link'>Torna alla Dashboard</a></div>"
    "<script>"
    "const params=new URLSearchParams(window.location.search);const idx=parseInt(params.get('idx'))||0;"
    "fetch('/api/set_index',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:idx})});"
    "fetch('/api/data').then(r=>r.json()).then(d=>{document.getElementById('subtitle').textContent=d.codice_articolo+' - OdP: '+d.ord_prod;});"
    "const volumeSlider=document.getElementById('volume');const volumeValue=document.getElementById('volume-value');const statusDiv=document.getElementById('status');"
    "volumeSlider.oninput=function(){volumeValue.textContent=this.value;};"
    "function loadSettings(){fetch('/api/settings').then(r=>r.json()).then(d=>{volumeSlider.value=d.volume||100;volumeValue.textContent=d.volume||100;});}"
    "function saveSettings(){const vol=parseInt(volumeSlider.value);fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({volume:vol})}).then(r=>r.json()).then(d=>{if(d.ok){statusDiv.className='status success';statusDiv.textContent='Volume salvato!';setTimeout(()=>statusDiv.style.display='none',3000);}else{statusDiv.className='status error';statusDiv.textContent='Errore salvataggio';}});}"
    "function registerBox(){const barcode=document.getElementById('barcode').value.trim();const statusBox=document.getElementById('status-box');if(!barcode){statusBox.className='status error';statusBox.textContent='Inserisci codice scatola';return;}fetch('/api/scatola',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({barcode:barcode})}).then(r=>r.json()).then(d=>{if(d.ok){statusBox.className='status success';statusBox.textContent='Scatola '+barcode+' registrata!';document.getElementById('barcode').value='';setTimeout(()=>statusBox.style.display='none',3000);}else{statusBox.className='status error';statusBox.textContent=d.error||'Errore registrazione';}});}"
    "function loginOperator(){const matricola=parseInt(document.getElementById('matricola').value);const statusOp=document.getElementById('status-operator');if(!matricola){statusOp.className='status error';statusOp.textContent='Inserisci matricola';return;}fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({matricola:matricola})}).then(r=>r.json()).then(d=>{if(d.ok){statusOp.className='status success';statusOp.textContent='Operatore '+matricola+' loggato!';setTimeout(()=>statusOp.style.display='none',3000);}else{statusOp.className='status error';statusOp.textContent=d.error||'Login fallito';}});}"
    "function reportScrap(){const qta=parseInt(document.getElementById('qta-scarti').value);const statusScrap=document.getElementById('status-scrap');if(!qta){statusScrap.className='status error';statusScrap.textContent='Inserisci quantita';return;}fetch('/api/scarto',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({qta:qta})}).then(r=>r.json()).then(d=>{if(d.ok){statusScrap.className='status success';statusScrap.textContent=qta+' scarti registrati!';setTimeout(()=>statusScrap.style.display='none',3000);}else{statusScrap.className='status error';statusScrap.textContent=d.error||'Errore scarto';}});}"
    "loadSettings();"
    "</script></body></html>";

// ─────────────────────────────────────────────────────────
// HTML Log Terminal
// ─────────────────────────────────────────────────────────
static const char logs_html[] =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Log Terminal</title><style>*{margin:0;padding:0;box-sizing:border-box}body{background:#0d1117;color:#c9d1d9;font-family:'Courier New',monospace;display:flex;flex-direction:column;height:100vh}.toolbar{background:#161b22;padding:10px 16px;display:flex;gap:10px;align-items:center;border-bottom:1px solid #30363d;flex-shrink:0}.toolbar span{color:#8b949e;font-size:13px;flex:1}button{padding:6px 14px;border:1px solid #30363d;border-radius:6px;background:#21262d;color:#c9d1d9;cursor:pointer;font-size:12px}#terminal{flex:1;overflow-y:auto;padding:12px 16px;font-size:12px;line-height:1.6}.log-I{color:#c9d1d9}.log-W{color:#e3b341}.log-E{color:#f85149}</style></head><body><div class='toolbar'><span>ESP32 Log Terminal</span><span id='counter'>righe: 0</span><button onclick='clearTerminal()'>Clear</button><a href='/'><button>Dashboard</button></a></div><div id='terminal'></div><script>let from=0,total=0;const term=document.getElementById('terminal');function appendLines(lines){lines.forEach(l=>{if(!l.trim())return;const d=document.createElement('div');d.textContent=l;term.appendChild(d);total++;});document.getElementById('counter').textContent='righe: '+total;term.scrollTop=term.scrollHeight;}function clearTerminal(){term.innerHTML='';total=0;document.getElementById('counter').textContent='righe: 0';}function poll(){fetch('/api/logs?from='+from).then(r=>r.json()).then(d=>{if(d.lines&&d.lines.length)appendLines(d.lines);from=d.next;}).finally(()=>setTimeout(poll,1000));}poll();</script></body></html>";

// ─────────────────────────────────────────────────────────
// HTML Collaudo Sim (solo COLLAUDI_BANCHETTO_ID)
// ─────────────────────────────────────────────────────────
#ifdef COLLAUDI_BANCHETTO_ID
static const char collaudo_sim_html[] =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Collaudo Sim</title>"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;padding:20px;display:flex;flex-direction:column;align-items:center;min-height:100vh}"
    ".container{max-width:620px;width:100%;background:#16213e;border-radius:12px;padding:30px;box-shadow:0 8px 32px rgba(0,0,0,0.3)}"
    "h1{color:#e94560;text-align:center;margin-bottom:6px;font-size:1.6em}"
    ".subtitle{text-align:center;color:#667eea;font-size:0.9em;margin-bottom:28px}"
    ".group{background:#0f3460;border-radius:10px;padding:20px;margin-bottom:20px}"
    ".group h2{color:#aaa;font-size:1em;margin-bottom:14px;text-transform:uppercase;letter-spacing:1px}"
    ".val{text-align:center;font-size:2.4em;font-weight:bold;color:#fff;margin-bottom:16px;font-family:monospace}"
    ".btns{display:flex;gap:12px}"
    ".btn{flex:1;padding:18px 0;font-size:1.5em;font-weight:bold;border:none;border-radius:8px;cursor:pointer;transition:transform 0.1s,opacity 0.1s}"
    ".btn:active{transform:scale(0.95);opacity:0.85}"
    ".btn-plus{background:linear-gradient(135deg,#2ecc71,#27ae60);color:#fff}"
    ".btn-minus{background:linear-gradient(135deg,#e74c3c,#c0392b);color:#fff}"
    ".step-row{display:flex;align-items:center;justify-content:space-between;margin-top:12px}"
    ".step-row label{color:#aaa;font-size:0.9em}"
    "select{background:#1a1a2e;color:#fff;border:1px solid #667eea;border-radius:6px;padding:6px 10px;font-size:0.95em}"
    ".back{display:block;text-align:center;margin-top:20px;color:#667eea;text-decoration:none;font-size:0.95em}"
    ".status{text-align:center;color:#aaa;font-size:0.8em;margin-top:16px}"
    ".main-row{display:flex;gap:12px;align-items:stretch}"
    ".left-col{flex:2;display:flex;flex-direction:column}"
    ".group-fuel{flex:1;display:flex;flex-direction:column}"
    ".fuel-bar-wrap{flex:1;min-height:100px;background:#0a1628;border-radius:8px;margin-bottom:12px;overflow:hidden;display:flex;align-items:flex-end}"
    ".fuel-bar{width:100%;transition:height 0.4s,background-color 0.4s;border-radius:6px 6px 0 0}"
    "</style></head><body>"
    "<div class='container'>"
    "<h1>Simulatore Bilancia</h1>"
    "<div class='subtitle'>Collaudo ID " COLLAUDI_BANCHETTO_ID "</div>"
    "<div class='main-row'>"
    "<div class='left-col'>"
    "<div class='group'>"
    "<h2>Consumo [kg/h]</h2>"
    "<div class='val' id='val-consumo'>0.000</div>"
    "<div class='btns'>"
    "<button class='btn btn-minus' onclick='adj(\"consumo\",-1)'>&#8722;</button>"
    "<button class='btn btn-plus'  onclick='adj(\"consumo\",+1)'>&#43;</button>"
    "</div>"
    "<div class='step-row'><label>Passo:</label>"
    "<select id='step-consumo'>"
    "<option value='0.001'>0.001</option>"
    "<option value='0.01'>0.01</option>"
    "<option value='0.1' selected>0.1</option>"
    "<option value='0.5'>0.5</option>"
    "<option value='1'>1.0</option>"
    "</select></div>"
    "</div>"
    "<div class='group'>"
    "<h2>Giri [RPM]</h2>"
    "<div class='val' id='val-giri'>0</div>"
    "<div class='btns'>"
    "<button class='btn btn-minus' onclick='adj(\"giri\",-1)'>&#8722;</button>"
    "<button class='btn btn-plus'  onclick='adj(\"giri\",+1)'>&#43;</button>"
    "</div>"
    "<div class='step-row'><label>Passo:</label>"
    "<select id='step-giri'>"
    "<option value='1'>1</option>"
    "<option value='10' selected>10</option>"
    "<option value='50'>50</option>"
    "<option value='100'>100</option>"
    "<option value='500'>500</option>"
    "</select></div>"
    "</div>"
    "</div>"
    "<div class='group group-fuel'>"
    "<h2>Benzina [%]</h2>"
    "<div class='fuel-bar-wrap'>"
    "<div class='fuel-bar' id='bar-benzina' style='height:100%;background:#2ecc71'></div>"
    "</div>"
    "<div class='val' id='val-benzina'>0%</div>"
    "<div class='btns'>"
    "<button class='btn btn-minus' onclick='adj(\"benzina\",-1)'>&#8722;</button>"
    "<button class='btn btn-plus'  onclick='adj(\"benzina\",+1)'>&#43;</button>"
    "</div>"
    "<input type='hidden' id='step-benzina' value='10'>"
    "</div>"
    "</div>"
    "<div class='status' id='status'>Pronto</div>"
    "<a href='/' class='back'>&#8592; Dashboard</a>"
    "</div>"
    "<script>"
    "let consumo=0,giri=0,benzina=452.2;"
    "function updBenz(){"
    "var b=document.getElementById('bar-benzina');"
    "var pct=Math.max(0,Math.min(100,(benzina-200)/(452.2-200)*100));"
    "b.style.height=pct+'%';"
    "b.style.background=benzina>330?'#2ecc71':benzina>200?'#f39c12':'#e74c3c';"
    "document.getElementById('val-benzina').textContent=benzina.toFixed(1)+' g';}"
    "function adj(type,sign){"
    "const step=parseFloat(document.getElementById('step-'+type).value);"
    "const delta=sign*step;"
    "fetch('/api/collaudo_sim',{method:'POST',"
    "headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({type:type,delta:delta})})"
    ".then(r=>r.json()).then(d=>{"
    "if(d.ok){consumo=d.consumo;giri=d.giri;benzina=d.benzina;"
    "document.getElementById('val-consumo').textContent=consumo.toFixed(3);"
    "document.getElementById('val-giri').textContent=Math.round(giri);"
    "updBenz();"
    "document.getElementById('status').textContent='OK';}"
    "else{document.getElementById('status').textContent='Errore: '+d.error;}"
    "}).catch(()=>document.getElementById('status').textContent='Errore connessione');}"
    "fetch('/api/collaudo_sim',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({type:'get',delta:0})})"
    ".then(r=>r.json()).then(d=>{if(d.ok){consumo=d.consumo;giri=d.giri;benzina=d.benzina;"
    "document.getElementById('val-consumo').textContent=consumo.toFixed(3);"
    "document.getElementById('val-giri').textContent=Math.round(giri);updBenz();}});"
    "</script></body></html>";
#endif // COLLAUDI_BANCHETTO_ID

// ─────────────────────────────────────────────────────────
// HANDLERS
// ─────────────────────────────────────────────────────────
static esp_err_t dashboard_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, dashboard_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t settings_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, settings_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t logs_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, logs_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t ota_update_handler(httpd_req_t *req)
{
    httpd_resp_send(req, "Aggiornamento OTA avviato... l'ESP32 si riavviera' a breve.", HTTPD_RESP_USE_STRLEN);
    ota_manager_start();
    return ESP_OK;
}

#ifdef COLLAUDI_BANCHETTO_ID
// Valori live lato server (unica source of truth per il browser)
static float s_sim_consumo  = 0.0f;
static float s_sim_giri     = 0.0f;
static float s_sim_benzina  = 452.2f;  // grammi — serbatoio pieno

static esp_err_t collaudo_sim_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, collaudo_sim_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t api_collaudo_sim_handler(httpd_req_t *req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "no data");
        return ESP_OK;
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_OK;
    }

    cJSON *type_item  = cJSON_GetObjectItem(json, "type");
    cJSON *delta_item = cJSON_GetObjectItem(json, "delta");

    if (type_item && cJSON_IsString(type_item) && delta_item && cJSON_IsNumber(delta_item)) {
        float delta = (float)delta_item->valuedouble;
        const char *type = type_item->valuestring;

        if (strcmp(type, "consumo") == 0) {
            s_sim_consumo += delta;
            if (s_sim_consumo < 0.0f) s_sim_consumo = 0.0f;
            collaudo_sim_set_consumo(s_sim_consumo);
        } else if (strcmp(type, "giri") == 0) {
            s_sim_giri += delta;
            if (s_sim_giri < 0.0f) s_sim_giri = 0.0f;
            collaudo_sim_set_giri(s_sim_giri);
        } else if (strcmp(type, "benzina") == 0) {
            s_sim_benzina += delta;
            if (s_sim_benzina < 200.0f) s_sim_benzina = 200.0f;
            if (s_sim_benzina > 452.2f) s_sim_benzina = 452.2f;
            collaudo_sim_set_benzina(s_sim_benzina);
        }
        // type=="get" → nessuna modifica, restituisce solo i valori
    }

    cJSON_Delete(json);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON_AddNumberToObject(resp, "consumo",  (double)s_sim_consumo);
    cJSON_AddNumberToObject(resp, "giri",     (double)s_sim_giri);
    cJSON_AddNumberToObject(resp, "benzina",  (double)s_sim_benzina);
    char *resp_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    return ESP_OK;
}
#endif // COLLAUDI_BANCHETTO_ID

static esp_err_t api_logs_handler(httpd_req_t *req)
{
    char query[32] = {0};
    uint32_t from = 0;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        char val[16] = {0};
        if (httpd_query_key_value(query, "from", val, sizeof(val)) == ESP_OK)
            from = (uint32_t)atoi(val);
    }
    const size_t buf_size = 16 * 1024;
    char *buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!buf)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    uint32_t last_idx = from;
    uint32_t count = log_manager_get_lines(from, buf, buf_size, &last_idx);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "next", last_idx);
    cJSON *arr = cJSON_AddArrayToObject(root, "lines");
    if (count > 0)
    {
        char *line = strtok(buf, "\n");
        while (line)
        {
            if (strlen(line) > 0)
                cJSON_AddItemToArray(arr, cJSON_CreateString(line));
            line = strtok(NULL, "\n");
        }
    }
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(buf);
    if (!json_str)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ESP_OK;
}

static esp_err_t api_data_handler(httpd_req_t *req)
{
    banchetto_data_t data;
    if (!banchetto_manager_get_data(&data))
    {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_send(req, "{\"error\":\"busy\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "banchetto", data.banchetto);
    cJSON_AddStringToObject(root, "operatore", data.operatore);
    cJSON_AddStringToObject(root, "matricola", data.matricola);
    cJSON_AddBoolToObject(root, "sessione_aperta", data.sessione_aperta);
    cJSON_AddStringToObject(root, "codice_articolo", data.codice_articolo);
    cJSON_AddStringToObject(root, "descrizione_articolo", data.descrizione_articolo);
    cJSON_AddStringToObject(root, "fase", data.fase);
    cJSON_AddStringToObject(root, "descr_fase", data.descr_fase);
    cJSON_AddStringToObject(root, "ciclo", data.ciclo);
    cJSON_AddStringToObject(root, "matr_scatola_corrente", data.matr_scatola_corrente);
    cJSON_AddNumberToObject(root, "ord_prod", data.ord_prod);
    cJSON_AddNumberToObject(root, "qta_totale", data.qta_totale);
    cJSON_AddNumberToObject(root, "qta_scatola", data.qta_scatola);
    cJSON_AddNumberToObject(root, "qta_totale_scatola", data.qta_totale_scatola);
    cJSON_AddNumberToObject(root, "qta_prod_fase", data.qta_prod_fase);
    cJSON_AddNumberToObject(root, "qta_prod_sessione", data.qta_prod_sessione);
    cJSON_AddNumberToObject(root, "qta_totale_giornaliera", data.qta_totale_giornaliera);
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json_str == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ESP_OK;
}

static esp_err_t api_data_all_handler(httpd_req_t *req)
{
    uint8_t count = banchetto_manager_get_count();
    uint8_t current = banchetto_manager_get_current_index();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "count", count);
    cJSON_AddNumberToObject(root, "current_index", current);
    cJSON *arr = cJSON_AddArrayToObject(root, "items");
    for (int i = 0; i < count; i++)
    {
        banchetto_data_t data;
        if (!banchetto_manager_get_item(i, &data))
            continue;
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "banchetto", data.banchetto);
        cJSON_AddStringToObject(item, "operatore", data.operatore);
        cJSON_AddStringToObject(item, "matricola", data.matricola);
        cJSON_AddBoolToObject(item, "sessione_aperta", data.sessione_aperta);
        cJSON_AddStringToObject(item, "codice_articolo", data.codice_articolo);
        cJSON_AddStringToObject(item, "descrizione_articolo", data.descrizione_articolo);
        cJSON_AddStringToObject(item, "fase", data.fase);
        cJSON_AddStringToObject(item, "descr_fase", data.descr_fase);
        cJSON_AddStringToObject(item, "ciclo", data.ciclo);
        cJSON_AddStringToObject(item, "matr_scatola_corrente", data.matr_scatola_corrente);
        cJSON_AddNumberToObject(item, "ord_prod", data.ord_prod);
        cJSON_AddNumberToObject(item, "qta_totale", data.qta_totale);
        cJSON_AddNumberToObject(item, "qta_scatola", data.qta_scatola);
        cJSON_AddNumberToObject(item, "qta_totale_scatola", data.qta_totale_scatola);
        cJSON_AddNumberToObject(item, "qta_prod_fase", data.qta_prod_fase);
        cJSON_AddNumberToObject(item, "qta_prod_sessione", data.qta_prod_sessione);
        cJSON_AddNumberToObject(item, "qta_totale_giornaliera", data.qta_totale_giornaliera);
        cJSON_AddItemToArray(arr, item);
    }
    const esp_app_desc_t *app_desc = esp_app_get_description();
    cJSON_AddStringToObject(root, "version", app_desc->version);
    cJSON_AddNumberToObject(root, "rssi", wifi_get_rssi());
    {
        banchetto_data_t bd;
        if (banchetto_manager_get_data(&bd) && strcmp(bd.banchetto, "233") == 0) {
            cJSON_AddBoolToObject(root, "mat_ok", tagliatubi_manager_is_materiale());
        }
    }
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str)
    {
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ESP_OK;
}

static esp_err_t api_set_index_handler(httpd_req_t *req)
{
    char buf[64];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "no data");
        return ESP_OK;
    }
    buf[ret] = '\0';
    cJSON *json = cJSON_Parse(buf);
    if (!json)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_OK;
    }
    cJSON *idx_item = cJSON_GetObjectItem(json, "index");
    if (idx_item && cJSON_IsNumber(idx_item))
    {
        uint8_t idx = (uint8_t)idx_item->valueint;
        banchetto_manager_set_current_index(idx);
        httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    }
    else
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing index");
    cJSON_Delete(json);
    return ESP_OK;
}

static esp_err_t api_settings_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "volume", settings_get_volume());
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ESP_OK;
}

static esp_err_t api_settings_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';
    cJSON *json = cJSON_Parse(buf);
    if (json)
    {
        cJSON *v = cJSON_GetObjectItem(json, "volume");
        if (v && cJSON_IsNumber(v))
        {
            settings_set_volume((uint8_t)v->valueint);
            httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
        }
        cJSON_Delete(json);
    }
    return ESP_OK;
}

static esp_err_t api_scatola_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';
    cJSON *json = cJSON_Parse(buf);
    if (json)
    {
        cJSON *b = cJSON_GetObjectItem(json, "barcode");
        if (b && cJSON_IsString(b))
        {
            banchetto_manager_set_barcode(b->valuestring);
            httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
        }
        cJSON_Delete(json);
    }
    return ESP_OK;
}

static esp_err_t api_login_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';
    cJSON *json = cJSON_Parse(buf);
    if (json)
    {
        cJSON *m = cJSON_GetObjectItem(json, "matricola");
        if (m && cJSON_IsNumber(m))
        {
            banchetto_manager_login_by_matricola((uint16_t)m->valueint);
            httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
        }
        cJSON_Delete(json);
    }
    return ESP_OK;
}

static esp_err_t api_scarto_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';
    cJSON *json = cJSON_Parse(buf);
    if (json)
    {
        cJSON *q = cJSON_GetObjectItem(json, "qta");
        if (q && cJSON_IsNumber(q))
        {
            banchetto_manager_scarto((uint32_t)q->valueint);
            httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
        }
        cJSON_Delete(json);
    }
    return ESP_OK;
}

void web_server_broadcast_update(void) {}

// ─────────────────────────────────────────────────────────
// INIT
// ─────────────────────────────────────────────────────────
esp_err_t web_server_init(void)
{
    ESP_LOGI(TAG, "Inizializzazione Web Server");
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    config.max_uri_handlers = 17; // Spazio per /update + /collaudo
    config.task_priority = 3;
    config.lru_purge_enable = true;
    config.core_id = 1;
    config.stack_size = 8192; // <--- AGGIUNGI QUESTA RIGA

    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Errore avvio server HTTP");
        return ESP_FAIL;
    }

    httpd_uri_t uris[] = {
        {.uri = "/", .method = HTTP_GET, .handler = dashboard_handler},
        {.uri = "/settings", .method = HTTP_GET, .handler = settings_handler},
        {.uri = "/logs", .method = HTTP_GET, .handler = logs_page_handler},
        {.uri = "/update", .method = HTTP_GET, .handler = ota_update_handler},
        {.uri = "/api/data", .method = HTTP_GET, .handler = api_data_handler},
        {.uri = "/api/data_all", .method = HTTP_GET, .handler = api_data_all_handler},
        {.uri = "/api/logs", .method = HTTP_GET, .handler = api_logs_handler},
        {.uri = "/api/settings", .method = HTTP_GET, .handler = api_settings_get_handler},
        {.uri = "/api/settings", .method = HTTP_POST, .handler = api_settings_post_handler},
        {.uri = "/api/set_index", .method = HTTP_POST, .handler = api_set_index_handler},
        {.uri = "/api/scatola", .method = HTTP_POST, .handler = api_scatola_post_handler},
        {.uri = "/api/login", .method = HTTP_POST, .handler = api_login_post_handler},
        {.uri = "/api/scarto", .method = HTTP_POST, .handler = api_scarto_post_handler},
    };

    for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++)
    {
        httpd_register_uri_handler(server, &uris[i]);
    }

#ifdef COLLAUDI_BANCHETTO_ID
    {
        httpd_uri_t coll_page = {.uri = "/collaudo",          .method = HTTP_GET,  .handler = collaudo_sim_page_handler};
        httpd_uri_t coll_api  = {.uri = "/api/collaudo_sim",  .method = HTTP_POST, .handler = api_collaudo_sim_handler};
        httpd_register_uri_handler(server, &coll_page);
        httpd_register_uri_handler(server, &coll_api);
        ESP_LOGI(TAG, "Simulatore bilancia attivo su /collaudo");
    }
#endif

    ESP_LOGI(TAG, "Server HTTP avviato su Core 1 con supporto OTA");
    return ESP_OK;
}