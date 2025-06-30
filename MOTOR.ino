#define STEP    18
#define DIR     5
#define ENABLE  4

// Pines del sensor de ultrasonidos
#define ECHO    12
#define TRIG    13

// Configuración lógica
#define FORW    LOW    // asumiendo que LOW es “hacia adelante” o “alejarse”
#define ON      HIGH
#define OFF     LOW

// Parámetros PWM
#define PWM_FREQUENCY   256
#define PWM_RESOLUTION  8
#define PWM_DUTY        128   // 50% de duty cycle (ajústalo si quieres más/menos velocidad)

// Umbral central y margen de histéresis (en cm)
const float UMBRAL_CENTRAL    = 30.0;   // 30 cm es el punto de parada
const float HISTERESIS        = 2.0;    // ±2 cm
const float UMBRAL_INFERIOR   = UMBRAL_CENTRAL - HISTERESIS;  // 28 cm
const float UMBRAL_SUPERIOR   = UMBRAL_CENTRAL + HISTERESIS;  // 32 cm

bool motorEnabled = true;  // estado actual del motor

void setup() {
  // Prepara pines del driver de motor
  pinMode(ENABLE, OUTPUT);
  pinMode(DIR, OUTPUT);

  // Prepara pines del sensor ultrasonidos
  pinMode(ECHO, INPUT);
  pinMode(TRIG, OUTPUT);
  digitalWrite(TRIG, LOW);  // asegurarse de empezar con TRIG en LOW

  // Habilita el driver y fija la dirección hacia adelante (alejarse)
  digitalWrite(ENABLE, ON);
  digitalWrite(DIR, FORW);

  // Configura PWM en STEP y arranca con duty > 0
  ledcAttach(STEP, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcWrite(STEP, PWM_DUTY);
}

void loop() {
  // 1) Disparar un pulso de 10 µs en TRIG
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  // 2) Leer el pulso en ECHO (duración en microsegundos)
  long duracion = pulseIn(ECHO, HIGH, 30000);  
  // timeout de 30 ms para evitar quedarse bloqueado si no hay respuesta

  // 3) Calcular distancia en cm
  float distancia_cm = (duracion / 2.0) / 29.1; 

  // 4) Lógica de histéresis:
  //
  //    - Si el motor está habilitado y la distancia ≥ UMBRAL_SUPERIOR, lo detenemos.
  //    - Si el motor está detenido y la distancia ≤ UMBRAL_INFERIOR, lo habilitamos.
  if (motorEnabled) {
    if (distancia_cm >= UMBRAL_SUPERIOR) {
      // Detener motor (activar freno)
      digitalWrite(ENABLE, OFF);
      motorEnabled = false;
    }
  } else {
    if (distancia_cm <= UMBRAL_INFERIOR) {
      // Volver a habilitar motor (seguir alejándose)
      digitalWrite(ENABLE, ON);
      motorEnabled = true;
    }
  }

  // 5) (Opcional) Imprimir por serie para depuración
  Serial.print("Distancia = ");
  Serial.print(distancia_cm);
  Serial.print(" cm   | Motor ");
  Serial.println(motorEnabled ? "ENABLED" : "DISABLED");

  // Pequeño retardo para no saturar las lecturas
  delay(50);
}
