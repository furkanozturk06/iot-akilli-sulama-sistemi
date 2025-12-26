#define BLYNK_TEMPLATE_ID "TMPL6Sllx9HHy"
#define BLYNK_TEMPLATE_NAME "Smart Plant Watering System"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>

char auth[] = "dNmpFwtpElHwff7kElaYUCG3nm34gR_u";
char ssid[] = "Galaxy";
char pass[] = "12345678";

String sheetURL = "https://script.google.com/macros/s/AKfycbxkCaclTNAgHDNEgsBcCYbiS5sIg3JczNKqv0ue_5zclNU9fRyE8xTxU1sfSu7a9mwT3A/exec";

#define DHTPIN 26
#define DHTTYPE DHT11
#define SOIL_PIN 34
#define LDR_PIN 35
#define WATER_PIN 32

#define RELAY_PIN 27

#define LCD_SDA 33
#define LCD_SCL 25
LiquidCrystal_I2C lcd(0x27, 16, 2);

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

float g_temp = 0;
float g_hum  = 0;
int   g_soil = 0;
int   g_light = 0;
int   g_water = 0;
int plantType = 1;
int soil_threshold = 20;
bool manualMode = false;

int lcdPage = 0;

bool lowWaterLogged = false;
bool lowLightLogged = false;
bool lowSoilLogged  = false;

#define LOW_WATER_THRESHOLD 10
#define LOW_LIGHT_THRESHOLD 15
#define LOW_SOIL_THRESHOLD  50
#define SOIL_WATER_THRESHOLD 20
#define TEMP_THRESHOLD 25
#define HUM_THRESHOLD  50

BLYNK_CONNECTED() {
  lowWaterLogged = false;
  lowLightLogged = false;
  lowSoilLogged  = false;
}

void sendToSheet() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(sheetURL);
    http.addHeader("Content-Type", "application/json");

    bool pumpState = (digitalRead(RELAY_PIN) == LOW);

    String payload = "{";
    payload += "\"soil\":" + String(g_soil) + ",";
    payload += "\"temp\":" + String(g_temp) + ",";
    payload += "\"hum\":" + String(g_hum) + ",";
    payload += "\"water\":" + String(g_water) + ",";
    payload += "\"pump\":" + String(pumpState);
    payload += "}";

    int code = http.POST(payload);
    Serial.print("HTTP Response ");
    Serial.println(code);

    http.end();
  }
}

bool canPumpRun() {
  if (g_water <= -1) {
    Serial.println("SAFE BLOCK Low water level! Pump STOPPED");
    digitalWrite(RELAY_PIN, HIGH);
    Blynk.virtualWrite(V1, 0);
    manualMode = false;
    return false;
  }
  return true;
}

BLYNK_WRITE(V1) {
  int pumpState = param.asInt();

  if (pumpState == 1) {
    if (!canPumpRun()) return;
    manualMode = true;
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("MANUAL - PUMP ON");
  } else {
    manualMode = false;
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("MANUAL - PUMP OFF , AUTO ACTIVE");
  }
}

BLYNK_WRITE(V5) {
  plantType = param.asInt();

  if (plantType == 1) soil_threshold = 0;
  if (plantType == 2) soil_threshold = 50;
  if (plantType == 3) soil_threshold = 85;

  Serial.print("Bitki modu ");
  Serial.println(plantType);

  Serial.print("Yeni esik = ");
  Serial.println(soil_threshold);
}

void readDHT() {
  g_hum  = dht.readHumidity();
  g_temp = dht.readTemperature();

  Serial.print("Temp ");
  Serial.print(g_temp);
  Serial.print(" C   Humidity ");
  Serial.println(g_hum);

  Blynk.virtualWrite(V6, g_temp);
  Blynk.virtualWrite(V7, g_hum);
}

void checkDHTAlerts() {
  if (g_temp > TEMP_THRESHOLD) {
    Serial.println("ALERT Sicaklik yuksek!");
    Blynk.logEvent("low_temp", "Sicaklik esigin ustunde!");
  }

  if (g_hum > HUM_THRESHOLD) {
    Serial.println("ALERT Nem seviyesi yuksek!");
    Blynk.logEvent("low_humidity", "Nem seviyesi esigin ustunde!");
  }
}

