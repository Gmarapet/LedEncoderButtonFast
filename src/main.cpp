#include <Arduino.h>

// Назначаем пины для диодов
#define ledPinW 9 // Теплые диоды
#define ledPinC 10 // Холодные диоды

// Назначаем пины энеоднра
#define enc0 2 // Канал A энкодера
#define enc1 3 // Канал B энкодера
#define encBtn 4 // Кнопка энкодера

// Назначаем пины кнопки
#define extBtn 5 // Кнопка включения лампы

// Переменные кнопки включения
bool lightOn = false;
uint32_t btnTimer = 0;

// Библиотека работы с энкодером версия ^3.0
#include <EncButton.h>
// Подключаем энкодер. Пины 2 и 3 - энкодер, пин 4 -кнопка
EncButton eb(enc0, enc1, encBtn, INPUT, INPUT); // энкодер с кнопкой <A, B, KEY>
// EncButton eb(enc0, enc1, btn);                    // пины энкодера и кнопки
// EncButton eb(enc0, enc1, btn, modeEnc);           // + режим пинов энкодера (умолч. INPUT)
// EncButton eb(enc0, enc1, btn, modeEnc, modeBtn);  // + режим пина кнопки (умолч. INPUT_PULLUP)
// EncButton eb(enc0, enc1, btn, modeEnc, modeBtn, btnLevel);  // + уровень кнопки (умолч. LOW)

// Кнопка
Button b(extBtn, INPUT); //
// // + режим пина кнопки (умолч. INPUT_PULLUP)

// Величина шага быстрого поворота энкодера
byte fastStep = 10;

// Каналы для записи ШИМ
int ledWarm, ledCold;

// Каналы яркости и оттенка
byte chHue, chBright;

// Задержка перехода энкодера в режим яркости одна минута
uint32_t hueDelay = 60000;

// Флаг задержки переключения энкодера в канал яркости
uint32_t chHueDelay = 0;

// Библиотека работы с памятью
#include <EEPROM.h>

// Режим работы по-умолчанию - яркость
bool modeHue = false;

// Объявление функций
void sendPWM();

