/*
  SISTEMA MODULAR - RELÓGIO DE REDE (GRID CLOCK) E MÁQUINA DE ESTADOS
*/
#include <OneWire.h>
#include <DallasTemperature.h>

#define PIN_ZC 2
#define PIN_DIM 3
#define PIN_FAN 6
#define PIN_DS18B20 8
#define PIN_CURRENT A0

// Globais - Dimmer
volatile unsigned long lastZC = 0; 
volatile int atrasoDimmerAtual = 8200; 
volatile int powerAtual = 0;
volatile unsigned long zcCounter = 0; // NOSSO NOVO RELÓGIO ABSOLUTO (120 Ticks/seg)

// Globais - Sensor
OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);

// Globais - Lógica de Ensaio
int ensaioTargetPower = 0;               
unsigned long ensaioDurationZC = 0;  // Agora contado em ciclos elétricos
unsigned long ensaioStartZC = 0;       
bool ensaioRunning = false;  
bool monitorMode = false;

// Globais - Bluetooth
const byte MASCARA_BUFFER = 64; 
char bufferRececao[MASCARA_BUFFER];

void setup() {
  Serial.begin(115200);
  pinMode(PIN_FAN, OUTPUT);
  digitalWrite(PIN_FAN, LOW);

  BT_Init();
  Sensor_Init();
  Dimmer_Init();
  Serial.println("--- Sistema Mega Modularizado Pronto ---");
}

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