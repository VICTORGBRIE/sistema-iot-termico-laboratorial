/*
// =================================================================
// MÓDULO 6: CONTROLADOR DIGITAL RST (ALOCAÇÃO DE POLOS)
// =================================================================

// Pode manter esses valores provisoriamente até ao novo ensaio
volatile float r[4] = {41.5817, -37.5204, 0.0000, 0.0000}; 
volatile float s[3] = {-1.0000, 0.0000, 0.0000};      
volatile float T_gain = 4.0613;                

// Memória de Estados (Equação de Diferenças)
volatile float y[4] = {0.0, 0.0, 0.0, 0.0}; 
volatile float u[4] = {0.0, 0.0, 0.0, 0.0}; 

unsigned long lastControlTime = 0;
const unsigned long Ts = 1084;              // Tempo de Amostragem (2 segundos)

void Controle_Init() {
  // Inicializa o passado com a temperatura atual para evitar um "chute" (Derivative Kick)
  float tempInicial = Sensor_GetTemp();
  if (tempInicial <= 0 || tempInicial > 120) tempInicial = 25.0; // Proteção
  
  for(int i = 0; i < 4; i++) {
    y[i] = tempInicial;
    u[i] = 0.0;
  }
}

void Controle_Update() {
  if (!ensaioRunning) {
    lastControlTime = millis();
    return;
  }

  if (millis() - lastControlTime >= Ts) {
    lastControlTime = millis();

    float tempAtual = Sensor_GetTemp();
    if (tempAtual <= 0 || tempAtual > 120) return;

    y[0] = tempAtual;

    u[0] = -(s[0]*u[1] + s[1]*u[2] + s[2]*u[3]) 
           -(r[0]*y[0] + r[1]*y[1] + r[2]*y[2] + r[3]*y[3]) 
           + (T_gain * setpointTemp);

    if (u[0] > 100.0) u[0] = 100.0;
    if (u[0] < 0.0)   u[0] = 0.0;

    // ====================================================================
    // A MÁGICA ESTÁ AQUI: Sincroniza a matemática com o Hardware do TRIAC
    // Sem isto, o dimmer dá curtos-circuitos lógicos e derruba o USB.
    // ====================================================================
    Dimmer_SetPower((int)u[0]);

    for(int i = 3; i > 0; i--) {
      u[i] = u[i-1];
      y[i] = y[i-1];
    }
  }
}
*/
// =================================================================
// MÓDULO 6: CONTROLADOR DIGITAL RST (ALOCAÇÃO DE POLOS + VENTOINHA)
// =================================================================

// Pode manter esses valores provisoriamente até ao novo ensaio
volatile float r[4] = {68.6307, -53.4824, 0.0000, 0.0000}; 
volatile float s[3] = {-1.0000, 0.0000, 0.0000};      
volatile float T_gain = 15.1483;                

// Memória de Estados (Equação de Diferenças)
volatile float y[4] = {0.0, 0.0, 0.0, 0.0}; 
volatile float u[4] = {0.0, 0.0, 0.0, 0.0}; 

unsigned long lastControlTime = 0;
const unsigned long Ts = 2000; // Tempo de Amostragem (2000ms para bater com o MATLAB)

void Controle_Init() {
  // Inicializa o passado com a temperatura atual para evitar um "chute" (Derivative Kick)
  float tempInicial = Sensor_GetTemp();
  if (tempInicial <= 0 || tempInicial > 120) tempInicial = 25.0; // Proteção
  
  for(int i = 0; i < 4; i++) {
    y[i] = tempInicial;
    u[i] = 0.0;
  }
  analogWrite(PIN_FAN, 0); // Garante que a ventoinha inicia desligada
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

    // Equação de Diferenças RST Clássica
    u[0] = -(s[0]*u[1] + s[1]*u[2] + s[2]*u[3]) 
           -(r[0]*y[0] + r[1]*y[1] + r[2]*y[2] + r[3]*y[3]) 
           + (T_gain * setpointTemp);

    // Anti-Windup e Saturação Lógica
    if (u[0] > 100.0) u[0] = 100.0;
    if (u[0] < 0.0)   u[0] = 0.0;

    // ====================================================================
    // FREIO TÉRMICO COM ANTECIPAÇÃO (MARGEM DE POUSO)
    // ====================================================================
    float erro = setpointTemp - y[0];
    
    // Só liga a ventoinha se estiver pelo menos 0.6°C ACIMA do alvo
    // Isso garante que ela desliga antes de tocar no Setpoint!
    if (erro <= -0.6) {
      float erro_absoluto = abs(erro);
      
      // Mapeia o erro de 0.6°C a 3.0°C para o PWM (100 a 255)
      // O valor 100 é um giro bem suave, só para circular o ar
      int fanPWM = map((int)(erro_absoluto * 10), 6, 30, 100, 255);
      
      fanPWM = constrain(fanPWM, 100, 255);
      analogWrite(PIN_FAN, fanPWM);
    } else {
      // Se a temperatura estiver a 0.5°C ou menos do alvo, desliga totalmente.
      // O sistema agora vai "deslizar" pela inércia até estabilizar no alvo.
      analogWrite(PIN_FAN, 0); 
    }
    // ====================================================================

    // Sincroniza a matemática com o Hardware do TRIAC
    Dimmer_SetPower((int)u[0]);

    // Desloca o histórico para preparar o próximo ciclo
    for(int i = 3; i > 0; i--) {
      u[i] = u[i-1];
      y[i] = y[i-1];
    }
  }
}