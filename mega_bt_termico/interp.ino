// =================================================================
// MÓDULO 4: COMANDOS E PARSER
// =================================================================

void CMD_Parse(String cmd) {
  if (cmd.startsWith("/def")) {
    if (ensaioRunning) {
      Serial1.println("ERRO: O ciclo esta a correr! Envie /stop primeiro para reconfigurar.");
    } else {
      int p, t_min;
      if (sscanf(cmd.c_str(), "/def %d %d", &p, &t_min) == 2) {
        ensaioTargetPower = constrain(p, 0, 100);
        // CONVERSÃO INFALÍVEL: t_min * 60 seg * 120 ciclos da rede/seg
        ensaioDurationZC = (unsigned long)t_min * 60UL * 120UL; 
        Serial1.println("INFO: Parametros Guardados. Use /play para iniciar.");
      } else {
        Serial1.println("ERRO: Formato -> /def [potencia] [tempo_minutos]");
      }
    }
  }
  else if (cmd == "/play") {
    if (ensaioDurationZC > 0) {
      if (!ensaioRunning) {
        ensaioStartZC = zcCounter; // Inicia a contagem baseada na rede elétrica
        ensaioRunning = true;
        Dimmer_SetPower(ensaioTargetPower); 
        Serial1.println("INFO: Ensaio Iniciado!");
      } else {
        Serial1.println("INFO: O ensaio ja esta a decorrer.");
      }
    } else {
      Serial1.println("ERRO: Configure com /def primeiro.");
    }
  } 
  else if (cmd == "/stop") {
    ensaioRunning = false;
    Dimmer_SetPower(0);
    Serial1.println("INFO: Ensaio Parado Manualmente.");
  }
  else if (cmd == "/ventoinha on") {
    digitalWrite(PIN_FAN, HIGH);
    Serial1.println("INFO: Ventoinha LIGADA");
  }
  else if (cmd == "/ventoinha off") {
    digitalWrite(PIN_FAN, LOW);
    Serial1.println("INFO: Ventoinha DESLIGADA");
  }
  else if (cmd == "/monitor on") {
    monitorMode = true;
    Serial1.println("INFO: Monitoramento de Temp LIGADO");
  }
  else if (cmd == "/monitor off") {
    monitorMode = false;
    Serial1.println("INFO: Monitoramento de Temp DESLIGADO");
  }
}