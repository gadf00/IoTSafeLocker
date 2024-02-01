#include <Wire.h>
#include "Servo.h"
#include <dht.h>

Servo doorServo;
#define servoPin 3
#define DHT22_pin 2

dht DHT;
String pswState = "";
int dimensionePsw = 0;
int measure;

boolean rstBtn = false;

String receivedFunction;
String receivedMessage;

void setup() {
  Wire.begin(8);                // Inizializza la comunicazione I2C e assegna l'indirizzo 8
  Wire.onReceive(receiveData);  // Definisce la funzione callback per gestire la ricezione dal master
  Wire.onRequest(sendData); // Funzione callback per inviare al master
  Serial.begin(9600);
  doorServo.attach(servoPin);
  Serial.println("INIZIALIZZAZIONE COMPLETATA.");
  delay(1000);
}

void loop() {
  // Il loop può contenere eventuali altre operazioni, ma evita di bloccare il programma con ritardi lunghi
  measure = DHT.read22(DHT22_pin);
  delay(2000);
  Serial.print("\nTemperature: ");
  Serial.print(DHT.temperature);
  Serial.print("°C");
  Serial.print("\nHumidity: ");
  Serial.print(DHT.humidity);
  Serial.print("%");

  delay(2000);
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
  if(funzione == "IMP_CHECK") {
    if(messaggio == "IMP_OK") {
      //COMUNICA A NODERED CHE IMPRONTA OK
    }
    else if (messaggio == "IMP_ER") {
      //COMUNICA A NODERED CHE IMPRONTA ER
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
      //COMUNICA A NODERED CHE DOR_OP
    }
    else if (messaggio == "DOR_ER") {
      //COMUNICA A NODERED CHE DOR_CL
    }
  }
  if(funzione == "TMP_CHECK") {
    //COMUNICA A NODERED TEMPERATURA
  }
  if(funzione == "UMH_CHECK") {
    //COMUNICA A NODERED UMIDITA'
  }
  if(funzione == "ALM_CHECK") {
    //COMUNICA A NODERED ALLARME
  }
  if(funzione == "RST_CHECK") {
    if(messaggio == "RST_OK") {
      rstBtn = true;
      //COMUNICA A NODERED CHIUSURA
    }
  }
  if(funzione == "MOT_OPENC") {
    if(messaggio == "MOT_ON") {
      //APRIRE PORTA E COMUNICARLO A NODERED
      openMotor();
    }
    if(messaggio == "MOT_OF") {
      //CHIUDERE PORTA E COMUNICALO A NODERED
      closeMotor();
    }
  }
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
  }
  else {
    pswState = "PSW_ER";
    Serial.println("La Password non Corrisponde");
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
