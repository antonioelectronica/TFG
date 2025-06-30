#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_VEML7700.h>

// ─── CONFIGURACIÓN DE PINES ────────────────────────────────────────────────────
// Sensores de proximidad (lógica invertida: activo = LOW)
const uint8_t SENSOR1_PIN = 7;
const uint8_t SENSOR2_PIN = 20;
const uint8_t SENSOR3_PIN = 3;

// Tira de LEDs WS2812B
#define LED_PIN    4
#define NUM_LEDS   269
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Sensor de luz VEML7700 (I2C en pines 8 = SDA, 9 = SCL)
const uint8_t SDA_PIN = 8;
const uint8_t SCL_PIN = 9;
Adafruit_VEML7700 veml = Adafruit_VEML7700();

// Buzzer activo en el pin 10
const uint8_t BUZZER_PIN = 10;

// ─── VARIABLES PARA LÓGICA INTERNA ─────────────────────────────────────────────
// Intervalo para lectura de lux (en ms)
const unsigned long LUX_READ_INTERVAL = 1000;
unsigned long lastLuxMillis = 0;
// Último valor leído de lux
float currentLux = 0.0;

// Último número de LEDs encendidos (para evitar parpadeo)
int lastLedsToLight = -1;

// Estado actual de cada sensor (bools)
bool s1 = false, s2 = false, s3 = false;
// Número de LEDs a encender según sensores
int ledsToLight = 0;

// ─── CONFIGURACIÓN DE RED / AP / WEB ────────────────────────────────────────────
// SSID de la red Wi-Fi en modo AP (sin contraseña)
const char* AP_SSID = "TFG ANTONIO";

// Servidor DNS para captive portal
DNSServer dnsServer;
// Servidor HTTP en el ESP para servir la página y datos dinámicos
WebServer server(80);

// HTML + CSS + JS de la página web (como string de una sola pieza)
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>TFG Antonio - Monitorización</title>
  <style>
    body {
      margin: 0;
      font-family: "Helvetica Neue", Helvetica, Arial, sans-serif;
      background-color: #1e1f25;
      color: #eef0f3;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: flex-start;
      min-height: 100vh;
    }
    header {
      background-color: #26272d;
      width: 100%;
      padding: 15px 0;
      text-align: center;
      box-shadow: 0 2px 5px rgba(0,0,0,0.3);
    }
    header h1 {
      margin: 0;
      font-size: 1.8rem;
      color: #f1c40f;
    }
    main {
      flex: 1;
      width: 90%;
      max-width: 600px;
      margin-top: 20px;
    }
    section {
      background-color: #2e2f35;
      border-radius: 8px;
      padding: 20px;
      margin-bottom: 20px;
      box-shadow: 0 1px 4px rgba(0,0,0,0.2);
    }
    section h2 {
      margin-top: 0;
      font-size: 1.4rem;
      color: #3498db;
    }
    .data-row {
      display: flex;
      justify-content: space-between;
      padding: 6px 0;
      border-bottom: 1px solid #3a3b41;
    }
    .data-row:last-child {
      border-bottom: none;
    }
    .label {
      font-weight: 600;
    }
    .value {
      font-family: monospace;
    }
    footer {
      width: 100%;
      text-align: center;
      padding: 10px 0;
      font-size: 0.9rem;
      color: #7f8c8d;
    }
  </style>
</head>
<body>
  <header>
    <h1>TFG - Monitorización en Tiempo Real</h1>
  </header>
  <main>
    <!-- Sección: Luz ambiente (lux) -->
    <section>
      <h2>Iluminación (LUX)</h2>
      <div class="data-row">
        <div class="label">Valor actual:</div>
        <div id="luxValue" class="value">--</div>
      </div>
    </section>

    <!-- Sección: Estado Sensores -->
    <section>
      <h2>Estado de Sensores</h2>
      <div class="data-row">
        <div class="label">Sensor 1 (Pin 7):</div>
        <div id="sensor1" class="value">--</div>
      </div>
      <div class="data-row">
        <div class="label">Sensor 2 (Pin 20):</div>
        <div id="sensor2" class="value">--</div>
      </div>
      <div class="data-row">
        <div class="label">Sensor 3 (Pin 3):</div>
        <div id="sensor3" class="value">--</div>
      </div>
    </section>

    <!-- Sección: Patrón LED -->
    <section>
      <h2>Tira de LEDs</h2>
      <div class="data-row">
        <div class="label">LEDs encendidos:</div>
        <div id="ledCount" class="value">--</div>
      </div>
      <div class="data-row">
        <div class="label">Patrón:</div>
        <div id="ledPattern" class="value">Rojo – Azul – Azul (cíclico)</div>
      </div>
    </section>
  </main>
  <footer>
    Proyecto Fin de Grado – 2025
  </footer>

  <script>
    // Función que pide datos al endpoint /status y actualiza la página
    async function updateData() {
      try {
        const resp = await fetch('/status');
        if (!resp.ok) throw new Error('Error al obtener datos');
        const data = await resp.json();
        // Actualizar valores en el DOM
        document.getElementById('luxValue').innerText = data.lux.toFixed(2) + ' lx';
        document.getElementById('sensor1').innerText = data.s1 ? 'Activado' : 'Inactivo';
        document.getElementById('sensor2').innerText = data.s2 ? 'Activado' : 'Inactivo';
        document.getElementById('sensor3').innerText = data.s3 ? 'Activado' : 'Inactivo';
        document.getElementById('ledCount').innerText = data.leds + ' / ' + data.totalLeds;
      } catch (error) {
        console.error('Error actualización:', error);
      }
    }

    // Llamar a updateData() cada 1 segundo
    setInterval(updateData, 1000);
    // Llamar una vez al cargar
    window.onload = updateData;
  </script>
