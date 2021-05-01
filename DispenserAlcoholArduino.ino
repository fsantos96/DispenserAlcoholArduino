#include "ESP32_MailClient.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <UniversalTelegramBot.h>
#include "config.h";

WiFiClientSecure client;
SMTPData datosSMTP;
UniversalTelegramBot bot(BOTtoken, client);
unsigned long lastTimeBotRan;
int sensorBombaDuracion;
int sensorBombaDistancia;
int sensorNivelDuracion;
int sensorNivelDistancia;
bool stockDisponible = true;
bool alarmaActiva = false;
int cantidadDeDestinatarios = 0;

void setup()
{
  pinMode(pinMoffet, OUTPUT);
  pinMode(sensorBombaEcho, INPUT);
  pinMode(sensorNivelTrig, OUTPUT);
  pinMode(sensorNivelEcho, INPUT);
  pinMode(sensorBombaTrig, OUTPUT);
  pinMode(ONBOARD_LED, OUTPUT);

  Serial.begin(9600);
  client.setInsecure();

  delay(100);

  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(10);
}

void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";

    HTTPClient http;
    if (text == "/start") {
      String welcome = "Bienvenido, " + from_name + ".\n";
      welcome += "Ingrese /comenzar para registrar el inico de su turno. \n";
      welcome += "Ingrese /finalizar para registrar el fin de su turno. \n";
      welcome += "Ingrese /reconocer para registrar que ha visualizado una alerta. \n";
      welcome += "Ingrese /completado para registrar que ha completado el cambio. \n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }

    if (text == "/comenzar") {
      http.begin(baseApi + "/employee/start?employeeId=" + chat_id + "&deviceId=" + idDispositivo);
      http.addHeader("Content-Type", "application/json");
      int httpCode = http.POST("{\"employee\":{\"id\": \""+chat_id + "\", \"name\":\"" + from_name + "\", \"enabled\": \"true\"}}");
      bot.sendMessage(chat_id, "Que tengas una buena jornada " + from_name , "Markdown");
    }

    if (text == "/finalizar") {
      http.addHeader("Content-Type", "application/json");
      http.begin(baseApi + "/employee/end?employeeId=" + chat_id);
      int httpCode = http.POST("{}");
      bot.sendMessage(chat_id, "Que descanses! " + from_name , "Markdown");
    }

    if (text == "/reconocer") {
      http.begin(baseApi + "/employee/ack?employeeId="+ chat_id + "&deviceId="+ idDispositivo);
      //httpCode = http.POST();
      int httpCode = http.GET();
      Serial.println("http");
      Serial.println(httpCode);

      DynamicJsonDocument doc(8024);
      DeserializationError error = deserializeJson(doc, http.getStream());
      if (error) {
        Serial.print(F("deserializeJson() reconocer: "));
        Serial.println(error.f_str());
      }

      cantidadDeDestinatarios = doc["amountEmployee"];
      Serial.println(cantidadDeDestinatarios);
      for(int i = 0; i < cantidadDeDestinatarios; i++) {   
        String idEmpleadoChat = doc["employees"][i]["id"].as<String>();
        bot.sendMessage(idEmpleadoChat,  from_name + " reconocio la alerta de " + idDispositivo , "Markdown"); 
      }
    }
    
    if (text == "/completado") {
      http.addHeader("Content-Type", "application/json");
      http.begin(baseApi + "/employee/done?employeeId="+ chat_id + "&deviceId="+ idDispositivo);
      int httpCode = http.POST("{}");
      
      DynamicJsonDocument doc(8024);
      DeserializationError error = deserializeJson(doc, http.getStream());
      if (error) {
        Serial.print(F("deserializeJson() reconocer: "));
        Serial.println(error.f_str());
      }

      cantidadDeDestinatarios = doc["amountEmployee"];

      for(int i = 0; i < cantidadDeDestinatarios; i++) {   
        String idEmpleadoChat = doc["employees"][i]["id"].as<String>();
        bot.sendMessage(idEmpleadoChat,  from_name + " completo la recarga de " + idDispositivo , "Markdown"); 
      }

      alarmaActiva = false;
      stockDisponible = true;
    }

    http.end();
  }
}

