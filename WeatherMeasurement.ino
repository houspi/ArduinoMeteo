#include <TimerOne.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <iarduino_RTC.h>
#include <DHT.h>
#include <SPI.h>
#include <SD.h>

/*
 * Шаблон Имени Файла
 * YYYY_MM_DD.csv
 * запись в файле
 * DD.MM.YY HH:MM weather point temperature humidity
 */

#define DHT_TYPE DHT22   // Тип датчика темпе ратуры и влажности (AM2302)
#define DHT_PIN 2        // Вывод, к которому подключается датчик
#define LED_PIN 3        // Вывод светодиода
#define BTN_PIN 5        // Кнопка
#define PERIOD 250000    // Частота измерений 1/4 секунды

#define SHORT_CLICK_TIME  250
#define LONG_CLICK_TIME   750
#define BACKLIGHT_TIME   10000

#define WEATHER_NUM 6    // количество состояний погоды
#define POINTS_NUM 3     // количество мест измерений

//задаем адрес LCD экрана 0x27, 16 символов, 2 строки
LiquidCrystal_I2C lcd(0x27, 16, 2);

iarduino_RTC time(RTC_DS3231);

char* points[] = { "ChildRoom", "LivingRoom", "EastSide"};
char* weather[] = { "Clear", "Cloudy", "Overcast", "Rain", "Snow", "Fog" };

DHT dht(DHT_PIN, DHT_TYPE);
float prevouse_t, current_h;
float prevouse_h, current_t;

byte led_state, blink_state, rows;
byte buttonState;

byte action_counter, recoded, done;

unsigned long started, backligth_started, released;
byte weather_point, points_point;

void setup() {
    // Инициализируем порт
    Serial.begin(9600);
    Serial.println("Init start...");

    // Инициализируем дисплей и выводим на него строку об инициализации всего устройства
    lcd.init();
    lcd.clear();
    lcd.backlight();
    lcd.setCursor(0, 0);
    //выводим строку 1
    lcd.print("Init...");

    action_counter = 0;
    recoded = 0;
    done = 0;
    
    // Инициализируем таймер и устанавливаем обработчик прерываний.
    Timer1.initialize(PERIOD);
    Timer1.attachInterrupt(Timer1_action);
    
    // Инициализируем датчик
    // Считываем первоначальные показания
    // Устанавливаем их выше актуальных значений
    dht.begin();
    delay(250);
    prevouse_t = dht.readTemperature() + 2;
    prevouse_h = dht.readHumidity() + 2;

    // Инициализируем SD-карту
    if (!SD.begin(4)) {
        Serial.println("initialization failed!");
        return;
    }

    // Инициализируем часы
    time.begin();

    // Начальное состояние диода - включен
    // Мигание - включено
    pinMode(LED_PIN,  OUTPUT);
    led_state = HIGH;
    blink_state = 1;


    points_point = 0;
    weather_point = 0;
    rows = 0;
    Serial.println("Init complete");
    lcd.noBacklight();
    lcd.clear();
}

void blink_led() {
    if(led_state == LOW) {
        led_state = HIGH;
    } else {
        led_state = LOW;
    }
    digitalWrite(LED_PIN, led_state);
}

void short_click() {

    weather_point++;
    if(weather_point >= WEATHER_NUM) {
        weather_point = 0;
    }
    lcd.setCursor(0, 0);
    lcd.print(weather_point);
    lcd.print(" ");
    lcd.print(weather[weather_point]);
    lcd.print("      ");
    lcd.setCursor(0, 1);
    lcd.print("                ");
}

void long_click() {
    
    lcd.setCursor(0, 0);
    lcd.print("                ");
    points_point++;
    if(points_point >= POINTS_NUM) {
        points_point = 0;
    }
    lcd.setCursor(0, 1);
    lcd.print(points[points_point]);
    lcd.print("     ");
}

/*
 * Обработчик прерывания таймера
 * Измеряем температуру/влажность, если изменение больше 1, то мигаем диодом
 * Если изменений не было, гасим диод
 */
void Timer1_action() {
    if(blink_state) {
        blink_led();
    }
}

/*
 * Записываем измерения в файл
 */
