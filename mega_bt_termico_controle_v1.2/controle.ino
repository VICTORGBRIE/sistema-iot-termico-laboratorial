// =================================================================
// MÓDULO 6: CONTROLADOR DIGITAL FOPID REDUZIDO (RST / IIR 2ª ORDEM)
// =================================================================

unsigned long lastControlTime = 0;
const unsigned long Ts_ms = 2000; // Tempo de Amostragem 

void Controle_Init() {
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

    // 1. SUPERVISOR FUZZY TAKAGI-SUGENO (MÉDIA PONDERADA)
    float curr = Sensor_GetCorrente();
    float p_real = 165.8 * (curr * curr); 
    
    float w1 = 0.0, w2 = 0.0, w3 = 0.0;
    float P1_W = 9.16, P2_W = 19.73, P3_W = 35.09;

    if (p_real <= P1_W) {
      w1 = 1.0; 
    } else if (p_real > P1_W && p_real <= P2_W) {
      w1 = (P2_W - p_real) / (P2_W - P1_W); 
      w2 = (p_real - P1_W) / (P2_W - P1_W); 
    } else if (p_real > P2_W && p_real <= P3_W) {
      w2 = (P3_W - p_real) / (P3_W - P2_W); 
      w3 = (p_real - P2_W) / (P3_W - P2_W); 
    } else {
      w3 = 1.0; 
    }

    // 2. INTERPOLAÇÃO DOS COEFICIENTES FOPID
    float r_eq[3], s_eq[2];
    for(int i = 0; i < 3; i++) {
        r_eq[i] = (w1 * r1[i]) + (w2 * r2[i]) + (w3 * r3[i]);
    }
    for(int i = 0; i < 2; i++) {
        s_eq[i] = (w1 * s1[i]) + (w2 * s2[i]) + (w3 * s3[i]);
    }

    // 3. EQUAÇÃO DE DIFERENÇAS RST
    e_hist[2] = e_hist[1]; e_hist[1] = e_hist[0];
    u_hist[2] = u_hist[1]; u_hist[1] = u_hist[0];
    e_hist[0] = setpointTemp - tempAtual;

    float output = (r_eq[0] * e_hist[0]) + (r_eq[1] * e_hist[1]) + (r_eq[2] * e_hist[2])
                 - (s_eq[0] * u_hist[1]) - (s_eq[1] * u_hist[2]);

    // 4. PROTEÇÃO MATEMÁTICA E SOFT-START (Evita Desconexão USB)
    // Se a matemática gerar um número inválido, zera o sistema
    if (isnan(output) || isinf(output)) {
      output = 0.0; 
    }

    // Limites absolutos de segurança
    if (output > 100.0) output = 100.0;
    if (output < 0.0)   output = 0.0;

    // Soft-Start: Pré-aquecimento do filamento (Impede picos EMI)
    float variacao = output - u_hist[1];
    if (variacao > 4.0) {
      output = u_hist[1] + 4.0; 
    }

    u_hist[0] = output;
    powerAtual = (int)output;
    Dimmer_SetPower(powerAtual);
  }
}