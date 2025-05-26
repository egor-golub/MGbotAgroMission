#include <Wire.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define BOT_TOKEN ""
#define CHAT_ID ""
#define PUMP_PIN_1 14
#define PUMP_PIN_2 15
#define BME280_ADDRESS 0x77
#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET 10800
#define UPDATE_INTERVAL 60000
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, UTC_OFFSET, UPDATE_INTERVAL);
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
Adafruit_BME280 bme;
struct Plant {
  String name;
  float minTemp;
  float maxTemp;
  float minHumidity;
  float maxHumidity;
  int waterPerDay; 
};
struct WateringSchedule {
  int timesPerDay;
  float pumpRate; 
  String wateringTimes[5];
  int durationSec;
};
Plant currentPlant;
WateringSchedule watering;
bool plantConfigured = false;
bool wateringConfigured = false;
bool pumpStatus = false;
String lastWateringTime = ""; 
void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN_1, OUTPUT);
  pinMode(PUMP_PIN_2, OUTPUT);
  digitalWrite(PUMP_PIN_1, LOW);
  digitalWrite(PUMP_PIN_2, LOW);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi подключен!");
  if (!bme.begin(BME280_ADDRESS)) {
    Serial.println("Не удалось найти BME280 по адресу 0x77!");
    while (1);
  }
  timeClient.begin();
  client.setInsecure();
  bot.sendMessage(CHAT_ID, "🌱 Система полива запущена! Напишите /menu для списка команд.", "");
}

void loop() {
  timeClient.update();
  int newMessages = bot.getUpdates(bot.last_message_received + 1);
  for (int i = 0; i < newMessages; i++) {
    String text = bot.messages[i].text;
    String chat_id = bot.messages[i].chat_id;

    if (text == "/menu") {
      showMenu(chat_id);
    } 
    else if (text == "/sensor") {
      sendSensorData(chat_id);
    } 
    else if (text == "/settings") {
      showPlantSettings(chat_id);
    } 
    else if (text == "/water" && plantConfigured) {
      showWaterSettings(chat_id);
    } 
    else if (text == "/pump") {
      togglePump(chat_id);
    } 
    else if (text == "/time") {
      sendCurrentTime(chat_id);
    } 
    else if (text.indexOf('\n') != -1 && !plantConfigured) {
      parsePlantSettings(text, chat_id);
    } 
    else if (text.indexOf(' ') != -1 && plantConfigured && !wateringConfigured) {
      parseWaterSettings(text, chat_id);
    }
  }
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    checkEnvironmentalConditions();
    if (wateringConfigured) {
      checkWateringTime();
    }
  }

  delay(500);
}

void showMenu(String chat_id) {
  String menu = "📜 *Меню команд:*\n\n";
  menu += "🌡 `/sensor` - Текущие показания датчиков\n";
  menu += "🌿 `/settings` - Настроить растение\n";
  if (plantConfigured) menu += "🚰 `/water` - Настроить полив\n";
  menu += "⏱️ `/time` - Текущее время (МСК)\n";
  menu += "🔌 `/pump` - Ручное управление насосом\n";
  menu += "ℹ️ `/menu` - Показать это меню\n\n";

  if (plantConfigured) {
    menu += "📌 *Текущее растение:* " + currentPlant.name + "\n";
    menu += "💧 Норма воды: " + String(currentPlant.waterPerDay) + " мл/день\n";
    if (wateringConfigured) {
      menu += "⏰ *График полива:*\n";
      menu += "• Количество: " + String(watering.timesPerDay) + " раз/день\n";
      menu += "• Длительность: " + String(watering.durationSec) + " сек\n";
      menu += "• Мощность насоса: " + String(watering.pumpRate) + " л/мин\n";
    }
  }
  menu += "🚿 *Состояние насоса:* " + String(pumpStatus ? "ВКЛ" : "ВЫКЛ") + "\n";

  bot.sendMessage(chat_id, menu, "Markdown");
}

void sendSensorData(String chat_id) {
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;

  String message = "📊 *Текущие показания:*\n\n";
  message += "🌡 Температура: " + String(temp) + " °C\n";
  message += "💧 Влажность: " + String(hum) + " %\n";
  message += "🌀 Давление: " + String(pres) + " гПа\n";

  if (plantConfigured) {
    message += "\n⚖️ *Рекомендации:*\n";
    if (temp < currentPlant.minTemp) message += "- Температура ниже нормы!\n";
    else if (temp > currentPlant.maxTemp) message += "- Температура выше нормы!\n";
    if (hum < currentPlant.minHumidity) message += "- Влажность слишком низкая!\n";
    else if (hum > currentPlant.maxHumidity) message += "- Влажность слишком высокая!\n";
  }

  bot.sendMessage(chat_id, message, "Markdown");
}

void sendCurrentTime(String chat_id) {
  String timeStr = timeClient.getFormattedTime();
  bot.sendMessage(chat_id, "🕒 *Московское время:* " + timeStr, "Markdown");
}

