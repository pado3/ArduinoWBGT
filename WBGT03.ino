// BME280で測定した屋内の温湿度を元に暑さ指数WBGTを求め、秋月電子8x2LCDに表示する
// 2020/08/27 by パドラッパ (twitter: @pado3)
// 2020/08/30 LCDとBME280のVddをVcc固定から電圧可変にした（3.3V品はPWM=168）
// 2020/09/22 カラーLEDを点けるのに失敗。赤色LEDを13から移動して上半分で完結させた
// 2020/09/28 ATtiny85の8k/512/512に収めるトライ
// 参考:
// 屋内での暑さ指数（指針の図2） http://seikishou.jp/heatstroke.html
// LCDの商品ページ・使い方 https://www.switch-science.com/catalog/1405/
// BME280の商品ページ・使い方 https://www.switch-science.com/catalog/2236/
#define DEBUG 1
#include <Wire.h>
#include <EEPROM.h>
// EEPROM 0-339:WBGT data
// EEPROM 340-363 "WBGT use  indoor-C%DEBUG" (340-355, 356-358, 359-363)

// pin assign
#define LED   2     // LED @ D13 (2.2k)
#define VDD   3     // センサ・液晶用のVdd @ D3 PWM
#define Vin  14     // LCD VolumeのADC @ D14=A0
#define sdaPin 18   // Arduino A4
#define sclPin 19   // Arduino A5
// I2C addresses
#define LCDadr 0x3e // LCD address, fixed
#define BMEadr 0x76 // BME280 address, SDO:L

int Vdd_val = 169;  // LCDとBME280の電源電圧Value=51*Vdd (3.3V:169)
byte contrast = 10; // LCDコントラスト(0～63)、初期値10
boolean splash = false; // Indoor use message
unsigned long int hum_raw,temp_raw,pres_raw;
signed long int t_fine;
uint16_t dig_T1;
 int16_t dig_T2;
 int16_t dig_T3;
uint16_t dig_P1;
 int16_t dig_P2;
 int16_t dig_P3;
 
 int16_t dig_P4;
 int16_t dig_P5;
 int16_t dig_P6;
 int16_t dig_P7;
 int16_t dig_P8;
 int16_t dig_P9;
 int8_t  dig_H1;
 int16_t dig_H2;
 int8_t  dig_H3;
 int16_t dig_H4;
 int16_t dig_H5;
 int8_t  dig_H6;

void setup() {
  // センサ・LCDの電源を入れる
  pinMode(VDD,  OUTPUT);
  analogWrite(VDD, Vdd_val);  // LCDとBME280の電源電圧Value=51*Vdd (3.3V:169)
  delay(500);  // PWM電圧が落ち着いてセンサが動くまで待つ。時間は経験値
  
  // Contrast reader setup
  pinMode(Vin,  INPUT);
  contrast = analogRead(Vin) >> 4;  // ADC1024step ->4bit-> contrast64step
  
  // BME280 setup
  uint8_t osrs_t = 1;    // Temperature oversampling x 1
  uint8_t osrs_p = 1;    // Pressure oversampling x 1
  uint8_t osrs_h = 1;    // Humidity oversampling x 1
  uint8_t mode = 3;      // Normal mode
  uint8_t t_sb = 5;      // Tstandby 1000ms
  uint8_t filter = 0;    // Filter off 
  uint8_t spi3w_en = 0;  // 3-wire SPI Disable
  uint8_t ctrl_meas_reg = (osrs_t << 5) | (osrs_p << 2) | mode;
  uint8_t config_reg    = (t_sb << 5) | (filter << 2) | spi3w_en;
  uint8_t ctrl_hum_reg  = osrs_h;
  
  /* フラッシュ980byte, RAM 183byte消費
  Serial.begin(115200);
  delay(200);
  Serial.println("DEBUG");
  */
  
  Wire.begin();
  writeReg(0xF2,ctrl_hum_reg);
  writeReg(0xF4,ctrl_meas_reg);
  writeReg(0xF5,config_reg);
  readTrim();
  
  // LED setup
  pinMode(LED, OUTPUT);
  
  // LCD setup
  // delay(500);  // Vdd印加後のdelay()と挿し換え
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
  //delay(2);   // 要らなさそう
}


