const String WERSJA = "5.4";
/*
Zmiany w 5.4
  - LCD 20x4
Zmiany w 5.2
  - Poprawiony powrót do pozycji startowej po przerwaniu
Zmiany w 5.1
  - Zwiększono prędkość ruchu foto z 75 na 200
Zmiany w 5.0
  - dodano przerwanie cyklu fotografowania
*/

/*
 - Arduino
 - LCD I2C
 - Nettigo Keypad
 - Motor Shield R3 
 LCD:
 * 5V to Arduino 5V pin
 * GND to Arduino GND pin
 * CLK to Analog #A5       
 * DAT to Analog #A4       
 
 KeyPad:
  AD1:
  1 (marked with square pad) - +5V
  2 - A2 on Arduino
  3 - GND
*/


// Library
#include "Wire.h"
#include <LiquidCrystal_PCF8574.h>
//#include "LiquidCrystal.h"
#include <NettigoKeypad.h>
#include <Stepper.h>
#include <EEPROM.h>

//Pozycje MENU
char Opcje[7][18] ={"Ustaw START      ", 
                    "Ustaw STOP       ", 
                    "Ilosc krokow     ",
                    "Opozn. migawki   ", //[ms]
                    "Czas otw.migawki ", //[ms]
                    "Zapisz ustawienia",                   
                    "Czytaj ustawienia"};

// Map our pins to constants to make things easier to keep track of
const int pwmA = 3;
const int pwmB = 11;
const int brakeA = 9;
const int brakeB = 8;
const int dirA = 12;
const int dirB = 13;

const int pin_MIGAWKA = 7; //Pin sterujący migawką
const int CzasOtwarciaMigawki = 25; //czas przyciśnięcia spustu migawki [ms]

const int key_PLUS = 4;
const int key_MINUS = 3; 
const int key_SET = 5; 
const int key_RANGE = 2;
const int key_START = 1;
const int NIC =0;

int START = 0;
int STOP = 800; 
int POZYCJA = 0; //aktualna pozycja silnika
int KROK = 30; //krok silnika w fazie fotografowania (0.15 mm)

int Co_Nacisniete = 0;
int Nacisniete=0;

int shutter_DELAY = 500;    //Opóźnienie migawki [ms]
int shutter_TIME = 1500;     //Czas otwarcia migawki [ms]
int move_Speed = 200; //Prędkość silnika przy ustawianiu zakresu
int foto_Speed = 200; //Prędkość silnika przy fotografowaniu

// Connect via i2c, default address #0 (A0-A2 not jumpered)
LiquidCrystal_PCF8574 lcd(0x3F); //LCD 20x4-0x3F
//LiquidCrystal_PCF8574 lcd(0x27); //LCD 16x2-0x27
NG_Keypad keypad;

char* keys[] = { "NIC", "START", "RANGE", "-", "+", "SET" };


const int STEPS = 20; //Krok przy ustawianiu zakresu 

// Initialize the Stepper class
Stepper myStepper(100, dirA, dirB);

void setup() {
  // Set the RPM of the motor
  myStepper.setSpeed(move_Speed);
  
  // Turn on pulse width modulation
  pinMode(pwmA, OUTPUT);
  digitalWrite(pwmA, HIGH);
  pinMode(pwmB, OUTPUT);
  digitalWrite(pwmB, HIGH);
  
  // Turn off the brakes
  pinMode(brakeA, OUTPUT);
  digitalWrite(brakeA, LOW);
  pinMode(brakeB, OUTPUT);
  digitalWrite(brakeB, LOW);

  // Log some shit
  Serial.begin(9600);

  // set up the LCD's number of rows and columns: 
lcd.begin(20, 4);

  pinMode(pin_MIGAWKA, OUTPUT); //Migawka
//                                      digitalWrite(pin_MIGAWKA, HIGH);  //Migawka zamknięta
//                                      delay(1000);
  digitalWrite(pin_MIGAWKA, LOW);  //Migawka zamknięta
    lcd.setBacklight(255);
        lcd.home();
  lcd.clear();
  lcd.print("PhotoSTACKER ");
  lcd.print(WERSJA);
  delay(2000);
}



