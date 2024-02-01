#include <Wire.h>
#include "Servo.h"
#include <DHT.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>

// DICHIARAZIONE DATI WiFi
char ssid[] = "WiFi-LabIoT";
char pass[] = "s1jzsjkw5b";
WiFiClient espClient;

// DICHIARAZIONE DATI MQTT
const char* mqttServer = "192.168.1.21"; // SERVER LABORATORIO DI IoT 
const int mqttPort = 1883; // PORTA STANDARD
const char* mqttUser = ""; //LASCIARE VUOTO SE NON SERVE
const char* mqttPassword = ""; //LASCIARE VUOTO SE NON SERVE
PubSubClient client(espClient);

// DICHIARAZIONE SERVOMOTORE
Servo doorServo;
#define servoPin 3

// DICHIARAZIONE SENSORE TEMPERATURA
#define DHT22_PIN 2
DHT dht(DHT22_PIN, DHT22);

// DICHIARAZIONE VARIABILI
String pswState = "";
int dimensionePsw = 0;
int measure;
boolean rstBtn = false;

String statoImp = "ATTESA";
String statoPsw = "ATTESA";
String statoDoor = "CHIUSA";
String statoAlarm = "CICALINO NON IN AZIONE";

String receivedFunction;
String receivedMessage;

void setup() {
  Wire.begin(8);                // Inizializza la comunicazione I2C e assegna l'indirizzo 8
  
  Wire.beginTransmission(0);
  Wire.write(0);
  Wire.endTransmission();

  Wire.onReceive(receiveData);  // Definisce la funzione callback per gestire la ricezione dal master
  Wire.onRequest(sendData); // Funzione callback per inviare al master
  Serial.begin(9600);
  connectToWiFi();
  client.setServer(mqttServer, mqttPort);
  connectToMQTT();  // Muovi la connessione MQTT nel setup
  dht.begin();
  doorServo.attach(servoPin);
  closeMotor();
  Serial.println("INIZIALIZZAZIONE COMPLETATA.");
  delay(1000);
}

void connectToWiFi() {
  Serial.print("Connessione alla rete WiFi ");
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Connessione WiFi stabilita");
  Serial.print("Indirizzo IP: ");
  Serial.println(WiFi.localIP());
}

void connectToMQTT() {
  if (!client.connected()) {
    Serial.print("Tentativo di connessione al server MQTT...");
    while (!client.connect("ArduinoClient", mqttUser, mqttPassword)) {
      Serial.print(".");
      delay(5000);
    }

    if (client.connected()) {
      Serial.println(" Connesso al server MQTT");
    } else {
      Serial.println(" Connessione MQTT fallita");
    }
  }
}

void loop() {
  // Leggi temperatura e umidità dal sensore
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Verifica se la lettura è avvenuta con successo
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Errore nella lettura del sensore DHT!");
  } else {
    // Stampa i dati sulla seriale
    Serial.print("Temperatura: ");
    Serial.print(temperature);
    Serial.println(" °C");

    Serial.print("Umidità: ");
    Serial.print(humidity);
    Serial.println(" %");

    inviaMQTT_NodeRed("secureBox_temperatura", String(temperature));
    inviaMQTT_NodeRed("secureBox_umidita", String(humidity));
    inviaMQTT_NodeRed("secureBox_impronta", statoImp);
    inviaMQTT_NodeRed("secureBox_pswCheck", statoPsw);
    inviaMQTT_NodeRed("secureBox_porta", statoDoor);
    inviaMQTT_NodeRed("secureBox_allarme", statoAlarm);
  }

  // Aggiungi la gestione della connessione MQTT qui
  connectToMQTT();
  client.loop();  // Mantieni la connessione MQTT aperta

  delay(2000);
}

void inviaMQTT_NodeRed(String mqttTopic, String value) {
  connectToMQTT();
  client.publish(mqttTopic.c_str(), value.c_str());
  Serial.print("Inviato MQTT: ");
  Serial.print(mqttTopic);
  Serial.print("   Topic: ");
  Serial.print(value);
  Serial.println(";");
  // Non disconnettere qui
  //client.disconnect();
  delay(500);
}

void receiveData(int byteCount) {
  int dimData = 16;
  if (dimensionePsw > 0) dimData = dimensionePsw + 10;
  char receivedData[dimData];
  int index = 0;
  while (Wire.available()) {
    receivedData[index++] = Wire.read();
  }
  receivedData[index] = '\0';
  String receivedString = byteArrayToString(receivedData, sizeof(receivedData));
  receivedFunction = extractField(receivedString, '-', 0);
  receivedMessage = extractField(receivedString, '-', 1);
  Serial.print("FUNZIONE RICHIESTA: ");
  Serial.println(receivedFunction);
  Serial.print("MESSAGGIO: ");
  Serial.println(receivedMessage);
  if (dimensionePsw > 0) {
    dimensionePsw = 0;
    dimData = 16;
  }
  receiveFramework_slv(receivedFunction, receivedMessage);
}