void loop() {
  signed long int temp_cal;
  unsigned long int press_cal, hum_cal;
  int temp_acti=0, press_acti=0, hum_acti=0;
  int W, i;  // WBGT値, counter
  String tmpS;  // 数値から文字に変換する際の一時文字列
  char c[]="12345678"; // LCD buffer 1ライン8文字分
 if (DEBUG) {
   lcd_clear(); lcd_setCursor(0, 0); lcd_printStr("DEBUG"); delay(1000); //DEBUG追加
 }
  
  // 屋内専用であることのスプラッシュ表示 兼 BME280の初期化待ち
  if(!splash) {
    lcd_clear();
    for (i=340; i<348; i++) {
      c[i-340]=EEPROM.read(i);
    }
    lcd_setCursor(0, 0);
    lcd_printStr(c);
    for (i=348; i<356; i++) {
      c[i-348]=EEPROM.read(i);
    }
    lcd_setCursor(0, 1);
    lcd_printStr(c);
    delay(2000);
    splash = true;
  }
  
  // BME280データの読み込み
  readData();
  // rawデータはreadData() の中で設定される
  temp_cal = calibration_T(temp_raw);
  hum_cal = calibration_H(hum_raw);
  press_cal = calibration_P(pres_raw);
  
  // WBGTに渡すint値
  if (temp_cal < 0) { // 氷点下の場合（そもそもWBGTに渡す必要は無いのだが）
    temp_acti = (temp_cal-49) / 100;
  } else {
    temp_acti = (temp_cal+50) / 100;
  }
  hum_acti = (hum_cal+512) / 1024;
  press_acti = (press_cal+50) / 100;
  W = calcWBGT(temp_acti, hum_acti);
  
  // ここから表示
  lcd_clear();
  
  // １行目
  for (i=340; i<=344; i++) {
    c[i-340]=EEPROM.read(i);  // 1-5:"WBGT "
  }
  if (W < 15) {
    c[5]=EEPROM.read(356);    // 6-7:"--"
    c[6]=EEPROM.read(356);
  } else {
    tmpS = String(W);
    c[5]=tmpS.charAt(0);        // 6-7:WBGT値 ex."31"
    c[6]=tmpS.charAt(1);
  }
  c[7]=EEPROM.read(357);      // 8:"C"
  lcd_setCursor(0, 0);
  lcd_printStr(c);

  // 2行目
  // LCDに渡す温度のchar値
  tmpS=String(temp_cal);
  for (i=0; i<=3; i++) {
    c[i]=tmpS.charAt(i);
  }
  tempCal2Char(temp_cal, c); // cをLCD表示文字用の温度にして返す
  c[4]=EEPROM.read(357);      // 5:"C"
  // LCDに渡す湿度のchar値
  //String 
  tmpS = String(hum_acti);
  c[5]=tmpS.charAt(0);        // 6-7:湿度値 ex."99"
  c[6]=tmpS.charAt(1);
  c[7]=EEPROM.read(358);      // 8:"%"
  lcd_setCursor(0, 1);
  lcd_printStr(c);
  
  // LEDを同期点滅。危険のW>31で激しく点滅
  // ざっくり1秒ループにする。センサ読み込みが20msecぐらい。
  digitalWrite(LED, HIGH);  // ON
  if (W >= 31) {
    for (int i=0; i<3; i++) {
      delay(125);
      digitalWrite(LED, LOW);   // OFF
      delay(125);
      digitalWrite(LED, HIGH);  // ON
    }
    delay(125);
    digitalWrite(LED, LOW);   // OFF
    delay(105);
  } else {
    delay(10);
    digitalWrite(LED, LOW);   // OFF
    delay(970);
  }
}

