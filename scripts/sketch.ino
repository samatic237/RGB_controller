#include <Adafruit_NeoPixel.h>
#include <DHT.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define MQ135_PIN A0
#define LED_PIN 7
#define BUTTON_PIN 6
#define DHT_PIN 5
#define DHT_POWER_PIN 4
#define LCD_POWER_PIN 3
#define LED_COUNT 60
#define LM335_PIN A1
#define R_DIVIDER 1000.0  // Сопротивление делителя (1кОм)
const int maxModes = 5;

// Адреса EEPROM
#define EEPROM_BRIGHTNESS 0
#define EEPROM_MODE 1
#define EEPROM_SPEED 2
#define EEPROM_DHT_POWER 3
#define EEPROM_LCD_POWER 4
#define EEPROM_LCD_MODE 5
#define EEPROM_LCD_BACKLIGHT 6
#define EEPROM_VALUES 10

// Добавляем константы для расчёта MQ-135
#define MQ135_RLOAD 1.0    // Сопротивление нагрузки в кОм
#define MQ135_RZERO 5.57 // Калибровочное сопротивление в чистом воздухе
#define MQ135_PARAM_A 116.6020682
#define MQ135_PARAM_B 2.769034857
#define R0 50

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
DHT dht(DHT_PIN, DHT11);
LiquidCrystal_I2C lcd(0x27, 20, 4); // Адрес 0x27, 20 символов, 4 строки

struct ModeParams {
  int values[5] = {0};
} modeParams[maxModes];

// Глобальные переменные
int mode = 0;
bool needClear = false;
int brightness = 255;
bool dhtPower = true;
bool lcdPower = true;
bool lcdBacklight = true;
int lcdMode = 1; // 0-ручной, 1-датчик, 2-статус
int serialSpeed = 9600;
float stableTemp = 0.0;

// Добавляем переменные для хранения предыдущих значений
String lcdLines[4] = {"", "", "", ""};
bool lcdNeedsUpdate = false;
String lcdContent[4] = {"", "", "", ""};
bool lcdContentChanged = true;

// Переменные для кнопки
int buttonState = 0;
int lastButtonState = 0;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Переменные для DHT11
float minTemp = 1000, maxTemp = -1000;
float minHum = 1000, maxHum = -1000;
float tempSum = 0, humSum = 0;
int sampleCount = 0;
unsigned long lastSampleTime = 0;
unsigned long lastDHTReadTime = 0;
unsigned long lastLCDUpdateTime = 0;
float lastTemp = 0, lastHum = 0;

//переменные для MQ135
float mq135PPM = 0;
float mq135SumPPM = 0;
float minPPM = 10000;
float maxPPM = 0;
int mq135SampleCount = 0;
unsigned long lastMQ135ReadTime = 0;

void setup() {
  loadSettings();
  Serial.begin(serialSpeed);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(DHT_POWER_PIN, OUTPUT);
  pinMode(LCD_POWER_PIN, OUTPUT);
  pinMode(MQ135_PIN, INPUT);
  pinMode(LM335_PIN, INPUT);
  
  digitalWrite(DHT_POWER_PIN, dhtPower ? HIGH : LOW);
  digitalWrite(LCD_POWER_PIN, lcdPower ? HIGH : LOW);
  
  dht.begin();
  strip.begin();
  strip.setBrightness(brightness);
  strip.show();
  
  if (lcdPower) {
    lcd.init();
    if (lcdBacklight) {
      lcd.backlight();
    } else {
      lcd.noBacklight();
    }
    updateLCD();
  }
  
  Serial.println(F("Система готова. Введите 'help' для списка команд"));
}

