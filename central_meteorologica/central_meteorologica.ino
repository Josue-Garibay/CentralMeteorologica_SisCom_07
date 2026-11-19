#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>

// ========== 1. CONFIGURACION DEL PUNTO DE ACCESO (AP) ==========
const char* apSsid = "Central_Meteo_ESP32";
const char* apPassword = "meteo1314"; // Mínimo 8 caracteres

WebServer server(80);

// ========== 2. ESTRUCTURA Y ESTADO DE ESTACIONES ==========
typedef struct struct_mensaje {
  uint8_t idNodo;
  float temperatura;
  float humedad;
} struct_mensaje;

struct DatosEstacion {
  bool activa = false;
  float temperatura = 0.0;
  float humedad = 0.0;
  unsigned long ultimaVezMs = 0;
};

DatosEstacion est1;
DatosEstacion est2;
volatile bool hayNuevoDato = false;

// ========== 3. PAGINA WEB (HTML + JS) ==========
const char paginaWeb[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Central Meteorologica ESP-NOW</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin: 24px; background: #f0f2f5; }
    .grid { display: flex; flex-wrap: wrap; justify-content: center; gap: 16px; max-width: 720px; margin: 0 auto; }
    .card { background: #fff; flex: 1 1 280px; padding: 20px; border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.08); }
    h1 { color: #222; margin-bottom: 20px; }
    h2 { margin-top: 0; color: #0066cc; }
    .dato { font-size: 1.2em; margin: 12px 0; padding-bottom: 8px; border-bottom: 1px solid #eee; text-align: left; }
    .dato span { float: right; font-weight: bold; color: #111; }
    .estado { font-size: 0.9em; color: #555; margin-top: 10px; }
  </style>
  <script>
    function actualizarDatos() {
      fetch('/data')
        .then(res => res.json())
        .then(d => {
          document.getElementById('t1').textContent = d.est1.activa ? d.est1.temperatura.toFixed(1) + ' °C' : '--';
          document.getElementById('h1').textContent = d.est1.activa ? d.est1.humedad.toFixed(1) + ' %' : '--';
          document.getElementById('s1').textContent = d.est1.activa ? 'Recibiendo datos' : 'Sin datos aun';

          document.getElementById('t2').textContent = d.est2.activa ? d.est2.temperatura.toFixed(1) + ' °C' : '--';
          document.getElementById('h2').textContent = d.est2.activa ? d.est2.humedad.toFixed(1) + ' %' : '--';
          document.getElementById('s2').textContent = d.est2.activa ? 'Recibiendo datos' : 'Sin datos aun';
        })
        .catch(err => console.error('Error al consultar /data:', err));
    }
    setInterval(actualizarDatos, 2000);
    window.onload = actualizarDatos;
  </script>
</head>
<body>
  <h1>Central Meteorológica (ESP-NOW)</h1>
  <div class="grid">
    <div class="card">
      <h2>Estación 1</h2>
      <div class="dato">Temperatura <span id="t1">--</span></div>
      <div class="dato">Humedad <span id="h1">--</span></div>
      <div class="estado" id="s1">Esperando...</div>
    </div>
    <div class="card">
      <h2>Estación 2</h2>
      <div class="dato">Temperatura <span id="t2">--</span></div>
      <div class="dato">Humedad <span id="h2">--</span></div>
      <div class="estado" id="s2">Esperando...</div>
    </div>
  </div>
</body>
</html>
)rawliteral";

// ========== 4. CALLBACK ESP-NOW ==========
void alRecibir(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(struct_mensaje)) return;

  struct_mensaje msg;
  memcpy(&msg, incomingData, sizeof(msg));

  if (msg.idNodo == 1) {
    est1.activa = true;
    est1.temperatura = msg.temperatura;
    est1.humedad = msg.humedad;
    est1.ultimaVezMs = millis();
  } else if (msg.idNodo == 2) {
    est2.activa = true;
    est2.temperatura = msg.temperatura;
    est2.humedad = msg.humedad;
    est2.ultimaVezMs = millis();
  }
  hayNuevoDato = true;
}

// ========== 5. RUTAS HTTP ==========
void handleRoot() {
  server.send(200, "text/html", paginaWeb);
}

void handleData() {
  String json = "{";
  json += "\"est1\":{\"activa\":" + String(est1.activa ? "true" : "false") +
          ",\"temperatura\":" + String(est1.temperatura, 1) +
          ",\"humedad\":" + String(est1.humedad, 1) + "},";
  json += "\"est2\":{\"activa\":" + String(est2.activa ? "true" : "false") +
          ",\"temperatura\":" + String(est2.temperatura, 1) +
          ",\"humedad\":" + String(est2.humedad, 1) + "}";
  json += "}";
  server.send(200, "application/json", json);
}

// ========== 6. SETUP Y LOOP ==========
void setup() {
  Serial.begin(115200);
  delay(500);

  // Modo dual para recibir ESP-NOW y crear su propia red Wi-Fi (AP)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid, apPassword);

  Serial.print("Red Wi-Fi creada: ");
  Serial.println(apSsid);
  Serial.print("Abre en tu navegador: http://");
  Serial.println(WiFi.softAPIP()); // Normalmente http://192.168.4.1

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error al inicializar ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(alRecibir);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Servidor web iniciado en puerto 80.");
}

void loop() {
  server.handleClient();

  if (hayNuevoDato) {
    hayNuevoDato = false;
    Serial.printf("E1 [%.1f C, %.1f %%] | E2 [%.1f C, %.1f %%]\n",
                  est1.temperatura, est1.humedad,
                  est2.temperatura, est2.humedad);
  }
}