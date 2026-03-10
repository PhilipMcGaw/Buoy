#include <Arduino.h>

const String deviceIDN = "RS485 Test";  // IDN response
const String devicePath = "AHRS";     // root for DUT in MQTT messages)

struct MQTTTopicMessageStuct
{
  const PROGMEM String topic;            // Full MQTT topic (this is used to calculate the Adler16 topic ID) [remove once adler16 is pre-calculated]
  String adlerTopic;                    // Adler16 MQTT topic ID as string
  const bool direction;                  // input from serial = 0, output to serial = 1
  String message;                        // Current message  
  String lastMessage;                   // Last message sent
  const PROGMEM String defaultMessage;  // Default message (used to initialize the message at startup and set home positions on shutdown etc)
  const uint16_t period_ms;              // Period to re-transmit the message after (maximum period is 65,535 ~ 65 seconds)
  long next_sent_ms;                     // Next sent time in milliseconds
};

// topic,                              adlerTopic, direction, message, lastMessage, defaultMessage,  period_ms, last_sent_ms, description (comment),
MQTTTopicMessageStuct MQTTTopicMessageData[] =
{
  {"board/idn",                        "",          1,          "0",     "0",           "0",              5000,      0},           // Board ID
  {"board/sn",                         "",          1,          "0",     "0",           "0",              60000,     0},           // Board Serial Number
  {"board/ver",                        "",          1,          "0",     "0",           "0",              60000,     0},           // Software Version
  {"board/heartbeat",                  "",          1,          "0",     "0",           "0",              9999,      0},           // an Incramental number between 0-255, period_ms is over-sized; so should be ignored.
//  {"in/debug/demand",                  "",          0,         "0",     "0",          "0",              5000,      0},           // Debug mode demand from SBC
//  {"out/debug/status",                 "",          1,         "0",     "0",          "0",              60000,     0},           // Debug mode status to SBC
//  {"in/gyro/spin",                     "",          1,         "0",     "0",          "0",              2000,      0},           // Analog channel 0: Battery Level
//  {"in/gyro/tilt",                     "",          1,         "0",     "0",          "0",              2000,      0},           // Analog channel 1: Light Tracker
//  {"in/gyro/veer",                     "",          1,         "0",     "0",          "0",              2000,      0},           // Analog channel 2: un-used
//  {"in/acc/x",                         "",          1,         "0",     "0",          "0",              2000,      0},           // Analog channel 3: un-used
//  {"in/acc/y",                         "",          1,         "0",     "0",          "0",              2000,      0},           // Analog channel 4: un-used
//  {"in/acc/z",                         "",          1,         "0",     "0",          "0",              2000,      0},           // Analog channel 5: un-used
//  {"in/mag/x",                         "",          1,         "0",     "0",          "0",              2000,      0},           // Analog channel 6: un-used
//  {"in/mag/y",                         "",          1,         "0",     "0",          "0",              2000,      0},           // Analog channel 7: un-used
//  {"in/mag/z",                         "",          1,         "0",     "0",          "0",              2000,      0},           // Analog channel 8: un-used
//  {"in/imu/pitch",                     "",          1,         "0",     "0",          "0",              5000,      0},           // Pitch angle
//  {"in/imu/roll",                      "",          1,         "0",     "0",          "0",              5000,      0},           // Roll angle
//  {"in/imu/head",                      "",          1,         "0",     "0",          "0",              5000,      0},           // Magnetic Heading
//  {"out/gps/lock",                     "",          1,         "0",     "0",          "0",              5000,      0},           // GPS Lock status
//  {"out/gps/time",                     "",          1,         "0",     "0",          "000000",         5000,      0},           // GPS Time
//  {"out/gps/location/lng",             "",          1,         "0",     "0",          "00",             5000,      0},           // GPS Longitude
//  {"out/gps/location/lat",             "",          1,         "0",     "0",          "00",             5000,      0},           // GPS Latitude
//  {"out/gps/location/altitude",        "",          1,         "0",     "0",          "00",             5000,      0},           // GPS Altitude
//  {"out/misc/barometer",               "",          1,         "0",     "0",          "0",              5000,      0},           // Barometric Pressure
//  {"out/misc/altitude",                "",          1,         "0",     "0",          "0",              5000,      0},           // Altitude in meters
};

uint16_t topicSize;



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
const uint8_t LED_Frequancy = 2;   // frequancy in Hz at which to blink
uint8_t heartBeatCNT = 0;          // this will overflow at 255


// RS485 Pins
const byte UART1_TX = 2;
const byte UART1_TX_EN = 3;
const byte UART1_RX = 4;

SoftwareSerial rs485 (UART1_RX, UART1_TX);  // receive pin, transmit pin

// Address pins
const int ADDR_PIN_1 = A1;
const int ADDR_PIN_2 = A2;
uint8_t rs485Address = 0;

const String softwareVersion = String(CompileDate) + " " + String(CompileTime); // Software version string
String serialNumber = "NAN";

