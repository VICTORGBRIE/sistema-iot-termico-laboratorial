// =================================================================
// MÓDULO 4: LÓGICA DE ENSAIOS E DEGRAUS
// =================================================================

unsigned long staircaseInterval = 0;
unsigned long staircaseLastMillis = 0;
int staircaseCurrentPower = 0;
bool staircaseRising = true;

void Ensaio_Update() {
  // 1. Lógica do Loop em Degraus (Malha Aberta)
  if (staircaseRunning) {
    if (millis() - staircaseLastMillis >= staircaseInterval) {
      staircaseLastMillis = millis();

      if (staircaseRising) {
        if (staircaseCurrentPower < 100) {
          staircaseCurrentPower += 10;
        } else {
          staircaseRising = false;
          staircaseCurrentPower -= 10;
        }
      } else {
        staircaseCurrentPower -= 10;
        if (staircaseCurrentPower < 0) {
          staircaseRunning = false;
          staircaseCurrentPower = 0;
          Dimmer_SetPower(0);
          EnviarMensagem("INFO: Ensaio em Degraus Concluido");
          return;
        }
      }
      // O uso do Dimmer_SetPower aqui protege o Arduino do crash do USB
      Dimmer_SetPower(staircaseCurrentPower);
      EnviarMensagem("INFO: Potencia Degrau: " + String(staircaseCurrentPower) + "%");
    }
    return; 
  }

  // 2. Lógica da Malha Fechada (RST)
  if (ensaioRunning) {
    // Como você pediu, roda por tempo indeterminado! 
    // O controle matemático contínuo é feito na aba 'controle.ino'
  }
}

void Ensaio_Start(int durationSec, float targetTemp) {
  setpointTemp = targetTemp; 
  ensaioStartMillis = millis();
  
  staircaseRunning = false; // Garante que a escada pare se estiver a rodar
  
  Controle_Init(); 
  ensaioRunning = true;
  
  EnviarMensagem("INFO: Malha Fechada Iniciada (Alvo: " + String(targetTemp) + " C - Tempo Indeterminado)");
}

void Ensaio_Stop() {
  ensaioRunning = false;
  staircaseRunning = false;
  Dimmer_SetPower(0); // Força a lâmpada a desligar em segurança
  Controle_Init();
  EnviarMensagem("INFO: Controle Interrompido (Voltando para Manual)");
}

void Staircase_Start(int intervalSec) {
  staircaseInterval = (unsigned long)intervalSec * 1000;
  staircaseCurrentPower = 0;
  staircaseRising = true;
  staircaseLastMillis = millis();
  
  ensaioRunning = false; // Desliga o controlador RST se estiver ativo
  staircaseRunning = true;
  
  Dimmer_SetPower(0);
  EnviarMensagem("INFO: Ensaio em Degraus Iniciado (Intervalo: " + String(intervalSec) + "s)");
}