</body>
</html>
)rawliteral";

// ─── FUNCIONES PARA SERVIDOR WEB ────────────────────────────────────────────────
// Devuelve JSON con los datos actuales (lux, estado sensores, LEDs encendidos)
void handleStatus() {
  // Construimos manualmente el JSON
  String json = "{";
  json += "\"lux\":" + String(currentLux, 2) + ",";
  json += "\"s1\":" + String(s1 ? 1 : 0) + ",";
  json += "\"s2\":" + String(s2 ? 1 : 0) + ",";
  json += "\"s3\":" + String(s3 ? 1 : 0) + ",";
  json += "\"leds\":" + String(ledsToLight) + ",";
  json += "\"totalLeds\":" + String(NUM_LEDS);
  json += "}";
  server.send(200, "application/json", json);
}

// Manda la página HTML en cualquier petición no encontrada (captive portal)
void handleNotFound() {
  server.send_P(200, "text/html", htmlPage);
}

// ─── SETUP ─────────────────────────────────────────────────────────────────────
void setup() {
  // Iniciar Serial para debug / salida de lux
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("=== Iniciando ESP32-C3 en Modo AP con Captive Portal ===");

  // ─── 1. Configurar buzzer ────────────────────────────────────────────────────
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // ─── 2. Sensores de proximidad ───────────────────────────────────────────────
  // Usamos INPUT_PULLUP para lógica invertida (LOW = activo)
  pinMode(SENSOR1_PIN, INPUT_PULLUP);
  pinMode(SENSOR2_PIN, INPUT_PULLUP);
  pinMode(SENSOR3_PIN, INPUT_PULLUP);

  // ─── 3. Inicializar tira de LEDs ─────────────────────────────────────────────
  strip.begin();
  strip.show();            // Apagar todos al inicio
  strip.setBrightness(255);// Brillo (0-255)

  // ─── 4. Inicializar sensor VEML7700 ───────────────────────────────────────────
  Wire.begin(SDA_PIN, SCL_PIN);
  if (veml.begin()) {
    Serial.println("VEML7700 detectado correctamente.");
    // Un pitido breve para indicar que el sensor de lux se detectó bien
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    Serial.println("Error: VEML7700 no encontrado. Revisa conexiones I2C.");
    // Aunque no se detecte VEML7700, continuamos para que el AP se cree igual
  }

  // ─── 5. Configuración Modo AP (sin contraseña) ───────────────────────────────
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  IPAddress apIP = WiFi.softAPIP(); // Normalmente 192.168.4.1
  Serial.print("AP iniciado. SSID: ");
  Serial.print(AP_SSID);
  Serial.print("   IP local: ");
  Serial.println(apIP);

  // Dos pitidos para indicar que el AP se creó correctamente
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
  }

  // ─── 6. Configuración DNS para captive portal ────────────────────────────────
  // DNS responde con la IP del ESP para cualquier dominio (*), así fuerza el portal
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", apIP);

  // ─── 7. Configuración Servidor HTTP ─────────────────────────────────────────
  // Ruta para datos dinámicos
  server.on("/status", HTTP_GET, handleStatus);
  // Cualquier otra ruta sirve la página principal
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Servidor HTTP iniciado en puerto 80");
}

// ─── LOOP ──────────────────────────────────────────────────────────────────────
void loop() {
  // ─── 1. Actualizar estado de sensores (lógica invertida: LOW = detecta) ──────
  s1 = (digitalRead(SENSOR1_PIN) == LOW);
  s2 = (digitalRead(SENSOR2_PIN) == LOW);
  s3 = (digitalRead(SENSOR3_PIN) == LOW);

  // ─── 2. Calcular cuántos LEDs encender según lógica de sensores ──────────────
  if (s1 && !s2 && !s3) {
    ledsToLight = 89;
  } else if (s1 && s2 && !s3) {
    ledsToLight = 179;
  } else if (s1 && s2 && s3) {
    ledsToLight = NUM_LEDS;
  } else {
    ledsToLight = 0;
  }

  // ─── 3. Actualizar tira sólo si cambió la cantidad de LEDs (evita parpadeo) ──
  if (ledsToLight != lastLedsToLight) {
    lastLedsToLight = ledsToLight;
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i < ledsToLight) {
        switch (i % 3) {
          case 0:
            strip.setPixelColor(i, strip.Color(255, 0, 0));   // Rojo
            break;
          case 1:
          case 2:
            strip.setPixelColor(i, strip.Color(0, 0, 255));   // Azul
            break;
        }
      } else {
        strip.setPixelColor(i, 0); // Apagado
      }
    }
    strip.show();
  }

  // ─── 4. Leer lux cada LUX_READ_INTERVAL milisegundos ────────────────────────
  unsigned long ahora = millis();
  if (ahora - lastLuxMillis >= LUX_READ_INTERVAL) {
    lastLuxMillis = ahora;
    currentLux = veml.readLux();
    // También lo mostramos en Serial para debug
    Serial.print("Lux: ");
    Serial.println(currentLux);
  }

  // ─── 5. Atender peticiones del servidor web y DNS (captive portal) ───────────
  dnsServer.processNextRequest();
  server.handleClient();

  // ─── Breve delay para estabilidad ────────────────────────────────────────────
  delay(10);
}