//int calcWBGT(double temp, double hum) {
int calcWBGT(int temp, int hum) {
  // 温湿度の値からインデックス値を得る
  int t = temp - 21;
  int h = (hum - 20)/5;
  // WBGT初期値
  int W = 0;
  // インデックス圏外のときは初期値のまま戻す
  if (t<0)   return W;
  if (t>=20) return W;
  if (h<0)   return W;
  if (h>=17) return W;
  // ここまできたらWBGTを求めて返す
  W = EEPROM.read(t*17+h);
  return W;
}

// ここからBME280関係の関数
void readTrim() {
  uint8_t data[32],i=0;
  Wire.beginTransmission(BMEadr);
  Wire.write(0x88);
  Wire.endTransmission();
  Wire.requestFrom(BMEadr,24);
  while(Wire.available()){
    data[i] = Wire.read();
    i++;
  }
  
  Wire.beginTransmission(BMEadr);
  Wire.write(0xA1);
  Wire.endTransmission();
  Wire.requestFrom(BMEadr,1);
  data[i] = Wire.read();
  i++;
  
  Wire.beginTransmission(BMEadr);
  Wire.write(0xE1);
  Wire.endTransmission();
  Wire.requestFrom(BMEadr,7);
  while(Wire.available()){
    data[i] = Wire.read();
    i++;  
  }
  dig_T1 = (data[1] << 8) | data[0];
  dig_T2 = (data[3] << 8) | data[2];
  dig_T3 = (data[5] << 8) | data[4];
  dig_P1 = (data[7] << 8) | data[6];
  dig_P2 = (data[9] << 8) | data[8];
  dig_P3 = (data[11]<< 8) | data[10];
  dig_P4 = (data[13]<< 8) | data[12];
  dig_P5 = (data[15]<< 8) | data[14];
  dig_P6 = (data[17]<< 8) | data[16];
  dig_P7 = (data[19]<< 8) | data[18];
  dig_P8 = (data[21]<< 8) | data[20];
  dig_P9 = (data[23]<< 8) | data[22];
  dig_H1 = data[24];
  dig_H2 = (data[26]<< 8) | data[25];
  dig_H3 = data[27];
  dig_H4 = (data[28]<< 4) | (0x0F & data[29]);
  dig_H5 = (data[30] << 4) | ((data[29] >> 4) & 0x0F);
  dig_H6 = data[31];   
}

void writeReg(uint8_t reg_address, uint8_t data) {
  Wire.beginTransmission(BMEadr);
  Wire.write(reg_address);
  Wire.write(data);
  Wire.endTransmission();  
}

void readData() {
  int i = 0;
  uint32_t data[8];
  Wire.beginTransmission(BMEadr);
  Wire.write(0xF7);
  Wire.endTransmission();
  Wire.requestFrom(BMEadr,8);
  while(Wire.available()){
    data[i] = Wire.read();
    i++;
  }
  pres_raw = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
  temp_raw = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
  hum_raw  = (data[6] << 8) | data[7];
}

signed long int calibration_T(signed long int adc_T) {
  signed long int var1, var2, T;
  var1 = ((((adc_T >> 3) - ((signed long int)dig_T1<<1))) * ((signed long int)dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((signed long int)dig_T1)) * ((adc_T>>4) - ((signed long int)dig_T1))) >> 12) * ((signed long int)dig_T3)) >> 14;
  
  t_fine = var1 + var2;
  T = (t_fine * 5 + 128) >> 8;
  return T; 
}

unsigned long int calibration_P(signed long int adc_P) {
  signed long int var1, var2;
  unsigned long int P;
  var1 = (((signed long int)t_fine)>>1) - (signed long int)64000;
  var2 = (((var1>>2) * (var1>>2)) >> 11) * ((signed long int)dig_P6);
  var2 = var2 + ((var1*((signed long int)dig_P5))<<1);
  var2 = (var2>>2)+(((signed long int)dig_P4)<<16);
  var1 = (((dig_P3 * (((var1>>2)*(var1>>2)) >> 13)) >>3) + ((((signed long int)dig_P2) * var1)>>1))>>18;
  var1 = ((((32768+var1))*((signed long int)dig_P1))>>15);
  if (var1 == 0)
  {
    return 0;
  }  
  P = (((unsigned long int)(((signed long int)1048576)-adc_P)-(var2>>12)))*3125;
  if(P<0x80000000)
  {
     P = (P << 1) / ((unsigned long int) var1);   
  }
  else
  {
    P = (P / (unsigned long int)var1) * 2;  
  }
  var1 = (((signed long int)dig_P9) * ((signed long int)(((P>>3) * (P>>3))>>13)))>>12;
  var2 = (((signed long int)(P>>2)) * ((signed long int)dig_P8))>>13;
  P = (unsigned long int)((signed long int)P + ((var1 + var2 + dig_P7) >> 4));
  return P;
}

