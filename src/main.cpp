#include <Arduino.h>

// Назначаем пины для диодов
#define ledPinW 9  // Теплые диоды
#define ledPinC 10 // Холодные диоды

// Назначаем пины энкодера
#define enc0 2   // Канал A энкодера
#define enc1 3   // Канал B энкодера
#define encBtn 4 // Кнопка энкодера

// Назначаем пин кнопки включения лампы
#define extBtn 5

// Библиотека работы с энкодером
#include <EncButton.h> // версия ^3.0
// Подключаем энкодер
// EncButton eb(encPin0, encPin1, btn, modeEnc, modeBtn, btnLevel);  // + уровень кнопки (умолч. LOW)
EncButton eb(enc0, enc1, encBtn, INPUT, INPUT);

// Кнопка
// режим пина и уровень кнопки по-умолчанию (btn, INPUT_PULLUP, LOW)
Button onOff(extBtn, INPUT, HIGH);

// Счетчик сброса настроек каналов при выключении
uint32_t offCount = 0;
// Задержка сброса настроек при выключении
uint32_t offDelay = 3600000; // один час
// uint32_t offDelay = 5000; // 5 секунд

// Переменные дл хранения значения каналов до сброса
byte chHueTemp = 0;
byte chBrightTemp = 0;

// Величина шага быстрого поворота энкодера
byte fastStep = 10;

// Каналы для записи ШИМ signed int16_t два байта со знаком
int ledWarm = 0, ledCold = 0;

// Каналы яркости и оттенка
byte chHue = 0, chBright = 0;

// Задержка перехода энкодера в режим яркости десять секунд
uint32_t hueDelay = 10000;
// Счетчик задержки переключения энкодера в канал яркости
uint32_t chHueDelayMillis = 0;

// Переменные плавного включения/выключения диодов
byte fadeSmooth = 1;          // Шаг затухания в миллисекундах
uint32_t fadeDelayMillis = 0; // Счетчик затухания в миллисекундах

// Режим работы по-умолчанию - яркость
bool modeHue = false;

// Библиотека работы с памятью
#include <EEPROM.h>

// Объявление функций

// Функция возвращает скорректированное по CRT значение для 8 бит ШИМ
byte getCRT(byte val)
{ // Кубическая CRT гамма
  if (!val)
    return 0;                                    // В случае 0, вернуть 0
  return ((long)val * val * val + 130305) >> 16; // Рассчитываем, используя полином
}

// Функция рассчета ШИМ и вывода его на диоды
void sendPWM()
{
  byte delta;      // Разница в яркости каналов
  if (chHue < 128) // Если chHue меньше 128, ярче теплый канал
  {
    delta = chHue << 1; // Разницу умножаем на два, получаем значения от 0 до 254 с шагом 2
    delta = ~delta;     // Инвертируем значение разницы
    if (chHue == 127)
      delta = 0;                          // 127*2=254 (после инверсии = 1), принудительно меняем на 0
    ledWarm = chBright;                   // Теплый канал равен яркости
    ledCold = chBright - delta;           // Холодный канал меньше на дельту
    ledCold = constrain(ledCold, 0, 255); // Ограничиваем диапазон одним байтом
  }
  else // Если chHue больше 128, ярче холодный канал
  {
    delta = (chHue - 128) << 1; // Вычитаем из chHue 128 и умножаем на два получаем значения от 0 до 254 с шагом 2
    if (chHue == 255)
      delta = 255;                        // 127*2=254, принудительно присваеваем 255
    ledCold = chBright;                   // Холодный канал равен каналу яркости
    ledWarm = chBright - delta;           // Теплый канал менше на дельту
    ledWarm = constrain(ledWarm, 0, 255); // Ограничиваем диапазон одним байтом
  }

  // Корректируем гамму каналов ШИМ
  ledWarm = getCRT(ledWarm);
  ledCold = getCRT(ledCold);

  // Выводим конечные значения на пины
  analogWrite(ledPinW, ledWarm); // Теплые диоды
  analogWrite(ledPinC, ledCold); // Холодные диоды
  // Serial.println("chHue=" + String(chHue) + ", chBright" + String(chBright));
  // Serial.println("ledCold=" + String(ledCold) + ", ledWarm=" + String(ledWarm));
}

void setup()
{
  // Прошивка через программатор включает диоды  RXLED и TXLED. Выключаем.
  // RXLED0
  // TXLED0

  // // Установить частоту 31,4 кГц на пины D9 и D10 (Timer1)
  // TCCR1A = 0b00000001; // 8bit
  // TCCR1B = 0b00000001; // x1 phase correct
  // Пины D9 и D10 - 62.5 кГц
  TCCR1A = 0b00000001; // 8bit
  TCCR1B = 0b00001001; // x1 fast pwm

  // Назначаем пины на выход
  pinMode(ledPinW, OUTPUT);
  pinMode(ledPinC, OUTPUT);

  // Открываем Serial на скорости 9600 бод
  // Serial.begin(9600);
}

