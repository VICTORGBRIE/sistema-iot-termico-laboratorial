// =================================================================
// MÓDULO 3: GESTOR DE ENSAIO (BLINDADO AO GRID CLOCK)
// =================================================================

void Ensaio_Update() {
  if (ensaioRunning) {
    // Verifica os ciclos de rede (zcCounter) em vez do millis() corrompido
    if (zcCounter - ensaioStartZC >= ensaioDurationZC) {
      ensaioRunning = false;
      Dimmer_SetPower(0); 
      Serial1.println("INFO: Ensaio Concluido (Tempo Esgotado).");
    } else {
      Dimmer_SetPower(ensaioTargetPower);
    }
  } else {
    Dimmer_SetPower(0); 
  }
}