unsigned long int calibration_H(signed long int adc_H) {
  signed long int v_x1;
  v_x1 = (t_fine - ((signed long int)76800));
  v_x1 = (((((adc_H << 14) -(((signed long int)dig_H4) << 20) - (((signed long int)dig_H5) * v_x1)) + 
        ((signed long int)16384)) >> 15) * (((((((v_x1 * ((signed long int)dig_H6)) >> 10) * 
        (((v_x1 * ((signed long int)dig_H3)) >> 11) + ((signed long int) 32768))) >> 10) + (( signed long int)2097152)) * 
        ((signed long int) dig_H2) + 8192) >> 14));
   v_x1 = (v_x1 - (((((v_x1 >> 15) * (v_x1 >> 15)) >> 7) * ((signed long int)dig_H1)) >> 4));
   v_x1 = (v_x1 < 0 ? 0 : v_x1);
   v_x1 = (v_x1 > 419430400 ? 419430400 : v_x1);
   return (unsigned long int)(v_x1 >> 12);   
}

// ここからLCD関係の関数
void lcd_cmd(byte x) {
  Wire.beginTransmission(LCDadr);
  Wire.write(0b00000000); // CO = 0,RS = 0
  Wire.write(x);
  Wire.endTransmission();
}

void lcd_clear() {
  lcd_cmd(0b00000001);
}

void lcd_contdata(byte x) {
  Wire.write(0b11000000); // CO = 1, RS = 1
  Wire.write(x);
}

void lcd_lastdata(byte x) {
  Wire.write(0b01000000); // CO = 0, RS = 1
  Wire.write(x);
}

// 文字の表示
void lcd_printStr(const char *s) {
  Wire.beginTransmission(LCDadr);
  while (*s) {
    if (*(s + 1)) {
      lcd_contdata(*s);
    } else {
      lcd_lastdata(*s);
    }
    s++;
  }
  Wire.endTransmission();
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
    tmp[0]=c[0];
    tmp[1]=c[1];
    tmp[2]=c[2];
    tmp[3]='.';
  } else if (value <= -100) {// -n.m
    tmp[0]=c[0];
    tmp[1]=c[1];
    tmp[2]='.';
    tmp[3]=c[2];
  } else if (value <= -10) { // -.nm
    tmp[0]=c[0];
    tmp[1]='.';
    tmp[2]=c[1];
    tmp[3]=c[2];
  } else if (value < 0) {    // -.0n
    tmp[0]=c[0];
    tmp[1]='.';
    tmp[2]='0';
    tmp[3]=c[1];
  } else if (value < 10) {   // 0.0n
    tmp[0]='0';
    tmp[1]='.';
    tmp[2]='0';
    tmp[3]=c[0];
  } else if (value < 100) {  // 0.nm
    tmp[0]='0';
    tmp[1]='.';
    tmp[2]=c[0];
    tmp[3]=c[1];
  } else if (value < 1000) { // n.ml
    tmp[0]=c[0];
    tmp[1]='.';
    tmp[2]=c[1];
    tmp[3]=c[2];
  } else {                   // nm.l
    tmp[0]=c[0];
    tmp[1]=c[1];
    tmp[2]='.';
    tmp[3]=c[2];
  }
  // 返す文字列
  c[0]=tmp[0];
  c[1]=tmp[1];
  c[2]=tmp[2];
  c[3]=tmp[3];
  return c;
}

// end of program