void loop() {
  unsigned long currentMillis = millis();
  
  handleButton();
  handleSerialCommands();
  
  if (needClear) {
    strip.clear();
    strip.show();
    needClear = false;
  }

  // Обработка режимов
  switch(mode) {
    case 0: break; // Ручное управление
    case 1: colorWipe(strip.Color(modeParams[1].values[0], modeParams[1].values[1], modeParams[1].values[2]), modeParams[1].values[3]); break;
    case 2: rainbow(modeParams[2].values[0]); break;
    case 3: theaterChase(strip.Color(modeParams[3].values[0], modeParams[3].values[1], modeParams[3].values[2]), modeParams[3].values[3]); break;
    case 4: colorFade(modeParams[4].values[0]); break;
  }
  
  // Чтение DHT11 каждые 2 секунды
  if (dhtPower && currentMillis - lastDHTReadTime >= 2000) {
    lastDHTReadTime = currentMillis;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if (!isnan(t) && !isnan(h)) {
      lastTemp = t;
      lastHum = h;
      
      // Обновление мин/макс
      if (t < minTemp) minTemp = t;
      if (t > maxTemp) maxTemp = t;
      if (h < minHum) minHum = h;
      if (h > maxHum) maxHum = h;
      
      // Для средних значений
      tempSum += t;
      humSum += h;
      sampleCount++;
    }
  }
  if (currentMillis - lastMQ135ReadTime >= 1000) {
    lastMQ135ReadTime = currentMillis;
    mq135PPM = getMQ135PPM();
    
    // Обновление мин/макс
    if (mq135PPM < minPPM) minPPM = mq135PPM;
    if (mq135PPM > maxPPM) maxPPM = mq135PPM;
    
    // Для средних значений
    mq135SumPPM += mq135PPM;
    mq135SampleCount++;
  }
  
  static unsigned long lastTempRead = 0;
  if (millis() - lastTempRead >= 2000) {
    lastTempRead = millis();
    stableTemp = readStableTemperature();
  }
  
  // Обновление LCD только при необходимости
  if (lcdPower && (currentMillis - lastLCDUpdateTime >= 1000 || lcdContentChanged)) {
    lastLCDUpdateTime = currentMillis;
    updateLCD();
  }
  
  // Сброс средних каждую минуту
  if (currentMillis - lastSampleTime >= 60000) {
    lastSampleTime = currentMillis;
    tempSum = 0;
    humSum = 0;
    sampleCount = 0;
  }
}

void updateLCD() {
  if (!lcdPower) return;

  switch(lcdMode) {
    case 0: // Ручной режим
      if (lcdContentChanged) {
        lcd.clear();
        for (int i = 0; i < 4; i++) {
          lcd.setCursor(0, i);
          lcd.print(lcdContent[i]);
        }
        lcdContentChanged = false;
      }
      break;
      
    case 1: { // Режим датчика (изменённый)
      static float lastShownTemp = -1000, lastShownHum = -1000;
      static float lastShownAvgTemp = -1000, lastShownAvgHum = -1000;
      static float lastShownPPM = -1000, lastShownAvgPPM = -1000;
      
      bool needUpdate = (abs(lastTemp - lastShownTemp) > 0.1) || 
                       (abs(lastHum - lastShownHum) > 0.1) ||
                       (abs(mq135PPM - lastShownPPM) > 1) ||
                       (sampleCount > 0 && (abs((tempSum/sampleCount) - lastShownAvgTemp) > 0.1)) ||
                       (sampleCount > 0 && (abs((humSum/sampleCount) - lastShownAvgHum) > 0.1)) ||
                       (mq135SampleCount > 0 && (abs((mq135SumPPM/mq135SampleCount) - lastShownAvgPPM) > 1));
      
      if (needUpdate || lcdContentChanged) {
        lcd.clear();
        
        // Строка 1: Текущие значения температуры и влажности
        lcd.setCursor(0, 0);
        lcd.print("T:");
        lcd.print(lastTemp, 1);
        lcd.print("C H:");
        lcd.print(lastHum, 1);
        lcd.print("%");
        
        // Строка 2: Средние значения температуры и влажности
        if (sampleCount > 0) {
          lcd.setCursor(0, 1);
          lcd.print("Avg T:");
          lcd.print(tempSum / sampleCount, 1);
          lcd.print("C H:");
          lcd.print(humSum / sampleCount, 1);
          lcd.print("%");
        }
        
        // Строка 3: Текущее значение CO2
        lcd.setCursor(0, 2);
        lcd.print("CO2: ");
        lcd.print(mq135PPM, 0);
        lcd.print(" ppm");
        
        // Строка 4: Среднее значение CO2
        if (mq135SampleCount > 0) {
          lcd.setCursor(0, 3);
          lcd.print("Avg CO2: ");
          lcd.print(mq135SumPPM / mq135SampleCount, 0);
          lcd.print(" ppm");
        }
        
        // Обновляем последние показанные значения
        lastShownTemp = lastTemp;
        lastShownHum = lastHum;
        lastShownPPM = mq135PPM;
        if (sampleCount > 0) {
          lastShownAvgTemp = tempSum / sampleCount;
          lastShownAvgHum = humSum / sampleCount;
        }
        if (mq135SampleCount > 0) {
          lastShownAvgPPM = mq135SumPPM / mq135SampleCount;
        }
        lcdContentChanged = false;
      }
      break;
    }
      
    case 2: // Режим статуса
      {
        static int lastShownMode = -1;
        static bool lastShownDht = false;
        static bool lastShownBacklight = false;
        
        bool needUpdate = (mode != lastShownMode) || 
                         (dhtPower != lastShownDht) ||
                         (lcdBacklight != lastShownBacklight);
        
        if (needUpdate || lcdContentChanged) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Mode: ");
          lcd.print(mode);
          
          lcd.setCursor(0, 1);
          lcd.print("LEDs: ");
          lcd.print(LED_COUNT);
          
          lcd.setCursor(0, 2);
          lcd.print("DHT: ");
          lcd.print(dhtPower ? "ON " : "OFF");
          lcd.print(" BKL: ");
          lcd.print(lcdBacklight ? "ON" : "OFF");
          
          lcd.setCursor(0, 3);
          lcd.print("LCD Mode: ");
          lcd.print(lcdMode);
          
          lastShownMode = mode;
          lastShownDht = dhtPower;
          lastShownBacklight = lcdBacklight;
          lcdContentChanged = false;
        }
      }
      break;
  }
}

