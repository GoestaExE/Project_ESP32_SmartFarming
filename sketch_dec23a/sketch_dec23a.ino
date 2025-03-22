#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// akses wifi
const char* ssid = "BLACK";          // Nama WiFi untuk koneksi
const char* password = "NagaPage248";  // Password WiFi

// Telegram Bot
const char* botToken = "7544579849:AAGh_SjID1Mi-JxTSpHvB0GhzEKeMVvFZIw";  // Token bot Telegram
const char* chatID = "1042807202";                                        // ID chat Telegram untuk menerima pesan

// Initialize Telegram Bot
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

// Define DHT11 pin and type
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Soil moisture sensor
#define SOIL_MOISTURE_PIN 34
#define PUMP_PIN 5 // Relay pin
#define FAN_PIN 14 // Fan relay pin

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Variables
unsigned long previousMillis = 0;
int soilMoistureThreshold = 50;
int pumpDuration = 1; // Durasi pompa aktif dalam detik
bool isPumpRunning = false;
unsigned long pumpStartMillis = 0;
bool manualPumpControl = false;
bool systemDisabled = false;
bool isFanRunning = false;
bool manualFanControl = false;
int lastSoilMoisture = 0;
unsigned long checkStartMillis = 0;
int pumpActivationCount = 0; // Counter untuk jumlah pompa aktif

void setup() {
  Serial.begin(115200);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("LCD Ready");
  Serial.println("LCD Initialized!");

  // Initialize DHT sensor
  dht.begin();

  // Initialize pump and fan pins
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH); // Ensure the pump (relay) is OFF initially
  digitalWrite(FAN_PIN, HIGH); // Ensure the fan (relay) is OFF initially

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Allow time for SSL handshake
  client.setInsecure();
}

