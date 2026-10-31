#include <WiFi.h>
#include <WebServer.h>
#include "BluetoothSerial.h" // Importante para o Bluetooth

// --- Configuração do Wi-Fi ---
const char* ssid = "WiFi-LMA";
const char* password = "@ufam.lma";
WebServer server(80);

// --- Configuração do Bluetooth ---
BluetoothSerial SerialBT;
const char* NOME_ARDUINO_1 = "HC-06";
const char* NOME_ARDUINO_2 = "HC-05";
const char* PIN_BLUETOOTH = "1234";

// Endereços MAC que você descobriu
uint8_t ADDR_ARDUINO_1[6] = {0x98, 0xD3, 0x31, 0x70, 0x6E, 0x9F}; // MAC do HC-06
uint8_t ADDR_ARDUINO_2[6] = {0x00, 0x14, 0x01, 0x21, 0x21, 0x22}; // MAC do HC-05

int arduinoConectado = 0;

// --- Log Global para a Página Web ---
String webLog = "Servidor iniciado.\n";

// --- Funções de Controlo (Modificadas para reportar ao Log Web) ---

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
  for(int i = 0; i < 6; i++) { // Tenta por 3 segundos
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
    SerialBT.println("Conectado ao ESP32 via Gateway Web"); // Envia uma saudação
    arduinoConectado = numeroArduino;
  } else {
    msg = "FALHA ao conectar ao Arduino " + String(numeroArduino) + "\n";
    Serial.print(msg);
    webLog += msg;
    arduinoConectado = 0;
  }
  webLog += "----------------------------------------------------\n";
}

// --- Funções do Servidor Web ---

/**
 * @brief Envia o comando (recebido via Web) para o Bluetooth.
 */
void enviarComandoBT(String command) {
  command.trim();
  if (command.length() == 0) return;

  String msg = "Enviando (Web) -> " + command + "\n";
  Serial.print(msg);
  webLog += msg;

  // Processa comandos de seleção localmente
  if (command == "/select 1") {
    conectarAoArduino(1, NOME_ARDUINO_1, ADDR_ARDUINO_1);
  } 
  else if (command == "/select 2") {
    conectarAoArduino(2, NOME_ARDUINO_2, ADDR_ARDUINO_2);
  }
  // Envia todos os outros comandos para o Arduino conectado
  else if (arduinoConectado != 0 && SerialBT.connected()) {
    SerialBT.println(command);
  } 
  // Se não estiver conectado, reporta o erro
  else if (command.startsWith("/")) {
     msg = "Erro: Nenhum Arduino conectado.\n";
     Serial.print(msg);
     webLog += msg;
  }
}

/**
 * @brief Lida com a página web principal (/)
 */
