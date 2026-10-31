#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// --- Configuração dos seus Arduinos ---
const char* NOME_ARDUINO_1 = "HC-06";
const char* NOME_ARDUINO_2 = "HC-05";
const char* PIN_BLUETOOTH = "1234";

// Endereços MAC que você descobriu
uint8_t ADDR_ARDUINO_1[6] = {0x98, 0xD3, 0x31, 0x70, 0x6E, 0x9F}; // MAC do HC-06
uint8_t ADDR_ARDUINO_2[6] = {0x00, 0x14, 0x01, 0x21, 0x21, 0x22}; // MAC do HC-05

int arduinoConectado = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("--- Gerenciador Rápido de Arduinos (por MAC) ---");
  SerialBT.begin("ESP32_MultiMaster", true); 
  SerialBT.setPin(PIN_BLUETOOTH);
  Serial.println("Pronto. Use '/select 1' ou '/select 2'.");
  Serial.println("----------------------------------------------------");
}

void loop() {
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    if (comando == "/select 1") {
      // Passa o NOME (para o log) e o ENDEREÇO MAC (para a conexão)
      conectarAoArduino(1, NOME_ARDUINO_1, ADDR_ARDUINO_1);
    } 
    else if (comando == "/select 2") {
      conectarAoArduino(2, NOME_ARDUINO_2, ADDR_ARDUINO_2);
    }
    // Envia qualquer outro comando para o Arduino conectado
    else if (arduinoConectado != 0 && SerialBT.connected()) {
      Serial.printf("Enviando para Arduino %d: '%s'\n", arduinoConectado, comando.c_str());
      SerialBT.println(comando);
    } else if (arduinoConectado == 0) {
      Serial.println("Nenhum Arduino selecionado.");
    }
  }

  // Recebe dados do Arduino conectado e imprime no monitor
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }
}

// FUNÇÃO DE CONEXÃO ATUALIZADA (agora recebe o MAC Address)
void conectarAoArduino(int numeroArduino, const char* nomeDispositivo, uint8_t macAddress[]) {
  if (SerialBT.connected()) {
    Serial.println("Desconectando do dispositivo atual...");
    SerialBT.disconnect();
    delay(500); 
  }
  
  Serial.printf("Tentando conectar ao Arduino %d (%s)...\n", numeroArduino, nomeDispositivo);

  // A MÁGICA ACONTECE AQUI: conectando pelo endereço
  SerialBT.connect(macAddress);
  
  // Lógica de verificação "paciente"
  bool conectado = false;
  for(int i = 0; i < 6; i++) { // Tenta por 3 segundos
    if (SerialBT.connected()) {
      conectado = true;
      break;
    }
    delay(500);
  }

  if (conectado) {
    Serial.printf("CONECTADO com sucesso ao Arduino %d\n", numeroArduino);
    SerialBT.println("Conectado ao ESP");
    arduinoConectado = numeroArduino;
  } else {
    Serial.printf("FALHA ao conectar ao Arduino %d\n", numeroArduino);
    arduinoConectado = 0;
  }
  Serial.println("----------------------------------------------------");
}