void loop() {
  // Read DHT sensor data
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Read soil moisture level
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
  int soilMoisturePercent = map(soilMoistureValue, 4095, 0, 0, 100);

  // Display data on LCD
  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.print(temperature);
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("Humi:");
  lcd.print(humidity);
  lcd.print("%");
  lcd.setCursor(9, 1);
  lcd.print("Soil:");
  lcd.print(soilMoisturePercent);
  lcd.print("%");

  // Process Telegram messages
  int messageCount = bot.getUpdates(bot.last_message_received + 1);
  while (messageCount) {
    for (int i = 0; i < messageCount; i++) {
      String text = bot.messages[i].text;
      String fromName = bot.messages[i].from_name;

      if (text == "/pump_on") {
        if (!systemDisabled) {
          manualPumpControl = true;
          digitalWrite(PUMP_PIN, LOW); // Mengaktifkan Pompa (relay)
          bot.sendMessage(chatID, "Pompa diaktifkan secara manual.", "Markdown");
        } else {
          bot.sendMessage(chatID, "Sistem dinonaktifkan. Periksa pasokan air.", "Markdown");
        }
      } else if (text == "/pump_off") {
        manualPumpControl = false;
        digitalWrite(PUMP_PIN, HIGH); // Mematikan Pompa (relay)
        bot.sendMessage(chatID, "Pompa dinonaktifkan secara manual.", "Markdown");
      } else if (text == "/fan_on") {
        manualFanControl = true;
        digitalWrite(FAN_PIN, LOW); // Mengaktifkan Kipas (relay)
        isFanRunning = true;
        bot.sendMessage(chatID, "Kipas diaktifkan secara manual.", "Markdown");
      } else if (text == "/fan_off") {
        manualFanControl = false;
        digitalWrite(FAN_PIN, HIGH); // Mematikan Kipas (relay)
        isFanRunning = false;
        bot.sendMessage(chatID, "Kipas dinonaktifkan secara manual.", "Markdown");
      } else if (text == "/matikan") {
        systemDisabled = true;
        manualPumpControl = false;
        digitalWrite(PUMP_PIN, HIGH); // Mematikan Pompa (relay)
        bot.sendMessage(chatID, "Sistem dinonaktifkan: Pasokan air kosong.", "Markdown");
      } else if (text == "/hidupkan") {
        systemDisabled = false;
        pumpActivationCount = 0; // Reset counter pompa
        bot.sendMessage(chatID, "Sistem diaktifkan: Pasokan air dipulihkan.", "Markdown");
      }
    }
    messageCount = bot.getUpdates(bot.last_message_received + 1);
  }

  // Cek soil moisture dan kontrol pompa
  if (!manualPumpControl && !systemDisabled) {
    if (soilMoisturePercent < soilMoistureThreshold && !isPumpRunning) {
      digitalWrite(PUMP_PIN, LOW); // Pompa aktif
      isPumpRunning = true;
      pumpStartMillis = millis();
      checkStartMillis = millis();
      lastSoilMoisture = soilMoisturePercent;
      pumpActivationCount++;
      Serial.println("Pompa diaktifkan");
      bot.sendMessage(chatID, "Pompa diaktifkan: Kelembaban tanah rendah.", "Markdown");

      if (pumpActivationCount >= 5) {
        systemDisabled = true;
        digitalWrite(PUMP_PIN, HIGH); // matikan pompa
        bot.sendMessage(chatID, "Sistem dinonaktifkan: Pompa aktif 5 kali tanpa peningkatan kelembaban.Kirim perintah /hidupkan untuk mengaktifkan kembali.", "Markdown");
      }
    }

    if (soilMoisturePercent >= soilMoistureThreshold && isPumpRunning) {
      digitalWrite(PUMP_PIN, HIGH); // matikan pompa
      isPumpRunning = false;
      Serial.println("Pompa dinonaktifkan karena tanah lembab");
      bot.sendMessage(chatID, "Pompa dinonaktifkan: Kelembaban tanah cukup.", "Markdown");
    }

    if (isPumpRunning && millis() - pumpStartMillis >= pumpDuration * 1000) {
      digitalWrite(PUMP_PIN, HIGH); // matikan pompa
      isPumpRunning = false;
      Serial.println("Pompa dinonaktifkan karena durasi maksimum");
      bot.sendMessage(chatID, "Pompa dinonaktifkan: Durasi maksimum tercapai.", "Markdown");
    }

    if (isPumpRunning && millis() - checkStartMillis >= 5000) {
      if (soilMoisturePercent <= lastSoilMoisture) {
        digitalWrite(PUMP_PIN, HIGH); // matikan pompa
        isPumpRunning = false;
        systemDisabled = true; // matikan sistem
        Serial.println("Sistem dinonaktifkan: Kelembaban tidak meningkat");
        bot.sendMessage(chatID, "Sistem dinonaktifkan: Tidak ada peningkatan kelembaban tanah setelah 5 detik. Kirim perintah /hidupkan untuk mengaktifkan kembali.", "Markdown");
      } else { 
        lastSoilMoisture = soilMoisturePercent;
        checkStartMillis = millis(); // mengulang timer
      }
    }
  } 

  // Cek kelembapan udara dan kontrol kipas
  if (!manualFanControl && !systemDisabled) {
    if (humidity > 70 && !isFanRunning) {
      digitalWrite(FAN_PIN, LOW); // Aktifkan Kipas (relay)
      isFanRunning = true;
      Serial.println("Kipas diaktifkan karena kelembaban tinggi");
      bot.sendMessage(chatID, "Kipas diaktifkan: Kelembaban udara tinggi.", "Markdown");
    }

    if (humidity <= 70 && isFanRunning) {
      digitalWrite(FAN_PIN, HIGH); // Matikan Kipas (relay)
      isFanRunning = false;
      Serial.println("Kipas dinonaktifkan karena kelembaban normal");
      bot.sendMessage(chatID, "Kipas dinonaktifkan: Kelembaban udara normal.", "Markdown");
    }
  }

  // Send data to Telegram
  String message = "Smart Farming Update:\n";
  message += "Temperature: " + String(temperature) + " C\n";
  message += "Humidity: " + String(humidity) + " %\n";
  message += "Soil Moisture: " + String(soilMoisturePercent) + " %";

  bot.sendMessage(chatID, message, "Markdown");

  delay(1000);
}
