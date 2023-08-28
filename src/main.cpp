#include <Arduino.h>

// Назначаем пины для диодов
#define ledPin1 9
#define ledPin2 10
#define buttonPin 5

// Переменные кнопки включения
bool lightOn = false;
uint32_t btnTimer = 0;

// Библиотека работы с энкодером версия 2.0
#include <EncButton.h>
// Подключаем энкодер. Пины 2 и 3 - энкодер, пин 4 -кнопка
EncButton<EB_TICK, 2, 3, 4> enc; // энкодер с кнопкой <A, B, KEY>
// по умолчанию пины настроены в INPUT_PULLUP
// Если используется внешняя подтяжка - лучше перевести в INPUT
// EncButton<EB_TICK, 2, 3, 4> enc(INPUT);

// Величина шага быстрого поворота энкодера
byte fastStep = 10;

// Каналы для записи ШИМ
int ledWarm, ledCold;

// Каналы яркости и оттенка
byte chHue, chBright;

// Флаг задержки переключения в канал яркости
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
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);

  // Открываем Serial на скорости 9600 бод
  // Serial.begin(9600);
}

void loop()
{
  // Пороверка состояния кнопки включения
  bool btnState = digitalRead(buttonPin);

  if (btnState && !lightOn && millis() - btnTimer > 100)
  {
    lightOn = true;
    btnTimer = millis();
    // Serial.println("Кнопка нажата");
    // Прочитать значения каналов цвета и яркости из памяти
    EEPROM.get(0, chHue);
    EEPROM.get(1, chBright);

    // Установить счетчик энкодера равным каналу яркости
    enc.counter = chBright;

    // Включить диоды
    sendPWM();
  }
  if (!btnState && lightOn && millis() - btnTimer > 100)
  {
    lightOn = false;
    btnTimer = millis();
    // Serial.println("Кнопка отпущена");

    // Обнуляем каналы и счетчик
    chBright = 0;
    chHue = 0;
    enc.counter = 0;
    // Выключаем диоды
    sendPWM();
  }

  // Если кнопка включения активна
  if (lightOn)
  {
    // Автоматическое переключение в режим яркости с задержкой в три минуты
    if (modeHue && millis() - chHueDelay > 3000 * 60)
    {
      // Переходим в режим регулировки яркости
      modeHue = false;
      enc.counter = chBright; // Передаем счетчику канал яркости
      chHueDelay = 0;
    }

    enc.tick(); // опрос энкодера происходит здесь

    // Обработка нажатий кнопки
    if (enc.click())
    {
      modeHue = !modeHue; // При нажатии меняем режим энкодера
      if (modeHue)
      {
        enc.counter = chHue;   // Передаем счетчику канал оттенка
        chHueDelay = millis(); // Включаем задержку перехода в канал яркости
      }
      else
      {
        enc.counter = chBright; // Передаем счетчику канал яркости
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
      enc.counter = chBright; // Сохраняем в счетчик максимальную яркость
      sendPWM();              // Рассчитываем ШИМ и отправляем диодам

      // Serial.println("Максимальная яркость обоих каналов!!!");
    }

    // Обработка поворотов
    if (enc.turn())
    {

      if (enc.fast())
      { // В быстром режиме
        if (enc.right())
          enc.counter += fastStep; // при повороте направо прибавлять по fastStep,
        else
          enc.counter -= fastStep; // при повороте налево убавлять по fastStep
      }

      enc.counter = constrain(enc.counter, 0, 255); // Окраничиваем диапазон enc.counter

      // Определяемся в какой канал пишет энкодер
      if (modeHue)
      {
        chHue = enc.counter;   // Значения счетчика записываются в канал цвета
        chHueDelay = millis(); // Сбрасывам задежку переключения в канал яркости
      }
      else
      {
        chBright = enc.counter; // Значения счетчика записываются в канал яркости
      }
      sendPWM(); // Рассчитываем ШИМ и отправляем диодам
    }

    // Обработа нажатых поворотов
    if (enc.turnH())
    {
      modeHue = false;        // Преводим энкодер в режим яркости
      chBright = 255;         // Канал яркости - в максимум
      enc.counter = chBright; // Сохраняем в счетчик максимальную яркость
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
  analogWrite(ledPin1, ledWarm); // Теплые диоды
  analogWrite(ledPin2, ledCold); // Холодные диоды
  // Serial.println("chHue=" + String(chHue));
  // Serial.println("ledCold=" + String(ledCold) + ", ledWarm=" + String(ledWarm));
}