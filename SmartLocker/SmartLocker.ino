#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>

const byte LED_GREEN_PIN = 25;
const byte LED_RED_PIN = 27;
const byte BTN_GREEN_PIN = 31;
const byte BTN_RED_PIN = 2;
const byte BUZZER_PIN = 33;
const byte DOOR_PIN = 3;

const byte ROW_NUM    = 4; // quattro righe
const byte COLUMN_NUM = 4; // quattro colonne

int tentativiImp = 5;
int tentativiPsw = 5;

bool doorLocked = true;
bool firstWrite = true;
bool impOk = false;
bool pswOk = false;
bool login = false;

String doorCheck = "";
String tempCheck = "";
String umiCheck = "";
String alarmCheck = "";

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte pin_rows[ROW_NUM] = {36, 34, 32, 30}; // Collegherai questi ai pin 7-10
byte pin_column[COLUMN_NUM] = {28, 26, 24, 22}; // Collegherai questi ai pin 3-6

Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

#define mySerial Serial1

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
// Configura l'indirizzo I2C dell'LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Cambia 0x27 con l'indirizzo I2C corretto per il tuo schermo

volatile bool rstPressedbool = false;
volatile bool doorOpenedbool = false;
unsigned long lastDebounceTime_rst = 0;
unsigned long lastDebounceTime_door_o = 0;
unsigned long debounceDelay = 75;
int contRst = 0;
int contDoor = 0;

void setup() {
  Wire.begin();
  Serial.begin(9600);

  while (!Serial);
  finger.begin(57600);

  delay(5);

  if (finger.verifyPassword()) {
    Serial.println("Sensore di impronte rilevato;");
  } else {
    Serial.println("Sensore di impronte non rilevato;");
    while (1) { delay(1); }
  }

  lcd.init();
  lcd.begin(20, 4);
  lcd.backlight();
  lcd.clear();

  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_GREEN_PIN, INPUT_PULLUP);
  pinMode(BTN_RED_PIN, INPUT_PULLUP);
  pinMode(DOOR_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BTN_RED_PIN), rstPressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(DOOR_PIN), doorStatusChanged, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(DOOR_PIN), doorClosed, LOW);
}

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

void doorStatusChanged() {
  unsigned long currentMillis = millis();
  if(currentMillis - lastDebounceTime_door_o > debounceDelay) {
    lastDebounceTime_door_o = currentMillis;
    if(digitalRead(DOOR_PIN) == HIGH) {
      Serial.println("PORTA APERTA");
      sendFramework_srv(3, "DOR_OP");
      doorOpenedbool = true;
    }
    if(digitalRead(DOOR_PIN) == LOW) {
      Serial.println("PORTA CHIUSA");
      sendFramework_srv(3, "DOR_CL");
      doorOpenedbool = false;
    }
  }
}

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
  while(digitalRead(BTN_GREEN_PIN) == HIGH || (digitalRead(BTN_GREEN_PIN) == LOW && firstWrite));
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

  while(!checkImp() && !impOk && !rstPressedbool);

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
  
  while(!checkPsw() && !pswOk && !rstPressedbool);

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

void loop() {
  if(!rstPressedbool && !doorOpenedbool && !login) {
    login = faseLogin();
  }
  if(!login) {
    Serial.println("Login Fallita.");
  }
  if(!login && doorOpenedbool) {
    allarmeInfinito();
  }
  if(login && !rstPressedbool) {
    if(!varControllo1) {
      //FASE DI APERTURA
      //SI CONCLUDE CON CHIUSURA PORTA E ROSSO PREMUTO
      digitalWrite(LED_RED_PIN, LOW);
      digitalWrite(LED_GREEN_PIN, HIGH);
      //GESTIRE APERTURA 
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(" PREGO APRIRE ");
      lcd.setCursor(0,1);
      lcd.print(" PORTA ");     
      aprireMotore();
    }
    varControllo1 = true;
  }
  if(login && doorOpenedbool) {
    if(rstPressedbool) {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(" PORCODIO ");
      lcd.setCursor(0,2);
      lcd.print(" CHIUDI LA PORTA VA LA ");
      while(doorOpenedbool) {
        tonoBreve();
        delay(50);
      }
    }
  }
  if (rstPressedbool && !doorOpenedbool) {
    // CHIUDERE MOTORE EVENTUALMENTE
    chiudereMotore();
    //RESET DI IMP E PSW INSERITE
    impOk = false;
    pswOk = false;
    //RITORNARE ALLA LOGIN FINENDO IL LOOP
    rstPressedbool = false;
    login = false;
    varControllo1 = false;
    Serial.println("TUTTO RESETTATO, TRANNE TENTATIVI.");
  }
  delay(500);
}

