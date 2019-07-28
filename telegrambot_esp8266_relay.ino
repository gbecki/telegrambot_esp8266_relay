/*
 * Author: Guilherme Becki - PU5KNB - pu5knb@qsl.net
 */
#include <ESP8266WiFi.h>        
#include <WiFiClientSecure.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <UniversalTelegramBot.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

//Define pressao atmosferica (varia com a altitude do local)
#define SEALEVELPRESSURE_HPA (1013.25)

ADC_MODE(ADC_VCC);

//Cria o objeto de webserver escutando na porta 80 TCP
ESP8266WebServer server(80);

//Pino onde está o Relê que chaveia a antena
#define RELAY_PIN 2

//Intervalo entre as checagens de novas mensagens no Telegram
#define INTERVAL 1000

//Token do seu Bot. Troque pela mostrada no BotFather
#define BOT_TOKEN "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

//Troque pelo ssid e senha da sua rede WiFi
const char* ssid = "XXXXXXXXXXXXXX";
const char* password = "XXXXXXXXXX";

//Comandos aceitos via Telegram
const String ANTENA = "ant";
const String STATS = "status";
const String START = "/start";

//Objeto que realiza a leitura da temperatura e umidade
Adafruit_BME280 bme;
float temperature, humidity, pressure, altitude;

//Cliente para conexões seguras
WiFiClientSecure net_ssl;
//Objeto com os métodos para comunicarmos pelo Telegram
UniversalTelegramBot bot(BOT_TOKEN, net_ssl);
//Tempo em que foi feita a última checagem
uint32_t lastCheckTime = 0;

void setup()
{
  Serial.begin(74880);
  Serial.println();

  bme.begin(0x76);

  WiFi.begin(ssid, password);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  
  //Variavel de tensao
  float v=0.00f;
  v = ESP.getVcc();

  //Saidas de status no console serial
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Signal strength: ");
  Serial.print(WiFi.RSSI());
  Serial.println("dBm");
  Serial.print("Battery: ");
  Serial.print(v/1024.00f);
  Serial.println(" V");
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  //Inicia o mDNS repondendo por esp8266relay.local
  if (MDNS.begin("esp8266relay")) {
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  server.on("/metrics", HTTP_GET, handleExporter); //Chama a funcao 'handleExporter' quando o cliente solicitar a URI "/metrics"
  server.onNotFound(handleNotFound); //Quando o cliente solicitar uma URI diferente (i.e. algo diferente de "/metrics"), chama a funcao "handleNotFound"

  server.begin(); //Inicia servidor http
  Serial.println("HTTP server started");
}

void handleNewMessages(int numNewMessages)
{
  for (int i=0; i<numNewMessages; i++) //Para cada mensagem nova
  {
    String chatId = String(bot.messages[i].chat_id); //ID do chat  
    String text = bot.messages[i].text; //Texto que chegou

    if (text.equalsIgnoreCase(START))
    {
      handleStart(chatId, bot.messages[i].from_name); //Mostra as opções
    }
    else if(text.equalsIgnoreCase(ANTENA))
    {
     handleSwitchAntenna(chatId); //Ativa o relê
    }
    else if (text.equalsIgnoreCase(STATS))
    {
      handleStatus(chatId); //Envia info do ESP, temperatura, umidade e pressão
    }
  }
}

void handleStart(String chatId, String fromName)
{
  //Mostra Olá e o nome do contato seguido das mensagens válidas
  String message = "<b>Olá " + fromName + ".</b>\n";
  message += getCommands();
  bot.sendMessage(chatId, message, "HTML");
}

String getCommands()
{
  //String com a lista de mensagens que são válidas e explicação sobre o que faz
  String message = "Os comandos disponíveis são:\n\n";
  message += "<b>" + ANTENA + "</b>: Para chavear a antena.\n";
  message += "<b>" + STATS + "</b>: Para verificar o estado do ESP e clima.";
  return message;
}

void handleSwitchAntenna(String chatId)
{
  //Muda o estado do rele e envia mensagem confirmando 
  digitalWrite(RELAY_PIN, LOW);
  delay(500);
  digitalWrite(RELAY_PIN, HIGH);  
  bot.sendMessage(chatId, "A Antena foi <b>alterada</b>", "HTML");
}

String getPowerClimateMessage()
{
  //Faz a leitura de dados do ESP e climaticos
  float v=0.00f;
  v = ESP.getVcc();
  int sinal = WiFi.RSSI();
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;

  {
    //Retorna uma string com os valores
    String message = "";
    message += "A temperatura é de " + String(temperature, 1)+ " °C\n";
    message += "A umidade é de " + String(humidity, 0) + "%\n";
    message += "A pressão é de " + String(pressure, 1) + " mbar\n";
    message += "A tensão da bateria é " + String(v/1024.00f)+ " V\n";
    message += "A intensidade do sinal é " + String(sinal) + "dBm";
    return message;
  }
}

void handleStatus(String chatId)
{
  String message = "";

  //Adiciona à mensagem o valor da temperatura e umidade
  message += getPowerClimateMessage();

  //Envia a mensagem para o contato
  bot.sendMessage(chatId, message, "");
}

// Quando a URI /metrics for requisitada via webserver, envia uma pagina contendo metricas prometheus
void handleExporter() {
  
  //Faz a leitura de dados do ESP e climaticos
  float voltage=0.00f;
  voltage = ESP.getVcc();
  int sinal = WiFi.RSSI();
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure();

  //Envia valores no formato de metricas Prometheus
  String reply = "";
  reply += "# HELP esp8266_bm280_temperature Reads Celsius temperature from BME280 sensor\n";
  reply += "# TYPE esp8266_bm280_temperature gauge\n";
  reply += "esp8266_bm280_temperature "+ String(temperature)+ "\n";
  reply += "# HELP esp8266_bm280_humidity Reads humidity percentile from BME280 sensor\n";
  reply += "# TYPE esp8266_bm280_humidity gauge\n";
  reply += "esp8266_bm280_humidity "+ String(humidity)+ "\n";
  reply += "# HELP esp8266_bm280_pressure Reads atmospheric pressure from BME280 sensor\n";
  reply += "# TYPE esp8266_bm280_pressure gauge\n";
  reply += "esp8266_bm280_pressure "+ String(pressure)+ "\n";
  reply += "# HELP esp8266_voltage Reads voltage aplied to esp8266 device\n";
  reply += "# TYPE esp8266_voltage gauge\n";
  reply += "esp8266_voltage "+ String(voltage)+ "\n";
  reply += "# HELP esp8266_rssi Reads wifi intensity from esp8266 conectivity\n";
  reply += "# TYPE esp8266_rssi gauge\n";
  reply += "esp8266_rssi "+ String(sinal)+ "\n";
  server.send(200, "text/html",reply);
}

// Envia HTTP status 404 (Not Found) qundo nao ha Handler para atender a requisicao
void handleNotFound(){
  server.send(404, "text/plain", "404: Not found");
}

void loop()
{
  //Tempo agora desde o boot
  uint32_t now = millis();

  // Inicia o servidor para receber requisições dos clientes
  server.handleClient();

  //Se o tempo passado desde a última checagem for maior que o intervalo determinado
  if (now - lastCheckTime > INTERVAL) 
  {
    //Coloca o tempo de útlima checagem como agora e checa por mensagens
    lastCheckTime = now;
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    handleNewMessages(numNewMessages);
  }
}
