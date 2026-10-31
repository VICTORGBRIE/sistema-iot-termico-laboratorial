#include <SoftwareSerial.h>
#include "DHT.h"  // Biblioteca para o sensor DHT

// --- Configuração dos Pinos (Conforme o seu código) ---
#define DHT_DATA_PIN 8  // Pino onde o sensor está conectado
#define PIN_HEATER 11    // Pino de controlo do relé (Aquecedor)
#define PIN_FAN 6       // Pino DIGITAL para a Ponte-H (Ventoinha)
#define DHTTYPE DHT11   // Define o modelo do sensor

// Inicializa os objetos
DHT dht(DHT_DATA_PIN, DHTTYPE);
SoftwareSerial moduloBT(3, 4);  // RX, TX

// --- Variáveis de Estado para Streaming do Sensor ---
bool isStreaming = false;    // Controla se o streaming está ativo
unsigned long dhtTimer = 0;  // Temporizador para o sensor
// NOTA: O DHT11 é lento. 1000ms é o limite. Se tiver erros 'NaN', aumente para 2000.
const long dhtInterval = 1000;  // Ler o sensor e enviar dados a cada 1 segundo

// --- Variáveis de Estado para o Controle Térmico ---
bool isControleActive = false;  // O controle (aquecedor/ventoinha) está ativo?
bool isHeating = false;          // Estado do controlo: true = Aquecendo, false = Arrefecendo

// --- Variáveis de Setpoint ---
float setpoint_high;              // Definido pelo utilizador
float setpoint_low;               // Definido pelo utilizador
bool setpointsDefinidos = false;  // Flag para saber se /def foi executado
int setupState = 0;               // 0=Idle, 1=Aguardando setpoint_high, 2=Aguardando setpoint_low

// --- PROTÓTIPOS DAS FUNÇÕES ---
void runControleLogic(float temp);
void enviarDadosDoSensor(float temperatura, float umidade);

void setup() {
  Serial.begin(9600);
  Serial.println("Arduino 1 (HC-06) - Controle Térmico v6 (Def/Play)");

  // Configura os pinos de saída
  pinMode(PIN_HEATER, OUTPUT);
  pinMode(PIN_FAN, OUTPUT);

  // --- CORREÇÃO: Garante que tudo começa desligado ---
  digitalWrite(PIN_HEATER, HIGH);
  digitalWrite(PIN_FAN, LOW);

  // Inicia a comunicação Bluetooth na velocidade reconfigurada
  moduloBT.begin(38400);

  // Inicia o sensor DHT11
  dht.begin();

  Serial.println("Aguardando comandos...");
  Serial.println("Streaming: /status | /stop");
  Serial.println("Controle: /def (para definir) | /play (para iniciar) | /stop_controle");

  // Inicializa o temporizador
  dhtTimer = millis();
}

