void enviarDadosDoSensor(float temperatura, float umidade) {
  String resposta = "DADOS: T=" + String(temperatura) + "C, H=" + String(umidade) + "%";
  Serial.println(resposta);
  moduloBT.println(resposta);

}