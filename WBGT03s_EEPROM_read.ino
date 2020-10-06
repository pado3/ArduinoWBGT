// ATtiny85のEEPROMに書き込んだWBGTデータ読み込み、
// 秋月電子8x2LCDに表示する by @pado3 2020/10/05
// LCDの商品ページ https://akizukidenshi.com/catalog/g/gK-06795/
#include <TinyWireM.h>
#include <EEPROM.h>
// EEPROM 0-339:WBGT data
// EEPROM 340-363 "WBGT use  indoor-C%DEBUG" (340-355, 356-358, 359-363)

// pin assign
#define VDD   1     // センサ・液晶用Vdd @D1(PWM) = pin6
#define LED   3     // LED @D3 (2.2k) = pin2 動作確認用
#define Vin  A2     // LCD VolumeのADC @A2 (= D4) = pin3
// I2C addresses
#define LCDadr 0x3e // LCD address, fixed

int Vdd_val = 169;  // LCDの電源電圧Value=51*Vdd (3.3V:169)
byte contrast = 10; // LCDコントラスト(0～63)、初期値10

void setup() {
  // LCDの電源を入れる
  pinMode(VDD,  OUTPUT);
  analogWrite(VDD, Vdd_val);  // LCDとBME280の電源電圧Value=51*Vdd (3.3V:169)
  delay(500);  // PWM電圧が落ち着いてセンサが動くまで待つ。時間は経験値
  
  // Contrast reader setup コントラストを外部読み込みしないときはコメントアウト
  pinMode(Vin,  INPUT);
  contrast = analogRead(Vin) >> 4;  // ADC1024step ->4bit-> contrast64step

  // LED setup
  pinMode(LED, OUTPUT);
  
  // LCD setup
  TinyWireM.begin();
  lcd_cmd(0b00111000); // function set
  lcd_cmd(0b00111001); // function set
  lcd_cmd(0b00000100); // EntryModeSet
  lcd_cmd(0b00010100); // interval osc
  lcd_cmd(0b01110000 | (contrast & 0xF)); // contrast Low
  lcd_cmd(0b01011100 | ((contrast >> 4) & 0x3)); // contast High/icon/power
  lcd_cmd(0b01101100); // follower control
  delay(200);
  lcd_cmd(0b00111000); // function set
  lcd_cmd(0b00001100); // Display On
  lcd_cmd(0b00000001); // Clear Display
}


void loop() {
  int W, i, j;  // WBGT値, counter
  String tmpS;  // 数値から文字に変換する際の一時文字列
  char c[] = "12345678"; // LCD buffer 1ライン8文字分
  char c2;      // EEPROMから1文字ずつ読み込むbuffer
  digitalWrite(LED, HIGH);  // ON
  lcd_setCursor(0, 0);
  lcd_printStr("Read dat");
  lcd_setCursor(0, 1);
  lcd_printStr("fmEEPROM");
  delay(2000);
  digitalWrite(LED, LOW);   // OFF
  // EEPROM read start
  // WBGT data
  for (i = 0; i < 340; i++) {
    for (j = 0; j < 8; j++) { c[j] = ' '; } // clear
    tmpS = String(21 + i/17);
    tmpS += "C,";
    tmpS += String(20 + 5*(i%17));
    tmpS += '%';
    for (j=0; j<tmpS.length(); j++) {
      c[j] = tmpS.charAt(j);
    }
    lcd_setCursor(0, 0);
    lcd_printStr(c);
    for (j = 0; j < 8; j++) { c[j] = ' '; } // clear
    W = EEPROM.read(i);
    //delay(100);
    tmpS = "W:";
    tmpS += String(W);
    tmpS += 'C';
    for (j = 0; j < tmpS.length(); j++) {
      c[j] = tmpS.charAt(j);
    }
    lcd_setCursor(0, 1);
    lcd_printStr(c);
    delay(1000); // WBGT値表示時間
    if ( i % 17 == 16 ) { // 気温の変わり目
      digitalWrite(LED, HIGH);
      delay(5000);
      digitalWrite(LED, LOW);
    }
    for (j = 0; j < 8; j++) { c[j] = ' '; } // clear
    lcd_setCursor(0, 1);
    lcd_printStr(c);  // blink
    delay(100);
  }
  // message part
  lcd_clear();
  digitalWrite(LED, HIGH);
  lcd_setCursor(0, 0);
  lcd_printStr("Message ");
  lcd_setCursor(0, 1);
  lcd_printStr("    part");
  for (j = 0; j < 8; j++) { c[j] = ' '; } // clear
  delay(1000);
  lcd_setCursor(0, 1);
  lcd_printStr(c);
  digitalWrite(LED, LOW);
  tmpS = "";
  for (i = 340; i < 348; i++) { // "WBGT use"
    c2 = EEPROM.read(i);
    tmpS += c2;
  }
  for (j = 0; j < tmpS.length(); j++) {
    c[j] = tmpS.charAt(j);
  }
  lcd_setCursor(0, 1);
  lcd_printStr(c);
  delay(1000);
  for (j = 0; j < 8; j++) { c[j] = ' '; } // clear
  lcd_setCursor(0, 1);
  lcd_printStr(c);
  delay(100);
  tmpS = "";
  for (i = 348; i < 356; i++) { // "  indoor"
    c2 = EEPROM.read(i);
    tmpS += c2;
  }
  for (j = 0; j < tmpS.length(); j++) {
    c[j] = tmpS.charAt(j);
  }
  lcd_setCursor(0, 1);
  lcd_printStr(c);
  delay(1000);
  for (j = 0; j < 8; j++) { c[j] = ' '; } // clear
  lcd_setCursor(0, 1);
  lcd_printStr(c);
  delay(100);
  tmpS = "";
  for (i = 356; i <= 358; i++) { // "-C%"
    c2 = EEPROM.read(i);
    tmpS += c2;
  }
  for (j = 0; j < tmpS.length(); j++) {
    c[j] = tmpS.charAt(j);
  }
  lcd_setCursor(0, 1);
  lcd_printStr(c);
  delay(1000);
  for (j = 0; j < 8; j++) { c[j] = ' '; } // clear
  lcd_setCursor(0, 1);
  lcd_printStr(c);
  delay(100);
  tmpS = "";
  for (i = 359; i <= 363; i++) { // "DEBUG"
    c2 = EEPROM.read(i);
    tmpS += c2;
  }
  for (j = 0; j < tmpS.length(); j++) {
    c[j] = tmpS.charAt(j);
  }
  lcd_setCursor(0, 1);
  lcd_printStr(c);
  delay(1000);
  for (j = 0; j < 8; j++) { c[j] = ' '; } // clear
  lcd_setCursor(0, 1);
  lcd_printStr(c);
  delay(100);
  tmpS = "";
  // EEPROM read test end
  
  // fin
  digitalWrite(LED, LOW);   // OFF
  lcd_clear();
  delay(1000);
}

