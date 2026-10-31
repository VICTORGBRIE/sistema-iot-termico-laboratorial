#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h> // Biblioteca para resolver o nome .local
#include "BluetoothSerial.h" 

// --- Configuração do Wi-Fi ---


const char* ssid = "WiFi-LMA";
const char* password = "@ufam.lma";
WebServer server(80);

/*
const char* ssid = "A16 de Victor";
const char* password = "Fl@mengo16";
WebServer server(80);
*/

// --- Configuração do Bluetooth ---
BluetoothSerial SerialBT;
const char* NOME_ARDUINO_1 = "HC-06";
const char* NOME_ARDUINO_2 = "HC-05";
const char* PIN_BLUETOOTH = "1234";

uint8_t ADDR_ARDUINO_1[6] = {0x98, 0xD3, 0x31, 0x70, 0x6E, 0x9F}; 
uint8_t ADDR_ARDUINO_2[6] = {0x00, 0x14, 0x01, 0x21, 0x21, 0x22}; 

int arduinoConectado = 0;
String webLog = "Servidor iniciado.\n";

// --- Funções de Controle ---

void conectarAoArduino(int numeroArduino, const char* nomeDispositivo, uint8_t macAddress[]) {
  if (SerialBT.connected()) {
    String msg = "Desconectando do dispositivo atual...\n";
    Serial.print(msg);
    webLog += msg;
    SerialBT.disconnect();
    delay(500); 
  }
  
  String msg = "Tentando conectar ao Arduino " + String(numeroArduino) + " (" + nomeDispositivo + ")...\n";
  Serial.print(msg);
  webLog += msg;

  SerialBT.connect(macAddress);
  
  bool conectado = false;
  for(int i = 0; i < 6; i++) {
    if (SerialBT.connected()) {
      conectado = true;
      break;
    }
    delay(500);
  }

  if (conectado) {
    msg = "CONECTADO com sucesso ao Arduino " + String(numeroArduino) + "\n";
    Serial.print(msg);
    webLog += msg;
    SerialBT.println("Conectado ao ESP32 via Gateway Web");
    arduinoConectado = numeroArduino;
  } else {
    msg = "FALHA ao conectar ao Arduino " + String(numeroArduino) + "\n";
    Serial.print(msg);
    webLog += msg;
    arduinoConectado = 0;
  }
  webLog += "----------------------------------------------------\n";
}

//

void enviarComandoBT(String command) {
  command.trim();
  if (command.length() == 0) return;
  String msg = "Enviando (Web) -> " + command + "\n";
  Serial.print(msg);
  webLog += msg;

  if (command == "/select 1") {
    conectarAoArduino(1, NOME_ARDUINO_1, ADDR_ARDUINO_1);
  } 
  else if (command == "/select 2") {
    conectarAoArduino(2, NOME_ARDUINO_2, ADDR_ARDUINO_2);
  }
  else if (arduinoConectado != 0 && SerialBT.connected()) {
    SerialBT.println(command);
  } 
  else if (command.startsWith("/")) {
     msg = "Erro: Nenhum Arduino conectado.\n";
     Serial.print(msg);
     webLog += msg;
  }
}

void handleRoot() {
  String html = "<html><head><meta charset='UTF-8'>";
  html += "<title>Controle Bluetooth via Wi-Fi</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f4f4f4;}textarea{width:100%;height:350px;font-family:monospace;}</style>";
  html += "</head><body><h1>Log de Controle Bluetooth</h1>";
  html += "<h3>Host: esp-thermal.local</h3>";
  html += "<h3>IP: " + WiFi.localIP().toString() + "</h3>";
  html += "<textarea readonly id='log'>" + webLog + "</textarea>";
  html += "<script>function updateLog(){fetch('/log').then(r=>r.text()).then(t=>{let a=document.getElementById('log');if(a.value!=t){a.value=t;a.scrollTop=a.scrollHeight;}});}setInterval(updateLog, 2000);</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleLog() { server.send(200, "text/plain", webLog); }
void handleSend() { if (server.hasArg("command")) enviarComandoBT(server.arg("command")); server.send(200, "text/plain", "OK"); }
void handleClear() { webLog = "Log limpo.\n"; server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); }

void setup() {
  Serial.begin(115200);

  // 1. Inicia o Wi-Fi
  Serial.println("\nConectando ao Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // --- OBTENDO E EXIBINDO O ENDEREÇO MAC ---
  String mac = WiFi.macAddress();
  Serial.println("\nWi-Fi conectado!");
  Serial.print("Endereço IP: "); Serial.println(WiFi.localIP());
  Serial.print("Endereço MAC: "); Serial.println(mac);
  
  webLog += "Wi-Fi conectado!\n";
  webLog += "IP: " + WiFi.localIP().toString() + "\n";
  webLog += "MAC: " + mac + "\n";

  // --- CONFIGURAÇÃO DO mDNS (Nome amigável) ---
  // Isso permite que você use "esp-thermal.local" na interface
  if (!MDNS.begin("esp-thermal")) {
    Serial.println("Erro ao configurar mDNS!");
    webLog += "Erro ao configurar mDNS!\n";
  } else {
    Serial.println("mDNS iniciado: esp-thermal.local");
    webLog += "Hostname: esp-thermal.local\n";
  }

  // 2. Inicia o Bluetooth
  SerialBT.begin("ESP32_Gateway", true); 
  SerialBT.setPin(PIN_BLUETOOTH, 4);

  // 3. Inicia o Servidor Web
  server.on("/", handleRoot);
  server.on("/send", handleSend);
  server.on("/clear", handleClear);
  server.on("/log", handleLog);
  
  server.begin();
  MDNS.addService("http", "tcp", 80);
  
  webLog += "Servidor pronto.\n----------------------------------------------------\n";
}

void loop() {
  server.handleClient();
  if (SerialBT.available()) {
    String line = SerialBT.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      webLog += "Arduino -> " + line + "\n"; 
    }
  }
}