#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>

#define mySerial Serial1

// DICHIARAZIONE PIN 
const byte LED_GREEN_PIN = 25;
const byte LED_RED_PIN = 27;
const byte BTN_GREEN_PIN = 31;
const byte BTN_RED_PIN = 2;
const byte BUZZER_PIN = 33;
const byte DOOR_PIN = 3;

// DICHIARAZIONE TASTIERINO
const byte ROW_NUM    = 4; // quattro righe
const byte COLUMN_NUM = 4; // quattro colonne
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte pin_rows[ROW_NUM] = {36, 34, 32, 30}; // Collegherai questi ai pin 7-10
byte pin_column[COLUMN_NUM] = {28, 26, 24, 22}; // Collegherai questi ai pin 3-6
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

// DICHIARAZIONE LETTORE IMPRONTE
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// DICHIARAZIONE LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Cambia 0x27 con l'indirizzo I2C corretto per il tuo schermo

// DICHIARAZIONE VARIABILI UTILI
int tentativiImp = 5;
int tentativiPsw = 5;
bool doorLocked = true;
bool firstWrite = true;
bool impOk = false;
bool pswOk = false;
bool login = false;
bool pswRicevuta = false;
// String doorCheck = "";
// String tempCheck = "";
// String umiCheck = "";
// String alarmCheck = "";

// DICHIARAZIONE VARIABILI PER ATTACHINTERRUPT
volatile bool rstPressedbool = false;
volatile bool doorOpenedbool = false;
unsigned long lastDebounceTime_rst = 0;
unsigned long lastDebounceTime_door_o = 0;
unsigned long debounceDelay = 75;
int contRst = 0;
int contDoor = 0;

// INIZIO PROGRAMMA
void setup() {
  Wire.begin(); // DICHIARAZIONE I2C
  Serial.begin(9600); // DICHIARAZIONE SERIALE
  while (!Serial);
  finger.begin(57600); // INIZIALIZZAZIONE LETTORE DI IMPRONTE
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Sensore di impronte rilevato;");
  } else {
    Serial.println("Sensore di impronte non rilevato;");
    while (1) { delay(1); }
  }
  lcd.init(); // INIZIALIZZAZIONE LCD
  lcd.begin(20, 4);
  lcd.backlight();
  lcd.clear();

  // INIZIALIZZAZIONE PIN DISPOSITIVI
  pinMode(LED_GREEN_PIN, OUTPUT); 
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_GREEN_PIN, INPUT_PULLUP);
  pinMode(BTN_RED_PIN, INPUT_PULLUP);
  pinMode(DOOR_PIN, INPUT_PULLUP);

  // DICHIARAZIONE DEGLI INTERRUPT
  attachInterrupt(digitalPinToInterrupt(BTN_RED_PIN), rstPressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(DOOR_PIN), doorStatusChanged, CHANGE);
}

// CATCH DEL PULSANTE RESET PREMUTO
void rstPressed() {
  unsigned long currentMillis = millis();
  if(currentMillis - lastDebounceTime_rst > debounceDelay) {
    lastDebounceTime_rst = currentMillis;
    rstPressedbool = true;
    contRst++;
    Serial.print("BOTTONE RESET PREMUTO: ");
    Serial.println(contRst);
  }
}

// CATCH DEL CAMBIO STATO PORTA
void doorStatusChanged() {
  unsigned long currentMillis = millis();
  if(currentMillis - lastDebounceTime_door_o > debounceDelay) {
    lastDebounceTime_door_o = currentMillis;
    if(digitalRead(DOOR_PIN) == HIGH) {
      Serial.println("PORTA APERTA");
      //sendFramework_srv(3, "DOR_OP");
      doorOpenedbool = true;
    }
    if(digitalRead(DOOR_PIN) == LOW) {
      Serial.println("PORTA CHIUSA");
      //sendFramework_srv(3, "DOR_CL");
      doorOpenedbool = false;
    }
  }
}

