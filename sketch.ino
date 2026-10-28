#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Phan cung
const byte TRIG_PIN = 5;
const byte ECHO_PIN = 19;
const byte FEED_BUTTON_PIN = 4;
const byte REFILL_BUTTON_PIN = 15;
const byte SERVO_PIN = 18;
const byte BUZZER_PIN = 23;

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;
Servo foodServo;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Wokwi-GUEST dung khi mo phong. Khi lap mach that, doi thanh Wi-Fi cua ban.
const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASS = "";
const char *MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;

// Tien to rieng cua nhom, tranh trung lenh tren MQTT broker cong khai.
const char *TOPIC_PREFIX = "petfeeder_phutrong_azgk_2026";
const char *TOPIC_FEED_COMMAND = "petfeeder_phutrong_azgk_2026/feed/cmd";
const char *TOPIC_FOOD_LEVEL = "petfeeder_phutrong_azgk_2026/food-level";
const char *TOPIC_STATUS = "petfeeder_phutrong_azgk_2026/status";
const char *TOPIC_ALERT = "petfeeder_phutrong_azgk_2026/alert";
const char *TOPIC_SCHEDULE_COMMAND = "petfeeder_phutrong_azgk_2026/schedule/set";
const char *TOPIC_SCHEDULE_STATUS = "petfeeder_phutrong_azgk_2026/schedule";
const char *TOPIC_DEMO_COMMAND = "petfeeder_phutrong_azgk_2026/demo/set";
const char *TOPIC_DEMO_STATUS = "petfeeder_phutrong_azgk_2026/demo";

// Lich cho an thuc te
byte feedingHours[] = {7, 12, 18};
byte feedingMinutes[] = {0, 0, 0};
const byte NUMBER_OF_SCHEDULES = sizeof(feedingHours);
const int LOW_FOOD_THRESHOLD = 20;
const int FOOD_DECREMENT_PER_FEED = 15;

int lastFedDay = -1;
int lastFedHour = -1;
int lastFedMinute = -1;
int foodLevel = 100;
float lastMeasuredDistance = -1;
bool lowFoodAlerted = false;
bool lastFeedButtonState = HIGH;
bool lastRefillButtonState = HIGH;
unsigned long lastDistanceRead = 0;
unsigned long lastTelemetryPublish = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastFeedButtonChange = 0;
unsigned long lastRefillButtonChange = 0;

// Gia tri mac dinh. Co the bat/tat va thay doi thoi gian tu dashboard web.
bool demoMode = true;
unsigned long demoFeedInterval = 10000;
unsigned long lastDemoFeed = 0;

void beep(byte times = 1) {
  for (byte i = 0; i < times; i++) {
    tone(BUZZER_PIN, 1500, 150);
    delay(220);
  }
}

bool isScheduledTime(int hour, int minute) {
  for (byte i = 0; i < NUMBER_OF_SCHEDULES; i++) {
    if (hour == feedingHours[i] && minute == feedingMinutes[i]) return true;
  }
  return false;
}

void publishFoodLevel() {
  if (!mqttClient.connected()) return;
  char payload[8];
  snprintf(payload, sizeof(payload), "%d", foodLevel);
  mqttClient.publish(TOPIC_FOOD_LEVEL, payload, true);
}

void publishStatus(const char *event) {
  if (!mqttClient.connected()) return;
  char payload[160];
  snprintf(payload, sizeof(payload),
           "{\"event\":\"%s\",\"food_level\":%d,\"low_food\":%s}",
           event, foodLevel, foodLevel < LOW_FOOD_THRESHOLD ? "true" : "false");
  mqttClient.publish(TOPIC_STATUS, payload);
}

void publishSchedule() {
  if (!mqttClient.connected()) return;
  char payload[16];
  snprintf(payload, sizeof(payload), "%02d:%02d,%02d:%02d,%02d:%02d",
           feedingHours[0], feedingMinutes[0], feedingHours[1], feedingMinutes[1],
           feedingHours[2], feedingMinutes[2]);
  mqttClient.publish(TOPIC_SCHEDULE_STATUS, payload, true);
}

