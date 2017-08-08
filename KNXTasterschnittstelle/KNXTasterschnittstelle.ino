#include <Wire.h>
#include <math.h>

#define DEBUG // Uncomment this for debug logging to serial
#define LED 9 // Onboard LED on D9

// I2C communication variables
int slaveAddress     = 0x08;    // I2C address of KIMaip board
char i2c_msg[50];               // I2C message will be stored in here
int messageLength    = 0;       // length of I2C message
String commandString = "";      // KIMaip command extracted from the I2C message
int object           = 0;       // Object ID as int
String valueString   = "";      // KIMaip value extracted from the I2C message

// Utility variables
int i = 0;


// Struct definition of a reed record.
typedef struct
{
  int id;              // Reed ID
  int pin;             // Pin the reed contact is attached to Arduino, use a LOW active button --> connect GND to this pin when reed is closed
  String GA;           // GA
  boolean state;       // Current state of the reed contact, 0 = closed , 1 = open
} reedStruct ;


// Reed defintions arcording the buttton structure
// Arduino Pro Mini digital I/O pins used 2-12 + 14-17
// http://www.dominicdube.com/wp-content/uploads/ProMiniPinout.png
// Pins 1+2, not working as TX/RX pins
// Pin 13, not working due to attached onboard LED
// Pins 18+19, already in use for I2C communiction
reedStruct reeds[] = {
  { 1, 14, "10/4/0",  0},
  { 2, 15, "10/4/1",  0},
  { 3, 16, "10/4/2",  0},
  { 4, 17, "10/4/3",  0},
  { 5, 12, "10/4/4",  0},
  { 6, 11, "10/4/5",  0},
  { 7, 10, "10/4/6",  0},
  { 8,  9, "10/4/7",  0},
  { 9,  8, "10/4/8",  0},
  {10,  7, "10/4/9", 0},
  {11,  6, "10/4/10", 0},
  {12,  5, "10/4/11", 0},
  {13,  4, "10/4/12", 0},
  {14,  3, "10/4/13", 0},
  {15,  2, "10/4/14", 0},
};



void setup()
{

  // Instantiate all reed PINs from array
  for (i = 0; i < 15; i++) {
    pinMode(reeds[i].pin, INPUT_PULLUP);
  }

  Serial.begin(9600);
  Serial.println("Starting up...");
  
  // Start I2C bus communication
  Wire.begin();
}

void loop()
{

  for (i = 0; i < 15; i++) {
    if (digitalRead(reeds[i].pin) != reeds[i].state) {
      handleReedStateChange(i);
    }
  }

}


void handleReedStateChange (int reedIndex) {
  
  // Save new reed state to array
  reeds[reedIndex].state = !reeds[reedIndex].state;
  
  // Debug logging
  Serial.print("Pin"); Serial.print(reeds[reedIndex].pin); Serial.print(" changed to "); Serial.println(reeds[reedIndex].state);


  String GA_hex = Dec2Hex(GroupETS2Addr(reeds[reedIndex].GA), 4);

  String GA_1 = GA_hex.substring(0, 2);
  String GA_2 = GA_hex.substring(2, 4);

  char GA_1_char[3];
  GA_1.toCharArray(GA_1_char, 3) ;

  char GA_2_char[3];
  GA_2.toCharArray(GA_2_char, 3) ;

  unsigned long GA_1_hex = strtoul(GA_1_char, NULL, 16);
  unsigned long GA_2_hex = strtoul(GA_2_char, NULL, 16);


  sendMessage_2Bytes('\x70', GA_1_hex, GA_2_hex, '\x00', reeds[reedIndex].state);
}


// Translate GA to ETS format
int GroupETS2Addr (String etsString) {
  int address = 0;
  address = etsString.substring(0, etsString.indexOf("/")).toInt() * 2048;
  etsString = etsString.substring(etsString.indexOf("/") + 1, etsString.length());
  address = address + etsString.substring(0, etsString.indexOf("/")).toInt() * 256;
  address = address + etsString.substring(etsString.indexOf("/") + 1, etsString.length()).toInt();
  return address;
}


// Translate decimal ETS to hex representation
String Dec2Hex (int dec, int hexDigits) {
  String hexNumber = "0123456789ABCDEF";
  String hex = "";
  while (hexDigits > 0)
  {
    hex = hexNumber.substring(dec % 16, dec % 16 + 1) + hex;
    dec = dec / 16;
    hexDigits = hexDigits - 1;
  }
  return hex;
}


// Send KNX telegram via I2C bus
void sendMessage_2Bytes(char KNXcommand, char GA_1_hex, char GA_2_hex, char Byte1, char Byte2) {
  char send_msg[7] = {'\x06', KNXcommand, GA_1_hex, GA_2_hex, '\x02', Byte1, Byte2};
  Wire.beginTransmission(slaveAddress);
  Serial.print("Sending: ");
  for (i = 0; i < sizeof(send_msg); i++)
  {
    Wire.write(send_msg[i]);
    Serial.print(send_msg[i], HEX);
    if (i < sizeof(send_msg) - 1) {
      Serial.print("|");
    }
  }
  Blink(13,500);
  Serial.print(" Errors:"); Serial.println(Wire.endTransmission());
}


void Blink(byte PIN, int DELAY_MS) // Local led blinking function
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN, HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN, LOW);
}

