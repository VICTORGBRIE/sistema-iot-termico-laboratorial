// =================================================================
// MÓDULO 6: CONTROLADOR DIGITAL RST + LÓGICA FUZZY TAKAGI-SUGENO
// =================================================================

extern volatile float setpointTemp; 
extern bool ensaioRunning;

// =================================================================
// BASE DE REGRAS FUZZY (Ganhos Locais Extraídos do MATLAB)
// =================================================================
// Planta 1 (Otimizado para região de 35 C | Degrau 0 a 10%)
volatile float r0_1 = 33.2423, r1_1 = -25.9565;
volatile float Tg_1 = 7.2859;

// Planta 2 (Otimizado para região de 50 C | Degrau 10 a 20%)
volatile float r0_2 = 27.8994, r1_2 = -25.0360;
volatile float Tg_2 = 2.8633;

// Planta 3 (Otimizado para região de 60 C | Degrau 20 a 30%)
volatile float r0_3 = 32.3948, r1_3 = -29.6224; 
volatile float Tg_3 = 2.7724;                

// Graus de Pertinência Fuzzy
volatile float p1 = 0.0;
volatile float p2 = 0.0;
volatile float p3 = 0.0;

// Memória de Estados da Equação de Diferenças
volatile float y[2] = {0.0, 0.0}; 
volatile float u[2] = {0.0, 0.0}; 

unsigned long lastControlTime = 0;
const unsigned long Ts = 2000; // Amostragem de 2s

void Controle_Init() {
  float tempInicial = Sensor_GetTemp();
  if (tempInicial <= 0 || tempInicial > 120) tempInicial = 25.0; 
  
  for(int i = 0; i < 2; i++) {
    y[i] = tempInicial;
    u[i] = 0.0;
  }
  analogWrite(PIN_FAN, 0); 
}

void Controle_Update() {
  if (!ensaioRunning) {
    lastControlTime = millis();
    analogWrite(PIN_FAN, 0);
    return;
  }

  if (millis() - lastControlTime >= Ts) {
    lastControlTime = millis();

    float tempAtual = Sensor_GetTemp();
    if (tempAtual <= 0 || tempAtual > 120) return;

    y[0] = tempAtual;

    // ====================================================================
    // ETAPA 1: FUZZIFICAÇÃO (Calcula o grau de pertinência T-S)
    // ====================================================================
    if (tempAtual <= 35.0) {
      p1 = 1.0; p2 = 0.0; p3 = 0.0;
    } 
    else if (tempAtual > 35.0 && tempAtual < 50.0) {
      p1 = (50.0 - tempAtual) / 15.0; 
      p2 = 1.0 - p1;
      p3 = 0.0;
    } 
    else if (tempAtual == 50.0) {
      p1 = 0.0; p2 = 1.0; p3 = 0.0;
    } 
    else if (tempAtual > 50.0 && tempAtual < 60.0) {
      p2 = (60.0 - tempAtual) / 10.0; 
      p3 = 1.0 - p2;
      p1 = 0.0;
    } 
    else if (tempAtual >= 60.0) {
      p1 = 0.0; p2 = 0.0; p3 = 1.0;
    }

    // ====================================================================
    // ETAPA 2: INFERÊNCIA TAKAGI-SUGENO (Lâmpada)
    // ====================================================================
    float r0_fuzzy = (p1 * r0_1) + (p2 * r0_2) + (p3 * r0_3);
    float r1_fuzzy = (p1 * r1_1) + (p2 * r1_2) + (p3 * r1_3);
    float Tg_fuzzy = (p1 * Tg_1) + (p2 * Tg_2) + (p3 * Tg_3);

    // ====================================================================
    // ETAPA 3: INFERÊNCIA TAKAGI-SUGENO (Ventoinha Dinâmica)
    // ====================================================================
    // Aqui usamos os mesmos pesos do Fuzzy para ajustar a ventoinha!
    // P1 (Frio): Começa em 100 PWM, atinge força total com 3.0°C de erro.
    // P3 (Quente): Começa em 60 PWM (mais suave), atinge força total só com 5.0°C de erro.
    float fan_min_pwm = (p1 * 100.0) + (p2 * 80.0) + (p3 * 60.0);
    float fan_max_err = (p1 * 30.0)  + (p2 * 40.0) + (p3 * 50.0); 
    
    // ====================================================================
    // ETAPA 4: APLICAÇÃO DA LEI DE CONTROLE 
    // ====================================================================
    u[0] = u[1] - (r0_fuzzy * y[0]) - (r1_fuzzy * y[1]) + (Tg_fuzzy * setpointTemp);

    // Anti-Windup
    if (u[0] > 100.0) u[0] = 100.0;
    if (u[0] < 0.0)   u[0] = 0.0;

    // Freio Térmico com Ventoinha Fuzzy
    float erro = setpointTemp - y[0];
    if (erro <= -0.6) {
      int erro_int = (int)(abs(erro) * 10);
      int max_err_int = (int)fan_max_err;
      
      int fanPWM = map(erro_int, 6, max_err_int, (int)fan_min_pwm, 255);
      analogWrite(PIN_FAN, constrain(fanPWM, (int)fan_min_pwm, 255));
    } else {
      analogWrite(PIN_FAN, 0); 
    }

    // Atuação segura no Dimmer
    Dimmer_SetPower((int)u[0]);

    // Deslocamento de memória
    u[1] = u[0];
    y[1] = y[0];
  }
}