void setLCDBacklight(String state) {
  if (state.equalsIgnoreCase("T")) {
    lcdBacklight = true;
    lcd.backlight();
    EEPROM.update(EEPROM_LCD_BACKLIGHT, 1);
    Serial.println(F("Подсветка LCD включена"));
  }
  else if (state.equalsIgnoreCase("F")) {
    lcdBacklight = false;
    lcd.noBacklight();
    EEPROM.update(EEPROM_LCD_BACKLIGHT, 0);
    Serial.println(F("Подсветка LCD выключена"));
  }
  else {
    Serial.println(F("Ошибка: используйте T или F (lcd_backlight T)"));
  }
}
String getLCDLine(int line) {
  // Для режима 0 возвращаем сохраненные строки
  if (lcdMode == 0) return lcdLines[line];
  
  // Для других режимов формируем строки динамически
  switch(lcdMode) {
    case 1:
      switch(line) {
        case 0: return "Temp: " + String(lastTemp) + "C";
        case 1: return "Hum:  " + String(lastHum) + "%";
        case 2: return sampleCount > 0 ? "Avg T:" + String(tempSum/sampleCount) + "C" : "";
        case 3: return sampleCount > 0 ? "Avg H:" + String(humSum/sampleCount) + "%" : "";
      }
      break;
    case 2:
      switch(line) {
        case 0: return "Mode: " + String(mode);
        case 1: return "LEDs: " + String(LED_COUNT);
        case 2: return String("DHT: ") + (dhtPower ? "ON " : "OFF") + " BKL: " + (lcdBacklight ? "ON" : "OFF");
        case 3: return "LCD Mode: " + String(lcdMode);
      }
      break;
  }
  return "";
}

void loadSettings() {
  brightness = EEPROM.read(EEPROM_BRIGHTNESS);
  mode = EEPROM.read(EEPROM_MODE);
  serialSpeed = EEPROM.read(EEPROM_SPEED) * 100 + 2400;
  dhtPower = EEPROM.read(EEPROM_DHT_POWER);
  lcdPower = EEPROM.read(EEPROM_LCD_POWER);
  lcdMode = EEPROM.read(EEPROM_LCD_MODE);
  lcdBacklight = EEPROM.read(EEPROM_LCD_BACKLIGHT);

  // Чтение параметров режимов
  for (int i = 0; i < maxModes; i++) {
    for (int j = 0; j < 5; j++) {
      modeParams[i].values[j] = EEPROM.read(EEPROM_VALUES + i*5 + j);
    }
  }
}

