#include <Arduino.h>

const String deviceIDN = "RS485 Test";  // IDN response
String devicePath = "RS485";     // root for DUT in MQTT messages, or ADDR for using the Address Pins.

struct MQTTTopicMessageStuct
{
  const PROGMEM String topic;            // Full MQTT topic (this is used to calculate the Adler16 topic ID)
  String adlerTopic;                     // Adler16 MQTT topic ID as string (calculated at run time to allow for dynamic addressing using address pins)
  const bool direction;                  // input from serial = 0, output to serial = 1
  String message;                        // Current message  
  String lastMessage;                    // Last message sent
  const PROGMEM String defaultMessage;   // Default message (used to initialize the message at startup and set home positions etc)
  const uint16_t period_ms;              // Period to re-transmit the message after (maximum period is 65,535 ~ 65 seconds)
  long next_sent_ms;                     // Next time to send in milliseconds
};

// topic,                              adlerTopic, direction, message, lastMessage, defaultMessage,  period_ms, last_sent_ms, description (comment),
MQTTTopicMessageStuct MQTTTopicMessageData[] =
{
  {"board/idn",                        "",         1,         "0",     "0",         "0",              65000,    0},           // Board ID (65 seconds between TX)
  {"board/sn",                         "",         1,         "0",     "0",         "0",              65000,    0},           // Board Serial Number (65 seconds between TX)
  {"board/ver",                        "",         1,         "0",     "0",         "0",              65000,    0},           // Software Version (65 seconds between TX)
  {"board/heartbeat",                  "",         1,         "0",     "0",         "0",              65000,    0},           // an Incramental number between 0-255, period_ms is over-sized; so should be ignored.
  {"in/analog/0",                      "",         1,         "0",     "0",         "0",                200,    0},           // Analog channel 0: Battery Level  -- 34A6
//  {"in/analog/1",                      "",         1,         "0",     "0",         "0",                200,    0},           // Analog channel 1: Light Tracker
//  {"in/analog/2",                      "",         1,         "0",     "0",         "0",                200,    0},           // Analog channel 2: un-used
//  {"in/analog/3",                      "",         1,         "0",     "0",         "0",                200,    0},           // Analog channel 3: un-used  -- 37A9
  {"out/digital/5",                    "",         0,         "0",     "0",         "0",                  0,    0},           // Digital channel 5: un-used  -- 2D9D
  {"out/digital/6",                    "",         0,         "0",     "0",         "0",                  0,    0},           // Digital channel 6: un-used  -- 2E9E
  {"out/digital/7",                    "",         0,         "0",     "0",         "0",                  0,    0},           // Digital channel 7: un-used  -- 2F9F
  {"out/digital/8",                    "",         0,         "0",     "0",         "0",                  0,    0},           // Digital channel 8: un-used  -- 30A0
//  {"out/digital/9",                    "",         0,         "0",     "0",         "0",                  0,    0},           // Digital channel 9: un-used
//  {"out/digital/10",                   "",         0,         "0",     "0",         "0",                  0,    0},           // Digital channel 10: un-used
//  {"out/digital/11",                   "",         0,         "0",     "0",         "0",                  0,    0},           // Digital channel 11: un-used
//  {"out/digital/12",                   "",         0,         "0",     "0",         "0",                  0,    0},           // Digital channel 12: un-used
};

uint8_t topicSize;



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


#define SERIAL_PORT_SPEED 115200
#define CompileDate __DATE__
#define CompileTime __TIME__

// Built in LED for heartbeat
const uint8_t LED_Frequancy = 2;   // frequancy in Hz at which to blink
const byte LED_PIN = 13;           // Which pin is the LED on (built in LED is on pin 13)
bool LED_STATUS = true;            // initial state of the LED
unsigned long previousMillis = 0;  // will store last time LED was updated
uint8_t heartBeatCNT = 0;          // this will overflow at 255


// Serial stuff for sending / receiving communications
const char delimiter = ':';             // use '' NOT ""
String serialResponse;
String topic;
String message;
int delimiterLocation;

// RS485 Pins
const byte UART1_TX = 2;
const byte UART1_TX_EN = 3;
const byte UART1_RX = 4;
bool CTS = false;
bool poll = false;

SoftwareSerial rs485 (UART1_RX, UART1_TX);  // receive pin, transmit pin

// Address pins
const int ADDR_PIN_1 = A1;
const int ADDR_PIN_2 = A2;
uint8_t rs485Address = 0;

String softwareVersion = String(CompileDate) + " " + String(CompileTime); // Software version string
String serialNumber = "NAN";


