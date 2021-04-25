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
bool armarActiva = false;

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

    if (text == "/start") {
      String welcome = "Bienvenido, " + from_name + ".\n";
      welcome += "Ingrese /comenzar para registrar el inico de su turno. \n";
      welcome += "Ingrese /finalizar para registrar el fin de su turno. \n";
      welcome += "Ingrese /reconocer para registrar que ha visualizado una alerta. \n";
      welcome += "Ingrese /complatado para registrar que ha completado el cambio. \n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }

    if (text == "/comenzar") {
      // mandar a la api que esta disponible
    }

    if (text == "/finalizar") {
      // mandar a la api que no esta disponible
    }

    if (text == "/reconocer") {
      // mandar ack
    }

    if (text == "/complatado") {
      // mandar completado
    }
  }
}

void enviarCorreo(String asunto, String mensaje, String destinatario) {
  //host, puerto, cuenta y contraseña
  datosSMTP.setLogin("smtp.gmail.com", 465, "federico.santos@davinci.edu.ar", "Tatifede21092");
  // Remitente
  datosSMTP.setSender("ESP32S", "fede@gmail.com");
  datosSMTP.setPriority("High");
  datosSMTP.setSubject(asunto);
  datosSMTP.setMessage(mensaje, false);
  // destinatarios
  datosSMTP.addRecipient(destinatario);
  

  //Comience a enviar correo electrónico.
  if (!MailClient.sendMail(datosSMTP))
    Serial.println("Error enviando el correo, " + MailClient.smtpErrorReason());
  //Borrar todos los datos del objeto datosSMTP para liberar memoria
  datosSMTP.empty();
}

void notificarAlarmas() {
  Serial.println("Alarma");
  if (!armarActiva) {
    armarActiva = true;
  }

  int alerta = 0;
  int cantidadDestinatarios = 2;
  // HTTP para obtener la alerta
  //alertData = api respuesta

  switch (alerta) {
    case 1:
      // se detecta una alerta por primera vez
      while (cantidadDestinatarios) {
        cantidadDestinatarios = cantidadDestinatarios - 1;
        //  bot.sendMessage(alertData.destinatarios[cantidadDestinatarios].id, "El dispositivo " + idDispositivo + " necesita una recarga", "Markdown");
      }
      break;

    case 2:
      // la alerta ya fue notificada pero nadie la reconocio
      while (cantidadDestinatarios) {
        cantidadDestinatarios = cantidadDestinatarios - 1;
        //  enviarCorreo("Nadie a Respondio a las alertas", "Se han registrado alertas pero nadie las ha reconocido", alertData.destinatarios[cantidadDestinatarios].email )
        //  bot.sendMessage(alertData.destinatarios[cantidadDestinatarios].id, "El dispositivo " + idDispositivo + " necesita una recarga", "Markdown");
      }
    break;

    case 3:
      // la alerta ya fue reconociada pero no resulta
      while (cantidadDestinatarios) {
        cantidadDestinatarios = cantidadDestinatarios - 1;
        //  enviarCorreo("Nadie a Respondio a las alertas", "Se han registrado alertas pero nadie las ha reconocido", alertData.destinatarios[cantidadDestinatarios].email )
        //  bot.sendMessage(alertData.destinatarios[cantidadDestinatarios].id, "El dispositivo " + idDispositivo + " necesita una recarga", "Markdown");
      }
    break;
  }
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

  // ultrasonido de bomba
  digitalWrite(ONBOARD_LED, LOW);
  digitalWrite(sensorBombaTrig, HIGH);
  delay(1);
  digitalWrite(sensorBombaTrig, LOW);

  sensorBombaDuracion = pulseIn(sensorBombaEcho, HIGH);
  sensorBombaDistancia = convertirCM(sensorBombaDuracion);

  //ultrasonido de nivel
  digitalWrite(sensorNivelTrig, HIGH);
  delay(1);
  digitalWrite(sensorNivelTrig, LOW);

  sensorNivelDuracion = pulseIn(sensorNivelEcho, HIGH);
  sensorNivelDistancia = convertirCM(sensorNivelDuracion);

  Serial.println(sensorNivelDistancia);

  if (sensorNivelDistancia < minBombaSensorPermitido) {
    stockDisponible = false;
  } else {
    stockDisponible = true;
  }

  if ((sensorBombaDistancia >= minBombaSensor || sensorBombaDistancia < maxBombaSensor) && stockDisponible ) {
    digitalWrite(ONBOARD_LED, HIGH);
    digitalWrite(pinMoffet, LOW);

  } else {
    digitalWrite(ONBOARD_LED, LOW);
    digitalWrite(pinMoffet, HIGH);
  }

  // valdamos nivel
  if (stockDisponible && sensorNivelDistancia < 10) {
    notificarAlarmas();
  }

  delay(3000);
}
