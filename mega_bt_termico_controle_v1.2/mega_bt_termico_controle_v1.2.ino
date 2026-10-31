/*
  SISTEMA MODULAR - ARQUITETURA HÍBRIDA ESTÁVEL (OMNICANAL + DSP)
  Versão: Controle FOPID (Ordem Fracionária - Aproximação Oustaloup)
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
bool staircaseRunning = false;

volatile float setpointTemp = 30.0; 

// =================================================================
// VARIÁVEIS DO CONTROLO FOPID (Ordem 5)
// =================================================================
// =================================================================
// VARIÁVEIS DO CONTROLO FOPID
// =================================================================
// Valores registados para a sua interface (meramente informativos)
/*
volatile float kp = 6.8062e-11, ki = 1.8359e-10, kd = 22.6685;
volatile float lamb = 0.554660, mu = 0.9;

#define TAM_FILTRO 11

// NOSSOS COEFICIENTES OTIMIZADOS:
volatile float b[TAM_FILTRO] = {4.299875e+01, -2.845180e+02, 7.871267e+02, -1.149452e+03, 8.850566e+02, -2.369421e+02, -1.396557e+02, 1.182964e+02, -1.673497e+01, -8.600695e+00, 2.425031e+00};
volatile float a[TAM_FILTRO] = {1.000000e+00, -4.785904e+00, 7.840943e+00, -2.538065e+00, -7.180923e+00, 8.617816e+00, -2.372067e+00, -1.315851e+00, 7.611639e-01, 2.200372e-02, -4.911592e-02};

// Histórico de Estados (ajusta-se automaticamente para 11 posições)
volatile float e_hist[TAM_FILTRO] = {0}; 
volatile float u_hist[TAM_FILTRO] = {0};
*/

// =================================================================
// VARIÁVEIS DO CONTROLE FOPID REDUZIDO (2.ª ORDEM - HANKEL)
// =================================================================
#define TAM_FILTRO 3  // Precisamos apenas de k, k-1 e k-2

// COEFICIENTES DISCRETOS EXATOS (Gerados nativamente no MATLAB)
// Região 1 (Kp=5, Ki=1, Kd=2, Lam=0.81271, Mu=0.62996)
volatile float r1[3] = {28.159052, -4.944946, -23.118227};
volatile float s1[2] = {-0.002628, -0.996774};

// Região 2 (Kp=5, Ki=1, Kd=2, Lam=0.72245, Mu=0.68271)
volatile float r2[3] = {30.550319, -2.493505, -27.928915};
volatile float s2[2] = {-0.003204, -0.996236};

// Região 3 (Kp=5, Ki=1, Kd=2, Lam=0.99122, Mu=0.56279)
volatile float r3[3] = {30.702280, -18.586565, -12.048402};
volatile float s3[2] = {-0.001691, -0.997653};

// Histórico de Estados (Erro e Saída)
volatile float e_hist[TAM_FILTRO] = {0, 0, 0}; 
volatile float u_hist[TAM_FILTRO] = {0, 0, 0};

// Variáveis essenciais para o Bluetooth
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
  
  // Inicializa os vetores do controlador FOPID
  Controle_Init(); 
  
  EnviarMensagem("--- Sistema Hibrido Estavel Pronto (Controle FOPID) ---");
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

  // 4. Execução da Malha de Controle
  Controle_Update();

  // 5. Telemetria para a Interface (250ms)
  if (millis() - lastTelemetryTime >= 250) {
    lastTelemetryTime = millis();
    BT_SendTelemetry();
  }

  Ensaio_Update();
  BT_Listen(); 
}