# ArduinoWBGT
Indoor WBGT meter with ATmega328P and ATtiny85<br />
日本生気象学会の「日常生活における熱中症予防」 http://seikishou.jp/heatstroke.html の指針ver.3 図2に掲載されている、日射の無い屋内での暑さ指数推定表を元にして、Arduinoで測定した温度・湿度から暑さ指数を表示するスケッチです。しつこいようですが屋内限定です。<br />
温湿度の測定にはBoschのBME280を用い、表示には秋月電子通商のAQM0802Aを用いています。<br />
当初ATmega328P(flash/EEPROM/RAM = 32k/1k/2k byte)向けに書いていたものを、ATtiny85(8k/512/512)向けに移植することを思い立ち、その移植経過が分かるようにブランチを分けています。<br />
mainブランチにあるWBGT01.inoが当初のスケッチ、<br />
WBGT03ブランチにあるものがATmega328P上でメモリ削減したスケッチ、<br />
WBGT03sブランチにあるものがATtiny85上で動作しているスケッチです。<br />
ご利用は自己責任でお願いします。
