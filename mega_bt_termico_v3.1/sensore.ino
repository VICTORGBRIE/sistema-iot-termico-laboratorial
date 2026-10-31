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
    
    // --- CALIBRAÇÃO EXATA PARA O SEU CIRCUITO EM SÉRIE ---
    // Fator ajustado de 185.0 para 172.0 baseado na sua leitura física
    float correnteBruta = (tensaoRMS * 1000.0) / 172.0; 
    
    // --- NOVO LIMIAR DE RUÍDO ---
    // Como a corrente nominal é baixa, o corte desceu para 30mA (0.030A)
    if (correnteBruta < 0.030) {
      lastCorrente = 0.0;
    } else {
      // O seu vetor de DSP para linearização está mantido!
      float fatores[11] = {0.000, 0.254, 0.368, 0.487, 0.554, 0.635, 0.692, 0.743, 0.792, 0.809, 0.873};
      int idx = powerAtual / 10;
      int idxSup = (idx < 10) ? idx + 1 : 10;
      float resto = (powerAtual % 10) / 10.0; 
      float fatorCalibrado = fatores[idx] + (fatores[idxSup] - fatores[idx]) * resto;
      
      float correnteNova = correnteBruta * fatorCalibrado;
      
      // Filtro Exponencial (EMA) para estabilizar os números na interface
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