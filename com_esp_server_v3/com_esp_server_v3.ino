#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "BluetoothSerial.h"

// --- Configurações de Rede ---

const char* ssid = "WiFi-LMA";
const char* password = "@ufam.lma";
WebServer server(80);
/*
const char* ssid = "VENHA VOCE PARA O MUNDO NET MNT";
const char* password = "993260034";
WebServer server(80);
*/
// --- Configurações Bluetooth ---
BluetoothSerial SerialBT;
const char* NOME_ARDUINO_1 = "HC-06";
const char* NOME_ARDUINO_2 = "HC-05";
const char* PIN_BLUETOOTH = "1234";

// Endereços MAC (Verificados)
/* uint8_t ADDR_ARDUINO_1[6] = { 0x98, 0xDA, 0x60, 0x0B, 0xE1, 0xFD}; */
uint8_t ADDR_ARDUINO_1[6] = {0x98, 0xD3, 0x31, 0x70, 0x6E, 0x9F}; 
uint8_t ADDR_ARDUINO_2[6] = {0x00, 0x14, 0x01, 0x21, 0x21, 0x22}; 

int arduinoConectado = 0;
String webLog = ""; 
bool systemRunning = false;

// --- Variáveis para Validação de Comandos ---
String lastCommand = "";
String lastResponse = "";
unsigned long commandSentTime = 0;
bool awaitingResponse = false;
const unsigned long RESPONSE_TIMEOUT = 2000; // 2 segundos para esperar resposta

// --- Função Robusta de Conexão Bluetooth ---
void conectarAoArduino(const char* nomeDispositivo, int num, uint8_t mac[]) {
  // 1. Limpeza de conexão anterior
  if (SerialBT.connected()) {
    webLog += "[SISTEMA] A desconectar do dispositivo atual...\n";
    SerialBT.disconnect();
    delay(1500); // Delay aumentado para garantir desconexão total
  }

  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  String msg = "Iniciando conexao com Arduino " + String(num) + " (" + nomeDispositivo + ")...\n";
  Serial.print(msg);
  webLog += msg; 

  String msgMac = "Alvo MAC: " + String(macStr) + "\n";
  Serial.print(msgMac);
  webLog += msgMac;

  // CORREÇÃO: Adicionado o tamanho do PIN (4) como segundo argumento
  SerialBT.setPin(PIN_BLUETOOTH, 4);

  // 2. Loop de Tentativas (Retry Logic)
  bool sucesso = false;
  for (int i = 1; i <= 3; i++) {
    String tentativaMsg = "Tentativa " + String(i) + " de 3...\n";
    Serial.print(tentativaMsg);
    webLog += tentativaMsg;
    
    // Tenta conectar
    if (SerialBT.connect(mac)) {
      sucesso = true;
      break; // Conectou! Sai do loop
    }
    
    // Se falhou, espera um pouco antes de tentar de novo
    if (i < 3) {
      webLog += "Sem resposta. Aguardando 2s...\n";
      delay(2000); 
    }
  }

  // 3. Resultado Final
  if (sucesso) {
    webLog += "[SISTEMA]  CONECTADO com sucesso ao Arduino " + String(num) + "!\n";
    Serial.println("Bluetooth Conectado!");
    arduinoConectado = num;
  } else {
    String msgFail = "FALHA DEFINITIVA ao conectar ao Arduino " + String(num) + "\n";
    Serial.print(msgFail);
    webLog += msgFail;
    webLog += "Falha na conexao.\n";
    webLog += "[SISTEMA]  FALHA na conexao Bluetooth.\n";
    webLog += "[DICA] Verifique se o HC-06 nao esta conectado ao celular.\n";
    arduinoConectado = 0;
    Serial.println("Falha na conexao.");
  }
}

// --- Handlers do Servidor Web ---

void handleLog() {
  if (webLog.length() > 0) {
    server.send(200, "text/plain", webLog);
    webLog = ""; 
  } else {
    server.send(200, "text/plain", "");
  }
}

