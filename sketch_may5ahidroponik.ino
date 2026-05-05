// =====================================================
//  SISTEM HIDROPONIK INDOOR CERDAS - ESP32
//  Fitur: 2x Water Level, DHT22, pH, 2x LDR, LCD I2C
// =====================================================

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// 1. KONFIGURASI — GANTI SESUAI DATA KAMU
// =====================================================
#define WIFI_SSID       "sya"
#define WIFI_PASSWORD   "22222222"
#define FIREBASE_HOST   "monitoring-hidroponik-caisim-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH   "TDddGUUEWfh6SVtdrqMvBbSqqhMyFw0dIL8JT5ug"

// =====================================================
// 2. PIN KONFIGURASI
// =====================================================
#define DHT_PIN              15    // Pin D4
#define DHT_TYPE             DHT22
#define LDR_GROWLIGHT_PIN    35   // Sinyal Digital (DO) - Pin D35
#define LDR_MATAHARI_PIN     15   // Sinyal Analog (AO)  - Pin D15
#define WATER_LEVEL_1_PIN    32   // Sinyal Analog (AO)  - Pin D32
#define WATER_LEVEL_2_PIN    33   // Sinyal Analog (AO)  - Pin D33
#define PH_SENSOR_PIN        34   // Sinyal Analog (AO)  - Pin D34
#define FAN_PIN              25   // Pin D25
#define GROWLIGHT_PIN        26   // Pin D26
#define POMPA_PIN            27   // Pin D27

// =====================================================
// 3. THRESHOLD (Batas Otomatisasi)
// =====================================================
#define SUHU_MAKS            28.0
#define LDR_MATAHARI_MIN     1500 
#define WATER_LEVEL_RENDAH   800  
#define FIREBASE_INTERVAL    5000 

// =====================================================
// 4. INISIALISASI OBJEK & VARIABEL
// =====================================================
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 20, 4);
FirebaseData    fbdo;
FirebaseAuth    auth;
FirebaseConfig  config;

float suhu = 0, kelembaban = 0, ph = 0;
int water1 = 0, water2 = 0, ldrSun = 0;
bool statusGelapGrow = false; 
bool statusFan = false, statusGrowlight = false, statusPompa = false;

unsigned long lastFirebase = 0, lastLcd = 0;
int lcdPage = 0;

// =====================================================
// 5. SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  // Mode Pin
  pinMode(LDR_GROWLIGHT_PIN, INPUT); 
  pinMode(FAN_PIN, OUTPUT);
  pinMode(GROWLIGHT_PIN, OUTPUT);
  pinMode(POMPA_PIN, OUTPUT);

  // Matikan semua aktuator di awal
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(GROWLIGHT_PIN, LOW);
  digitalWrite(POMPA_PIN, LOW);

  dht.begin();
  Wire.begin(); // Menggunakan pin default SDA(21) & SCL(22)
  lcd.begin();
  lcd.backlight();
  
  lcd.setCursor(0, 0); lcd.print("Hidroponik V2.1");
  lcd.setCursor(0, 1); lcd.print("WiFi Connecting");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Konfigurasi Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  lcd.clear();
  lcd.print("Firebase Ready");
  delay(1500);
}

// =====================================================
// 6. LOOP UTAMA
// =====================================================
void loop() {
  // --- BACA SENSOR ---
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) suhu = t;
  if (!isnan(h)) kelembaban = h;

  ldrSun = analogRead(LDR_MATAHARI_PIN);
  water1 = analogRead(WATER_LEVEL_1_PIN);
  water2 = analogRead(WATER_LEVEL_2_PIN);
  
  // Baca pH (0-14)
  int phRaw = analogRead(PH_SENSOR_PIN);
  ph = (phRaw * 14.0) / 4095.0;

  // Baca LDR Grow Digital (Modul DO biasanya HIGH saat GELAP)
  statusGelapGrow = digitalRead(LDR_GROWLIGHT_PIN);

  // --- LOGIKA OTOMATIS ---
  // 1. Kipas (Fan)
  statusFan = (suhu > SUHU_MAKS);
  digitalWrite(FAN_PIN, statusFan ? HIGH : LOW);

  // 2. Growlight (Nyala jika dalam gelap DAN luar tidak ada matahari)
  statusGrowlight = (statusGelapGrow == HIGH && ldrSun < LDR_MATAHARI_MIN);
  digitalWrite(GROWLIGHT_PIN, statusGrowlight ? HIGH : LOW);

  // 3. Pompa (Nyala jika salah satu sensor air mendeteksi level rendah)
  statusPompa = (water1 < WATER_LEVEL_RENDAH || water2 < WATER_LEVEL_RENDAH);
  digitalWrite(POMPA_PIN, statusPompa ? HIGH : LOW);

  // --- KIRIM KE FIREBASE ---
  if (millis() - lastFirebase > FIREBASE_INTERVAL) {
    lastFirebase = millis();
    if (Firebase.ready()) {
      Firebase.setFloat(fbdo, "/sensor/suhu", suhu);
      Firebase.setFloat(fbdo, "/sensor/kelembaban", kelembaban);
      Firebase.setFloat(fbdo, "/sensor/ph", ph);
      Firebase.setInt(fbdo, "/sensor/water1", water1);
      Firebase.setInt(fbdo, "/sensor/water2", water2);
      Firebase.setInt(fbdo, "/sensor/ldr_matahari", ldrSun);
      Firebase.setBool(fbdo, "/aktuator/fan", statusFan);
      Firebase.setBool(fbdo, "/aktuator/growlight", statusGrowlight);
      Firebase.setBool(fbdo, "/aktuator/pompa", statusPompa);
      Serial.println("Firebase Updated!");
    }
  }

  // --- UPDATE LCD (Bergantian tiap 3 detik) ---
  if (millis() - lastLcd > 3000) {
    lastLcd = millis();
    lcd.clear();
    if (lcdPage == 0) {
      lcd.setCursor(0,0); lcd.printf("T:%.1fC H:%.1f%%", suhu, kelembaban);
      lcd.setCursor(0,1); lcd.printf("pH:%.1f W1:%d", ph, water1);
      lcdPage = 1;
    } else {
      lcd.setCursor(0,0); lcd.printf("W2:%d Sun:%d", water2, ldrSun);
      lcd.setCursor(0,1); lcd.printf("F:%s G:%s P:%s", 
                        statusFan?"ON":"OFF", 
                        statusGrowlight?"ON":"OFF", 
                        statusPompa?"ON":"OFF");
      lcdPage = 0;
    }
  }

  delay(200); // Small delay untuk stabilitas
}