void readSoil() {
  int raw = analogRead(SOIL_PIN);
  g_soil = map(raw, 0, 4095, 100, 0);

  Serial.print("Soil Moisture ");
  Serial.println(g_soil);

  Blynk.virtualWrite(V0, g_soil);
}

void readLight() {
  int raw = analogRead(LDR_PIN);
  g_light = map(raw, 0, 4095, 0, 100);

  Serial.print("Light ");
  Serial.println(g_light);

  Blynk.virtualWrite(V2, g_light);
}

void autoPumpControl() {
  if (manualMode) return;

  if (g_soil < soil_threshold) {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("AUTO - PUMP ON");
  }

  if (g_soil >= soil_threshold) {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("AUTO - PUMP OFF");
  }
}

void readWater() {
  int raw = analogRead(WATER_PIN);
  g_water = map(raw, 0, 4095, 0, 100);

  Serial.print("Water Level ");
  Serial.println(g_water);

  Blynk.virtualWrite(V3, g_water);
}

void checkEvents() {
  if (!Blynk.connected()) return;

  if (g_water < LOW_WATER_THRESHOLD && !lowWaterLogged) {
    Blynk.logEvent("low_water_level", "Su seviyesi cok dusuk!");
    lowWaterLogged = true;
  }
  if (g_water >= LOW_WATER_THRESHOLD) {
    lowWaterLogged = false;
  }

  if (g_light < LOW_LIGHT_THRESHOLD && !lowLightLogged) {
    Blynk.logEvent("low_light", "Isik seviyesi cok dusuk!");
    lowLightLogged = true;
  }
  if (g_light >= LOW_LIGHT_THRESHOLD) {
    lowLightLogged = false;
  }

  if (g_soil < LOW_SOIL_THRESHOLD && !lowSoilLogged) {
    Blynk.logEvent("low_moisture", "Toprak nemi cok dusuk!");
    lowSoilLogged = true;
  }
  if (g_soil >= LOW_SOIL_THRESHOLD) {
    lowSoilLogged = false;
  }
}

void updateLCD() {
  lcd.clear();

  switch (lcdPage) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Sicaklik");
      lcd.print(g_temp);
      lcd.print("C");
      lcd.setCursor(0, 1);
      lcd.print("Nem");
      lcd.print(g_hum);
      lcd.print("%");
      break;

    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Toprak Nemi");
      lcd.setCursor(0, 1);
      lcd.print(g_soil);
      lcd.print("%");
      break;

    case 2:
      lcd.setCursor(0, 0);
      lcd.print("Isik Seviyesi");
      lcd.setCursor(0, 1);
      lcd.print(g_light);
      lcd.print("%");
      break;

    case 3:
      lcd.setCursor(0, 0);
      lcd.print("Su Seviyesi");
      lcd.setCursor(0, 1);
      lcd.print(g_water);
      lcd.print("%");
      break;
  }

  lcdPage++;
  if (lcdPage > 3) lcdPage = 0;
}

void setup() {
  Serial.begin(115200);
  Serial.println("SETUP BASLADI!");

  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.begin();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Plant");
  lcd.setCursor(0, 1);
  lcd.print("System Start");
  delay(1500);
  lcd.clear();

  dht.begin();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  Serial.println("Blynk baglaniyor...");
  Blynk.begin(auth, ssid, pass);
  Serial.println("Blynk baglandi!");

  timer.setInterval(5000, readDHT);
  timer.setInterval(2000, readSoil);
  timer.setInterval(2000, readLight);
  timer.setInterval(2000, readWater);
  timer.setInterval(2000, updateLCD);
  timer.setInterval(3000, checkEvents);
  timer.setInterval(3000, autoPumpControl);
  timer.setInterval(3000, checkDHTAlerts);
  timer.setInterval(10000, sendToSheet);
  Wire.setClock(100000);
}

void loop() {
  Blynk.run();
  timer.run();
}
