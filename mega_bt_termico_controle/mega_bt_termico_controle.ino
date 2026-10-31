/*
  SISTEMA MODULAR - ARQUITETURA HÍBRIDA ESTÁVEL (OMNICANAL + DSP)
  Versão: Controle RST - Malha Fechada
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

volatile float setpointTemp = 30.0; //


// Variáveis essenciais para o Bluetooth que faltavam

const byte MASCARA_BUFFER = 64; 
char bufferUSB[MASCARA_BUFFER];
char bufferBT[MASCARA_BUFFER];

// Controladores de Tempo

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
  
  // Inicializa os vetores do controlador RST
  Controle_Init(); 
  
  EnviarMensagem("--- Sistema Hibrido Estavel Pronto (Controle RST) ---");
}

void loop() {
  // 1. Amostragem de corrente contínua
  Sensor_AmostraContinua();

  // 2. Cálculo RMS da corrente (250ms)
  if (millis() - lastCurrentTime >= 250) {
    lastCurrentTime = millis();
    Sensor_CalcularRMS();
  }

  // 3. Atualização da Temperatura (800ms)
  if (millis() - lastTempTime >= 800) {
    lastTempTime = millis();
    Sensor_AmostraTemperatura();
  }

  // 4. Execução da Malha de Controle (Respeita o Ts de 2s definido no módulo)
  Controle_Update(); 

  // 5. Telemetria para a Interface (250ms)
  if (millis() - lastTelemetryTime >= 250) {
    lastTelemetryTime = millis();
    BT_SendTelemetry();
  }

  Ensaio_Update();
  BT_Listen(); 
}