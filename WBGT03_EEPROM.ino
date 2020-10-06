// WBGTデータをEEPROMに書き込む
#include <EEPROM.h>

#define DEBUG 0  // dataのデバッグ、充分に確認してから
#define WRITE 0  // WBGTデータを書き込む(340byte, 0-339)
#define DEBUG2 0 // メッセージのデバッグ、充分に確認してから
#define WRITE2 0 // メッセージを書き込む(24byte, 340-363)

void setup() {
  int i;  // 文字列アドレス, 340-363
  //          012345678901234567890123
  String mes="WBGT use  indoor-C%DEBUG";
  String s=""; // read message
  
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("START " __FILE__ " from " __DATE__ " " __TIME__));
  
  // 上から下:21-40°C, 左から右:5ステップで20-100%RH
  int t, h, w; // t=0:21deg, t=19:40deg, h=0:20%, h=16:100%
  int wbgt[20][17] = {
    {15,15,16,16,17,17,18,19,19,20,20,21,21,22,23,23,24},
    {15,16,17,17,18,18,19,19,20,21,21,22,22,23,24,24,25},
    {16,17,17,18,19,19,20,20,21,22,22,23,23,24,25,25,26},
    {17,18,18,19,19,20,21,21,22,22,23,24,24,25,26,26,27},
    {18,18,19,20,20,21,22,22,23,23,24,25,25,26,27,27,28},
    {18,19,20,20,21,22,22,23,24,24,25,26,26,27,28,28,29},
    {19,20,21,21,22,23,23,24,25,25,26,27,27,28,29,29,30},
    {20,21,21,22,23,23,24,25,25,26,27,28,28,29,30,30,31},
    {21,21,22,23,24,24,25,26,26,27,28,29,29,30,31,31,32},
    {21,22,23,24,24,25,26,27,27,28,29,29,30,31,32,32,33},
    {22,23,24,24,25,26,27,27,28,29,30,30,31,32,33,33,34},
    {23,24,25,25,26,27,28,28,29,30,31,31,32,33,34,34,35},
    {24,25,25,26,27,28,28,29,30,31,32,32,33,34,35,35,36},
    {25,25,26,27,28,29,29,30,31,32,33,33,34,35,36,37,37},
    {25,26,27,28,29,29,30,31,32,33,33,34,35,36,37,38,38},
    {26,27,28,29,29,30,31,32,33,34,34,35,36,37,38,39,39},
    {27,28,29,29,30,31,32,33,34,35,35,36,37,38,39,40,41},
    {28,28,29,30,31,32,33,34,35,35,36,37,38,39,40,41,42},
    {28,29,30,31,32,33,34,35,35,36,37,38,39,40,41,42,43},
    {29,30,31,32,33,34,35,35,36,37,38,39,40,41,42,43,44}
  };
  if (DEBUG) {
    Serial.println("===== WBGT data =====");
    Serial.print(" t ");
    for (h=0; h<17; h++) {
      Serial.print(h*5+20);
      Serial.print(" ");
    }
    Serial.println();
    for (t=0; t<20; t++) {
      Serial.print(t+21);
      Serial.print(" ");
      for (h=0; h<17; h++) {
        Serial.print(wbgt[t][h]);
        Serial.print(" ");
      }
      Serial.println();
    }
    Serial.println("END of data");
    Serial.println("===== sequence =====");
    Serial.print(" t ");
    for (h=0; h<17; h++) {
      Serial.print(h*5+20);
      Serial.print(" ");
    }
    Serial.println();
    for (t=0; t<20; t++) {
      Serial.print(t+21);
      Serial.print(" ");
      for (h=0; h<17; h++) {
        Serial.print(t*17+h);
        Serial.print(" ");
      }
      Serial.println();
    }
    Serial.println("END of sequence");
  }
  if (DEBUG2) {
    Serial.println("===== Message Strings =====");
    for (i=340; i<=363; i++) {
      Serial.print(mes.charAt(i-340));
    }
    Serial.println();
    Serial.println("END of strings");
  }
  if (WRITE) {  // 書き込みは慎重に！
    Serial.println("===== EEPROM writing (data)=====");
    Serial.print(" t ");
    for (h=0; h<17; h++) {
      Serial.print(h*5+20);
      Serial.print(" ");
    }
    for (t=0; t<20; t++) {
      Serial.print(t+21);
      Serial.print(" ");
      for (h=0; h<17; h++) {
        Serial.print(".");
        EEPROM.write(t*17+h, wbgt[t][h]);
      }
      Serial.println();
    }
    Serial.println("END of data write process");
  }
  if (WRITE2) {
    Serial.println("===== EEPROM writing (message) =====");
    for (i=340; i<=363; i++) {
      EEPROM.write(i, mes.charAt(i-340));
    }
    Serial.println("END of message write process");
  }
  // 読み込む
  Serial.println("===== Read from EEPROM =====");
  Serial.print(" t ");
  for (h=0; h<17; h++) {
    Serial.print(h*5+20);
    Serial.print(" ");
  }
  Serial.println();
  for (t=0; t<20; t++) {
    Serial.print(t+21);
    Serial.print(" ");
    for (h=0; h<17; h++) {
      w=EEPROM.read(t*17+h);
      Serial.print(w);
      Serial.print(" ");
    }
    Serial.println();
  }
  char c;
  for (i=340; i<348; i++) { // "WBGT use"
    c=EEPROM.read(i);
    s+=c;
    //s+=EEPROM.read(i);
  }
  Serial.println(s);
  s="";
  for (i=348; i<356; i++) { // "  indoor"
    c=EEPROM.read(i);
    s+=c;
    //s+=EEPROM.read(i);
  }
  Serial.println(s);
  s="";
  for (i=356; i<=358; i++) { // "-C%"
    c=EEPROM.read(i);
    s+=c;
    //s+=EEPROM.read(i);
  }
  Serial.println(s);
  s="";
  for (i=359; i<=363; i++) { // "DEBUG"
    c=EEPROM.read(i);
    s+=c;
    //s+=EEPROM.read(i);
  }
  Serial.println(s);
  s="";
  Serial.println("END of read");
}

void loop() {
  // do not loop anything
}

// end of program
