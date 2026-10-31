// =================================================================
// MÓDULO 3: GESTOR DE ENSAIO E DEGRAUS
// =================================================================

extern bool isClosedLoop;

unsigned long staircaseUserInterval = 0; 
unsigned long staircaseLastMillis = 0;
int staircaseCurrentPower = 0;
bool staircaseRising = true;

void Ensaio_Update() {
  // 1. Lógica do Loop em Degraus
  if (staircaseRunning) {
    unsigned long currentTargetInterval = (staircaseCurrentPower == 0) ? 20000UL : staircaseUserInterval;

    if (millis() - staircaseLastMillis >= currentTargetInterval) {
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
      Dimmer_SetPower(staircaseCurrentPower);
      EnviarMensagem("INFO: Potencia Degrau: " + String(staircaseCurrentPower) + "%");
    }
    return; 
  }

  // 2. Lógica do Ensaio Normal (Acomoda Malha Aberta e Fechada)
  if (ensaioRunning) {
    if (millis() - ensaioStartMillis >= ensaioDurationMillis) {
      ensaioRunning = false;
      isClosedLoop = false;
      Dimmer_SetPower(0); 
      EnviarMensagem("INFO: Ensaio Concluido (Tempo Esgotado).");
    } else {
      // O DIMMER SÓ É FORÇADO AQUI SE FOR MALHA ABERTA (MANUAL)
      if (!isClosedLoop) {
        Dimmer_SetPower(ensaioTargetPower);
      }
    }
  }
}

void Staircase_Start(int intervalSec) {
  staircaseUserInterval = (unsigned long)intervalSec * 1000UL;
  staircaseCurrentPower = 0;
  staircaseRising = true;
  staircaseLastMillis = millis();
  staircaseRunning = true;
  ensaioRunning = false; 
  isClosedLoop = false;
  Dimmer_SetPower(0);
  EnviarMensagem("INFO: Ensaio em Degraus Iniciado (Intervalo: " + String(intervalSec) + "s / Base 0%: 20s)");
}