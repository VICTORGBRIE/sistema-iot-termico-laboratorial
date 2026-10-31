// =================================================================
// MÓDULO 5: COMUNICAÇÃO BLUETOOTH E TELEMETRIA FLUÍDA
// =================================================================

unsigned long lastTelemetryZC = 0;

void BT_Init() { Serial1.begin(115200); }

void BT_Listen() {
  static byte indice = 0;
  boolean novaMensagem = false;

  while (Serial1.available() > 0 && !novaMensagem) {
    char c = Serial1.read();
    if (c != '\n') {
      if (c != '\r') { 
        bufferRececao[indice++] = c;
        if (indice >= MASCARA_BUFFER) indice = MASCARA_BUFFER - 1; 
      }
    } else {
      bufferRececao[indice] = '\0'; 
      indice = 0;
      novaMensagem = true; 
    }
  }

  if (novaMensagem) {
    String cmd = String(bufferRececao);
    cmd.trim();
    CMD_Parse(cmd);
  }
}

void BT_SendTelemetry() {
  if (!ensaioRunning && !monitorMode) return;

  // Envia pacotes a cada 30 pulsos da rede (Exatos 4 quadros por segundo)
  if (zcCounter - lastTelemetryZC >= 30) {
    lastTelemetryZC = zcCounter;

    float temp = Sensor_GetTemp();
    float curr = Sensor_GetCorrente();

    if (temp != DEVICE_DISCONNECTED_C) {
      String pacote = "DADOS: T=" + String(temp, 2) + " P=" + String(powerAtual) + " I=" + String(curr, 3);
      Serial1.println(pacote);
    } 
    else {
      Serial1.println("ERRO: Sensor de Temperatura falhou ou desconectou!");
      String pacote = "DADOS: T=0.01 P=" + String(powerAtual) + " I=" + String(curr, 3);
      Serial1.println(pacote);
    }
  }
}