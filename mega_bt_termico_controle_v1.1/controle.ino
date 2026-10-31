// =================================================================
// MÓDULO 6: CONTROLADOR DIGITAL FOPID REDUZIDO (RST / IIR 2ª ORDEM)
// =================================================================

unsigned long lastControlTime = 0;
const unsigned long Ts_ms = 2000; // Tempo de Amostragem (Sincronizado com o MATLAB)

void Controle_Init() {
  // Zera o histórico para evitar transientes no arranque
  for(int i = 0; i < TAM_FILTRO; i++) {
    e_hist[i] = 0.0;
    u_hist[i] = 0.0;
  }
  lastControlTime = millis();
}

void Controle_Update() {
  if (!ensaioRunning) {
    lastControlTime = millis();
    return;
  }

  if (millis() - lastControlTime >= Ts_ms) {
    lastControlTime = millis();

    float tempAtual = Sensor_GetTemp();
    if (tempAtual <= 0 || tempAtual > 120) return; 

    // 1. Deslocamento do histórico (Shift Register)
    e_hist[2] = e_hist[1];
    e_hist[1] = e_hist[0];
    
    u_hist[2] = u_hist[1];
    u_hist[1] = u_hist[0];

    // 2. Cálculo do Erro Atual
    e_hist[0] = setpointTemp - tempAtual;

    // 3. Equação de Diferenças RST de 2ª Ordem:
    // u[k] = r0*e[k] + r1*e[k-1] + r2*e[k-2] - s1*u[k-1] - s2*u[k-2]
    float output = (r[0] * e_hist[0]) + (r[1] * e_hist[1]) + (r[2] * e_hist[2])
                 - (s[0] * u_hist[1]) - (s[1] * u_hist[2]);

    // 4. Saturação e Aplicação
    if (output > 100.0) output = 100.0;
    if (output < 0.0)   output = 0.0;

    u_hist[0] = output;
    powerAtual = (int)output;
    Dimmer_SetPower(powerAtual);
  }
}