// PROMPT DI LOGIN
boolean faseLogin() {
  lcd.clear();
  lcd.print("     Benvenuto!     ");
  lcd.setCursor(0,1);
  lcd.print("   Secure Box n.1   ");
  lcd.setCursor(0,3);
  lcd.print(" Verde per iniziare ");
  if(doorLocked){
    digitalWrite(LED_GREEN_PIN,LOW);
    digitalWrite(LED_RED_PIN,HIGH);
  }
  else{
    digitalWrite(LED_RED_PIN,LOW);
    digitalWrite(LED_GREEN_PIN,HIGH);
  }
  while((digitalRead(BTN_GREEN_PIN) == HIGH) && !doorOpenedbool) {
    // || (digitalRead(BTN_GREEN_PIN) == LOW && firstWrite && !doorOpenedbool))
    delay(50);
  }
  firstWrite = false;
  if(rstPressedbool) {
    Serial.println("INTERROMPO OPERAZIONE, RST PREMUTO DOPO INIZIO.");
    return false;
  }
  if(doorOpenedbool) {
    Serial.println("INTERROMPO OPERAZIONE, ALLARME PORTA APERTA.");
    return false;
  }
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("      Inserire      ");
  lcd.setCursor(0,1);
  lcd.print(" Impronta  Digitale ");
  while(!checkImp() && !impOk && !rstPressedbool) {
    delay(50);
  }
  if(rstPressedbool) {
    Serial.println("INTERROMPO OPERAZIONE, RST PREMUTO DOPO IMPRONTA.");
    return false;
  }
  if(doorOpenedbool) {
    Serial.println("INTERROMPO OPERAZIONE, ALLARME PORTA APERTA.");
    return false;
  }
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" Inserire Password: ");
  lcd.setCursor(0,2);
  lcd.print("Verde per confermare");
  lcd.setCursor(0,3);
  lcd.print("* per cancellare");
  while(!checkPsw() && !pswOk && !rstPressedbool && !doorOpenedbool) {
    delay(50);
  }
  if(rstPressedbool) {
    Serial.println("INTERROMPO OPERAZIONE, RST PREMUTO DOPO PSW.");
    return false;
  }
  if(doorOpenedbool) {
    Serial.println("INTERROMPO OPERAZIONE, ALLARME PORTA APERTA.");
    return false;
  }
  return true;
}

bool varControllo1 = false;

// LOOP
void loop() {
  if(!rstPressedbool && !doorOpenedbool && !login) {
    login = faseLogin();
  }
  if(!login) {
    Serial.println("Login Fallita.");
  }
  if(!login && doorOpenedbool) {
    comunicaPorta();
    allarmeInfinito();
  }
  if(login && !rstPressedbool) {
    if(!varControllo1) { 
      // SE LA LOGIN E' FATTA E IL BOTTONE DI RESET NON RISULTA PREMUTO
      // SIAMO NEL PUNTO IN CUI POSSIAMO APRIRE LA PORTA, QUINDI SBLOCCHIAMO IL SERVOMOTORE
      // LA VAR CONTROLLO MI SERVE PER NON RIPETERE AD OGNI LOOP QUESTA OPERAZIONE
      digitalWrite(LED_RED_PIN, LOW);
      digitalWrite(LED_GREEN_PIN, HIGH); 
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("    PREGO APRIRE    ");
      lcd.setCursor(0,2);
      lcd.print("       PORTA.       ");     
      sendFramework_srv(8, "MOT_ON");
      while(!doorOpenedbool && !rstPressedbool){
        delay(50);
      }
      comunicaPorta();
    }
    varControllo1 = true;
  }
  if(login && doorOpenedbool) {
    if(rstPressedbool) {
      // SE LA LOGIN E' FATTA E LA PORTA E' APERTA MA VIENE PREMUTO IL RESET
      // DOBBIAMO AVVISARE DI CHIUDERE LA PORTA
      // FINCHE' NON VERRA CHIUSA CI SARA' UN ALLARME PIU' TENUE 
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("   ATTENZIONE!!!!   ");
      lcd.setCursor(0,1);
      lcd.print(" CHIUDERE LA PORTA! ");
      lcd.setCursor(0,2);
      lcd.print("   PER CONCLUDERE   ");
      while(doorOpenedbool) {
        tonoBreve();
        delay(50);
      }
      comunicaPorta();
    }
  }
  if (rstPressedbool && !doorOpenedbool) {
    // SE VIENE PREMUTO IL BOTTONE DI RESET E LA PORTA E' CHIUSA
    // CHIUDIAMO IL SERVO CHE BLOCCA LA PORTA
    sendFramework_srv(8, "MOT_OF");
    comunicaPorta();
    // RESET DI IMPRONTA E PASSWORD INSERITE E TUTTE LE VARIABILI DI CONTROLLO
    // NON RESETTIAMO I TENTATIVI PER EVITARE TENTATIVI INFINITI
    impOk = false;
    pswOk = false;
    rstPressedbool = false;
    login = false;
    varControllo1 = false;
    pswRicevuta = false;
    sendFramework_srv(7, "RST_OK");
    Serial.println("RESET AVVENUTO, VARIABILI AZZERATE (TRANNE I TENTATIVI)");
  }
  delay(250);
}

