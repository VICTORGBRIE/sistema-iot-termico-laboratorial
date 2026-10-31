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

// =================================================================
// O PARSER DE COMANDOS (Que estava a faltar!)
// =================================================================
void CMD_Parse(String cmd) {
  cmd.trim();
  
  if (cmd.startsWith("/play ")) {
    int p1 = cmd.indexOf(' ');
    int p2 = cmd.indexOf(' ', p1 + 1);
    if (p1 != -1 && p2 != -1) {
      int dur = cmd.substring(p1 + 1, p2).toInt();
      float target = cmd.substring(p2 + 1).toFloat();
      Ensaio_Start(dur, target); // Chama a função da Malha Fechada
    }
  } 
  else if (cmd.startsWith("/staircase ")) {
    int interval = cmd.substring(11).toInt();
    Staircase_Start(interval);   // Chama a função da Malha Aberta em Degraus
  }
  else if (cmd == "/stop") {
    Ensaio_Stop();
    staircaseRunning = false;
    powerAtual = 0;
    EnviarMensagem("INFO: Sistema Parado");
  }
  else if (cmd.startsWith("/setpoint ")) {
    setpointTemp = cmd.substring(10).toFloat();
    EnviarMensagem("INFO: Novo Setpoint = " + String(setpointTemp) + " C");
  }
  else if (cmd.startsWith("/monitor on")) {
    monitorMode = true;
    EnviarMensagem("INFO: Monitoramento Ativado");
  }
  else if (cmd.startsWith("/monitor off")) {
    monitorMode = false;
    EnviarMensagem("INFO: Monitoramento Desativado");
  }
  else if (cmd.startsWith("/fan ")) {
    int val = cmd.substring(5).toInt();
    analogWrite(PIN_FAN, map(val, 0, 100, 0, 255));
  }
}

// =================================================================
// LEITURA DE DADOS
// =================================================================
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
      if(cmd.length() > 0) CMD_Parse(cmd);
    }
  }
}

// =================================================================
// ENVIO DE TELEMETRIA
// =================================================================
void BT_SendTelemetry() {
  static bool lastStateActive = false;
  bool currentStateActive = (ensaioRunning || monitorMode || staircaseRunning);

  if (!currentStateActive) {
    if (lastStateActive) {
      float temp = Sensor_GetTemp();
      if (temp == DEVICE_DISCONNECTED_C) temp = 0.01;
      // Zera o SP (Setpoint) quando o sistema desliga
      String pacote = "DADOS: T=" + String(temp, 2) + " SP=0.0 P=0 PR=0.00 I=0.000";
      EnviarMensagem(pacote); 
      lastStateActive = false;
    }
    return; 
  }
  
  lastStateActive = true; 

  float temp = Sensor_GetTemp();
  float curr = Sensor_GetCorrente();

  // Calcula a Potência Real com base na resistência de bancada
  float p_real = 192.0 * (curr * curr); 

  // Pega o setpoint real só se estiver na Malha Fechada (RST)
  float send_sp = ensaioRunning ? setpointTemp : 0.0;

  if (temp != DEVICE_DISCONNECTED_C) {
    String pacote = "DADOS: T=" + String(temp, 2) + " SP=" + String(send_sp, 2) + " P=" + String(powerAtual) + " PR=" + String(p_real, 2) + " I=" + String(curr, 3);
    EnviarMensagem(pacote);
  } 
  else {
    EnviarMensagem("ERRO: Sensor de Temperatura falhou!");
    String pacote = "DADOS: T=0.01 SP=" + String(send_sp, 2) + " P=" + String(powerAtual) + " PR=" + String(p_real, 2) + " I=" + String(curr, 3);
    EnviarMensagem(pacote);
  }
}