// Serial stuff for sending / receiving communications
const char delimiter = ':';             // use '' NOT ""

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


int address(int pin_1 = 0, int pin_2 = 0, int pin_3 = 0)
{
  // Pin three is un-used at the moment😊
  uint8_t Addr_1 = analogRead(pin_1);
  uint8_t Addr_2 = analogRead(pin_2);
  uint8_t Addr = 0;

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
  
  Addr = Addr_1 + Addr_2;
  
//  Serial.print("val: ");
//  Serial.print(Addr_1);
//  Serial.print(" - ");
//  Serial.print(Addr_2);
//  Serial.print(" - ");
//  Serial.println(Addr);

  return Addr;
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
    digitalWrite(LED_PIN,LED_STATUS); // Set the LED status
    heartBeatCNT ++;                  // increase the counter at twice the LED frequancy           // increment the heartbeat, hearbeat is an unsigned 8 bit int; so will overflow at 255 wrapping back to 0
  }
}

// callback routines
  
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
  while (!Serial);
  pinMode (UART1_TX_EN, OUTPUT);    // RS485 driver output enable
  pinMode (LED_PIN, OUTPUT);        // built-in LED Pin
  digitalWrite(LED_PIN,LED_STATUS); // Turn the built-in LED on

  lcd.init(); //initialize lcd screen
  lcd.clear(); // Clears the LCD screen 

  lcd.backlight(); // turn on the backlight

  rs485Address = address(ADDR_PIN_1, ADDR_PIN_2); // Read in the value from the two address pins
  serialNumber = SerialNumber(); // Gets the raw string
  serialNumber = adler(serialNumber); // Gets the adled verion

  //clean up software version to remove : so that its not the same as the deliminator
  softwareVersion.replace(":", ".");
  


  
  

  // This pre-fills the MQTTTopicMessageData struct.

  devicePath.toLowerCase();   // should only need done once

  topicSize = sizeof(MQTTTopicMessageData) / sizeof(MQTTTopicMessageData[0]);   // get the size of the MQTTTopicMessageData array (number of rows/topics)
  for (int i = 0; i < topicSize; i++) {
    MQTTTopicMessageStuct& currentTopic = MQTTTopicMessageData[i];                 // The & is important to create a reference to the array element rather than a copy! -- https://www.reddit.com/r/arduino/comments/1qno8ko/comment/o1vlejc/

    currentTopic.message = currentTopic.defaultMessage;                            // Initialize the message to the default message,. This will get over-written in specific cases below

    String fullTopic = devicePath + "/" + currentTopic.topic;                      

    if (currentTopic.adlerTopic == "");
    {
      currentTopic.adlerTopic = adler(fullTopic);
    }

    if (currentTopic.topic == "board/sn")                                          // Fill in the serial number
    {
      currentTopic.message = serialNumber;
    }

    if (currentTopic.topic == "board/idn")                                         // Fill in the IDN
    {
      currentTopic.message = deviceIDN;
    }

    if (currentTopic.topic == "board/ver")                                         // Fill in software Version
    {
      currentTopic.message = softwareVersion;
    }


    // At the end of Setup topics should look like:
    // "ahrs/board/idn -> E969:0"
    Serial.print(fullTopic);
    Serial.print(" -> ");
    Serial.println(currentTopic.adlerTopic + delimiter + currentTopic.message);
  }
  delay(1000);
}


void loop()
{

  heartbeat();
  
  lcd.setCursor(0,0);
  lcd.print(String(deviceIDN));
  lcd.setCursor(0,1);
  lcd.print("ADDR: " + String(rs485Address));
  lcd.setCursor(9,1);
  lcd.print("SN: " + String(serialNumber));
  lcd.setCursor(0,2);
  lcd.print(String(softwareVersion));



  for (int i = 0; i < topicSize; i++) 
  {
    MQTTTopicMessageStuct& currentTopic = MQTTTopicMessageData[i];                     // The & is important to create a reference to the array element rather than a copy

    if (currentTopic.direction == 1 && (currentTopic.lastMessage != currentTopic.message || millis() >= currentTopic.next_sent_ms)) // is it an output topic to serial AND (has it changed or time to resend it)?
    {
      currentTopic.lastMessage = currentTopic.message;                                 // update the last message sent
      currentTopic.next_sent_ms = millis() + currentTopic.period_ms + random(20);      // Reset the next sent time with a small random delay to avoid collisions

      Serial.println(currentTopic.adlerTopic + delimiter + currentTopic.message);      // send the topic and message to serial
      
    }

    if (currentTopic.topic == "board/heartbeat")                                       // heartbeat is a special case
    {
      currentTopic.message = heartBeatCNT;                                             // update the heartbeat message from the counter on the screen.

      lcd.setCursor(0,3);
      lcd.print("Heartbeat:"); 
      lcd.setCursor(11,3); 
      lcd.print("   ");
      lcd.setCursor(11,3); 
      lcd.print(String(currentTopic.message)); 

    }
  }




}  // end of loop