void handleRoot() {
  String html = "<html><head>";
  html += "<title>Controle Bluetooth via Wi-Fi</title>";
  // --- REMOVIDO o 'refresh' automático ---
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  // Estilo CSS básico para uma interface limpa
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f4f4f4; }";
  html += "h1 { color: #333; }";
  // --- ADICIONADO: id='log' para o JavaScript ---
  html += "textarea { width: 100%; height: 350px; margin-bottom: 10px; border: 1px solid #ccc; border-radius: 5px; font-family: monospace; }";
  html += "form { display: flex; margin-bottom: 10px;}";
  html += "input[type='text'] { flex-grow: 1; padding: 10px; border: 1px solid #ccc; border-radius: 5px; }";
  html += "input[type='submit'] { padding: 10px 20px; background: #007bff; color: white; border: none; border-radius: 5px; margin-left: 10px; cursor: pointer; }";
  html += ".buttons a { text-decoration: none; display: inline-block; padding: 10px 15px; background: #6c757d; color: white; border-radius: 5px; margin-right: 5px; margin-bottom: 5px; }";
  html += "</style></head><body>";
  
  html += "<h1>Log de Controle Bluetooth</h1>";
  html += "<h3>IP: " + WiFi.localIP().toString() + "</h3>";
  
  // O "Monitor Serial" na web
  // --- ADICIONADO: id='log' ---
  html += "<textarea readonly id='log'>" + webLog + "</textarea>";
  
  // Formulário para enviar comandos (como /play ou 60)
  html += "<h3>Enviar Comando Manual:</h3>";
  // --- ADICIONADO: id='commandForm' e id='commandInput' ---
  html += "<form action='/send' method='POST' id='commandForm'>";
  html += "<input type='text' name='command' placeholder='Digite o comando (ex: /play ou 60)' id='commandInput'>";
  html += "<input type='submit' value='Enviar'>";
  html += "</form>";
  
  // Botões para comandos rápidos
  html += "<div class='buttons'>";
  html += "<h3>Comandos Rápidos:</h3>";
  html += "<a href='/send?command=/select 1'>/select 1</a>";
  html += "<a href='/send?command=/select 2'>/select 2</a>";
  html += "<a href='/send?command=/def'>/def</a>";
  html += "<a href='/send?command=/play'>/play</a>";
  html += "<a href='/send?command=/stop_controle'>/stop_controle</a>";
  html += "<a href='/send?command=/status'>/status</a>";
  html += "<a href='/send?command=/stop'>/stop</a>";
  html += "<a href='/clear' style='background: #dc3545;'>Limpar Log</a>";
  html += "</div>";

  // --- ADICIONADO: JavaScript (AJAX) para atualização dinâmica ---
  html += "<script>";
  html += "function updateLog() {";
  html += "  fetch('/log').then(response => response.text()).then(text => {";
  html += "    var logArea = document.getElementById('log');";
  html += "    if (logArea.value != text) {";
  html += "      logArea.value = text;";
  html += "      logArea.scrollTop = logArea.scrollHeight;"; // Auto-scroll
  html += "    }";
  html += "  });";
  html += "}";
  html += "setInterval(updateLog, 2000);"; // Atualiza o log a cada 2 segundos

  // Função para enviar comandos sem recarregar
  html += "function sendCommand(cmd) {";
  html += "  fetch('/send?command=' + encodeURIComponent(cmd));";
  html += "}";

  // Intercepta o formulário e os botões
  html += "window.onload = function() {";
  // Botões de atalho
  html += "  var buttons = document.querySelectorAll('.buttons a');";
  html += "  for (var i = 0; i < buttons.length; i++) {";
  html += "    if (buttons[i].getAttribute('href') !== '/clear') {"; // Deixa o botão /clear funcionar normalmente
  html += "       buttons[i].onclick = function(e) {";
  html += "         e.preventDefault();"; // Cancela a navegação
  html += "         var cmd = this.getAttribute('href').split('=')[1];";
  html += "         sendCommand(decodeURIComponent(cmd));";
  html += "       };";
  html += "    }";
  html += "  }";
  // Formulário de texto
  html += "  var form = document.getElementById('commandForm');";
  html += "  var input = document.getElementById('commandInput');";
  html += "  form.onsubmit = function(e) {";
  html += "    e.preventDefault();"; // Cancela o envio
  html += "    sendCommand(input.value);";
  html += "    input.value = '';"; // Limpa a caixa
  html += "  };";
  html += "};";
  html += "</script>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

/**
 * @brief Lida com o envio de comandos (do formulário ou dos botões)
 */
void handleSend() {
  if (server.hasArg("command")) {
    String command = server.arg("command");
    enviarComandoBT(command);
  }
  // --- ALTERADO: Responde com OK em vez de redirecionar ---
  server.send(200, "text/plain", "OK");
}

/**
 * @brief Limpa o log da web
 */
void handleClear() {
  webLog = "Log limpo.\n";
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Redirecionando...");
}

// --- NOVO: Handle para servir apenas o log ---
/**
 * @brief Envia apenas o conteúdo do log
 */
void handleLog() {
  server.send(200, "text/plain", webLog);
}

// --- SETUP (FUNÇÃO DE INICIALIZAÇÃO COMBINADA) ---
void setup() {
  Serial.begin(115200);

  // 1. Inicia o Wi-Fi
  Serial.println("Conectando ao Wi-Fi...");
  webLog += "Conectando ao Wi-Fi...\n";
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // Não polui o log web durante a conexão
  }
  Serial.println("\nWi-Fi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
  webLog += "Wi-Fi conectado! IP: " + WiFi.localIP().toString() + "\n";

  // 2. Inicia o Bluetooth
  Serial.println("Iniciando Bluetooth...");
  webLog += "Iniciando Bluetooth...\n";
  SerialBT.begin("ESP32_MultiMaster", true); 
  // --- CORREÇÃO PARA A VERSÃO 3.3.4 ---
  SerialBT.setPin(PIN_BLUETOOTH, 4);
  Serial.println("Bluetooth pronto.");
  webLog += "Bluetooth pronto.\n";
  
  // 3. Inicia o Servidor Web
  server.on("/", handleRoot);
  server.on("/send", HTTP_POST, handleSend); 
  server.on("/send", HTTP_GET, handleSend);  
  server.on("/clear", handleClear);
  // --- ADICIONADO: Nova rota para o log ---
  server.on("/log", handleLog);
  
  server.begin();
  Serial.println("Servidor web iniciado!");
  webLog += "Servidor web iniciado! Acesse o IP no seu navegador.\n";
  webLog += "----------------------------------------------------\n";
}

// --- LOOP (FUNÇÃO PRINCIPAL COMBINADA) ---
void loop() {
  // 1. Gere as requisições web
  server.handleClient();

  // 2. Gere os dados recebidos do Bluetooth
  if (SerialBT.available()) {
    String line = SerialBT.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      Serial.print("BT Recebido: ");
      Serial.println(line);
      // Adiciona a resposta do Arduino ao log da web
      webLog += "Arduino -> " + line + "\n"; 
    }
  }
}