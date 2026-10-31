// =================================================================
// MÓDULO 6: CONTROLADOR DIGITAL PI (Ação Direta no Erro - ZOH)
// =================================================================

extern volatile float setpointTemp; // Pega o Setpoint configurado pelo Python

// Constantes r0 e r1 calculadas pelo MATLAB (ensaio_def1002.txt)
volatile float r[2] = {20.3445, -19.2608}; 

// Memória de Estados (Equação de Diferenças)
volatile float u[2]    = {0.0, 0.0}; // u[0] é o atual, u[1] é o anterior
volatile float erro[2] = {0.0, 0.0}; // erro[0] é o atual, erro[1] é o anterior

unsigned long lastControlTime = 0;
const unsigned long Ts = 2000; // Tempo de Amostragem de 2 segundos

void Controle_Init() {
  for(int i = 0; i < 2; i++) {
    u[i] = 0.0;
    erro[i] = 0.0;
  }
  analogWrite(PIN_FAN, 0); // Garante que a ventoinha inicia desligada
}

void Controle_Update() {
  // Se a malha fechada não estiver a rodar, sai da função
  if (!ensaioRunning) {
    lastControlTime = millis();
    analogWrite(PIN_FAN, 0);
    return;
  }

  if (millis() - lastControlTime >= Ts) {
    lastControlTime = millis();

    float tempAtual = Sensor_GetTemp();
    if (tempAtual <= 0 || tempAtual > 120) return;

    // 1. Calcula o erro atual
    erro[0] = setpointTemp - tempAtual;

    // 2. Lei de Controle Digital (Equação de Diferenças do PI)
    // u(k) = u(k-1) + r0*e(k) + r1*e(k-1)
    u[0] = u[1] + (r[0] * erro[0]) + (r[1] * erro[1]);

    // 3. Anti-Windup e Saturação Lógica (Limita o esforço de 0 a 100%)
    if (u[0] > 100.0) u[0] = 100.0;
    if (u[0] < 0.0)   u[0] = 0.0;

    // ====================================================================
    // FREIO TÉRMICO (VENTOINHA)
    // Se a temperatura passar 0.6°C do alvo, a ventoinha liga para resfriar
    // ====================================================================
    if (erro[0] <= -0.6) {
      int fanPWM = map((int)(abs(erro[0]) * 10), 6, 30, 100, 255);
      fanPWM = constrain(fanPWM, 100, 255);
      analogWrite(PIN_FAN, fanPWM);
    } else {
      analogWrite(PIN_FAN, 0); 
    }

    // 4. Sincroniza a matemática com o Hardware do TRIAC (Dimmer)
    Dimmer_SetPower((int)u[0]);

    // 5. Desloca o histórico para preparar o próximo ciclo
    u[1] = u[0];
    erro[1] = erro[0];
  }
}