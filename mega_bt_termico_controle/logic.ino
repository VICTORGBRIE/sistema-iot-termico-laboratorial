// =================================================================
// MÓDULO 4: LÓGICA DE ENSAIOS E SEGURANÇA
// =================================================================

void Ensaio_Update() {
  // A verificação de tempo (millis() - ensaioStartMillis >= ensaioDurationMillis)
  // foi completamente REMOVIDA.
  // Agora, uma vez iniciada, a malha fechada roda por tempo indeterminado.
  if (!ensaioRunning) return;
}

void Ensaio_Start(int durationSec, float targetTemp) {
  // Recebemos o "durationSec" da interface para não quebrar a comunicação,
  // mas ignoramos o seu valor para que o ciclo seja infinito.
  
  setpointTemp = targetTemp; // Atribui a temperatura alvo ao controlador
  ensaioStartMillis = millis();
  
  // Prepara o controlador para um novo ciclo de controle (Zera o histórico)
  Controle_Init(); 
  ensaioRunning = true;
  
  EnviarMensagem("INFO: Malha Fechada Iniciada (Alvo: " + String(targetTemp) + " C - Tempo Indeterminado)");
}

void Ensaio_Stop() {
  ensaioRunning = false;
  powerAtual = 0;
  Controle_Init();
  EnviarMensagem("INFO: Ensaio Interrompido (Voltando para Malha Aberta)");
}