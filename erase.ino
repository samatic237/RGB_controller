#include <EEPROM.h>

// Определяем адреса EEPROM как в основном скетче
#define EEPROM_BRIGHTNESS 0
#define EEPROM_MODE 1
#define EEPROM_SPEED 2
#define EEPROM_DHT_POWER 3
#define EEPROM_VALUES 10  // Начальный адрес для параметров режимов

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  Serial.println(F("=== Сброс EEPROM до заводских настроек ==="));
  Serial.println(F("Этот скетч полностью очистит EEPROM"));
  Serial.println(F("и установит значения по умолчанию"));
  
  // Запрос подтверждения
  Serial.println(F("Отправьте 'Y' для продолжения или любой символ для отмены"));
  while (!Serial.available());
  
  if (Serial.read() != 'Y') {
    Serial.println(F("Отменено пользователем"));
    return;
  }

  // Полная очистка EEPROM (записываем 0)
  Serial.println(F("Очистка EEPROM..."));
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }

  // Установка значений по умолчанию
  Serial.println(F("Установка заводских настроек..."));
  
  // Основные настройки
  EEPROM.write(EEPROM_BRIGHTNESS, 255);  // Яркость
  EEPROM.write(EEPROM_MODE, 0);         // Режим (0)
  EEPROM.write(EEPROM_SPEED, 72);       // Скорость порта (9600 бод)
  EEPROM.write(EEPROM_DHT_POWER, 1);    // Питание DHT11 (вкл)

  // Параметры режимов
  // Режим 1: Color Wipe (R,G,B,задержка)
  EEPROM.write(EEPROM_VALUES + 0, 255);  // R
  EEPROM.write(EEPROM_VALUES + 1, 255);  // G
  EEPROM.write(EEPROM_VALUES + 2, 255);  // B
  EEPROM.write(EEPROM_VALUES + 3, 50);   // Задержка

  // Режим 2: Rainbow (скорость)
  EEPROM.write(EEPROM_VALUES + 5, 10);   // Скорость

  // Режим 3: Theater Chase (R,G,B,задержка)
  EEPROM.write(EEPROM_VALUES + 10, 255); // R
  EEPROM.write(EEPROM_VALUES + 11, 0);   // G
  EEPROM.write(EEPROM_VALUES + 12, 0);   // B
  EEPROM.write(EEPROM_VALUES + 13, 50);  // Задержка

  // Режим 4: Color Fade (скорость)
  EEPROM.write(EEPROM_VALUES + 15, 50);  // Скорость

  Serial.println(F("Готово! EEPROM сброшена к заводским настройкам."));
  Serial.println(F("Теперь загрузите основной скетч."));
}

void loop() {}