void togglePump(String chat_id) {
  pumpStatus = !pumpStatus;
  digitalWrite(PUMP_PIN_1, pumpStatus ? HIGH : LOW);
  digitalWrite(PUMP_PIN_2, pumpStatus ? LOW : HIGH);
  
  bot.sendMessage(chat_id, "🚿 Насос " + String(pumpStatus ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН"), "");
  
  if (!pumpStatus) {
    delay(1000);
    digitalWrite(PUMP_PIN_2, LOW);
  }
}

void showPlantSettings(String chat_id) {
  bot.sendMessage(chat_id, "🌿 *Настройка растения*\n\n"
                          "Введите данные в формате:\n"
                          "<Название>\n"
                          "<Мин. температура>\n"
                          "<Макс. температура>\n"
                          "<Мин. влажность>\n"
                          "<Макс. влажность>\n"
                          "<Вода в день (мл)>\n\n"
                          "Пример:\n"
                          "Орхидея\n"
                          "18\n"
                          "25\n"
                          "40\n"
                          "70\n"
                          "200", "Markdown");
}

void parsePlantSettings(String text, String chat_id) {
  int lineBreak = text.indexOf('\n');
  currentPlant.name = text.substring(0, lineBreak);
  text = text.substring(lineBreak + 1);

  lineBreak = text.indexOf('\n');
  currentPlant.minTemp = text.substring(0, lineBreak).toFloat();
  text = text.substring(lineBreak + 1);

  lineBreak = text.indexOf('\n');
  currentPlant.maxTemp = text.substring(0, lineBreak).toFloat();
  text = text.substring(lineBreak + 1);

  lineBreak = text.indexOf('\n');
  currentPlant.minHumidity = text.substring(0, lineBreak).toFloat();
  text = text.substring(lineBreak + 1);

  lineBreak = text.indexOf('\n');
  currentPlant.maxHumidity = text.substring(0, lineBreak).toFloat();
  text = text.substring(lineBreak + 1);

  currentPlant.waterPerDay = text.toInt();
  plantConfigured = true;

  bot.sendMessage(chat_id, "✅ Растение *" + currentPlant.name + "* успешно настроено!\n"
                          "Теперь настройте полив командой `/water`.", "Markdown");
}

void showWaterSettings(String chat_id) {
  bot.sendMessage(chat_id, "🚰 *Настройка полива*\n\n"
                          "Введите данные в формате:\n"
                          "<Количество поливов>\n"
                          "<Мощность насоса (л/мин)>\n"
                          "<Время поливов через пробел>\n\n"
                          "Пример:\n"
                          "2\n"
                          "1.5\n"
                          "08:00 20:00", "Markdown");
}

void parseWaterSettings(String text, String chat_id) {
  int lineBreak = text.indexOf('\n');
  watering.timesPerDay = text.substring(0, lineBreak).toInt();
  text = text.substring(lineBreak + 1);

  lineBreak = text.indexOf('\n');
  watering.pumpRate = text.substring(0, lineBreak).toFloat();
  text = text.substring(lineBreak + 1);
  int spacePos = 0;
  for (int i = 0; i < watering.timesPerDay; i++) {
    spacePos = text.indexOf(' ');
    if (spacePos == -1) spacePos = text.length();
    watering.wateringTimes[i] = text.substring(0, spacePos);
    if (spacePos != text.length()) text = text.substring(spacePos + 1);
  }

  float totalWater = currentPlant.waterPerDay / 1000.0; // мл -> литры
  float waterPerSession = totalWater / watering.timesPerDay;
  watering.durationSec = (waterPerSession / watering.pumpRate) * 60;

  wateringConfigured = true;

  String message = "✅ *Полив настроен!*\n\n";
  message += "💦 Воды за полив: " + String(waterPerSession, 2) + " л\n";
  message += "⏳ Длительность: " + String(watering.durationSec) + " сек\n";
  message += "🕒 График: ";
  for (int i = 0; i < watering.timesPerDay; i++) {
    message += watering.wateringTimes[i] + " ";
  }

  bot.sendMessage(chat_id, message, "Markdown");
}

void checkEnvironmentalConditions() {
  if (!plantConfigured) return;

  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  String alerts = "";

  if (temp < currentPlant.minTemp - 2) {
    alerts += "⚠️ Температура слишком низкая! ";
    alerts += String(temp) + "°C при норме " + String(currentPlant.minTemp) + "°C\n";
  } 
  else if (temp > currentPlant.maxTemp + 2) {
    alerts += "⚠️ Температура слишком высокая! ";
    alerts += String(temp) + "°C при норме " + String(currentPlant.maxTemp) + "°C\n";
  }

  if (hum < currentPlant.minHumidity - 5) {
    alerts += "⚠️ Влажность слишком низкая! ";
    alerts += String(hum) + "% при норме " + String(currentPlant.minHumidity) + "%\n";
  } 
  else if (hum > currentPlant.maxHumidity + 5) {
    alerts += "⚠️ Влажность слишком высокая! ";
    alerts += String(hum) + "% при норме " + String(currentPlant.maxHumidity) + "%\n";
  }

  if (alerts != "") {
    bot.sendMessage(CHAT_ID, alerts, "");
  }
}

void checkWateringTime() {
  String currentTime = timeClient.getFormattedTime().substring(0, 5);

  if (currentTime.equals(lastWateringTime)) {
    return;
  }

  for (int i = 0; i < watering.timesPerDay; i++) {
    if (currentTime.equals(watering.wateringTimes[i])) {
      lastWateringTime = currentTime; 
      startWatering();
      delay(watering.durationSec * 1000);
      stopWatering();
      break;
    }
  }
}

void startWatering() {
  digitalWrite(PUMP_PIN_1, HIGH);
  pumpStatus = true;
  bot.sendMessage(CHAT_ID, "💦 *Запуск автоматического полива*", "Markdown");
}

void stopWatering() {
  digitalWrite(PUMP_PIN_1, LOW);
  digitalWrite(PUMP_PIN_2, HIGH);
  pumpStatus = false;
  delay(1000);
  digitalWrite(PUMP_PIN_2, LOW);
  bot.sendMessage(CHAT_ID, "🛑 *Полив завершен*", "Markdown");
}