void write_measure() {
    File myFile;
    
    time.gettime();
    if (!SD.exists("weather.cvs")) {
        myFile = SD.open("weather.csv", FILE_WRITE);
        myFile.println("Date;Time;Place;Weather;Temperature;Humidity");
    } else {
        myFile = SD.open("weather.csv", FILE_WRITE);
    }
    if(myFile) {
        myFile.print(time.gettime("d-m-Y;H:i;"));
        myFile.print(points[points_point]);
        myFile.print(";");
        myFile.print(weather[weather_point]);
        myFile.print(";");
        myFile.print(current_t, 2);
        myFile.print(";");
        myFile.print(current_h, 2);
        myFile.println();
        myFile.close();
        rows++;
    } else {
      Serial.println("Cant open file on SD");
    }
}

void show_measurement() {

    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(current_t);
    lcd.print(" H:");
    lcd.print(" ");
    lcd.print(current_h);
    lcd.setCursor(0, 1);
    lcd.print("Rows: ");
    lcd.print(rows);
    lcd.print("         ");
}

void show_settings() {
  
    Serial.println("show settings");
    lcd.setCursor(0, 0);
    lcd.print(weather[weather_point]);
    lcd.print("         ");

    lcd.setCursor(0, 1);
    lcd.print(points[points_point]);
    lcd.print("         ");
}

void show_datetime()
{
    time.gettime();
    lcd.setCursor(0, 0);
    lcd.print(time.gettime("d-m-Y, D"));
    lcd.setCursor(0, 1);
    lcd.print(time.gettime("H:i:s           "));
}

void display_info() {

    time.gettime();
    int what = (int)floor(time.seconds/5) % 3;
    switch (what) {
        case 0:       // Выводим показания
            show_measurement();
            break;
        case 1:       // Выводим настройки
            show_settings();
            break;
        case 2:       // Выводим дату/время
            show_datetime();
            break;
    }
}

void loop() 
{

    time.gettime();
    // Производим проверку измерений один раз в 30 секунд
    if ( !(time.seconds % 30) ) {
        // считывание температуры и влажности 
        current_h = dht.readHumidity();
        current_t = dht.readTemperature();
        // проверяем, были ли ошибки при считывании и, если были, начинаем заново
        if (isnan(current_h) || isnan(current_t)) {
            lcd.setCursor(0, 0);
            lcd.print("Failed read DHT!");
            return;
        }
        /*  Поскольку цикл выполняется несколько раз в секунду, вводим переменную action_counter,
         * которая контролирует проверку измерений
         */
        if(!action_counter) {
            action_counter++;
//            Serial.print("Humidity: ");
//            Serial.print(current_h);
//            Serial.print(" %\t");
//            Serial.print("Temperature: ");
//            Serial.print(current_t);
//            Serial.println(" *C ");
            // Если измерения еще не закончены, мигаем диодом
            if ( (abs(prevouse_h - current_h) >= 1) || (abs(prevouse_t - current_t)>=1)) {
                blink_state = 1;
                prevouse_h = current_h;
                prevouse_t = current_t;
            } else {
                digitalWrite(LED_PIN, LOW);
                blink_state = 0;
            }
        }
        show_measurement();
    } else {
        action_counter=0;
    }

    // Производим запись измерений один раз в 60 секунд
    if (time.seconds == 0 ) {
        if( !blink_state && !recoded ) {
            write_measure();
            recoded = 1;
        }
    } else {
        recoded = 0;
    }

    /* Управение по кнопке
     *  быстрый клик - переключение погоды
     *  длинный клик - переключение места измерения
     *  удерживание  - переклчение режима отображения информации
     *      - текущие показания
     *      - настройки
     *      - дата/время
     */
    buttonState = digitalRead(BTN_PIN);
    if (buttonState == HIGH) {   // button is pressed
        backligth_started = millis();
        lcd.backlight();
        if( !started ) { // Кнопку нажали только что
            started = millis();
            released = 0;
            delay(15);   // защита от "паразитных" срабатываний
        } else {         // Кнопку нажали давно
            unsigned long now = millis();
            if((now - started) > LONG_CLICK_TIME) {  // сли это удержание, то включаем диод и включаем режим отображения
                led_state = HIGH;
                display_info();
            }
        }
    } else {  // button isn't pressed
        if( started ) {  // кнопака была нажата и ее только что отпустили
            released = millis();
            if ( (released - started) < SHORT_CLICK_TIME ) {        // быстрый клик
                short_click();
            } else if ( (released - started) < LONG_CLICK_TIME ) {  // длинный клик
                long_click();
            }
            started = 0;
        }
        if( led_state == HIGH ) {
            led_state = LOW;
        }
    }
    digitalWrite(LED_PIN, led_state);
    if ( backligth_started && ((backligth_started + BACKLIGHT_TIME) < millis()) ) {
        backligth_started = 0;
        lcd.noBacklight();
    }
}

