// =================================================================
// MÓDULO 5: COMUNICAÇÃO OMNICANAL (USB + HC-06)
// =================================================================

void BT_Init() { 
  Serial1.begin(115200); 
}

void EnviarMensagem(String msg) {
  Serial.println(msg);   
  Serial1.println(msg);  
}

void BT_Listen() {
  static byte idxUSB = 0;
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') { 
      bufferUSB[idxUSB++] = c;
      if (idxUSB >= MASCARA_BUFFER) idxUSB = MASCARA_BUFFER - 1; 
    } else if (c == '\n') {
      bufferUSB[idxUSB] = '\0'; 
      idxUSB = 0;
      String cmd = String(bufferUSB);
      cmd.trim();
      if(cmd.length() > 0) CMD_Parse(cmd);
    }
  }

  static byte idxBT = 0;
  while (Serial1.available() > 0) {
    char c = Serial1.read();
    if (c != '\n' && c != '\r') { 
      bufferBT[idxBT++] = c;
      if (idxBT >= MASCARA_BUFFER) idxBT = MASCARA_BUFFER - 1; 
    } else if (c == '\n') {
      bufferBT[idxBT] = '\0'; 
      idxBT = 0;
      String cmd = String(bufferBT);
      cmd.trim();
      if(cmd.length() > 0) CMD_Parse(cmd);
    }
  }
}

void BT_SendTelemetry() {
  static bool lastStateActive = false;
  bool currentStateActive = (ensaioRunning || monitorMode);

  if (!currentStateActive) {
    if (lastStateActive) {
      float temp = Sensor_GetTemp();
      if (temp == DEVICE_DISCONNECTED_C) temp = 0.01;
      String pacote = "DADOS: T=" + String(temp, 2) + " P=0 I=0.000";
      EnviarMensagem(pacote); 
      lastStateActive = false;
    }
    return; 
  }
  
  lastStateActive = true; 

  float temp = Sensor_GetTemp();
  float curr = Sensor_GetCorrente();

  if (temp != DEVICE_DISCONNECTED_C) {
    String pacote = "DADOS: T=" + String(temp, 2) + " P=" + String(powerAtual) + " I=" + String(curr, 3);
    EnviarMensagem(pacote);
  } 
  else {
    EnviarMensagem("ERRO: Sensor de Temperatura falhou!");
    String pacote = "DADOS: T=0.01 P=" + String(powerAtual) + " I=" + String(curr, 3);
    EnviarMensagem(pacote);
  }
}