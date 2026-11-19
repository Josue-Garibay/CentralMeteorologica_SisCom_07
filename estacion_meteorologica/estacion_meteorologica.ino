#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>

// Cambiar valor del ID dependiendo de la estación (ESP32)
const uint8_t NODO_ID = 2;

// Configuración del sensor DHT11 en GPIO 4 (D4)
#define PIN_DHT 4
#define TIPO_DHT DHT11
DHT dht(PIN_DHT, TIPO_DHT);

// Dirección MAC de la ESP32 Central (04:B2:47:20:0A:6C)
uint8_t macCentral[] = {0x04, 0xB2, 0x47, 0x20, 0x0A, 0x6C};

typedef struct struct_mensaje {
  uint8_t idNodo;
  float temperatura;
  float humedad;
} struct_mensaje;

struct_mensaje datosEnviar;
esp_now_peer_info_t peerInfo;

const unsigned long intervaloEnvio = 2000;
unsigned long ultimoEnvio = 0;

void alEnviar(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Estado del envio: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Exito" : "Fallo");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  dht.begin();

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error al inicializar ESP-NOW");
    return;
  }

  esp_now_register_send_cb(alEnviar);

  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, macCentral, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error al agregar la central como peer");
    return;
  }

  Serial.printf("Emisor DHT11 #%d listo en GPIO %d.\n", NODO_ID, PIN_DHT);
}

void loop() {
  unsigned long ahora = millis();
  if (ahora - ultimoEnvio >= intervaloEnvio) {
    ultimoEnvio = ahora;

    float hum = dht.readHumidity();
    float temp = dht.readTemperature();

    if (isnan(hum) || isnan(temp)) {
      Serial.println("Error al leer el DHT11. Revisar conexiones.");
      return;
    }

    datosEnviar.idNodo = NODO_ID;
    datosEnviar.temperatura = temp;
    datosEnviar.humedad = hum;

    esp_err_t resultado = esp_now_send(macCentral, (uint8_t *)&datosEnviar, sizeof(datosEnviar));
    if (resultado == ESP_OK) {
      Serial.printf("Nodo %d -> Temp: %.1f C | Hum: %.1f %%\n",
                    datosEnviar.idNodo, datosEnviar.temperatura, datosEnviar.humedad);
    } else {
      Serial.println("Error al encolar paquete ESP-NOW");
    }
  }
}