//COMUNICAZIONE PORTA APERTA/CHIUSA
void comunicaPorta() {
  if(doorOpenedbool) {
    //COMUNICARE PORTA APERTA
    sendFramework_srv(3, "DOR_OP");
  }
  else {
    //COMUNICARE PORTA CHIUSA
    sendFramework_srv(3, "DOR_CL");
  }
}

// ALLARME FORTE
void allarmeInfinito() {
  // AVVISO ARDUINO REV2 E PARTE L'ALLARME
  sendFramework_srv(6, "ALM_ON");
  lcd.clear();
  while(true) {
    lcd.setCursor(0,0);
    lcd.print("   Secure Box n.1   ");
    lcd.setCursor(0,1);
    lcd.print("TENTATIVO INTRUSIONE");
    lcd.setCursor(0,2);
    lcd.print(" GUARDIE IN ARRIVO! ");
    tone(BUZZER_PIN, 2000, 250);
    delay(250);
    noTone(BUZZER_PIN);
    delay(250);
  }
}

// ALLARME TENTATIVI ESAURITI
void allarmeTentativiEsauriti() {
  // AVVISO ARDUINO REV2 E PARTE L'ALLARME
  sendFramework_srv(6, "ALM_ON");
  lcd.clear();
  while(true) {
    lcd.setCursor(0,0);
    lcd.print("   Secure Box n.1   ");
    lcd.setCursor(0,1);
    lcd.print("TENTATIVI ESAURITI!!");
    lcd.setCursor(0,2);
    lcd.print("CONTATTATO UN ADMIN!");
    tone(BUZZER_PIN, 1300, 250);
    delay(250);
    noTone(BUZZER_PIN);
    delay(250);
  }
}

void tonoBreve() {
  tone(BUZZER_PIN, 1300, 250);
  delay(250);
  noTone(BUZZER_PIN);
  delay(250);
}

// CHECK DELLA PASSWORD 
boolean checkPsw(){
  lcd.setCursor(0,1);
  String entered_code = "";
  while(digitalRead(BTN_GREEN_PIN) == HIGH) {
    if(rstPressedbool) {
      Serial.println("INTERROMPO OPERAZIONE, RESET PREMUTO.");
      return false;
    }
    if(doorOpenedbool) {
      Serial.println("INTERROMPO OPERAZIONE, PORTA APERTA.");
      return false;
    }
    char key = keypad.getKey();
    if(key) {
      entered_code += key;
      lcd.print("*");
      if(key == '*') {
        entered_code = "";
        lcd.setCursor(0,1);
        lcd.print("                    ");
        lcd.setCursor(0,1);
      }
    }
  }
  Serial.print("Codice Inserito: ");
  Serial.println(entered_code);
  // INVIO AL REV2 PER IL CONTROLLO
  sendFramework_srv(2, entered_code);

  // RICEVO LA RISPOSTA DAL REV2
  Serial.print("Inizio attesa password:");
  for(int i = 0; i < 3; i++){
    Serial.print(" -");
    delay(1000);
  }
  pswOk = receiveDataFromSlave();
  Serial.println("Attesa Finita;");
  if(pswOk) {
    Serial.println("La Password Corrisponde;");
    tentativiPsw = 5;
    return true;
  }
  else {
    Serial.println("La Password è sbagliata;");
    tentativiPsw--;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("      ERRORE!!      ");
    lcd.setCursor(0,1);
    lcd.print("Password Errata");
    lcd.setCursor(0,2);
    lcd.print("Ancora ");
    lcd.print(tentativiPsw);
    lcd.print(" tentativi.");
    if(tentativiPsw == 0) {
      Serial.println("Tentativi massimi raggiunti, attivo allarme;");
      allarmeTentativiEsauriti();
      return false;
    }
    delay(2000);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(" Inserire Password: ");
    lcd.setCursor(0,2);
    lcd.print("Verde per confermare");
    lcd.setCursor(0,3);
    lcd.print("* per cancellare");
    return false;
  }
}