float getMQ135PPM() {
  int raw = analogRead(MQ135_PIN);
  float voltage = raw * (5.0 / 1023.0);
  
  // Правильный расчёт сопротивления датчика
  float RS = (5.0 - voltage) / voltage; // Для схемы с нагрузочным резистором 1кОм
  // Или если у вас другой RL:
  // float RS = (5.0 * RL) / voltage - RL; // Где RL - значение нагрузочного резистора
  
  // Калибровка для CO2 (должно уменьшаться при наличии газа)
  float ratio = RS / R0; // R0 - сопротивление в чистом воздухе
  float ppm = 116.6020682 * pow(ratio, -2.769034857); // Обратная зависимость
  
  return ppm;
}

float readStableTemperature() {
  int raw = analogRead(LM335_PIN);
  if (raw < 10 || raw > 1013) { // Проверка на обрыв/КЗ
    Serial.println("Ошибка датчика!");
    return -999.9;
  }
  
  float tempC = (raw * (5.0 / 1023.0) * 100.0) - 273.15;
  
  // Калибровка под конкретный датчик
  tempC += 260.7; // Пример калибровки
  
  return tempC;
}

void saveSettings() {
  EEPROM.update(EEPROM_BRIGHTNESS, brightness);
  EEPROM.update(EEPROM_MODE, mode);
  EEPROM.update(EEPROM_SPEED, (serialSpeed - 2400) / 100);
  EEPROM.update(EEPROM_DHT_POWER, dhtPower ? 1 : 0);
  EEPROM.update(EEPROM_LCD_POWER, lcdPower ? 1 : 0);
  EEPROM.update(EEPROM_LCD_MODE, lcdMode);
  EEPROM.update(EEPROM_LCD_BACKLIGHT, lcdBacklight ? 1 : 0);

  for (int i = 0; i < maxModes; i++) {
    for (int j = 0; j < 5; j++) {
      EEPROM.update(EEPROM_VALUES + i*5 + j, modeParams[i].values[j]);
    }
  }
}

void setDefaultValues() {
  brightness = 255;
  mode = 0;
  dhtPower = true;
  serialSpeed = 9600;

  // Очищаем параметры
  for (int i = 0; i < maxModes; i++) {
    for (int j = 0; j < 5; j++) {
      modeParams[i].values[j] = 0;
    }
  }

  // Устанавливаем значения по умолчанию
  // Режим 1: Color Wipe
  modeParams[1].values[0] = 255; // R
  modeParams[1].values[1] = 255; // G
  modeParams[1].values[2] = 255; // B
  modeParams[1].values[3] = 50;  // Задержка

  // Режим 2: Rainbow
  modeParams[2].values[0] = 10;  // Скорость

  // Режим 3: Theater Chase
  modeParams[3].values[0] = 255; // R
  modeParams[3].values[1] = 0;   // G
  modeParams[3].values[2] = 0;   // B
  modeParams[3].values[3] = 50;  // Задержка

  // Режим 4: Color Fade
  modeParams[4].values[0] = 50;  // Скорость

  saveSettings();
  Serial.println(F("Настройки сброшены к заводским"));
}

void handleButton() {
  int reading = digitalRead(BUTTON_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      
      if (buttonState == LOW) {
        mode = (mode + 1) % maxModes;
        Serial.print(F("Режим изменен на: "));
        Serial.println(mode);
        needClear = true;
        EEPROM.update(EEPROM_MODE, mode);
      }
    }
  }
  
  lastButtonState = reading;
}

void printHelp() {
  Serial.println(F("\n=== Доступные команды ==="));
  Serial.println(F("help - показать эту справку"));
  Serial.println(F("view_set - показать текущие настройки"));
  Serial.println(F("view_dht11 - показать данные датчика температуры/влажности"));
  Serial.println(F("dht_view_all - показать статистику датчика"));
  Serial.println(F("dht11_power [T/F] - включить/выключить датчик"));
  Serial.println(F("ch_mode [0-4] - сменить режим работы ленты"));
  Serial.println(F("ch_color [пиксель] [0xRRGGBB] - изменить цвет пикселя (режим 0)"));
  Serial.println(F("ch_all [0xRRGGBB] - изменить цвет всей ленты (режим 0)"));
  Serial.println(F("ch_br [0-255] - установить яркость"));
  Serial.println(F("ch_val [режим] [значения] - установить параметры режима"));
  Serial.println(F("ch_val_def - сбросить параметры к заводским"));
  Serial.println(F("ch_serial_spd [скорость] - изменить скорость порта"));
  Serial.println(F("lcd [T/F] - включить/выключить LCD дисплей"));
  Serial.println(F("lcd_mode [0-2] - установить режим LCD"));
  Serial.println(F("lcd_print [строка] [текст] - напечатать текст (режим 0)"));
  Serial.println(F("lcd_backlight [T/F] - управление подсветкой LCD"));
  Serial.println(F("mq135_view - посмотреть CO2 в воздухе"));
  Serial.println(F("\nПримеры:"));
  Serial.println(F("ch_val 1 255 255 255 50 - установить режим 1 (белый, задержка 50)"));
  Serial.println(F("ch_val 2 20 - установить скорость 20 для режима 2"));
  Serial.println(F("ch_br 150 - установить яркость 150"));
  Serial.println(F("========================\n"));
}

