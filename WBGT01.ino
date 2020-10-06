// BME280で測定した屋内の温湿度を元に暑さ指数WBGTを求め、秋月電子8x2LCDに表示する
// 2020/08/27 by パドラッパ (twitter: @pado3)
// 2020/08/30 LCDとBME280のVddをVcc固定から電圧可変にした（3.3V品はPWM=168）
// 2020/09/22 カラーLEDを点けるのに失敗。赤色LEDを13から移動して上半分で完結させた
// 参考:
// 屋内での暑さ指数（指針の図2） http://seikishou.jp/heatstroke.html
// 使用した旧Arduino nano https://www.switch-science.com/catalog/750/
// LCDの商品ページ・使い方 https://www.switch-science.com/catalog/1405/
// BME280の商品ページ・使い方 https://www.switch-science.com/catalog/2236/
#include <Wire.h>

// pin assign
#define LED   2     // LED @ D13 (2.2k)
#define VDD   3     // センサ・液晶用のVdd @ D3
#define Vin  14     // LCD VolumeのADC @ D14=A0
#define VOLH 15     // LCD VolumeのH側 @ D15=A1 (10k), LはGND直結
#define sdaPin 18   // Arduino A4
#define sclPin 19   // Arduino A5
// I2C addresses
#define LCDadr 0x3e // LCD address, fixed
#define BMEadr 0x76 // BME280 address, SDO:L

float Vdd_V = 3.3;  // センサ・液晶のVdd電圧 3.3V or 5.0V in typical
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
  // Contrast reader setup
  pinMode(VDD,  OUTPUT);
  pinMode(VOLH, OUTPUT);
  pinMode(Vin,  INPUT);
  analogWrite(VDD, int(255*Vdd_V/5));    // LCDとBME280の電源電圧(value/255)*5V
  digitalWrite(VOLH, HIGH); // 5V
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
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("START " __FILE__ " from " __DATE__ " " __TIME__));
  
  Wire.begin();
  writeReg(0xF2,ctrl_hum_reg);
  writeReg(0xF4,ctrl_meas_reg);
  writeReg(0xF5,config_reg);
  readTrim();
  
  // LED setup
  pinMode(LED, OUTPUT);
  
  // LCD setup
  delay(500);
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
  delay(2);
}


void loop() {
  double temp_act = 0.0, press_act = 0.0, hum_act=0.0;
  signed long int temp_cal;
  unsigned long int press_cal, hum_cal;
  // 屋内専用であることのスプラッシュ表示 兼 BME280の初期化待ち
  if (!splash) {
    lcd_clear();
    lcd_setCursor(0, 0);
    lcd_printStr("WBGT use");
    lcd_setCursor(0, 1);
    lcd_printStr("  indoor");
    delay(2000);
    splash = true;
  }
  // BME280データの読み込み
  readData();
  // rawデータはreadData() の中で設定される
  temp_cal = calibration_T(temp_raw);
  hum_cal = calibration_H(hum_raw);
  press_cal = calibration_P(pres_raw);
  temp_act = (double)temp_cal / 100.0;
  hum_act = (double)hum_cal / 1024.0;
  press_act = (double)press_cal / 100.0;
  // WBGT計算のデバッグ用
  // temp_act = 40.0;
  // hum_act  = 99.0;
  // calc WBGT
  int W = calcWBGT(temp_act, hum_act);
  // シリアルモニタで確認できるようにしておく
  Serial.print("TEMP: ");
  Serial.print(temp_act);
  Serial.print("degC, HUM: ");
  Serial.print(hum_act);
  Serial.print("%RH, PRESS: ");
  Serial.print(press_act);
  Serial.print("hPa, WBGT: ");
  Serial.print(W);
  Serial.print("degC, index[");
  Serial.print(round(temp_act - 21));
  Serial.print(", ");
  Serial.print(round((hum_act - 20)/5));
  Serial.print("], contrast: ");
  Serial.println(contrast);
  // ここから表示
  lcd_clear();
  // WBGT
  lcd_setCursor(0, 0);
  lcd_printStr("WBGT ");
  if (W < 15) {
    lcd_printStr("--");
  } else {
    lcd_printInt(W);
  }
  lcd_printStr("C");
  // 温湿度
  char tmp[16];
  char hum[16];
  dtostrf(temp_act, 4, 1, tmp);
  dtostrf(hum_act, 2, 0, hum);
  lcd_setCursor(0, 1);
  lcd_printStr(tmp);
  lcd_printStr("C");
  lcd_printStr(hum);
  lcd_printStr("%");
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
  // コントラスト設定値をシリアルコンソールで確認する用。設定はリセット時のみ。
  contrast = analogRead(Vin) >> 4;
}

int calcWBGT(double temp, double hum) {
  // 上から下:21-40°C, 左から右:5ステップで20-100%RH
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
  // 温湿度の値からインデックス値を得る
  int t = round(temp - 21);
  int h = round((hum - 20)/5);
  // WBGT初期値
  int W = 0;
  // インデックス圏外のときは初期値のまま戻す
  if (t<0)   return W;
  if (t>=20) return W;
  if (h<0)   return W;
  if (h>=17) return W;
  // ここまできたらWBGTを求めて返す
  W = wbgt[t][h];
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

// 整数の表示
void lcd_printInt(int num) {
  char int2str[10];
  sprintf(int2str,"%d",num);
  lcd_printStr(int2str);  
}

// 表示位置の指定
void lcd_setCursor(byte x, byte y) {
  lcd_cmd(0x80 | (y * 0x40 + x));
}