void sendData() {
  if (receivedFunction.equals("PSW_CHECK")) {
    String response = String("PSW_CHECK-") + String(pswState);
    byte byteResponse[16];
    stringToByteArray(response, byteResponse, sizeof(byteResponse));
    Wire.write(byteResponse, sizeof(byteResponse));
    Serial.println("MESSAGGIO INVIATO AL SERVER");
    delay(500);
  }
  else Serial.println("FUNZIONE NON SUPPORTATA");
}

void receiveFramework_slv(String funzione, String messaggio) {
  // FRAMEWORK, MESSAGGI SUPPORTATI
  // 1 - IMP_CHECK - IMPRONTA
  // 2 - PSW_CHECK - PASSWORD
  // 3 - DOR_CHECK - PORTA
  // 4 - TMP_CHECK - TEMPERATURA
  // 5 - UMH_CHECK - UMIDITA'
  // 6 - ALM_CHECK - ALLARME SONORO
  // 7 - RST_CHECK - RESET
  // 8 - MOT_OPENC - CONTROLLO MOTORE

  // FRAMEWORK, MESSAGGI POSSIBILI
  // 1 - IMP_OK IMP_ER
  // 2 - PSW
  // 3 - DOR_OP DOR_CL
  // 4 - TEMPERATURE
  // 5 - UMIDITY
  // 6 - ALM_ON ALM_OF
  // 7 - RST_OK
  // 8 - MOT_ON, MOT_OF
  /*if(funzione.equals("IMP_CHECK")) {
    if(messaggio.equals("IMP_OK")) {
      statoImp = "RICONOSCIUTA";
    }
    else if (messaggio.equals("IMP_ER")) {
      statoImp = "NON RICONOSCIUTA";
    }
  }
  if(funzione == "PSW_CHECK") {
    checkPassword(messaggio);
  }
  if(funzione == "PSW_DIMEN") {
    dimensionePsw = messaggio.toInt();
  }
  if(funzione == "DOR_CHECK") {
    if(messaggio == "DOR_OP") {
      statoDoor = "APERTA";
    }
    else if (messaggio == "DOR_CL") {
      statoDoor = "CHIUSA";
    }
  }
  if(funzione == "TMP_CHECK") {
    // 
  }
  if(funzione == "UMH_CHECK") {
    //
  }
  if(funzione == "ALM_CHECK") {
    if(messaggio == "ALM_ON") {
      statoAlarm = "CICALINO IN AZIONE";
    }
    if(messaggio == "ALM_OF") {
      statoAlarm = "CICALINO NON IN AZIONE";
    }
  }
  if(funzione == "RST_CHECK") {
    if(messaggio == "RST_OK") {
      rstBtn = true;
      statoImp = "ATTESA";
      statoPsw = "ATTESA";
      statoDoor = "CHIUSA";
      statoAlarm = "CICALINO NON IN AZIONE";
    }
  }
  if(funzione == "MOT_OPENC") {
    if(messaggio == "MOT_ON") {
      openMotor();
    }
    if(messaggio == "MOT_OF") {
      closeMotor();
    }
  }*/
}

void openMotor() {
  doorServo.write(90);
  Serial.println("MOTORE SU APERTO.");
}

void closeMotor() {
  doorServo.write(180);
  Serial.println("MOTORE SU CHIUSO.");
}

void checkPassword(String psw) {
  if(psw == "123AB") {
    pswState = "PSW_OK";
    Serial.println("La Password Corrisponde");
    statoPsw = "PASSWORD RICONOSCIUTA";
  }
  else {
    pswState = "PSW_ER";
    Serial.println("La Password non Corrisponde");
    statoPsw = "PASSWORD NON RICONOSCIUTA";
  }
}

// Funzione per estrarre un campo da una stringa
String extractField(String input, char separator, int index) {
  int separatorCount = 0;
  int startIndex = 0;

  for (int i = 0; i <= input.length(); i++) {
    if (input[i] == separator || i == input.length()) {
      separatorCount++;

      if (separatorCount == index + 1) {
        return input.substring(startIndex, i);
      }

      startIndex = i + 1;
    }
  }
  // Se l'indice richiesto è troppo grande o il separatore non è presente, restituisci una stringa vuota
  return "";
}

// Funzione per convertire un array di byte in una String
String byteArrayToString(byte* array, int length) {
  String result = "";
  for (int i = 0; i < length; i++) {
    result += char(array[i]);
  }
  return result;
}

// Funzione per convertire una String in un array di byte
void stringToByteArray(String input, byte* output, int maxLength) {
  for (int i = 0; i < maxLength && i < input.length(); i++) {
    output[i] = input.charAt(i);
  }
}