void aprireMotore() {
  sendFramework_srv(8, "MOT_ON");
}

void chiudereMotore() {
  sendFramework_srv(8, "MOT_OF");
}

void allarmeInfinito() {
  sendFramework_srv(6, "ALM_ON");
  while(true) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("     HEY LADRO!     ");
    lcd.setCursor(0,1);
    lcd.print(" GUARDIE IN ARRIVO! ");
    tone(BUZZER_PIN, 2000, 300);  // Emette il suono
    delay(300);  // Pausa tra i suoni
    noTone(BUZZER_PIN);  // Arresta il suono
    delay(250);  // Pausa tra i suoni successivi
  }
}

boolean checkPsw(){
  lcd.setCursor(0,1);
  String entered_code = "";
  while(digitalRead(BTN_GREEN_PIN) == HIGH) {
    if(rstPressedbool) {
      Serial.println("INTERROMPO OPERAZIONE, RST PREMUTO.");
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
  sendFramework_srv(2, entered_code);
  delay(300);
  pswOk = receiveDataFromSlave();
  delay(300);
  if(pswOk) 
  {
    Serial.println("La Password Corrisponde;");
    tentativiPsw = 5;
    return true;
  }
  else 
  {
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
      tonoBreve();
      callAdmin();
      return false;
    }
    delay(1500);
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

boolean checkImp() {
  uint8_t p = finger.getImage();
  if(p != FINGERPRINT_NOFINGER){
    //Serial.println("Impronta acquisita;");
    p = finger.image2Tz();
    if(p == FINGERPRINT_OK){
      //Serial.println("Impronta elaborata;");
      p = finger.fingerSearch();
      if(p == FINGERPRINT_OK){
        Serial.print("Impronta trovata con id: ");
        Serial.println(finger.fingerID);
        tentativiImp = 5;
        sendFramework_srv(1,"IMP_OK");
        lcd.setCursor(0,2);
        lcd.print("                    ");
        lcd.setCursor(0,3);
        lcd.print("  Impronta Trovata  ");
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
        sendFramework_srv(1,"IMP_ER");
        if (tentativiImp == 0) {
          Serial.println("Tentativi massimi raggiunti, attivo allarme;");
          tonoBreve();
          callAdmin();
          return false;
        }
      }
    }
  }
  return false;
}

void callAdmin() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("   Secure Box n.1   ");
  lcd.setCursor(0,2);
  lcd.print("   Contattare  un   ");
  lcd.setCursor(0,3);
  lcd.print("   Amministratore   ");
  while(true); // GESTIRE QUESTA COSA
}

void tonoBreve() {
  for (int i = 0; i < 5; i++) {  
    tone(BUZZER_PIN, 1300, 500);  // Emette il suono
    delay(500);  // Pausa tra i suoni
    noTone(BUZZER_PIN);  // Arresta il suono
    delay(250);  // Pausa tra i suoni successivi
  }
}

void sendFramework_srv(int nFunction, String dataMessage) {
  Serial.print(nFunction);
  Serial.print("     ");
  Serial.println(dataMessage);
  String msgFinale = "";
  // FRAMEWORK, MESSAGGI SUPPORTATI (nFunction)
  // 1 - IMP_CHECK - IMPRONTA
  // 2 - PSW_CHECK - PASSWORD
  // 3 - DOR_CHECK - PORTA
  // 4 - TMP_CHECK - TEMPERATURA
  // 5 - UMH_CHECK - UMIDITA'
  // 6 - ALM_CHECK - ALLARME SONORO
  // 7 - RST_CHECK - RESET
  // 8 - MOT_OPENC - APRI CHIUDI MOTORE

  // FRAMEWORK, MESSAGGI POSSIBILI
  // 1 - IMP_OK IMP_ER MESSO
  // 2 - PSW
  // 3 - DOR_OP DOR_CL
  // 4 - TEMPERATURE
  // 5 - UMIDITY
  // 6 - ALM_ON ALM_OF MESSO 
  // 7 - RST_OK
  // 8 - MOT_ON, MOT_OFF MESSO
  if(nFunction == 8) {
    msgFinale = String("MOT_OPENC") + "-" + dataMessage;
    Serial.print("prima msg: ");
    Serial.println(msgFinale);
    performSend(msgFinale);
    Serial.print("dopo msg: ");
    Serial.println(msgFinale);
  }
  switch (nFunction) {
    case 1:
      msgFinale = String("IMP_CHECK") + "-" + dataMessage;
      performSend(msgFinale);
      break;
    case 2:
      String dimString = String(dataMessage.length());
      padZerosLeft(dimString, 6);
      String msgFin1 = String("PSW_DIMEN") + "-" + dimString;
      String msgFin2 = String("PSW_CHECK") + "-" + dataMessage;
      byte byteData[msgFin1.length()];
      stringToByteArray(msgFin1, byteData, sizeof(byteData));
      Wire.beginTransmission(8);
      Wire.write(byteData, sizeof(byteData));
      Wire.endTransmission();
      Serial.println("INVIATO1");
      delay(1000);
      byte byteData2[msgFin2.length()];
      stringToByteArray(msgFin2, byteData2, sizeof(byteData2));
      Wire.beginTransmission(8);
      Wire.write(byteData2, sizeof(byteData2));
      Wire.endTransmission();
      Serial.println("INVIATO2");
      break;
    case 3:
      msgFinale = String("DOR_CHECK") + "-" + dataMessage;
      performSend(msgFinale);
      break;
    case 4:
      msgFinale = String("TMP_CHECK") + "-" + dataMessage;
      performSend(msgFinale);
      break;
    case 5:
      msgFinale = String("UMH_CHECK") + "-" + dataMessage;
      performSend(msgFinale);
      break;
    case 6:
      msgFinale = String("ALM_CHECK") + "-" + dataMessage;
      performSend(msgFinale);
      break;
    case 7:
      msgFinale = String("RST_CHECK") + "-" + "RST_OK";
      performSend(msgFinale);
      break;
    case 8:
      
      break;
    default:
      Serial.println("Errore: FUNZIONE non valida.");
      break;
  }
}

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

void performSend(String dataToSend) {
  byte byteData[16];
  stringToByteArray(dataToSend, byteData, sizeof(byteData));
  Wire.beginTransmission(8);
  Wire.write(byteData, sizeof(byteData));
  Wire.endTransmission();
}

boolean receiveDataFromSlave() {
  Wire.requestFrom(8, 16);
  byte receivedData[16];
  int index = 0;
  while (Wire.available()) {
    receivedData[index++] = Wire.read();
  }
  String receivedString = byteArrayToString(receivedData, sizeof(receivedData));
  String receivedFunction = extractField(receivedString, '-', 0);
  String receivedMessage = extractField(receivedString, '-', 1);
  Serial.print("Funzione dallo Slave: ");
  Serial.println(receivedFunction);
  Serial.print("Messaggio dallo Slave: ");
  Serial.println(receivedMessage);
  return receiveFramework_srv(receivedFunction, receivedMessage);
}

boolean receiveFramework_srv(String function, String dataMessage){
  if(function == "PSW_CHECK") {
    if(dataMessage == "PSW_OK") {
      return true;
    }
    else if(dataMessage == "PSW_ER") {
      return false;
    }
  }
  else {
    return false;
  }
}

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