void handleSend() {
  if (server.hasArg("command")) {
    String cmd = server.arg("command");
    cmd.trim();
    
    webLog += "[GATEWAY] Repassando: " + cmd + "\n";
    
    if (cmd == "/play") {
      systemRunning = true;
    } else if (cmd == "/stop") {
      systemRunning = false;
    }

    // Comandos de Seleção
    if (cmd == "/select 1") {
      conectarAoArduino(NOME_ARDUINO_1, 1, ADDR_ARDUINO_1);
    } else if (cmd == "/select 2") {
      conectarAoArduino(NOME_ARDUINO_2, 2, ADDR_ARDUINO_2);
    } 
    // Repasse de comandos
    else if (SerialBT.connected()) {
      lastCommand = cmd;
      lastResponse = "";
      awaitingResponse = true;
      commandSentTime = millis();
      
      SerialBT.println(cmd);
      delay(50);
    } else {
      webLog += "[SISTEMA] Erro: Bluetooth nao conectado.\n";
      lastResponse = "ERRO: Bluetooth nao conectado";
      awaitingResponse = false;
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleRoot() {
  String html = "<html><head><meta charset='UTF-8'><title>Gateway Térmico IoT</title></head>";
  html += "<body style='font-family:sans-serif; padding:20px;'>";
  html += "<h1>ESP32 Gateway Online</h1>";
  html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
  html += "<pre id='l' style='background:#eee; padding:10px; height:300px; overflow:auto;'>Aguardando...</pre>";
  html += "<script>setInterval(()=>fetch('/log').then(r=>r.text()).then(t=>{if(t){let p=document.getElementById('l'); p.innerText+=t; p.scrollTop=p.scrollHeight;}}), 1000);</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleStatus() {
  // Limpa caracteres de nova linha da resposta
  String respostaLimpa = lastResponse;
  respostaLimpa.replace("\n", "");
  respostaLimpa.replace("\r", "");
  
  // Retorna um JSON com o status do último comando
  String json = "{";
  json += "\"comando\":\"" + lastCommand + "\",";
  json += "\"resposta\":\"" + respostaLimpa + "\",";
  json += "\"aguardando\":" + String(awaitingResponse ? "true" : "false") + ",";
  json += "\"arduino_conectado\":" + arduinoConectado;
  json += "}";
  
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  String macESP = WiFi.macAddress();
  Serial.println("\nWiFi Conectado!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  
  if (MDNS.begin("esp-thermal")) {
    MDNS.addService("http", "tcp", 80);
  }

  // Nome do ESP no Bluetooth (Não confundir com o nome do Arduino)
  SerialBT.begin("ESP32_Master", true); 
  
  // CORREÇÃO: Adicionado o tamanho do PIN (4) como segundo argumento
  SerialBT.setPin(PIN_BLUETOOTH, 4); 

  server.on("/", handleRoot);
  server.on("/log", handleLog);
  server.on("/send", handleSend);
  server.on("/status", handleStatus);
  server.on("/clear", [](){ webLog = ""; server.send(200, "text/plain", "OK"); });
  
  server.begin();
  webLog = "[SISTEMA] Gateway Pronto. Conecte no PC.\n";
}

void loop() {
  server.handleClient();

  while (SerialBT.available()) {
    char c = SerialBT.read();
    webLog += c;
    
    // Se estamos aguardando resposta, captura para a variável lastResponse
    if (awaitingResponse) {
      lastResponse += c;
      
      // Timeout de resposta
      if (millis() - commandSentTime > RESPONSE_TIMEOUT) {
        awaitingResponse = false;
        if (lastResponse.isEmpty()) {
          lastResponse = "TIMEOUT: Sem resposta do Arduino";
        }
      }
    }
    
    if (webLog.length() > 4000) webLog = webLog.substring(2000); 
  }
  
  // Verifica timeout enquanto aguarda resposta
  if (awaitingResponse && (millis() - commandSentTime > RESPONSE_TIMEOUT)) {
    awaitingResponse = false;
    if (lastResponse.isEmpty()) {
      lastResponse = "TIMEOUT: Sem resposta do Arduino";
    }
  }
}