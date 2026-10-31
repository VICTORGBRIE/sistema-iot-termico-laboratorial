void runControleLogic(float temp) {
  if (isHeating && temp >= setpoint_high) {
    Serial.println("Atingiu limite alto → trocando para modo RESFRIAR.");
    isHeating = false;
  }

  // Se está resfriando e a temperatura caiu abaixo do limite inferior → muda para aquecer
  else if (!isHeating && temp <= setpoint_low) {
    Serial.println("Atingiu limite baixo → trocando para modo AQUECER.");
    isHeating = true;
  }

  // --- Define os atuadores conforme o modo atual ---
  if (!isControleActive) {
    // Controle desativado manualmente
    digitalWrite(PIN_HEATER, LOW);
    digitalWrite(PIN_FAN, LOW);
  }
  else if (isHeating) {
    // Aquecendo
    digitalWrite(PIN_HEATER, LOW);
    digitalWrite(PIN_FAN, LOW);
  }
  else {
    // Resfriando
    digitalWrite(PIN_HEATER, HIGH);
    digitalWrite(PIN_FAN, HIGH);
  }

  // --- Saída de debug ---
  Serial.print("Temp: "); Serial.print(temp);
  Serial.print(" | Modo: "); Serial.print(isHeating ? "AQUECENDO" : "RESFRIANDO");
  Serial.print(" | Aquecedor: "); Serial.print(digitalRead(PIN_HEATER));
  Serial.print(" | Ventoinha: "); Serial.println(digitalRead(PIN_FAN));
}