bool updateSchedule(const String &csv) {
  char buffer[32];
  csv.toCharArray(buffer, sizeof(buffer));
  byte newHours[NUMBER_OF_SCHEDULES];
  byte newMinutes[NUMBER_OF_SCHEDULES];
  byte index = 0;
  char *part = strtok(buffer, ",");

  while (part != nullptr && index < NUMBER_OF_SCHEDULES) {
    int hour, minute;
    if (sscanf(part, "%d:%d", &hour, &minute) != 2 || hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;
    newHours[index++] = hour;
    newMinutes[index - 1] = minute;
    part = strtok(nullptr, ",");
  }
  if (index != NUMBER_OF_SCHEDULES || part != nullptr) return false;

  for (byte i = 0; i < NUMBER_OF_SCHEDULES; i++) {
    feedingHours[i] = newHours[i];
    feedingMinutes[i] = newMinutes[i];
  }
  publishSchedule();
  publishStatus("schedule");
  return true;
}

void publishDemoSettings() {
  if (!mqttClient.connected()) return;
  char payload[64];
  snprintf(payload, sizeof(payload), "{\"enabled\":%s,\"interval_seconds\":%lu}",
           demoMode ? "true" : "false", demoFeedInterval / 1000);
  mqttClient.publish(TOPIC_DEMO_STATUS, payload, true);
}

bool updateDemoSettings(const String &message) {
  int separator = message.indexOf(',');
  if (separator < 0) return false;

  String enabledText = message.substring(0, separator);
  enabledText.trim();
  enabledText.toUpperCase();
  long seconds = message.substring(separator + 1).toInt();
  if ((enabledText != "ON" && enabledText != "OFF") || seconds < 3 || seconds > 3600) return false;

  demoMode = enabledText == "ON";
  demoFeedInterval = (unsigned long)seconds * 1000;
  lastDemoFeed = millis();
  publishDemoSettings();
  publishStatus("demo");
  return true;
}

void publishLowFoodAlert() {
  bool lowFood = foodLevel < LOW_FOOD_THRESHOLD;
  if (lowFood == lowFoodAlerted) return;

  lowFoodAlerted = lowFood;
  if (mqttClient.connected()) {
    char payload[120];
    snprintf(payload, sizeof(payload),
             "{\"low_food\":%s,\"food_level\":%d}",
             lowFood ? "true" : "false", foodLevel);
    mqttClient.publish(TOPIC_ALERT, payload, true);
  }
  if (lowFood) beep();
}

void readFoodLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return;

  float distance = duration * 0.0343 / 2.0;
  // Chi cap nhat theo HC-SR04 luc khoi dong hoac khi nguoi dung thay doi muc thuc an ro rang.
  if (lastMeasuredDistance < 0 || abs(distance - lastMeasuredDistance) >= 2.0) {
    lastMeasuredDistance = distance;
    foodLevel = constrain(map((int)distance, 20, 4, 0, 100), 0, 100);
    publishFoodLevel();
    publishLowFoodAlert();
  }
}

void refillFood(const char *source) {
  foodLevel = 100;
  // Giu lai gia tri khoang cach hien tai. Neu dat thanh 4 cm o day,
  // lan doc HC-SR04 tiep theo (vi du 8 cm trong Wokwi) se ghi de 100% thanh 75%.
  // Chi khi nguoi dung thay doi distance >= 2 cm, readFoodLevel() moi cap nhat lai.
  beep(3);
  lcd.clear();
  lcd.print("DA NAP DAY 100%");
  lcd.setCursor(0, 1);
  lcd.print(source);
  publishFoodLevel();
  publishLowFoodAlert();
  publishStatus("refill");
  delay(900);
}