void setup()
{
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
  // Пороверка состояния кнопки включения
  bool btnState = digitalRead(buttonPin);

  if (btnState && !lightOn && millis() - btnTimer > 100) // В момент включения кнопки
  {
    lightOn = true;
    btnTimer = millis();
    // Serial.println("Кнопка нажата");
    // Прочитать значения каналов цвета и яркости из памяти
    EEPROM.get(0, chHue);
    EEPROM.get(1, chBright);

    // Установить счетчик энкодера равным каналу яркости
    eb.counter = chBright;

    // Включить диоды
    sendPWM();
  }
  if (!btnState && lightOn && millis() - btnTimer > 100) // В момент выключения кнопки
  {
    lightOn = false;
    btnTimer = millis();
    // Serial.println("Кнопка отпущена");

    // Обнуляем каналы и счетчик
    chBright = 0;
    chHue = 0;
    eb.counter = 0;
    // Выключаем диоды
    sendPWM();
  }

  // Если кнопка включения активна
  if (lightOn)
  {
    // Автоматическое переключение в режим яркости с задержкой hueDelay
    if (modeHue && millis() - chHueDelay > hueDelay)
    {
      // Переходим в режим регулировки яркости
      modeHue = false;
      eb.counter = chBright; // Передаем счетчику канал яркости
      chHueDelay = 0;
    }

    enc.tick(); // опрос энкодера происходит здесь

    // Обработка нажатий кнопки
    if (enc.click())
    {
      modeHue = !modeHue; // При нажатии меняем режим энкодера
      if (modeHue)
      {
        eb.counter = chHue;   // Передаем счетчику канал оттенка
        chHueDelay = millis(); // Включаем задержку перехода в канал яркости
      }
      else
      {
        eb.counter = chBright; // Передаем счетчику канал яркости
        chHueDelay = 0;
      }
    }

    // Удержание кнопки
    if (enc.held())
    {
      modeHue = false;        // Преводим энкодер в режим яркости
      chHueDelay = 0;         // Контроль задержки перехода в канал яркости
      chBright = 255;         // Канал яркости - в максимум
      chHue = 127;            // Применяем яркость на оба канала
      eb.counter = chBright; // Сохраняем в счетчик максимальную яркость
      sendPWM();              // Рассчитываем ШИМ и отправляем диодам

      // Serial.println("Максимальная яркость обоих каналов!!!");
    }

    // Обработка поворотов
    if (enc.turn())
    {

      if (enc.fast())
      { // В быстром режиме
        if (enc.right())
          eb.counter += fastStep; // при повороте направо прибавлять по fastStep,
        else
          eb.counter -= fastStep; // при повороте налево убавлять по fastStep
      }

      eb.counter = constrain(eb.counter, 0, 255); // Окраничиваем диапазон eb.counter

      // Определяемся в какой канал пишет энкодер
      if (modeHue)
      {
        chHue = eb.counter;   // Значения счетчика записываются в канал цвета
        chHueDelay = millis(); // Сбрасывам задежку переключения в канал яркости
      }
      else
      {
        chBright = eb.counter; // Значения счетчика записываются в канал яркости
      }
      sendPWM(); // Рассчитываем ШИМ и отправляем диодам
    }

    // Обработа нажатых поворотов
    if (enc.turnH())
    {
      modeHue = false;        // Преводим энкодер в режим яркости
      chBright = 255;         // Канал яркости - в максимум
      eb.counter = chBright; // Сохраняем в счетчик максимальную яркость
      if (enc.leftH())
        chHue = 255; // Нажатый поворот налево оттенок теплый
      if (enc.rightH())
        chHue = 0; // Нажатый поворот направо оттенок холодный
      sendPWM();   // Рассчитываем ШИМ и отправляем диодам
    }

    // Запись настроек в память по тройному клику
    if (enc.hasClicks(3))
    {
      modeHue = !modeHue;      // Исправляем изменившийся после трех кликов режим энкодера
      EEPROM.put(0, chHue);    // Запоминаем в EEPROM значение канала оттенка
      EEPROM.put(1, chBright); // и канала яркости

      // Serial.println("action 3 clicks");
      // Serial.println("chHue = " + String(chHue) + ", chBright = " + String(chBright) + ".");
    }
  }
}

// Функция возвращает скорректированное по CRT значение
// для 8 бит ШИМ
byte getCRT(byte val)
{ // Кубическая CRT гамма
  if (!val)
    return 0;                                    // В случае 0, вернуть 0
  return ((long)val * val * val + 130305) >> 16; // Рассчитываем, используя полином
}

// Функция рассчета ШИМ и вывода его на диоды
void sendPWM()
{
  byte delta;
  if (chHue < 128)
  {
    delta = chHue << 1;
    delta = ~delta;
    if (chHue == 127)
      delta = 0;
    ledWarm = chBright;
    ledCold = chBright - delta;
    ledCold = constrain(ledCold, 0, 255);
  }
  else
  {
    delta = (chHue - 128) << 1;
    if (chHue == 255)
      delta = 255;
    ledCold = chBright;
    ledWarm = chBright - delta;
    ledWarm = constrain(ledWarm, 0, 255);
  }

  // Корректируем гамму каналов ШИМ
  ledWarm = getCRT(ledWarm);
  ledCold = getCRT(ledCold);

  // Выводим конечные значения на пины
  analogWrite(ledPinW, ledWarm); // Теплые диоды
  analogWrite(ledPinC, ledCold); // Холодные диоды
  // Serial.println("chHue=" + String(chHue));
  // Serial.println("ledCold=" + String(ledCold) + ", ledWarm=" + String(ledWarm));
}