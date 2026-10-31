// =================================================================
// MÓDULO 6: CONTROLADOR HÍBRIDO FUZZY-FOPID (TAKAGI-SUGENO)
// =================================================================
#include <math.h> // Necessário para a função exp() da curva Sigmoidal

// -----------------------------------------------------------------
// COPIE AQUI OS VETORES GERADOS PELO MATLAB (RST Reduzido de Hankel)
// -----------------------------------------------------------------
const float LIMITE_ZONA = 60.0; // Fronteira entre as zonas térmicas

// Zona 1 (Frio - Temperatura < 60°C)
const float R1[3] = {232.0723, -411.1998, 179.5633}; // Substitua pelos seus valores reais
const float S1[3] = {1.0000, -1.9846, 0.9846};   // S1[0] é sempre 1.0000

// Zona 2 (Quente - Temperatura > 60°C)
const float R2[3] = {9518.8480, -15314.3453, 5828.5343}; // Substitua pelos seus valores reais
const float S2[3] = {1.0000, -1.9867, 0.9867};    // S2[0] é sempre 1.0000
// -----------------------------------------------------------------

// Memória de Estados (Equação de Diferenças de 2ª Ordem)
volatile float e_hist[3] = {0.0, 0.0, 0.0}; // Histórico do Erro
volatile float u_hist[3] = {0.0, 0.0, 0.0}; // Histórico do Esforço de Controle

unsigned long lastControlTime = 0;
const unsigned long Ts = 2000; // Tempo de Amostragem (2 segundos)

void Controle_Init() {
  for(int i = 0; i < 3; i++) {
    e_hist[i] = 0.0;
    u_hist[i] = 0.0;
  }
  analogWrite(PIN_FAN, 0); // Garante ventoinha desligada
  lastControlTime = millis();
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

    // 1. Atualiza o Erro Atual
    e_hist[0] = setpointTemp - tempAtual;

    // ====================================================================
    // 2. CÁLCULO DOS CONTROLADORES FOPID (EQUAÇÃO DE DIFERENÇAS)
    // O polinômio R atua sobre o Erro. O polinômio S atua sobre a Saída.
    // ====================================================================
    // Resposta do Filtro 1 (Zona Fria)
    float u1 = - (S1[1] * u_hist[1] + S1[2] * u_hist[2]) 
               + (R1[0] * e_hist[0] + R1[1] * e_hist[1] + R1[2] * e_hist[2]);

    // Resposta do Filtro 2 (Zona Quente)
    float u2 = - (S2[1] * u_hist[1] + S2[2] * u_hist[2]) 
               + (R2[0] * e_hist[0] + R2[1] * e_hist[1] + R2[2] * e_hist[2]);

    // ====================================================================
    // 3. SUPERVISOR FUZZY TAKAGI-SUGENO (CLUSTERIZAÇÃO SIGMOIDAL)
    // ====================================================================
    // Calcula o peso de cada zona usando a curva sigmoidal igual ao MATLAB
    // a = 0.2 (taxa de transição) | c = LIMITE_ZONA (60.0)
    float mu_frio = 1.0 / (1.0 + exp(0.2 * (tempAtual - LIMITE_ZONA)));
    float mu_quente = 1.0 / (1.0 + exp(-0.2 * (tempAtual - LIMITE_ZONA)));

    // Fusão ponderada das duas respostas (O Agendamento de Ganhos)
    float u_final = (u1 * mu_frio + u2 * mu_quente) / (mu_frio + mu_quente);

    // ====================================================================
    // 4. SATURAÇÃO, ANTI-WINDUP E FREIO TÉRMICO
    // ====================================================================
    if (u_final > 100.0) u_final = 100.0;
    if (u_final < 0.0)   u_final = 0.0;

    // Freio Térmico Ativo (Mantido da versão anterior para garantir travagem)
    if (e_hist[0] <= -0.6) {
      float erro_absoluto = abs(e_hist[0]);
      int fanPWM = map((int)(erro_absoluto * 10), 6, 30, 100, 255);
      fanPWM = constrain(fanPWM, 100, 255);
      analogWrite(PIN_FAN, fanPWM);
    } else {
      analogWrite(PIN_FAN, 0); 
    }

    // 5. Aplica a potência final ao TRIAC
    Dimmer_SetPower((int)u_final);

    // 6. Deslocamento do Histórico para o próximo ciclo (Evita Windup)
    // Usamos o u_final (saturado) no histórico para evitar falha no Integrador!
    e_hist[2] = e_hist[1];
    e_hist[1] = e_hist[0];
    
    u_hist[2] = u_hist[1];
    u_hist[1] = u_final; 
  }
}