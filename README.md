# i2r-02
WiFi Bluetooth PLC (4채널 릴레이, , ESP32) KC인증
![i2r-02-포트설명](https://drive.google.com/uc?id=1JGUkbzkz4xsO_-nzenS6Kv9PnJQRW6eD)

# 사양
- 정격전압 : 5V DC, 보드내에서는 5V로 설계했습니다.
- 입력전압 : 7~26V DC Free Volt, 7~26V 사이 전압을 공급하면         레귤레이터에서 5V 로 전원을 공급합니다.
- 작동온도 : -40 ℃ - 85 ℃
- 입력 : 4개, 접점만 연결되면 동작합니다. 별도의 전압을 인가하면 고장의 원인이 됩니다.
- 출력 : 1개는 30A 250VAC/30VDC
3개는 10A VAC, 10A VDC, 10A 125VAC, 10A 28VDC
- 통신: WIFI 802.11 b / g / n (802.11n에서 최대 150Mbps) 및 Bluetooth 4.2 BR / EDR + BLE
와이파이는 2.4G에 연결하세요. 5G는 동작하지 않습니다.
- RS232 통신 : 보드내에 TTL Level의 rx, tx 단자가 있습니다.

# "Play 스토어" 에서 어플을 다운로드 하세요 프로그램 하지 않아도 원격으로 모니터링/제어 할 수 있습니다.  

다운로드 QR CODE
![다운로드 QR코드](<자료/i2r-02 qr code.jpg>) ](https://drive.google.com/uc?id=10pkyNTbp9vKg8EGv4bFPIv3Bg4Hw1s85)

스마트폰에 어플 설치와 와이파이 연결을 보여줍니다. 그림을 크릭하세요  
[아두이노 소스프로그램 링크](https://github.com/kdi6033/i2r-03/tree/main/0%20Android%20App%20Program/board-i2r-03)  
[스마트폰 ionic 소스프로그램 링크](https://github.com/kdi6033/i2r-03/tree/main/0%20Android%20App%20Program)  
[![21-3 안드로이드 어플 사용 블루투스 와이파이 MQTT 통신](https://img.youtube.com/vi/FT0muFM24xc/0.jpg)](https://youtu.be/FT0muFM24xc)
 1) 4채널 릴레이  
4채널 릴레이가 탑재된 보드입니다. 릴레이 출력단에 A접점 B접점을 활용해 장치를 연결할 수 있습니다.
다양한 장치를 연결해서 손쉽게 원격제어 시스템을 구현해보세요. 모든 소스프로그램은 설명글 하단을 참조하세요.<br><br>
릴레이에 연결된 ESP32핀은<br> 
입력부:<br> 전원 DC 와 가까운것부터 <br>16 17 18 19 <br>출력부 <br>10A 출력 시작부터<br>
26 27 32 33(30A출력)

![i2r-02-포트설명](https://drive.google.com/uc?id=1pxizXd6QIjc_xDqR-Dd8z5uucSM2aN9H)
2) WiFi, BLE 통신  
ESP32가 탑재되어 WiFi, BLE 통신 가능합니다. WiFi 를 활용해 PC 및 스마트폰에서 4채널 릴레이를
원격제어 및 모니터링 가능합니다. 
BLE 를 활용해 근거리 제어가 가능합니다. IoT와 관련해 다양하게 활용 가능합니다.

![i2r-02-포트설명](https://drive.google.com/uc?id=1HpVNIifQ-3BNp22NvPgIVSQWa-dA-X8t)
# Input Output 아두이노 프로그램
입력과 출력, 온도, 습도를 측정하는 아두이노 기본 프로그램  
[아두이노 소스프로그램 링크](https://github.com/kdi6033/i2r-03/tree/main/1%20input%20ouput/in-out)  
[![Input Output 아두이노 프로그램](https://img.youtube.com/vi/CTg_foy56oA/0.jpg)](https://youtu.be/CTg_foy56oA)

# MQTT 통신 연결하기
아두이노로 mqtt 통신을 연결한다.
ArduinoJson.h 를 사용해 데이터 처리방법을 설명한다.
IoT MQTT Panel을 이용해 스마트폰으로 보드의 Relay를 제어 한다.
이 프로그램을 이용해 인터넷 상에서 원격으로 입력과 출력을 제어 할 수 있습니다.
[![MQTT 통신 연결하기](https://img.youtube.com/vi/u4NejCu5xnw/0.jpg)](https://youtu.be/u4NejCu5xnw)

