// =================================================================
// MÓDULO 1: CONTROLO DE POTÊNCIA SÍNCRONO
// =================================================================

int tabelaDimmer[101];

void Dimmer_CalcularCurva() {
  for (int p = 0; p <= 100; p++) {
    if (p == 0) { tabelaDimmer[0] = 8200; continue; }
    if (p == 100) { tabelaDimmer[100] = 10; continue; }
    
    int t_min = 10;
    int t_max = 8200;
    int t_ans = 8200;
    
    for (int i = 0; i < 15; i++) {
      int t_mid = (t_min + t_max) / 2;
      float alpha = (float)t_mid * PI / 8333.33;
      float pot_calc = 1.0 - (alpha / PI) + (sin(2.0 * alpha) / (2.0 * PI));
      pot_calc *= 100.0;
      if (pot_calc < p) t_max = t_mid;
      else { t_min = t_mid; t_ans = t_mid; }
    }
    tabelaDimmer[p] = t_ans; 
  }
}

void zeroCrossInterrupt() {
  unsigned long agora = micros();
  if (agora - lastZC < 4000) return;
  lastZC = agora;
  zcCounter++; // O NOSSO MOTOR DE TEMPO (Bate 120x por segundo)

  if (powerAtual <= 0) { digitalWrite(PIN_DIM, LOW); return; }
  if (powerAtual >= 100) { digitalWrite(PIN_DIM, HIGH); return; }

  delayMicroseconds(atrasoDimmerAtual); 
  digitalWrite(PIN_DIM, HIGH); 
  delayMicroseconds(50);       
  digitalWrite(PIN_DIM, LOW);  
}

void Dimmer_Init() {
  pinMode(PIN_ZC, INPUT_PULLUP);
  pinMode(PIN_DIM, OUTPUT);
  digitalWrite(PIN_DIM, LOW);
  Dimmer_CalcularCurva(); 
  attachInterrupt(digitalPinToInterrupt(PIN_ZC), zeroCrossInterrupt, RISING);
}

void Dimmer_SetPower(int pot) {
  if (powerAtual != pot) {
    powerAtual = constrain(pot, 0, 100);
    atrasoDimmerAtual = tabelaDimmer[powerAtual];
    if (powerAtual <= 0) digitalWrite(PIN_DIM, LOW);
    else if (powerAtual >= 100) digitalWrite(PIN_DIM, HIGH);
  }
}

void Dimmer_Update() {}