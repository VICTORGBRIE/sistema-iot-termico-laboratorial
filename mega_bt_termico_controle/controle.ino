// =================================================================
// MÓDULO 6: CONTROLADOR DIGITAL RST (ALOCAÇÃO DE POLOS)
// =================================================================

// Coeficientes do Controlador (Vetorizados)
volatile float r[4] = {20.3667, -19.2819, 0.0000, 0.0000}; 
volatile float s[3] = {-1.0000, 0.0000, 0.0000};      
volatile float T_gain = 1.0848;                

// Memória de Estados (Equação de Diferenças)
volatile float y[4] = {0.0, 0.0, 0.0, 0.0}; // Histórico de Saída (Temperatura)
volatile float u[4] = {0.0, 0.0, 0.0, 0.0}; // Histórico de Controle (Potência)

//volatile float setpointTemp = 30.0;         // Referência inicial
unsigned long lastControlTime = 0;
const unsigned long Ts = 2000;              // Tempo de Amostragem (2 segundos)

void Controle_Init() {
  for(int i = 0; i < 4; i++) {
    y[i] = 0.0;
    u[i] = 0.0;
  }
}

void Controle_Update() {
  // Executa apenas se o ensaio estiver rodando em malha fechada
  if (!ensaioRunning) {
    lastControlTime = millis();
    return;
  }

  if (millis() - lastControlTime >= Ts) {
    lastControlTime = millis();

    float tempAtual = Sensor_GetTemp();
    
    // Proteção contra leitura inválida
    if (tempAtual <= 0 || tempAtual > 100) return;

    y[0] = tempAtual;

    // Equação de Diferenças RST: u(k) = -[S*u] - [R*y] + [T*Ref]
    u[0] = -(s[0]*u[1] + s[1]*u[2] + s[2]*u[3]) 
           -(r[0]*y[0] + r[1]*y[1] + r[2]*y[2] + r[3]*y[3]) 
           + (T_gain * setpointTemp);

    // Saturação do Atuador (Dimmer 0-100%)
    if (u[0] > 100.0) u[0] = 100.0;
    if (u[0] < 0.0)   u[0] = 0.0;

    // Aplica o valor calculado ao sistema
    powerAtual = (int)u[0];

    // Shift dos históricos
    for(int i = 3; i > 0; i--) {
      u[i] = u[i-1];
      y[i] = y[i-1];
    }
  }
}