void loop() {
  lcd.clear();
  lcd.print(">START,RANGE,SET");
  lcd.setCursor(0,1); //x=0. Linia=2
  lcd.print(float(STOP)/200);  //Wyświeltenie długości zakresu z jednym miejscem po przecinku
  lcd.print("/");
  lcd.print(float(KROK)/200);  //Długość kroku
  lcd.print("mm ");
  lcd.print(STOP/KROK+1);      //Ilość zdjęć
  lcd.print("x");
  lcd.setCursor(0,2); //x=0. Linia=3
  lcd.print(float(STOP)/200);
  lcd.setCursor(0,3); //x=0. Linia=4
  lcd.print(float(STOP)/200);
   delay(500);
  do {
      Co_Nacisniete = Klawisz();
      } while (Co_Nacisniete != key_RANGE &&  Co_Nacisniete != key_START && Co_Nacisniete != key_SET);
      lcd.clear();
      switch (Co_Nacisniete)
                  {
                  case key_RANGE:   //Ustawienie zakresu ruchu
                        STOP = Zakres_ruchu();
                        break;
                  case key_START:
                        //Uruchomienie ruchu krokowego i wyzwalania migawki
                        FOTO(KROK, STOP, shutter_DELAY, shutter_TIME);
                        break;
                  case key_SET:    //ustawienie opóżnienia, czasu migawki i kroku
                        lcd.clear();                        
                        Parametry();
                        break;
                  }
}


//---------------------------------------------------
//FUNKCJE

int Klawisz() //Odczytuje naciśnięty kalwisz. Odczyt pinu A2.Możliwe stany: 0, key_PLUS, key_MINUS, key_SET, key_MENU, key_START
{
  int rd;
  rd = analogRead(2);
  return keypad.key_pressed(rd);
}

//------------------------------------------------------------------------------
//Ustawienie pozycji silnika (START lub STOP) zwraca ilość kroków
int Ustaw()
{
  int Ustawienie = 0;
  do {
    switch (Klawisz())
                  {
                  case key_PLUS:
                        myStepper.step(STEPS);
                        Serial.println(STEPS);
                        Ustawienie = Ustawienie + STEPS;
                        lcd.setCursor(0,1); //x=0. Linia=2
                        lcd.print(float(Ustawienie)/200);
                        lcd.print("mm   ");
                        break;
                  case key_MINUS:
                        myStepper.step(-STEPS);
                        Serial.println(-STEPS);  
                        Ustawienie = Ustawienie - STEPS;
                        lcd.setCursor(0,1); //x=0. Linia=2
                        lcd.print(float(Ustawienie)/200);
                        lcd.print("mm   ");
                        break;
                  case key_SET:
                        return Ustawienie;
                  }
  } while (true);
}
//---------------------------------------------------------------
void FOTO(int STEP, int END, int OPOZNIENIE, int MIGAWKA)
{
   //zmniejszenie prędkości i przesuwanie skokowe silnika
  myStepper.setSpeed(foto_Speed);
  lcd.clear();
  lcd.print("PROCESSING");
  POZYCJA=0;
  do {
      lcd.setCursor(0,1); //x=0. Linia=2
      lcd.print(float(POZYCJA)/200);
      lcd.print("mm/");
      lcd.print(float(END)/200);
      lcd.print("mm");
      delay(OPOZNIENIE);
      //
      //--------------------------------------------------
      // WYZWOLENIE MIGAWKI
        digitalWrite(pin_MIGAWKA, HIGH);      //Otwarcie migawki
        delay(CzasOtwarciaMigawki);
        digitalWrite(pin_MIGAWKA, LOW);      //Zamknięcie migawki

      //--------------------------------------------------
      //
      delay(MIGAWKA);
      myStepper.step(STEP);
      POZYCJA = POZYCJA + STEP;      
      //Przerwanie fotografowania po ponownymn naciśnięciu START i Powrót do pozycji startowej
      if (Klawisz()==key_START)
          {
            lcd.clear();
            lcd.setCursor(0,0); //x=0. Linia=2
            lcd.print("CANCELING");
            lcd.setCursor(0,1); //x=0. Linia=2
            lcd.print("BACK to START");
            delay(1000);
            myStepper.step(-POZYCJA+STEP);   //Powrót na START      
            delay(1000);
            return;          
          }
    } while (POZYCJA <= END);  //Zdjęcie musi się zrobic również w ostatnim położeniu, dlatego END+STEP
   lcd.setCursor(0,1); //x=0. Linia=2
   lcd.print(float(POZYCJA)/200);
   lcd.print("mm/");
   delay(OPOZNIENIE);
   //--------------------------------------------------
   // WYZWOLENIE MIGAWKI
   digitalWrite(pin_MIGAWKA, HIGH);     //Otwarcie migawki
   delay(CzasOtwarciaMigawki);
   digitalWrite(pin_MIGAWKA, LOW);      //Zamknięcie migawki
   //--------------------------------------------------
   delay(MIGAWKA);
   myStepper.setSpeed(move_Speed); 
                        //Powrót do pozycji START
                        lcd.clear();
                        lcd.setCursor(0,0); //x=0. Linia=1
                        lcd.print("END PROCESS");
                        lcd.setCursor(0,2); //x=0. Linia=2
                        lcd.print("Move to START");
                        myStepper.step(-END);
                        lcd.clear();                        
 
}
//------------------------------------
int Zakres_ruchu()
  {
    int POCZATEK;
    int KONIEC;
    int POZYCJA;
    myStepper.setSpeed(move_Speed);
    //Ustawienie głowicy w położenie początkowe
    lcd.setCursor(0,0); //x=0. Linia=1
    lcd.print("Set START");
    Ustaw();
    POCZATEK=0;
    //Kasowanie wyświetlacza
    lcd.clear();
    lcd.print("SAVE");
    delay(1000);
    lcd.clear();
    lcd.setCursor(0,0); //x=0. Linia=1
    //Ustawienie głowicy w położenie początkowe
    lcd.print("Set STOP ");
    KONIEC=Ustaw();
    POZYCJA=KONIEC;
    //Kasowanie wyświetlacza
    lcd.clear();
    lcd.print("SAVE");
    delay(1000);
    lcd.clear();
    if (KONIEC<0)
      {
        KONIEC = -KONIEC;
        POZYCJA=KONIEC;
      }
    else
      { 
        lcd.print("Move to START");
        myStepper.step(-POZYCJA); //Powrót do pozycji START
      }  
    return KONIEC;
  }