void enviarCorreo(String asunto, String mensaje, String destinatario) {
  //host, puerto, cuenta y contraseña
  datosSMTP.setLogin("smtp.gmail.com", 465, "federico.santos@davinci.edu.ar", "Tatifede21092");
  // Remitente
  datosSMTP.setSender("ESP32S", "fede@gmail.com");
  //Armando del mail
  datosSMTP.setPriority("High");
  datosSMTP.setSubject(asunto);
  datosSMTP.setMessage(mensaje, false);
  // destinatarios
  datosSMTP.addRecipient(destinatario);
  
  //Comience a enviar correo electrónico.
  if (!MailClient.sendMail(datosSMTP)) {
   Serial.println("Error enviando el correo, " + MailClient.smtpErrorReason()); 
  }
  
  //Borrar todos los datos del objeto datosSMTP para liberar memoria
  datosSMTP.empty();
}

void notificarAlarmas() {
  Serial.println("Alarma");
  int alerta = 0;
  HTTPClient http;

  http.begin(baseApi + "/employee/alert/?alarmaActiva="+ alarmaActiva + "&deviceId="+ idDispositivo);
  int httpCode = http.GET();
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, http.getStream());
  if (error) {
    Serial.print(F("deserializeJson() reconocer: "));
    Serial.println(error.f_str());
  }

  cantidadDeDestinatarios = doc["amountManagers"];
  alerta = doc["alert"];
  switch (alerta) {
    case 1:
      for(int i = 0; i < cantidadDeDestinatarios; i++) {
         bot.sendMessage(doc["managers"][i]["id"].as<String>(), "El dispositivo " + idDispositivo + " necesita ser recargado" , "Markdown");
      }

      alarmaActiva = true;
      break;
    case 2:
      for(int i = 0; i < cantidadDeDestinatarios; i++) {
       enviarCorreo("Nadie a Respondio a las alertas", "Se han registrado alertas en el dispositivo " + idDispositivo + " pero nadie las ha reconocido", doc["managers"][i]["email"].as<String>() );
      }
    break;

    case 3:
      for(int i = 0; i < cantidadDeDestinatarios; i++) {
       enviarCorreo("La alerta no fue resuelta", doc["employeeACK"].as<String>() + " reconocio la alerta del dispositivo pero no efectuo el cambio", doc["managers"][i]["email"].as<String>());
      }
    break;
  }

  http.end();
}

int convertirCM(int medida) {
  return medida / 58.2;
}

void loop()
{
  // escucha los mensajes de telegrm
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    lastTimeBotRan = millis();
  }

//  // ultrasonido de bomba
//  digitalWrite(ONBOARD_LED, LOW);
//  digitalWrite(sensorBombaTrig, HIGH);
//  delay(1);
//  digitalWrite(sensorBombaTrig, LOW);
//
//  sensorBombaDuracion = pulseIn(sensorBombaEcho, HIGH);
//  sensorBombaDistancia = convertirCM(sensorBombaDuracion);
//
//  //ultrasonido de nivel
//  digitalWrite(sensorNivelTrig, HIGH);
//  delay(1);
//  digitalWrite(sensorNivelTrig, LOW);
//
//  sensorNivelDuracion = pulseIn(sensorNivelEcho, HIGH);
//  sensorNivelDistancia = convertirCM(sensorNivelDuracion);
//
//  Serial.println(sensorNivelDistancia);
//
//  if (sensorNivelDistancia < minBombaSensorPermitido) {
//    stockDisponible = false;
//  } else {
//    stockDisponible = true;
//  }
//
//  if ((sensorBombaDistancia >= minBombaSensor || sensorBombaDistancia < maxBombaSensor) && stockDisponible ) {
//    digitalWrite(ONBOARD_LED, HIGH);
//    digitalWrite(pinMoffet, LOW);
//
//  } else {
//    digitalWrite(ONBOARD_LED, LOW);
//    digitalWrite(pinMoffet, HIGH);
//  }
//
//  // valdamos nivel
//  if (stockDisponible && sensorNivelDistancia < 10) {
      notificarAlarmas();  
//  }
//
//  delay(3000);
}