void dispenseFood(const char *reason) {
  foodLevel = max(0, foodLevel - FOOD_DECREMENT_PER_FEED);
  lcd.clear();
  lcd.print("Dang cho an...");
  lcd.setCursor(0, 1);
  lcd.print(reason);
  beep(2);
  foodServo.write(100);
  delay(2500);
  foodServo.write(0);
  delay(500);
  publishFoodLevel();
  publishLowFoodAlert();
  publishStatus("feed");
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  message.trim();
  String command = message;
  command.toUpperCase();

  if (String(topic) == TOPIC_FEED_COMMAND) {
    if (command == "FEED") {
      dispenseFood("Lenh Web");
    } else if (command == "REFILL") {
      refillFood("Lenh Web");
    }
  } else if (String(topic) == TOPIC_SCHEDULE_COMMAND) {
    if (!updateSchedule(message)) publishStatus("invalid_schedule");
  } else if (String(topic) == TOPIC_DEMO_COMMAND) {
    if (!updateDemoSettings(message)) publishStatus("invalid_demo");
  }
}

void connectMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqttClient.connected()) return;
  if (millis() - lastMqttAttempt < 4000) return;
  lastMqttAttempt = millis();

  String clientId = String("esp32-") + TOPIC_PREFIX + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (mqttClient.connect(clientId.c_str())) {
    mqttClient.subscribe(TOPIC_FEED_COMMAND);
    mqttClient.subscribe(TOPIC_SCHEDULE_COMMAND);
    mqttClient.subscribe(TOPIC_DEMO_COMMAND);
    publishFoodLevel();
    publishSchedule();
    publishDemoSettings();
    publishStatus("online");
  }
}

void publishTelemetry() {
  if (!mqttClient.connected() || millis() - lastTelemetryPublish < 4000) return;
  lastTelemetryPublish = millis();
  publishFoodLevel();
  publishStatus("telemetry");
}

void showStatus(const DateTime &now) {
  char line1[17];
  snprintf(line1, sizeof(line1), "%02d:%02d Muc:%d%%", now.hour(), now.minute(), foodLevel);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  if (foodLevel < LOW_FOOD_THRESHOLD) lcd.print("CAN NAP THEM!");
  else if (mqttClient.connected()) lcd.print("Web: da ket noi");
  else lcd.print("San sang cho an");
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(FEED_BUTTON_PIN, INPUT_PULLUP);
  pinMode(REFILL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.print("May cho thu an");
  lcd.setCursor(0, 1);
  lcd.print("Khoi dong...");

  if (!rtc.begin()) {
    lcd.clear();
    lcd.print("Loi RTC!");
    while (true) delay(10);
  }
  if (!rtc.isrunning()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  foodServo.attach(SERVO_PIN);
  foodServo.write(0);
  beep();
  readFoodLevel();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  DateTime now = rtc.now();

  if (WiFi.status() == WL_CONNECTED) {
    connectMqtt();
    mqttClient.loop();
  }

  if (millis() - lastDistanceRead >= 1000) {
    lastDistanceRead = millis();
    readFoodLevel();
  }
  publishTelemetry();

  bool feedButtonState = digitalRead(FEED_BUTTON_PIN);
  if (lastFeedButtonState == HIGH && feedButtonState == LOW && millis() - lastFeedButtonChange > 250) {
    lastFeedButtonChange = millis();
    dispenseFood("Nut bam");
  }
  lastFeedButtonState = feedButtonState;

  bool refillButtonState = digitalRead(REFILL_BUTTON_PIN);
  if (lastRefillButtonState == HIGH && refillButtonState == LOW && millis() - lastRefillButtonChange > 250) {
    lastRefillButtonChange = millis();
    refillFood("Nut nap");
  }
  lastRefillButtonState = refillButtonState;

  if (demoMode && millis() - lastDemoFeed >= demoFeedInterval) {
    lastDemoFeed = millis();
    dispenseFood("Tu dong demo");
  } else if (!demoMode && now.second() < 3 && isScheduledTime(now.hour(), now.minute()) &&
             (lastFedDay != now.day() || lastFedHour != now.hour() || lastFedMinute != now.minute())) {
    lastFedDay = now.day();
    lastFedHour = now.hour();
    lastFedMinute = now.minute();
    dispenseFood("Theo lich hen");
  }

  showStatus(now);
  delay(100);
}
