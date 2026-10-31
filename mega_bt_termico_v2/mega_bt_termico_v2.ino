/*
  SISTEMA MODULAR - ARQUITETURA HÍBRIDA ESTÁVEL (OMNICANAL + DSP)
*/
#include <OneWire.h>
#include <DallasTemperature.h>

#define PIN_ZC 2
#define PIN_DIM 3
#define PIN_FAN 6
#define PIN_DS18B20 8
#define PIN_CURRENT A0

volatile unsigned long lastZC = 0; 
volatile int atrasoDimmerAtual = 8200; 
volatile int powerAtual = 0;
volatile unsigned long zcCounter = 0;

OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);

int ensaioTargetPower = 0;               
unsigned long ensaioDurationMillis = 0;  
unsigned long ensaioStartMillis = 0;       
bool ensaioRunning = false;  
bool monitorMode = false;

// Variáveis código 1
unsigned long ensaioDurationZC = 0;  // Agora contado em ciclos elétricos
unsigned long ensaioStartZC = 0;

const byte MASCARA_BUFFER = 64; 
char bufferUSB[MASCARA_BUFFER];
char bufferBT[MASCARA_BUFFER];

// Controladores de Tempo (Substituem o Timer3 problemático)
unsigned long lastCurrentTime = 0;
unsigned long lastTempTime = 0;
unsigned long lastTelemetryTime = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_FAN, OUTPUT);
  digitalWrite(PIN_FAN, LOW);

  BT_Init();
  Sensor_Init();
  Dimmer_Init();

  EnviarMensagem("--- Sistema Hibrido Estavel Pronto ---");
}

void loop() {
  // 1. Lê a corrente continuamente (muito rápido, não trava nada)
  Sensor_AmostraContinua();

  // 2. Calcula a matemática da corrente a cada 250ms
  if (millis() - lastCurrentTime >= 250) {
    lastCurrentTime = millis();
    Sensor_CalcularRMS();
  }

  // 3. Atualiza a Temperatura a cada 800ms
  if (millis() - lastTempTime >= 800) {
    lastTempTime = millis();
    Sensor_AmostraTemperatura();
  }

  // 4. Envia os dados para a Interface a cada 250ms
  if (millis() - lastTelemetryTime >= 250) {
    lastTelemetryTime = millis();
    BT_SendTelemetry();
  }

  Ensaio_Update();
  BT_Listen(); 
}

/*
void loop() {
  // Simula o pulso (100Hz) para manter a interface viva se a energia AC cair
  static unsigned long lastFakeZc = 0;
  if (millis() - lastFakeZc > 10) {
      lastFakeZc = millis();
      if (micros() - lastZC > 20000) zcCounter++; 
  }

  Sensor_Update();      // 1. Amostra os sensores SEM TRAVAR o processador
  Dimmer_Update();      // 2. Segurança do Dimmer
  BT_Listen();          // 3. Escuta os comandos Python
  Ensaio_Update();      // 4. Checa o cronômetro do ensaio
  BT_SendTelemetry();   // 5. Envia dados a 4 FPS fluidos
}
*/