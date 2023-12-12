/**
 * Created by K. Suwatchai (Mobizt)
 *
 * Email: k_suwatchai@hotmail.com
 *
 * Github: https://github.com/mobizt/Firebase-ESP8266
 *
 * Copyright (c) 2023 mobizt
 *
 */

/** Esse exemplo mostra o acesso ao Realtime Database (RTDB) em modo teste (sem autenticação) para envio de dados do sensor DHT11 (temperatura e umidade).
 *  Além disso, o sistema permite o envio do status de um led conectado a uma porta digital, bem como o recebimento de comandos de acionamento.
 *  
 *  Exemplo modificado por: Lucas Mendes
 *  
 *  Email: lucas.mendes.lfm@gmail.com
 *  
 *  Github: 
 *  
 */

#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

// DHT11
#include <DHT.h> // Inclui a biblioteca DHT Sensor Library

#define DHTPIN 32 // Pino digital 32 conectado ao DHT11
#define DHTTYPE DHT11 //DHT 11

/* Definição das credenciais Wi-Fi */
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

/* Definição da URL do Realtime Database no Firebase */
#define DATABASE_URL "" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* Definição do objeto de dados Firebase */
FirebaseData fbdo;

/* Definição dos dados de autenticação */
FirebaseAuth auth;

/* Definição dos dados de configuração */
FirebaseConfig config;

/* variável para controle do intervalo entre requisições (evitando o uso de delay) */
unsigned long dataMillis = 0;

/* Definição dos pinos para o led e o botão de acionamento */
int LED = 25;
int BTN = 34;

int estadoLed; // Guarda o último estado estável do led;
bool stable;    // Guarda o último estado estável do botão;
bool unstable;  // Guarda o último estado instável do botão;
uint32_t bounce_timer; // Timer para checar o efeito de trepidação do botão

DHT dht(DHTPIN, DHTTYPE); // Inicializando o objeto dht do tipo DHT passando como parâmetro o pino (DHTPIN) e o tipo do sensor (DHTTYPE)

/* Função para verificar se houve de fato uma mudança estável no estado do botão.
 *  Essa verificação é necessária para evitar problemas relacionados ao efeito de trepidação no acionamento do botão.
 *  Teste intervalos de tempo diferentes para chegar ao valor que melhor se ajuste ao seu caso. Esse exemplo utiliza um intervalo de 10 ms.
*/
bool changed() {
  bool now = digitalRead(BTN);   // Lê o estado atual do botão;
  if (unstable != now) {       // Checa se houve mudança;
    bounce_timer = millis();   // Atualiza timer;
    unstable = now;            // Atualiza estado instável;
  }
  else if (millis() - bounce_timer > 10) {  // Checa o tempo de trepidação acabou (10 ms);
    if (stable != now) {           // Checa se a mudança ainda persiste;
      stable = now;                  // Atualiza estado estável;
      return 1;
    }
  }
  return 0;
}

void setup()
{

    Serial.begin(115200);

    pinMode(LED, OUTPUT);
    pinMode(BTN, INPUT);

    dht.begin();//Inicializa o sensor DHT11

    stable = digitalRead(BTN);
    estadoLed = 0;

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Conectando ao Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Conectado com o IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    Serial.printf("Cliente Firebase v%s\n\n", FIREBASE_CLIENT_VERSION);

    /* Assign the certificate file (optional) */
    // config.cert.file = "/cert.cer";
    // config.cert.file_storage = StorageType::FLASH;

    /* Assign the database URL(required) */
    config.database_url = DATABASE_URL;

    config.signer.test_mode = true;

    /**
     Set the database rules to allow public read and write.

       {
          "rules": {
              ".read": true,
              ".write": true
          }
        }

    */

    // Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
    Firebase.reconnectNetwork(true);

    // Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
    // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
    fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

    /* Inicialização da biblioteca com dados de aconfiguração e autenticação */
    Firebase.begin(&config, &auth);
}

void loop()
{
    if (changed()) {
      if(stable){
        digitalWrite(LED, !estadoLed);
        estadoLed = !estadoLed;
        
        // Ao detectar uma mudança no estado do botão, realiza o acionamento do led (apaga ou acende) e envia o status e o comando ao RTDB 
        Serial.printf("Set int... %s\n", Firebase.setInt(fbdo, "/led_1", estadoLed) ? "ok" : fbdo.errorReason().c_str());
        Serial.printf("Set int... %s\n", Firebase.setInt(fbdo, "/c_led_1", estadoLed) ? "ok" : fbdo.errorReason().c_str());
      }
    }

    // Intervalo de 2 segundos para o envio dos dados
    if (millis() - dataMillis > 2000)
    {
        dataMillis = millis();
        int iVal = 0;
        // Verifica se houve envio de comando remoto de acionamento do Led 
        Serial.printf("Get int ref... %s\n", Firebase.getInt(fbdo, F("/c_led_1"), &iVal) ? String(iVal).c_str() : fbdo.errorReason().c_str());

        // Caso o comando enviado represente um estado diferente ao qual o led se encontra, modifica o estado do led e confirma o novo estado no RTDB
        if(estadoLed != iVal){
          digitalWrite(LED, iVal);
          estadoLed = !estadoLed;
          Serial.printf("Set int... %s\n", Firebase.setInt(fbdo, "/led_1", estadoLed) ? "ok" : fbdo.errorReason().c_str());
        }

        float h = dht.readHumidity(); // lê o valor da umidade e armazena na variável h do tipo float (aceita números com casas decimais)
        float t = dht.readTemperature(); // lê o valor da temperatura e armazena na variável t do tipo float (aceita números com casas decimais)
        
        if (isnan(h) || isnan(t)) { // Verifica se a umidade ou temperatura são ou não um número
          return; // Caso não seja um número, retorna
        }

        // Imprime os dados do sensor no monitor serial
        Serial.print("Umidade: ");
        Serial.print(h);
        Serial.println("%"); 
        Serial.print("Temperatura: "); 
        Serial.print(t);
        Serial.println("°C "); 
        
        // enviando dados de temperatura e umidade ao RTDB
        Serial.printf("Set temp... %s\n", Firebase.setInt(fbdo, "/temp", t) ? "ok" : fbdo.errorReason().c_str());
        Serial.printf("Set umidade... %s\n", Firebase.setInt(fbdo, "/umidade", h) ? "ok" : fbdo.errorReason().c_str());
    }
    
}