//----------------------------------------  
  void Parametry()
  {
      delay(500);
      KROK=WARTOSC("Set FOTO STEP", KROK,2, 200, "mm", 2);
      delay(500);
      shutter_DELAY=WARTOSC("TIME BEFORE FOTO",shutter_DELAY,10, 1000, "s", 10);
      delay(500);
      shutter_TIME=WARTOSC("SHUTTER TIME",shutter_TIME,10, 1000, "s", 10);
      delay(500);
      /*
          foto_Speed==WARTOSC("FOTO SPEED",foto_Speed,25,1, "", 25);
        delay(500);
      */
      lcd.clear();
  }
//----------------------------------------------------------  
  int WARTOSC(String TEXT, int X, int SKOK, int SKALA, String JEDNOSTKA, int MINIMUM)
  {
    lcd.clear();
    lcd.print(TEXT);
    lcd.setCursor(0,1);
    lcd.print(float(X)/SKALA); 
    lcd.print(JEDNOSTKA);
    do {
         switch (Klawisz())
         {
           case key_PLUS:
                        if (Nacisniete>=5)
                            {X += 5*SKOK;}
                        else
                            {X += SKOK;}
                        lcd.clear();
                        lcd.print(TEXT);
                        lcd.setCursor(0,1);
                        lcd.print(float(X)/SKALA);
                        lcd.print(JEDNOSTKA);
                        delay(100);
                        Nacisniete+=1;
                        break;
           case key_MINUS:
                        if (Nacisniete>=5)
                          {X -= 5*SKOK;}
                        else
                          {X -= SKOK;}
                        if (X<=0)
                          {X=MINIMUM;}
                        lcd.clear();
                        lcd.print(TEXT);
                        lcd.setCursor(0,1);
                        lcd.print(float(X)/SKALA);
                        lcd.print(JEDNOSTKA);
                        delay(100);
                        Nacisniete+=1;
                        break;
           case key_SET:
                        lcd.clear();
                        lcd.print("SAVE");
                        Nacisniete=0;
                        return X;
           case NIC:
                        Nacisniete=0;
                        break;                        
                  }
        } while (true);
  }