void loop() {
  unsigned long currentMillis = millis();  // Obtém o tempo atual uma vez por loop

  // --- 1. Verificador de Comandos do ESP32 ---
  if (moduloBT.available()) {
    String comandoRecebido = moduloBT.readStringUntil('\n');
    comandoRecebido.trim();

    if (comandoRecebido.length() > 0) {
      Serial.print("Recebido: '");
      Serial.print(comandoRecebido);
      Serial.println("'");

      // --- MÁQUINA DE ESTADOS DE CONFIGURAÇÃO (Iniciada por /def) ---
      if (setupState == 1) {
        setpoint_high = comandoRecebido.toFloat();
        Serial.print("Setpoint Alto definido para: ");
        Serial.println(setpoint_high);

        moduloBT.println("Quantos graus para a ventoinha ser desligada:");
        setupState = 2;  // Avança para o próximo estado
      } else if (setupState == 2) {
        setpoint_low = comandoRecebido.toFloat();
        Serial.print("Setpoint Baixo definido para: ");
        Serial.println(setpoint_low);

        if (setpoint_low >= setpoint_high) {
          moduloBT.println("Erro: Temp. Baixa deve ser MENOR que a Temp. Alta. A reconfigurar.");
          moduloBT.println("Quantos graus para lampada ser desligada:");
          setupState = 1;  // Reinicia a configuração
        } else {
          // Apenas termina a configuração, NÃO inicia o controle
          moduloBT.println("Configuracao concluida. Use /play para iniciar.");
          setpointsDefinidos = true;  // Marca que os valores são válidos
          isControleActive = false;   // Garante que o controle está parado
          setupState = 0;             // Retorna ao estado idle
        }
      }

      // --- PROCESSADOR DE COMANDOS (Estado Idle) ---
      else if (setupState == 0) {

        // --- CORREÇÃO: Comando /status agora envia resposta imediata ---
        if (comandoRecebido == "/status") {
          if (!isStreaming) {
            isStreaming = true;
            Serial.println("Streaming INICIADO.");
            moduloBT.println("Arduino 1: Streaming de sensor iniciado.");
          }
          // Envia o status imediato, quer o streaming esteja a ligar ou já ligado
          Serial.println("A enviar status imediato...");
          float t = dht.readTemperature();
          float h = dht.readHumidity();
          if (isnan(t) || isnan(h)) {
            moduloBT.println("Arduino 1: Erro ao ler sensor para status.");
          } else {
            enviarDadosDoSensor(t, h);
          }
          // Reseta o timer principal para evitar um envio duplicado logo a seguir
          dhtTimer = currentMillis;
        }

        else if (comandoRecebido == "/stop") {
          if (isStreaming) {
            isStreaming = false;
            Serial.println("Streaming PARADO.");
            moduloBT.println("Arduino 1: Streaming de sensor parado.");
          }
        }

        // Comando /def
        else if (comandoRecebido == "/def") {
          isControleActive = false;  // Para o ciclo se estiver ativo
          digitalWrite(PIN_HEATER, HIGH);
          digitalWrite(PIN_FAN, LOW);

          moduloBT.println("Quantos graus para lampada ser desligada:");
          setupState = 1;              // Entra no estado de configuração
          setpointsDefinidos = false;  // Valores estão a ser redefinidos
          Serial.println("A aguardar Setpoint Alto...");
        }

        // Comando /play
        else if (comandoRecebido == "/play") {
          if (!setpointsDefinidos) {
            moduloBT.println("Erro: Defina as temperaturas com /def primeiro.");
          } else if (isControleActive) {
            moduloBT.println("Controle ja esta ativo.");
          } else {
            // Inicia o controle
            isControleActive = true;
            moduloBT.println("Controle iniciado!");
            Serial.println("Controle térmico ATIVADO.");
            


            // Lógica de arranque imediato
            Serial.println("A fazer leitura inicial do sensor...");
            float t = dht.readTemperature();
            float h = dht.readHumidity();
            if (isnan(t) || isnan(h)) {
              Serial.println("Leitura inicial falhou, a aguardar próximo ciclo.");
            } else {
              isControleActive = true;
              Serial.println("A aplicar lógica de controle inicial.");
              runControleLogic(t);  // Aplica o controle imediatamente (t) é a variavel enviada que sera relacionada com temp em runControleLogic
            }
            dhtTimer = currentMillis;  // Reseta o timer
          }
        }

        else if (comandoRecebido == "/stop_controle") {
          isControleActive = false;
          setupState = 0;  // Cancela qualquer configuração pendente
          digitalWrite(PIN_HEATER, HIGH);
          digitalWrite(PIN_FAN, LOW);
          Serial.println("Controle térmico PARADO.");
          moduloBT.println("Arduino 1: Controle térmico parado.");
        }

        else {
          moduloBT.println("Arduino 1: Comando não reconhecido.");
        }
      }
    }
  }

  // --- 2. Processador de Sensor e Controle (Temporizado) ---
  if (currentMillis - dhtTimer >= dhtInterval) {
    dhtTimer = currentMillis;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(t) || isnan(h)) {
      Serial.println("Erro de leitura do Sensor DHT11!");
      if (isStreaming) {
        moduloBT.println("Arduino 1: Erro no sensor!");
      }
      if (isControleActive) {
        digitalWrite(PIN_HEATER, HIGH);
        digitalWrite(PIN_FAN, LOW);
      }
    } else {
      if (isControleActive) {
        runControleLogic(t);
      }
      if (isStreaming) {
        enviarDadosDoSensor(t, h);
      }
    }
  }

  // --- 3. Enviar do Monitor Serial (Função de teste mantida) ---
  if (Serial.available()) {
    String msgManual = Serial.readStringUntil('\n');
    msgManual.trim();
    moduloBT.println(msgManual);
    Serial.print("Msg manual enviada: ");
    Serial.println(msgManual);
  }
}