void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.equals("help")) {
      printHelp();
    }
    else if (command.equals("view_set")) {
      viewSettings();
    }
    else if (command.equals("view_dht11")) {
      readDHT11();
    }
    else if (command.equals("dht_view_all")) {
      viewDHTAll();
    }
    else if (command.equals("ch_val_def")) {
      setDefaultValues();
    }
    else if (command.startsWith("dht11_power ")) {
      setDHTPower(command.substring(12));
    }
    else if (command.startsWith("ch_mode ")) {
      changeMode(command.substring(8));
    }
    else if (command.startsWith("ch_color ")) {
      handleColorCommand(command);
    }
    else if (command.startsWith("ch_all ")) {
      handleAllCommand(command);
    }
    else if (command.startsWith("ch_br ")) {
      handleBrightnessCommand(command.substring(6));
    }
    else if (command.startsWith("ch_val ")) {
      handleValuesCommand(command);
    }
    else if (command.startsWith("ch_serial_spd ")) {
      handleSerialSpeedCommand(command.substring(14));
    }
    else if (command.startsWith("lcd ")) {
      setLCDPower(command.substring(4));
    }
    else if (command.startsWith("lcd_mode ")) {
      setLCDMode(command.substring(9));
    }
    else if (command.startsWith("lcd_print ")) {
      handleLCDPrint(command.substring(10));
    }
    else if (command.startsWith("lcd_backlight ")) {
      setLCDBacklight(command.substring(14));
    }
    else if (command.equals("mq135_view")) {
      viewMQ135Data();
    }
    else if (command.equals("stab_temp")) {
      stableTemp = readStableTemperature();
      Serial.print(F("Стабильная температура: "));
      Serial.print(stableTemp);
      Serial.println(F("°C"));
    }
    else {
      Serial.println(F("Неизвестная команда. Введите 'help' для списка команд"));
    }
  }
}

void viewMQ135Data() {
  Serial.println(F("\n=== Данные MQ-135 ==="));
  Serial.print(F("Текущее значение: "));
  Serial.print(mq135PPM);
  Serial.println(F(" ppm"));
  
  if (mq135SampleCount > 0) {
    Serial.print(F("Среднее за минуту: "));
    Serial.print(mq135SumPPM / mq135SampleCount);
    Serial.println(F(" ppm"));
  }
  
  Serial.print(F("Минимальное значение: "));
  Serial.print(minPPM);
  Serial.println(F(" ppm"));
  Serial.print(F("Максимальное значение: "));
  Serial.print(maxPPM);
  Serial.println(F(" ppm"));
  Serial.println(F("===================="));
}

void setLCDPower(String state) {
  if (state.equalsIgnoreCase("T")) {
    lcdPower = true;
    digitalWrite(LCD_POWER_PIN, HIGH);
    lcd.init();
    lcd.backlight();
    updateLCD();
    EEPROM.update(EEPROM_LCD_POWER, 1);
    Serial.println(F("LCD включен"));
  }
  else if (state.equalsIgnoreCase("F")) {
    lcdPower = false;
    digitalWrite(LCD_POWER_PIN, LOW);
    EEPROM.update(EEPROM_LCD_POWER, 0);
    Serial.println(F("LCD выключен"));
  }
  else {
    Serial.println(F("Ошибка: используйте T или F (lcd T)"));
  }
}