void loop()
{
  // Обработка кнопки включения
  onOff.tick();
  // В момент нажатия кнопки включения
  if (onOff.press())
  {
    // Режим регулировки яркости
    modeHue = false;

    // Если счетчик буфера выключения обнулен
    if (!offCount)
    {
      // Прочитать из памяти в буфер каналов значения цвета и яркости
      EEPROM.get(0, chHueTemp);
      EEPROM.get(1, chBrightTemp);
    }

    // Установить канал оттенка и счетчик экодера из буфера
    chHue = chHueTemp;
    eb.counter = chBrightTemp;
  }

  // В момент отключения кнопки сбрасываем каналы в буфер
  // и обнуляем счетчик энкодера
  if (onOff.release())
  {
    offCount = millis(); // Запускаем счетчик сброса настроек
    chHueTemp = chHue;   // Оттенок в буфер
    if (!modeHue)        // Если активен режим яркости
    {
      if (chBright == eb.counter) // Антидребезг
      {
        chBrightTemp = chBright; // Яркость в буфер
      }
    }
    else // В режиме оттенка
    {
      chBrightTemp = chBright; // Яркость в буфер
      modeHue = false;         // Переходим в режим яркости
    }

    eb.counter = 0; // Обнуляем счетчик энкодера
  }

  //  Если с момента выключения прошло времени больше offDelay, сбрасываем настройки
  if (millis() - offCount > offDelay)
    offCount = 0;

  // Если кнопка включения активна
  if (onOff.holding())
  {
    // Автоматическое переключение в режим яркости с задержкой hueDelay
    if (modeHue && millis() - chHueDelayMillis > hueDelay)
    {
      // Переходим в режим регулировки яркости
      modeHue = false;
      eb.counter = chBright; // Передаем счетчику канал яркости
      chHueDelayMillis = 0;
    }

    eb.tick(); // опрос энкодера происходит здесь

    // Обработка нажатий кнопки
    if (eb.click())
    {
      modeHue = !modeHue; // По клику меняем режим энкодера
      if (modeHue)
      {
        eb.counter = chHue;          // Передаем счетчику канал оттенка
        chHueDelayMillis = millis(); // Включаем задержку перехода в канал яркости
      }
      else
      {
        eb.counter = chBright; // Передаем счетчику канал яркости
        chHueDelayMillis = 0;
      }
    }

    // Удержание кнопки
    if (eb.hold())
    {
      modeHue = false;  // Преводим энкодер в режим яркости
      chHue = 127;      // Применяем яркость на оба канала
      eb.counter = 255; // Сохраняем в счетчик максимальную яркость
      if (chBright == 255)
        sendPWM(); // Если chBright уже был в максимуме, принудительно пересчитываем ШИМ
    }

    // Обработка поворотов
    if (eb.turn())
    {
      if (eb.fast())
      { // В быстром режиме
        if (eb.right())
          eb.counter += fastStep; // при повороте направо прибавлять по fastStep,
        if (eb.left())
          eb.counter -= fastStep; // при повороте налево убавлять по fastStep
      }
      eb.counter = constrain(eb.counter, 0, 255); // Окраничиваем диапазон eb.counter
    }

    // Обработа нажатых поворотов
    if (eb.turnH())
    {
      modeHue = false; // Преводим энкодер в режим яркости
      if (eb.leftH())
        chHue = 0; // Нажатый поворот налево оттенок теплый
      if (eb.rightH())
        chHue = 255;    // Нажатый поворот направо оттенок холодный
      eb.counter = 255; // Сохраняем в счетчик максимальную яркость
      if (chBright == 255)
        sendPWM(); // Если chBright уже был в максимуме, принудительно пересчитываем ШИМ
    }

    // Запись настроек в память по тройному клику
    if (eb.hasClicks(3))
    {
      modeHue = false;         // Переводим энкодер в режим яркости
      eb.counter = chBright;   // и передаём в счётчик значение канала яркости
      EEPROM.put(0, chHue);    // Запоминаем в EEPROM значение канала оттенка
      EEPROM.put(1, chBright); // и канала яркости
    }
  }

  // Плавно передаем значение на диоды
  if (modeHue && chHue != eb.counter)
  {
    if (millis() - fadeDelayMillis > fadeSmooth) // Если задержка больше заданной
    {
      if (chHue < eb.counter)
        ++chHue;
      if (chHue > eb.counter)
        --chHue;
      fadeDelayMillis = millis();  // Сбрасывам задежку затухания
      chHueDelayMillis = millis(); // Сбрасывам задежку переключения в канал яркости
      sendPWM();                   // Рассчитываем ШИМ и отправляем на диоды
    }
  }
  if (!modeHue && chBright != eb.counter)
  {
    if (millis() - fadeDelayMillis > fadeSmooth)
    {
      if (chBright < eb.counter)
        chBright++;
      if (chBright > eb.counter)
        chBright--;
      fadeDelayMillis = millis(); // Сбрасывам задежку затухания
      sendPWM();                  // Рассчитываем ШИМ и отправляем на диоды
    }
  }
}