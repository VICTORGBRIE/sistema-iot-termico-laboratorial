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
volatile float setpointTemp = 0.0;       
bool ensaioRunning = false;  
bool monitorMode = false;

// Flag que define se o PID está no controle
bool isClosedLoop = false; 

// Variável Global para o Loop de Degraus
bool staircaseRunning = false; 

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
  Controle_Init(); // Inicia as variáveis do PI Digital

  EnviarMensagem("--- Sistema Hibrido Estavel Pronto ---");
}

void loop() {
  Sensor_AmostraContinua();

  if (millis() - lastCurrentTime >= 250) {
    lastCurrentTime = millis();
    Sensor_CalcularRMS();
  }

  if (millis() - lastTempTime >= 800) {
    lastTempTime = millis();
    Sensor_AmostraTemperatura();
  }

  if (millis() - lastTelemetryTime >= 250) {
    lastTelemetryTime = millis();
    BT_SendTelemetry();
  }

  Ensaio_Update();
  Controle_Update(); // Executa o cálculo da Malha Fechada a cada 2 segundos
  BT_Listen(); 
}