void setLCDMode(String modeStr) {
  int newMode = modeStr.toInt();
  if (newMode >= 0 && newMode <= 2) {
    lcdMode = newMode;
    EEPROM.update(EEPROM_LCD_MODE, lcdMode);
    Serial.print(F("Режим LCD изменен на: "));
    Serial.println(lcdMode);
    if (lcdPower) updateLCD();
  } else {
    Serial.println(F("Ошибка: неверный режим LCD (0-2)"));
  }
}

void handleLCDPrint(String command) {
  if (lcdMode != 0) {
    Serial.println(F("Ошибка: печать возможна только в режиме 0"));
    return;
  }
  
  int spacePos = command.indexOf(' ');
  if (spacePos == -1) {
    Serial.println(F("Ошибка: неверный формат команды"));
    Serial.println(F("Используйте: lcd_print [строка(1-4)] [текст]"));
    return;
  }
  
  int line = command.substring(0, spacePos).toInt();
  String text = command.substring(spacePos + 1);
  
  if (line >= 1 && line <= 4) {
    lcdContent[line-1] = text.substring(0, 20);
    lcdContentChanged = true;
    Serial.print(F("Текст '"));
    Serial.print(text.substring(0, 20));
    Serial.print(F("' записан в строку "));
    Serial.println(line);
  } else {
    Serial.println(F("Ошибка: номер строки должен быть от 1 до 4"));
  }
}

void viewSettings() {
  Serial.println(F("\n=== Текущие настройки ==="));
  Serial.print(F("Яркость: ")); Serial.println(brightness);
  Serial.print(F("Текущий режим: ")); Serial.println(mode);
  Serial.print(F("Скорость порта: ")); Serial.println(serialSpeed);
  Serial.print(F("Питание DHT11: ")); Serial.println(dhtPower ? "Вкл" : "Выкл");
  
  Serial.println(F("\nПараметры режимов:"));
  Serial.print(F("Режим 1 (Color Wipe): "));
  Serial.print(modeParams[1].values[0]); Serial.print(" ");
  Serial.print(modeParams[1].values[1]); Serial.print(" ");
  Serial.print(modeParams[1].values[2]); Serial.print(" ");
  Serial.println(modeParams[1].values[3]);
  
  Serial.print(F("Режим 2 (Rainbow): "));
  Serial.println(modeParams[2].values[0]);
  
  Serial.print(F("Режим 3 (Theater Chase): "));
  Serial.print(modeParams[3].values[0]); Serial.print(" ");
  Serial.print(modeParams[3].values[1]); Serial.print(" ");
  Serial.print(modeParams[3].values[2]); Serial.print(" ");
  Serial.println(modeParams[3].values[3]);
  
  Serial.print(F("Режим 4 (Color Fade): "));
  Serial.println(modeParams[4].values[0]);
  Serial.println(F("========================"));
}

void readDHT11() {
  if (!dhtPower) {
    Serial.println(F("DHT11 выключен. Включите питание командой dht11_power T"));
    return;
  }
  
  if (isnan(lastTemp) || isnan(lastHum)) {
    Serial.println(F("Ошибка чтения данных с DHT11!"));
    return;
  }
  
  Serial.print(F("Текущие показания: Температура="));
  Serial.print(lastTemp);
  Serial.print(F("°C, Влажность="));
  Serial.print(lastHum);
  Serial.println(F("%"));
  
  if (sampleCount > 0) {
    Serial.print(F("Средние за минуту: Температура="));
    Serial.print(tempSum / sampleCount);
    Serial.print(F("°C, Влажность="));
    Serial.print(humSum / sampleCount);
    Serial.println(F("%"));
  }
}

void viewDHTAll() {
  Serial.println(F("\n=== Статистика DHT11 ==="));
  Serial.print(F("Минимальная температура: ")); Serial.print(minTemp); Serial.println(F("°C"));
  Serial.print(F("Максимальная температура: ")); Serial.print(maxTemp); Serial.println(F("°C"));
  Serial.print(F("Минимальная влажность: ")); Serial.print(minHum); Serial.println(F("%"));
  Serial.print(F("Максимальная влажность: ")); Serial.print(maxHum); Serial.println(F("%"));
  Serial.println(F("======================="));
}