String adler(String text)
{
  Adler16 adler16;    // no idea what this does
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
  // Pin three is un-used at the moment
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
  if (millis() - previousMillis >= ( 500 / LED_Frequancy))     // 500 is half a second / frequancy in Hz.
  {
    // save the last time you blinked the LED
    previousMillis = millis();
  
    // if the LED is off turn it on and vice-versa:
    if (LED_STATUS == true) {
      LED_STATUS = false;
    } else {
      LED_STATUS = true;
    }
    digitalWrite(LED_PIN,LED_STATUS); // Set the LED status
    heartBeatCNT ++;                  // increase the counter at twice the LED frequancy, hearbeat is an unsigned 8 bit int; so will overflow at 255 wrapping back to 0

    if (heartBeatCNT >= 10)            // I2C writes to the screen take ages, reduce from 0-255 to 0-9 to reduce number of charictors that get re-drawn
    {
      heartBeatCNT = 0;
    }
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
  pinMode (UART1_TX_EN, OUTPUT);    // RS485 driver output enable
  pinMode (LED_PIN, OUTPUT);        // built-in LED Pin
  pinMode (5, OUTPUT);
  pinMode (6, OUTPUT);
  pinMode (7, OUTPUT);
  pinMode (8, OUTPUT);
  
  digitalWrite(LED_PIN,LED_STATUS); // Set the built-in LED to the initial status


  lcd.init(); //initialize lcd screen
  lcd.clear(); // Clears the LCD screen 

  lcd.backlight(); // turn on the backlight

  rs485Address = address(ADDR_PIN_1, ADDR_PIN_2); // Read in the value from the two address pins

  // get the serial number and hash it.
  serialNumber = SerialNumber(); // Gets the raw string
  serialNumber = adler(serialNumber); // Gets the adled verion

  //clean up software version to remove : so that its not the same as the deliminator
  softwareVersion.replace(":", ".");
  

  // This pre-fills the MQTTTopicMessageData struct.

  devicePath.toLowerCase();   // only need done once

  if (devicePath == "addr")
  {
    devicePath = rs485Address;
    CTS = false;
    poll = true;
  }

  topicSize = sizeof(MQTTTopicMessageData) / sizeof(MQTTTopicMessageData[0]);      // get the size of the MQTTTopicMessageData array (number of rows/topics)
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

  lcd.setCursor(0,0);
  lcd.print(String(deviceIDN));
  lcd.setCursor(0,1);
  lcd.print("ADDR: " + String(devicePath));
  lcd.setCursor(12,1);
  lcd.print("SN: " + String(serialNumber));
  lcd.setCursor(0,2);
  lcd.print(String(softwareVersion));
  lcd.setCursor(0,3);                                                              // update the screen
  lcd.print("Booting..."); 
  delay(5000);
  lcd.setCursor(0,3); 
  lcd.print("Heartbeat:"); 
}


void loop()
{

  heartbeat();
  
  for (int i = 0; i < topicSize; i++) 
  {
    MQTTTopicMessageStuct& currentTopic = MQTTTopicMessageData[i];                     // The & is important to create a reference to the array element rather than a copy

    // update the messages from sources 

    if (currentTopic.topic == "board/heartbeat")                                       // heartbeat is a special case and is updated from an external counter
    {
      currentTopic.message = heartBeatCNT;                                             // update the heartbeat message from the counter

      lcd.setCursor(11,3); 
      lcd.print(String(currentTopic.message)); 
    }

    if (currentTopic.topic == "in/analog/0")
    {
      currentTopic.message = analogRead(A0);
    }

    if (currentTopic.topic == "in/analog/1")
    {
      currentTopic.message = analogRead(A1);
    }

    if (currentTopic.topic == "in/analog/2")
    {
      currentTopic.message = analogRead(A2);
    }

    if (currentTopic.topic == "in/analog/3")
    {
      currentTopic.message = analogRead(A3);
    }

    // Sends the message if required
    
    if (currentTopic.direction == 1 && (currentTopic.lastMessage != currentTopic.message || millis() >= currentTopic.next_sent_ms)) // is it an output topic to serial AND (has it changed OR time to resend it)? 
    {
      currentTopic.lastMessage = currentTopic.message;                                 // update the last message sent
      currentTopic.next_sent_ms = millis() + currentTopic.period_ms + random(20);      // Reset the next sent time with a small random jitter to reduce the risk of collisions

      Serial.println(currentTopic.adlerTopic + delimiter + currentTopic.message);      // send the topic and message to serial
    }
  }


  // inbound messages

  if (Serial.available() > 0)
  {
    serialResponse = Serial.readStringUntil('\n');                              // read the incoming data as string until newline character
    delimiterLocation = serialResponse.indexOf(delimiter);                      // find the location of the delimiter character  
    if (delimiterLocation != -1)                                                // make sure the delimiter was found
    {
      topic = serialResponse.substring(0, delimiterLocation);                   // extract the topic
      message = serialResponse.substring(delimiterLocation + 1);                // extract the message
    }
  
    for (int i = 0; i < topicSize; i++) 
    {
      MQTTTopicMessageStuct& currentTopic = MQTTTopicMessageData[i];            // The & is important to create a reference to the array element rather than a copy

      if (currentTopic.direction == 0 && currentTopic.adlerTopic == topic)
      {
        currentTopic.message = message;
      }

      if (currentTopic.topic == "out/digital/5")
      {
        digitalWrite(5,currentTopic.message.toInt());
      }

      if (currentTopic.topic == "out/digital/6")
      {
        digitalWrite(6,currentTopic.message.toInt());
      }

      if (currentTopic.topic == "out/digital/7")
      {
        digitalWrite(7,currentTopic.message.toInt());
      }

      if (currentTopic.topic == "out/digital/8")
      {
        digitalWrite(8,currentTopic.message.toInt());
      }

    }
  }

}  // end of loop