// CHECK DELL'IMPRONTA
boolean checkImp() {
  // LEGGI L'IMPRONTA, QUESTO E' UN WHILE, IMPOSSIBILE FARE CONTROLLI DURANTE QUESTO
  uint8_t p = finger.getImage(); 
  if(p != FINGERPRINT_NOFINGER){
    Serial.println("Impronta acquisita;");
    p = finger.image2Tz();
    if(p == FINGERPRINT_OK){
      p = finger.fingerSearch();
      if(p == FINGERPRINT_OK){
        Serial.print("Impronta trovata con id: ");
        Serial.println(finger.fingerID);
        tentativiImp = 5;
        // AVVISO REV2 CHE IMPRONTA E' OK
        sendFramework_srv(1,"IMP_OK");
        lcd.setCursor(0,2);
        lcd.print("                    ");
        lcd.setCursor(0,2);
        lcd.print("  Impronta Trovata  ");
        lcd.setCursor(0,3);
        lcd.print("       ID: ");
        lcd.print(finger.fingerID);
        delay(1500);
        impOk = true;
        return true;
      }
      else if (p == FINGERPRINT_NOTFOUND) {
        tentativiImp--;
        if(tentativiImp < 0){
          tentativiImp = 0;
        }
        lcd.setCursor(0,2);
        lcd.print("Impronta non trovata");
        lcd.setCursor(0,3);
        lcd.print("Tentativi rimasti: ");
        lcd.print(tentativiImp);
        Serial.print("Impronta non trovata, rimangono ");
        Serial.print(tentativiImp);
        Serial.println(" tentativi;");
        // AVVISO REV2 CHE IMPRONTA NON E' OK
        sendFramework_srv(1,"IMP_ER");
        if (tentativiImp == 0) {
          Serial.println("Tentativi massimi raggiunti, attivo allarme;");
          allarmeTentativiEsauriti();
          return false;
        }
      }
    }
  }
  return false;
}

// FRAMEWORK DI INVIO AL REV2
void sendFramework_srv(int nFunction, String dataMessage) {
  // FRAMEWORK, MESSAGGI SUPPORTATI (nFunction)
  // 1 - IMP_CHECK - IMPRONTA
  // 2 - PSW_CHECK - PASSWORD
  // 3 - DOR_CHECK - PORTA
  // 4 - TMP_CHECK - TEMPERATURA (NON SERVE PIU SENSORE SPOSTATO SUL REV2)
  // 5 - UMH_CHECK - UMIDITA' (NON SERVE PIU SENSORE SPOSTATO SUL REV2)
  // 6 - ALM_CHECK - ALLARME SONORO
  // 7 - RST_CHECK - RESET
  // 8 - MOT_OPENC - APRI CHIUDI MOTORE

  // FRAMEWORK, MESSAGGI POSSIBILI
  // 1 - IMP_OK IMP_ER MESSO
  // 2 - PSW
  // 3 - DOR_OP DOR_CL
  // 4 - TEMPERATURE
  // 5 - UMIDITY
  // 6 - ALM_ON ALM_OF
  // 7 - RST_OK
  // 8 - MOT_ON, MOT_OFF
  
  Serial.print("Invocato invio al REV2 - Funzione: ");
  Serial.print(nFunction);
  Serial.print(";   Messaggio: ");
  Serial.print(dataMessage);
  Serial.println(";");
  
  String msgFinale = "";
  int defaultLen = 16;
  if(nFunction == 1) {
    msgFinale = String("IMP_CHECK") + "-" + dataMessage;
    performSend(msgFinale, defaultLen);
  }
  else if(nFunction == 2) {
    String dimString = String(dataMessage.length());
    padZerosLeft(dimString, 6);
    String msgFin1 = String("PSW_DIMEN") + "-" + dimString;
    String msgFin2 = String("PSW_CHECK") + "-" + dataMessage;
    performSend(msgFin1, msgFin1.length());
    performSend(msgFin2, msgFin2.length());
  }
  else if(nFunction == 3) {
    msgFinale = String("DOR_CHECK") + "-" + dataMessage;
    performSend(msgFinale, defaultLen);
  }
  else if(nFunction == 4) {
    msgFinale = String("TMP_CHECK") + "-" + dataMessage;
    performSend(msgFinale, defaultLen);
  }
  else if(nFunction == 5) {
    msgFinale = String("UMH_CHECK") + "-" + dataMessage;
    performSend(msgFinale, defaultLen);
  }
  else if(nFunction == 6) {
    msgFinale = String("ALM_CHECK") + "-" + dataMessage;
    performSend(msgFinale, defaultLen);
  }
  else if(nFunction == 7) {
    msgFinale = String("RST_CHECK") + "-" + "RST_OK";
    performSend(msgFinale, defaultLen);
  }
  else if(nFunction == 8) {
    msgFinale = String("MOT_OPENC") + "-" + dataMessage;
    performSend(msgFinale, defaultLen);
  }
  else {
    Serial.println("Errore: E' stata richiesta una funzione non valida.");
  }
}

