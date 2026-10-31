// =================================================================
// MÓDULO 4: COMANDOS E PARSER
// =================================================================

extern volatile float setpointTemp; 
extern bool isClosedLoop;

void CMD_Parse(String cmd) {
  if (cmd.startsWith("/def ")) {
    if (ensaioRunning || staircaseRunning) {
      EnviarMensagem("ERRO: O ciclo esta a correr! Envie /stop primeiro.");
    } else {
      int p, t_min;
      if (sscanf(cmd.c_str(), "/def %d %d", &p, &t_min) == 2) {
        ensaioTargetPower = constrain(p, 0, 100);
        ensaioDurationMillis = (unsigned long)t_min * 60000UL; 
        EnviarMensagem("INFO: Parametros Guardados. Use /play.");
      } else {
        EnviarMensagem("ERRO: Formato -> /def [potencia] [tempo_minutos]");
      }
    }
  }
  else if (cmd.startsWith("/play ")) {
    // START MALHA FECHADA
    int p1 = cmd.indexOf(' ');
    int p2 = cmd.indexOf(' ', p1 + 1);
    if (p1 != -1 && p2 != -1) {
      ensaioDurationMillis = (unsigned long)cmd.substring(p1 + 1, p2).toInt() * 1000UL;
      setpointTemp = cmd.substring(p2 + 1).toFloat();
      
      ensaioStartMillis = millis(); 
      ensaioRunning = true;
      staircaseRunning = false; 
      isClosedLoop = true;
      EnviarMensagem("INFO: Malha Fechada Iniciada! SP = " + String(setpointTemp));
    }
  }
  else if (cmd == "/play") {
    // START MALHA ABERTA
    if (ensaioDurationMillis > 0) {
      if (!ensaioRunning) {
        ensaioStartMillis = millis(); 
        ensaioRunning = true;
        staircaseRunning = false; 
        isClosedLoop = false;
        Dimmer_SetPower(ensaioTargetPower); 
        EnviarMensagem("INFO: Malha Aberta Iniciada!");
      } else {
        EnviarMensagem("INFO: O ensaio ja esta a decorrer.");
      }
    } else {
      EnviarMensagem("ERRO: Configure com /def primeiro.");
    }
  }
  else if (cmd.startsWith("/setpoint ")) {
    setpointTemp = cmd.substring(10).toFloat();
    EnviarMensagem("INFO: Novo Setpoint = " + String(setpointTemp));
  }
  else if (cmd.startsWith("/staircase ")) {
    int interval = cmd.substring(11).toInt();
    if (interval > 0) Staircase_Start(interval);
    else EnviarMensagem("ERRO: Intervalo invalido.");
  } 
  else if (cmd == "/stop") {
    ensaioRunning = false;
    staircaseRunning = false;
    isClosedLoop = false;
    setpointTemp = 0.0;
    Dimmer_SetPower(0);
    EnviarMensagem("INFO: Ensaio Parado (Manual)");
  }
  else if (cmd.startsWith("/fan")) {
    int pwm_pct = 0;
    if (sscanf(cmd.c_str(), "/fan %d", &pwm_pct) == 1) {
      pwm_pct = constrain(pwm_pct, 0, 100);
      analogWrite(PIN_FAN, map(pwm_pct, 0, 100, 0, 255));              
      EnviarMensagem("INFO: Ventoinha PWM = " + String(pwm_pct) + "%");
    }
  }
  else if (cmd == "/monitor on") {
    monitorMode = true;
    EnviarMensagem("INFO: Monitoramento LIGADO");
  }
  else if (cmd == "/monitor off") {
    monitorMode = false;
    EnviarMensagem("INFO: Monitoramento DESLIGADO");
  }
}