void setDHTPower(String state) {
  if (state.equalsIgnoreCase("T")) {
    dhtPower = true;
    digitalWrite(DHT_POWER_PIN, HIGH);
    Serial.println(F("DHT11 включен"));
    delay(1000); // Даем время на инициализацию
    EEPROM.update(EEPROM_DHT_POWER, 1);
  }
  else if (state.equalsIgnoreCase("F")) {
    dhtPower = false;
    digitalWrite(DHT_POWER_PIN, LOW);
    Serial.println(F("DHT11 выключен"));
    EEPROM.update(EEPROM_DHT_POWER, 0);
  }
  else {
    Serial.println(F("Ошибка: используйте T или F (dht11_power T)"));
  }
}

void changeMode(String modeStr) {
  int newMode = modeStr.toInt();
  if (newMode >= 0 && newMode < maxModes) {
    mode = newMode;
    needClear = true;
    Serial.print(F("Режим изменен на: "));
    Serial.println(mode);
    EEPROM.update(EEPROM_MODE, mode);
  } else {
    Serial.println(F("Ошибка: неверный номер режима (0-4)"));
  }
}

void handleBrightnessCommand(String valueStr) {
  int newBrightness = valueStr.toInt();
  if (newBrightness >= 0 && newBrightness <= 255) {
    brightness = newBrightness;
    strip.setBrightness(brightness);
    strip.show();
    EEPROM.update(EEPROM_BRIGHTNESS, brightness);
    Serial.print(F("Яркость установлена на: "));
    Serial.println(brightness);
  } else {
    Serial.println(F("Ошибка: яркость должна быть от 0 до 255"));
  }
}

void handleColorCommand(String command) {
  if (mode != 0) {
    Serial.println(F("Ошибка: управление цветом доступно только в режиме 0"));
    return;
  }
  
  // Удаляем "ch_color " из начала строки
  command = command.substring(9);
  command.trim();
  
  // Находим первый пробел (разделитель между номером пикселя и цветом)
  int spacePos = command.indexOf(' ');
  if (spacePos == -1) {
    Serial.println(F("Ошибка: неверный формат команды"));
    Serial.println(F("Используйте: ch_color [пиксель] [0xRRGGBB]"));
    return;
  }
  
  // Извлекаем номер пикселя
  int pixel = command.substring(0, spacePos).toInt();
  
  // Извлекаем цвет (удаляем возможные пробелы в начале)
  String colorStr = command.substring(spacePos + 1);
  colorStr.trim();
  
  // Удаляем "0x" если есть
  if (colorStr.startsWith("0x")) {
    colorStr = colorStr.substring(2);
  }
  
  // Преобразуем строку в число
  long color = strtol(colorStr.c_str(), NULL, 16);
  
  if (pixel >= 0 && pixel < LED_COUNT) {
    strip.setPixelColor(pixel, color);
    strip.show();
    Serial.print(F("Цвет пикселя "));
    Serial.print(pixel);
    Serial.print(F(" изменен на: 0x"));
    Serial.println(colorStr);
  } else {
    Serial.print(F("Ошибка: неверный номер пикселя (0-"));  // Изменено здесь
    Serial.print(LED_COUNT - 1);                             // Изменено здесь
    Serial.println(F(")"));                                  // Изменено здесь
  }
}

