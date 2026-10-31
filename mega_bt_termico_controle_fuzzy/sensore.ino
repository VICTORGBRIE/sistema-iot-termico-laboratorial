/*
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
*/
// =================================================================
// MÓDULO 2: SENSORES COM DSP E FILTRO PASSA-BAIXA
// =================================================================

long somaQuadrados = 0;
long numAmostras = 0;
float lastCorrente = 0.0;
float lastValidTemp = 0.0;

void Sensor_Init() {
  sensors.begin();
  sensors.setWaitForConversion(false); 
  sensors.requestTemperatures();       
}

void Sensor_AmostraContinua() {
  if (powerAtual > 0) {
      long valorCentralizado = analogRead(PIN_CURRENT) - 512; 
      somaQuadrados += (valorCentralizado * valorCentralizado);
      numAmostras++;
  }
}

void Sensor_CalcularRMS() {
  if (numAmostras > 0 && powerAtual > 0) {
    float mediaQuadrados = (float)somaQuadrados / numAmostras;
    float tensaoRMS = (sqrt(mediaQuadrados) * 5.0) / 1023.0;
    float correnteBruta = (tensaoRMS * 1000.0) / 185.0; 
    
    // 4. Filtro de Ruído (Deadzone) ajustado para lâmpadas de 84W
    if (correnteBruta < 0.05) {
      lastCorrente = 0.0;
    } else {
      float fatores[11] = {0.000, 0.241, 0.357, 0.454, 0.535, 0.616, 0.681, 0.743, 0.850, 0.814, 0.990};
      int idx = powerAtual / 10;
      int idxSup = (idx < 10) ? idx + 1 : 10;
      float resto = (powerAtual % 10) / 10.0; 
      float fatorCalibrado = fatores[idx] + (fatores[idxSup] - fatores[idx]) * resto;
      
      float correnteNova = correnteBruta * fatorCalibrado;
      
      if (lastCorrente == 0.0) lastCorrente = correnteNova; 
      else lastCorrente = (lastCorrente * 0.70) + (correnteNova * 0.30);
    }
  } else {
    lastCorrente = 0.0;
  }
  
  somaQuadrados = 0;
  numAmostras = 0;
}

void Sensor_AmostraTemperatura() {
    lastValidTemp = sensors.getTempCByIndex(0);
    sensors.requestTemperatures();
}

float Sensor_GetTemp() { return lastValidTemp; }
float Sensor_GetCorrente() { return lastCorrente; }