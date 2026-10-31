#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "BluetoothSerial.h"

// --- Configurações ---
const char* ssid = "WiFi-LMA";
const char* password = "@ufam.lma";
const char* PIN_BLUETOOTH = "1234";

// Endereços MAC Fixos
uint8_t ADDR_1[6] = {0x98, 0xD3, 0x31, 0x70, 0x6E, 0x9F}; 
uint8_t ADDR_2[6] = {0x00, 0x14, 0x01, 0x21, 0x21, 0x22}; 

WebServer server(80);
BluetoothSerial SerialBT;

// --- Variáveis de Estado ---
String webLog = ""; 
uint8_t currentTarget[6]; // Guarda o MAC alvo
bool hasTarget = false;   // Indica se devemos estar conectados
unsigned long lastReconnectAttempt = 0;

// --- Configuração HTTP ---
void addCommonHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "keep-alive");
  server.sendHeader("Cache-Control", "no-cache");
}

// --- Conexão Bluetooth ---
void conectarArduino(uint8_t* address, String nome) {
  // Salva o alvo para o Watchdog
  memcpy(currentTarget, address, 6);
  hasTarget = true;

  if (SerialBT.connected()) {
    SerialBT.disconnect();
    delay(200); 
  }
  
  webLog += "[SISTEMA] Conectando a " + nome + "...\n";
  
  if (SerialBT.connect(address)) {
    webLog += "[SISTEMA] ✅ Conectado!\n";
  } else {
    webLog += "[SISTEMA] ❌ Falha inicial (Watchdog assumirá).\n";
  }
}

// --- Watchdog (Cão de Guarda) ---
void checkBluetoothConnection() {
  // Só atua se definimos um alvo (/select) mas não estamos conectados
  if (hasTarget && !SerialBT.connected()) {
    unsigned long now = millis();
    
    // Tenta reconectar a cada 3 segundos
    if (now - lastReconnectAttempt > 3000) {
      lastReconnectAttempt = now;
      Serial.println("Watchdog: Tentando reconectar...");
      
      // Tenta conectar (Bloqueia por aprox 2s)
      if (SerialBT.connect(currentTarget)) {
        webLog += "[WATCHDOG] ✅ Conexão recuperada!\n";
      } else {
        // Não enchemos o log de erro para não travar o Python
        Serial.println("Watchdog: Falha."); 
      }
    }
  }
}

// --- Handlers Web ---
void handleLog() {
  addCommonHeaders();
  if (webLog.length() > 0) {
    server.send(200, "text/plain", webLog);
    webLog = ""; // Limpa buffer após entrega
  } else {
    server.send(200, "text/plain", "");
  }
}

void handleSend() {
  addCommonHeaders();
  if (server.hasArg("command")) {
    String cmd = server.arg("command");
    
    // Log de envio
    webLog += "[GATEWAY] " + cmd + "\n";
    
    // Seletor de Arduino
    if (cmd == "/select 1") {
      conectarArduino(ADDR_1, "Arduino 1");
    } 
    else if (cmd == "/select 2") {
      conectarArduino(ADDR_2, "Arduino 2");
    }
    // Botão de Pânico (Para o Watchdog)
    else if (cmd == "/stop_reconnect") {
       hasTarget = false; 
       if(SerialBT.connected()) SerialBT.disconnect();
       webLog += "[SISTEMA] Desconectado manualmente.\n";
    }
    // Repasse de comando
    else if (SerialBT.connected()) {
      SerialBT.println(cmd);
    } 
    else {
      webLog += "[ERRO] Aguardando reconexão...\n";
    }
  }
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
  
  if (MDNS.begin("esp-thermal")) {
    MDNS.addService("http", "tcp", 80);
  }

  SerialBT.begin("ESP32_Gateway", true); 
  SerialBT.setPin(PIN_BLUETOOTH, 4); 

  server.on("/log", HTTP_GET, handleLog);
  server.on("/send", HTTP_ANY, handleSend); 
  
  server.begin();
  webLog = "[INIT] Gateway Pronto. IP: " + WiFi.localIP().toString() + "\n";
}

void loop() {
  server.handleClient();
  checkBluetoothConnection(); // Verifica se caiu e reconecta

  // Leitura rápida
  while (SerialBT.available()) {
    webLog += (char)SerialBT.read();
  }
  
  // Proteção de Memória
  if (webLog.length() > 5000) webLog = "";
}