void handleAllCommand(String command) {
  if (mode != 0) {
    Serial.println(F("Ошибка: управление цветом доступно только в режиме 0"));
    return;
  }
  
  // Удаляем "ch_all " из начала строки
  command = command.substring(7);
  command.trim();
  
  // Удаляем "0x" если есть
  if (command.startsWith("0x")) {
    command = command.substring(2);
  }
  
  // Преобразуем строку в число
  long color = strtol(command.c_str(), NULL, 16);
  
  for(int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
  Serial.print(F("Цвет всей ленты изменен на: 0x"));
  Serial.println(command);
}

void handleValuesCommand(String command) {
  // Удаляем "ch_val " из начала строки
  command = command.substring(7);
  command.trim();
  
  // Находим первый пробел (разделитель между номером режима и параметрами)
  int spacePos = command.indexOf(' ');
  if (spacePos == -1) {
    // Если параметров нет (только номер режима)
    int modeNum = command.toInt();
    if (modeNum >= 1 && modeNum <= 4) {
      Serial.print(F("Текущие параметры режима "));
      Serial.print(modeNum);
      Serial.print(F(": "));
      for (int i = 0; i < 5; i++) {
        Serial.print(modeParams[modeNum].values[i]);
        if (i < 4) Serial.print(" ");
      }
      Serial.println();
    } else {
      Serial.println(F("Ошибка: номер режима должен быть от 1 до 4"));
    }
    return;
  }

  // Извлекаем номер режима
  int modeNum = command.substring(0, spacePos).toInt();
  if (modeNum < 1 || modeNum > 4) {
    Serial.println(F("Ошибка: номер режима должен быть от 1 до 4"));
    return;
  }

  // Извлекаем параметры
  String paramsStr = command.substring(spacePos + 1);
  paramsStr.trim();
  
  // Разбиваем параметры на отдельные значения
  int values[5] = {0};
  int paramCount = 0;
  int lastPos = 0;
  
  while (lastPos < paramsStr.length() && paramCount < 5) {
    int nextSpace = paramsStr.indexOf(' ', lastPos);
    if (nextSpace == -1) {
      values[paramCount] = paramsStr.substring(lastPos).toInt();
      paramCount++;
      break;
    } else {
      values[paramCount] = paramsStr.substring(lastPos, nextSpace).toInt();
      paramCount++;
      lastPos = nextSpace + 1;
    }
  }

  // Проверяем минимальное количество параметров для каждого режима
  switch(modeNum) {
    case 1: // Color Wipe (R, G, B, скорость)
    case 3: // Theater Chase (R, G, B, скорость)
      if (paramCount < 4) {
        Serial.println(F("Ошибка: для этого режима нужно 4 параметра (R G B скорость)"));
        return;
      }
      break;
    case 2: // Rainbow (скорость)
    case 4: // Color Fade (скорость)
      if (paramCount < 1) {
        Serial.println(F("Ошибка: для этого режима нужно минимум 1 параметр (скорость)"));
        return;
      }
      break;
  }

  // Сохраняем параметры
  for (int i = 0; i < paramCount; i++) {
    modeParams[modeNum].values[i] = values[i];
  }

  // Сохраняем в EEPROM
  saveSettings();

  // Выводим подтверждение
  Serial.print(F("Параметры режима "));
  Serial.print(modeNum);
  Serial.print(F(" установлены: "));
  for (int i = 0; i < paramCount; i++) {
    Serial.print(values[i]);
    if (i < paramCount - 1) Serial.print(" ");
  }
  Serial.println();
}

void handleSerialSpeedCommand(String speedStr) {
  long newSpeed = speedStr.toInt();
  if (newSpeed == 2400 || newSpeed == 9600 || newSpeed == 19200 || newSpeed == 38400) {
    serialSpeed = newSpeed;
    EEPROM.update(EEPROM_SPEED, (serialSpeed - 2400) / 100);
    Serial.print(F("Скорость порта изменена на "));
    Serial.print(serialSpeed);
    Serial.println(F(". Перезагрузите устройство для применения."));
  } else {
    Serial.println(F("Ошибка: допустимые скорости - 2400, 9600, 19200, 38400"));
  }
}

// Эффекты для LED ленты
void colorWipe(uint32_t color, int wait) {
  static int i = 0;
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate >= wait) {
    lastUpdate = millis();
    if (i < strip.numPixels()) {
      strip.setPixelColor(i, color);
      strip.show();
      i++;
    } else {
      i = 0;
    }
  }
}

void rainbow(int wait) {
  static long firstPixelHue = 0;
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate >= wait) {
    lastUpdate = millis();
    for(int i = 0; i < strip.numPixels(); i++) {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    firstPixelHue += 256;
    if (firstPixelHue >= 5*65536) firstPixelHue = 0;
  }
}

void theaterChase(uint32_t color, int wait) {
  static int a = 0, b = 0;
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate >= wait) {
    lastUpdate = millis();
    if (b < 3) {
      strip.clear();
      for(int c = b; c < strip.numPixels(); c += 3) {
        strip.setPixelColor(c, color);
      }
      strip.show();
      b++;
    } else {
      b = 0;
      a++;
      if (a >= 10) a = 0;
    }
  }
}

void colorFade(int wait) {
  static int j = 0;
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate >= wait) {
    lastUpdate = millis();
    for(int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(j, 255 - j, (j + 128) % 256));
    }
    strip.show();
    j = (j + 1) % 256;
  }
}
