#include "arduino_secrets.h"
/* 
  Oğuzhan Çelik - Solar Tracker IoT Project (FINAL VERSION)
  - 30 Saniyede bir otomatik tarama
  - uW (Mikrowatt) ve mW arası akıllı geçiş
  - Tam isimlerle OLED ekran gösterimi (Voltage, Current, Power, Angle)
*/

#include "thingProperties.h" // Bulut değişkenlerini içerir
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_SH110X.h>
#include <ESP32Servo.h>

// OLED Ekran Ayarları
#define i2c_Address 0x3c
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);

Adafruit_INA219 ina219;
Adafruit_MPU6050 mpu;
Servo myservo;

// Zamanlayıcı Değişkenleri
unsigned long lastScanTime = 0; 
const unsigned long scanInterval = 30000; // 30 saniye

// Servo Kontrol Fonksiyonu
void setServoAngle(int target) {
  int corrected = 180 - target; 
  myservo.write(corrected);
}

// En Yüksek Voltaj Tarama Algoritması
void scanForBestVoltage() {
  Serial.println("--- OTOMATIK TARAMA BASLADI ---");
  float maxV = -1.0;
  int bestA = 90;

  for (int a = 0; a <= 180; a += 15) {
    setServoAngle(a);
    delay(600); 
    float currentV = ina219.getBusVoltage_V();
    if (currentV > maxV) {
      maxV = currentV;
      bestA = a;
    }
  }
  setServoAngle(bestA);
  manualAngle = bestA; // Cloud slider güncelleme
}

void setup() {
  Serial.begin(115200);
  delay(1500); 

  // Arduino IoT Cloud Başlatma
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);

  // Donanım Başlatma
  Wire.begin(21, 22);
  display.begin(i2c_Address, true);
  
  ina219.begin();
  ina219.setCalibration_16V_400mA(); // Hassas ölçüm modu

  mpu.begin();
  myservo.attach(18);
  
  setServoAngle(90);
  display.clearDisplay();
}

void loop() {
  ArduinoCloud.update(); // Bulut senkronizasyonu

  unsigned long currentMillis = millis();

  // Otomatik Mod Kontrolü
  if (isAutoMode) {
    if (currentMillis - lastScanTime >= scanInterval) {
      lastScanTime = currentMillis;
      scanForBestVoltage();
    }
  }

  // Sensör Verilerini Oku
  voltage = ina219.getBusVoltage_V();
  current = ina219.getCurrent_mA();
  power = voltage * current; // mW
  float power_uW = power * 1000.0; // uW

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  angle = (atan2(a.acceleration.y, a.acceleration.z) * 180.0) / M_PI;

  // OLED EKRAN GÜNCELLEME (Tam İsimler)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println("SOLAR IOT TRACKER");
  display.println("-----------------");
  
  // Voltaj Bilgisi
  display.print("Voltage: "); display.print(voltage, 3); display.println(" V");
  
  // Akım Bilgisi
  display.print("Current: "); display.print(current, 2); display.println(" mA");
  
  // Güç Bilgisi (uW/mW Akıllı Geçiş)
  display.print("Power  : "); 
  if(power_uW < 10000) { 
    display.print(power_uW, 0); display.println(" uW");
  } else {
    display.print(power, 2); display.println(" mW");
  }

  // Açı Bilgisi
  display.print("Angle  : "); display.print(angle, 1); display.println(" deg");
  
  // Tarama Geri Sayımı
  display.print("Next   : "); 
  if(isAutoMode) {
    display.print((scanInterval - (currentMillis - lastScanTime)) / 1000); display.println(" s");
  } else {
    display.println("MANUAL");
  }

  // Bağlantı Durumu
  display.setCursor(0, 54);
  display.print("Cloud: "); 
  display.println(ArduinoCloud.connected() ? "ONLINE" : "OFFLINE");
  
  display.display();

  delay(200); 
}

// Dashboard Değişiklik Tetikleyicileri
void onIsAutoModeChange()  {
  if (isAutoMode) {
    lastScanTime = millis();
    scanForBestVoltage();
  }
}

void onManualAngleChange()  {
  if (!isAutoMode) {
    setServoAngle(manualAngle);
  }
}