// ここからLCD関係の関数
void lcd_cmd(byte x) {
  TinyWireM.beginTransmission(LCDadr);
  TinyWireM.send(0b00000000); // CO = 0,RS = 0
  TinyWireM.send(x);
  TinyWireM.endTransmission();
}

void lcd_clear() {
  lcd_cmd(0b00000001);
}

void lcd_contdata(byte x) {
  TinyWireM.send(0b11000000); // CO = 1, RS = 1
  TinyWireM.send(x);
}

void lcd_lastdata(byte x) {
  TinyWireM.send(0b01000000); // CO = 0, RS = 1
  TinyWireM.send(x);
}

// 文字の表示
void lcd_printStr(const char *s) {
  TinyWireM.beginTransmission(LCDadr);
  while (*s) {
    if (*(s + 1)) {
      lcd_contdata(*s);
    } else {
      lcd_lastdata(*s);
    }
    s++;
  }
  TinyWireM.endTransmission();
}

// 表示位置の指定
void lcd_setCursor(byte x, byte y) {
  lcd_cmd(0x80 | (y * 0x40 + x));
}

// temp_calのint値（温度*100）と文字列から、LCD表示用に整形して返す
// 氷点下と桁数の取り扱いでごちゃっとしている。やってることはString((float)value/100);
char *tempCal2Char(int value, char *c) {
  char tmp[4];
  if (value <= -1000) {      // -nm.
    tmp[0] = c[0];
    tmp[1] = c[1];
    tmp[2] = c[2];
    tmp[3] = '.';
  } else if (value <= -100) {// -n.m
    tmp[0] = c[0];
    tmp[1] = c[1];
    tmp[2] = '.';
    tmp[3] = c[2];
  } else if (value <= -10) { // -.nm
    tmp[0] = c[0];
    tmp[1] = '.';
    tmp[2] = c[1];
    tmp[3] = c[2];
  } else if (value < 0) {    // -.0n
    tmp[0] = c[0];
    tmp[1] = '.';
    tmp[2] = '0';
    tmp[3] = c[1];
  } else if (value < 10) {   // 0.0n
    tmp[0] = '0';
    tmp[1] = '.';
    tmp[2] = '0';
    tmp[3] = c[0];
  } else if (value < 100) {  // 0.nm
    tmp[0] = '0';
    tmp[1] = '.';
    tmp[2] = c[0];
    tmp[3] = c[1];
  } else if (value < 1000) { // n.ml
    tmp[0] = c[0];
    tmp[1] = '.';
    tmp[2] = c[1];
    tmp[3] = c[2];
  } else {                   // nm.l
    tmp[0] = c[0];
    tmp[1] = c[1];
    tmp[2] = '.';
    tmp[3] = c[2];
  }
  // 返す文字列
  c[0] = tmp[0];
  c[1] = tmp[1];
  c[2] = tmp[2];
  c[3] = tmp[3];
  return c;
}

// end of program
