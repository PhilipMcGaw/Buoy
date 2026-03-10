#include <Arduino.h>


// https://www.gammon.com.au/forum/?id=11428

#include "src/RS485_protocol/RS485_protocol.h"
#include <SoftwareSerial.h>

// Serial number etc
#include "ArduinoUniqueID.h"
#include "Adler16.h"

// These are required for the I2C 20x4 LCD screen used for diagnostics.
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// initialize the liquid crystal library
// the first parameter is  the I2C address
// the second parameter is how many rows are on your screen
// the  third parameter is how many columns are on your screen
LiquidCrystal_I2C lcd(0x27,  20, 4);

#define SERIAL_PORT_SPEED 9600
#define CompileDate __DATE__
#define CompileTime __TIME__

// Built in LED for heartbeat
const byte LED_PIN = 13;
bool LED_STATUS = true;
unsigned long previousMillis = 0;  // will store last time LED was updated
const uint8_t LED_Frequancy = 1;  // frequancy in Hz at which to blink
uint8_t heartBeatCNT = 0; // this will overflow at 255


// RS485 Pins
const byte UART1_TX = 2;
const byte UART1_TX_EN = 3;
const byte UART1_RX = 4;

// Address pins
const int ADDR_PIN_1 = A1;
const int ADDR_PIN_2 = A2;
uint8_t Address = 0;

const String softwareVersion = String(CompileDate) + " " + String(CompileTime); // Software version string
String serialNumber = "NAN";

String adler(String text)
{
  Adler16 adler16;
  adler16.begin();
  for (int j = 0; text[j] != 0; j++) {
    adler16.add(text[j]);
  }
  text = String(adler16.getAdler(), HEX);
  text.toUpperCase();
  return text;
}


SoftwareSerial rs485 (UART1_RX, UART1_TX);  // receive pin, transmit pin

int address(int pin_1 = 0, int pin_2 = 0, int pin_3 = 0)
{
  // Pin three is un-used at the moment.
  uint8_t Addr_1 = analogRead(pin_1);
  uint8_t Addr_2 = analogRead(pin_2);

//  Serial.println("--------");
//  Serial.print("raw: "); 
//  Serial.print(Addr_1);
//  Serial.print(" - ");
//  Serial.println(Addr_2);

  Addr_1 = map(Addr_1, 0, 255, 0, 2);
  Addr_2 = map(Addr_2, 0, 255, 0, 2);

//  Serial.print("map: ");
//  Serial.print(Addr_1);
//  Serial.print(" - ");
//  Serial.println(Addr_2);

  if (Addr_2 == 0) {
    Addr_2 = 0;
  } else if (Addr_2 == 1) {
    Addr_2 = 3;
  } else {
    Addr_2 = 6;
  }
  
  Address = Addr_1 + Addr_2;
  
//  Serial.print("val: ");
//  Serial.print(Addr_1);
//  Serial.print(" - ");
//  Serial.print(Addr_2);
//  Serial.print(" - ");
//  Serial.println(Address);

  return Address;
}

String SerialNumber ()
{
  String sn;
  for (size_t i = 0; i < UniqueIDsize; i++)
  {
    sn += String(UniqueID[i], HEX);
  }
  sn.toUpperCase();
  return sn;
}

// callback routines

void heartbeat()
{
    if (millis() - previousMillis >= ( 500 / LED_Frequancy)) {
    // 500 is half a second / frequancy in Hz.
    // save the last time you blinked the LED
    previousMillis = millis();
    
    // if the LED is off turn it on and vice-versa:
    if (LED_STATUS == true) {
      LED_STATUS = false;
    } else {
      LED_STATUS = true;
    }

    heartBeatCNT ++;  // increase the counter at twice the LED frequancy

    digitalWrite(LED_PIN,LED_STATUS); // Set the LED status
  }
}
  
void fWrite (const byte what)
{
  rs485.write (what);  
}
  
int fAvailable ()
{
  return rs485.available ();  
}

int fRead ()
{
  return rs485.read ();  
}

void setup()
{
  rs485.begin (SERIAL_PORT_SPEED);
  Serial.begin (SERIAL_PORT_SPEED);
  pinMode (UART1_TX_EN, OUTPUT);    // RS485 driver output enable
  pinMode (LED_PIN, OUTPUT);        // built-in LED Pin
  digitalWrite(LED_PIN,LED_STATUS); // Turn the built-in LED on

  lcd.init(); //initialize lcd screen
  lcd.clear(); // Clears the LCD screen 

  lcd.backlight(); // turn on the backlight

  Address = address(ADDR_PIN_1, ADDR_PIN_2); // Read in the value from the two address pins
  serialNumber = SerialNumber(); // Gets the raw string
  serialNumber = adler(serialNumber); // Gets the adled verion
}


void loop()
{
  heartbeat();

  lcd.setCursor(0,0);
  lcd.print("Sub PCB Diagnostics");
  lcd.setCursor(0,1);
  lcd.print("ADDR: " + String(Address));
  lcd.setCursor(9,1);
  lcd.print("SN: " + String(serialNumber));
  lcd.setCursor(0,3);
  lcd.print("Heartbeat:      "); 
  lcd.setCursor(0,3);
  lcd.print("Heartbeat: " + String(heartBeatCNT)); 

}  // end of loop