// ATTO DI INVIO AL REV2
void performSend(String dataToSend, int dataLen) {
  byte byteData[dataLen];
  stringToByteArray(dataToSend, byteData, sizeof(byteData));
  Wire.beginTransmission(8);
  Wire.write(byteData, sizeof(byteData));
  Wire.endTransmission();
  Serial.print("Inviato Messaggio a REV2: ");
  Serial.println(dataToSend);
}

// FRAMEWORK DI RICEZIONE DAL REV2
boolean receiveFramework_srv(String function, String dataMessage) {
  // DATO CHE FUNZIONA TUTTO TRAMITE CONTROLLI
  // QUESTO FRAMEWORK RITORNA SOLAMENTE TRUE O FALSE
  if(function == "PSW_CHECK") {
    if(strcmp(dataMessage, "PSW_OK") == 0) {
      return true;
    }
    else if(strcpm(dataMessage, "PSW_ER") == 0) {
      return false;
    }
    else {
      return false;
    }
  }
  else {
    return false;
  }
}

// ATTO DI RICEZIONE DAL REV2
boolean receiveDataFromSlave() {
  // RITORNA TRUE O FALSE IN BASE AL RITORNO DEL FRAMEWORK
  Wire.requestFrom(8, 16);
  byte receivedData[16];
  int index = 0;
  while (Wire.available()) {
    receivedData[index++] = Wire.read();
  }
  String receivedString = byteArrayToString(receivedData, sizeof(receivedData));
  String receivedFunction = extractField(receivedString, '-', 0);
  String receivedMessage = extractField(receivedString, '-', 1);
  Serial.print("Ricevuto dallo Slave - Funzione: ");
  Serial.println(receivedFunction);
  Serial.print("; Messaggio dallo Slave: ");
  Serial.print(receivedMessage);
  Serial.println(";");
  return receiveFramework_srv(receivedFunction, receivedMessage);
}

//
// FUNZIONI DI UTILITA'
//

// RIEMPIE DI 0 UNA STRINGA PER FARLA ARRIVARE AD UNA LUNGHEZZA FISSATA
void padZerosLeft(String &str, int fixedLength) {
  int currentLength = str.length();
  if (currentLength < fixedLength) {
    int zerosToAdd = fixedLength - currentLength;
    String zeros = "0";
    for (int i = 1; i < zerosToAdd; i++) {
      zeros += "0";
    }
    str = zeros + str;
  }
}

// ESTRAZIONE DEI FIELD FUNZIONE IN BASE AL -
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
  return "";
}

// CONVERSIONE DA ARRAY DI BYTE A STRINGA
String byteArrayToString(byte* array, int length) {
  String result = "";
  for (int i = 0; i < length; i++) {
    result += char(array[i]);
  }
  return result;
}

// CONVERSIONE DA STRINGA AD ARRAY DI BYTE
void stringToByteArray(String input, byte* output, int maxLength) {
  for (int i = 0; i < maxLength && i < input.length(); i++) {
    output[i] = input.charAt(i);
  }
}
