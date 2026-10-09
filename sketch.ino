#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <ESP32Servo.h>

// Chan ket noi phan cung
const byte TRIG_PIN = 5;
const byte ECHO_PIN = 19;
const byte BUTTON_PIN = 4;
const byte SERVO_PIN = 18;
const byte BUZZER_PIN = 23;

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;
Servo foodServo;

// Gio cho an tu dong: 07:00, 12:00 va 18:00
const byte feedingHours[] = {7, 12, 18};
const byte NUMBER_OF_SCHEDULES = sizeof(feedingHours);
int lastFedDay = -1;
int lastFedHour = -1;
unsigned long lastDistanceRead = 0;
unsigned long lastButtonChange = 0;
bool lastButtonState = HIGH;
int foodLevel = 0;

const bool DEMO_MODE = true;
const unsigned long DEMO_FEED_INTERVAL = 20000; // 20 giay
unsigned long lastDemoFeed = 0;

int readFoodLevel()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0)
    return foodLevel; // giu gia tri cu neu cam bien chua doc duoc

  float distance = duration * 0.0343 / 2.0;
  // Gan cam bien = day thuc an. 4 cm la day, 20 cm la can.
  return constrain(map((int)distance, 20, 4, 0, 100), 0, 100);
}

void beep(byte times = 1)
{
  for (byte i = 0; i < times; i++)
  {
    tone(BUZZER_PIN, 1500, 150);
    delay(220);
  }
}

void dispenseFood(const char *reason)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dang cho an...");
  lcd.setCursor(0, 1);
  lcd.print(reason);
  beep(2);

  foodServo.write(100); // mo cua xa thuc an
  delay(2500);
  foodServo.write(0); // dong cua xa thuc an
  delay(500);
}

bool isScheduledHour(int hour)
{
  for (byte i = 0; i < NUMBER_OF_SCHEDULES; i++)
  {
    if (hour == feedingHours[i])
      return true;
  }
  return false;
}

void showStatus(const DateTime &now)
{
  char timeText[] = "00:00:00";
  snprintf(timeText, sizeof(timeText), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(timeText);
  lcd.print(" Muc:");
  lcd.print(foodLevel);
  lcd.print("%");
  lcd.setCursor(0, 1);
  if (foodLevel < 20)
  {
    lcd.print("CAN NAP THEM!");
  }
  else
  {
    lcd.print("San sang cho an");
  }
}

void setup()
{
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(115200);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.print("May cho thu an");
  lcd.setCursor(0, 1);
  lcd.print("Khoi dong...");

  if (!rtc.begin())
  {
    lcd.clear();
    lcd.print("Loi RTC!");
    while (true)
      delay(10);
  }
  if (!rtc.isrunning())
  {
    // Wokwi dong bo dong ho luc nap chuong trinh lan dau
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  foodServo.attach(SERVO_PIN);
  foodServo.write(0);
  beep();
  delay(1000);
}

void loop()
{
  DateTime now = rtc.now();

  if (millis() - lastDistanceRead >= 1000)
  {
    lastDistanceRead = millis();
    foodLevel = readFoodLevel();
  }

  // Nut nhan muc thap: bam de cho an ngay
  bool buttonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && buttonState == LOW && millis() - lastButtonChange > 250)
  {
    lastButtonChange = millis();
    dispenseFood("Thu cong");
  }
  lastButtonState = buttonState;

  // Chi cho an mot lan trong moi gio da hen
  if (DEMO_MODE)
  {
    if (millis() - lastDemoFeed >= DEMO_FEED_INTERVAL)
    {
      lastDemoFeed = millis();
      dispenseFood("Tu dong demo");
    }
  }
  else if (now.minute() == 0 && now.second() < 3 &&
           isScheduledHour(now.hour()) &&
           (lastFedDay != now.day() || lastFedHour != now.hour()))
  {
    lastFedDay = now.day();
    lastFedHour = now.hour();
    dispenseFood("Theo lich hen");
  }

  showStatus(now);
  delay(150);
}
