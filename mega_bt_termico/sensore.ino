// =================================================================
// MÓDULO 2: MÁQUINA DE ESTADOS DOS SENSORES
// =================================================================

long somaQuadrados = 0;
long numAmostras = 0;
unsigned long lastCalcZC = 0;
float lastCorrente = 0.0;

unsigned long lastTempZC = 0;
float lastValidTemp = 0.0;

void Sensor_Init() {
  sensors.begin();
  sensors.setWaitForConversion(false); 
  sensors.requestTemperatures();       
  lastTempZC = zcCounter;
}

void Sensor_Update() {
  // 1. AMOSTRAGEM DE CORRENTE INSTANTÂNEA E LIVRE
  if (powerAtual > 0) {
    long valorCentralizado = analogRead(PIN_CURRENT) - 512; 
    somaQuadrados += (valorCentralizado * valorCentralizado);
    numAmostras++;
  }

  // A CADA 3 CICLOS DE REDE (~25ms), CALCULA O RESULTADO FINAL
  if (zcCounter - lastCalcZC >= 3) {
    if (numAmostras > 0 && powerAtual > 0) {
      float mediaQuadrados = (float)somaQuadrados / numAmostras;
      float tensaoRMS = (sqrt(mediaQuadrados) * 5.0) / 1023.0;
      float corrente = ((tensaoRMS * 1000.0) / 185.0) - 0.040; // Com calibração
      lastCorrente = (corrente < 0) ? 0.0 : corrente;
    } else {
      lastCorrente = 0.0;
    }
    somaQuadrados = 0;
    numAmostras = 0;
    lastCalcZC = zcCounter;
  }

  // 2. ATUALIZA A TEMPERATURA A CADA 90 CICLOS DE REDE (Exatos 750ms)
  // Respeita o tempo exigido pelo chip DS18B20 sem travar nada
  if (zcCounter - lastTempZC >= 90) {
    lastValidTemp = sensors.getTempCByIndex(0);
    sensors.requestTemperatures();
    lastTempZC = zcCounter;
  }
}

float Sensor_GetTemp() { return lastValidTemp; }
float Sensor_GetCorrente() { return lastCorrente; }