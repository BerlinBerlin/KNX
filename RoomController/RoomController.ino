
/****************************************************************************************************************
KNX room controller running on Arduino attached to SIMKNX module

Features:
  - handles switch buttons (ON/OFF) with multi click (Single, Double, Triple click)
  - handles jalousie buttons (currently only "Wippenschalter") with multi click (Single, Double, Triple click)
  - handles reed contacts
*****************************************************************************************************************/



/****************************************************************************************************************
START include
*****************************************************************************************************************/


// This library will detect if a button was clicked multiple times  https://github.com/pkourany/clickButton/blob/master/firmware/clickButton.h
#include "ClickButton.h"

// This library is used to communicate with DS18S20 digital temperature sensor
#include <OneWire.h>

/****************************************************************************************************************
END include
*****************************************************************************************************************/



/****************************************************************************************************************
START struct definitions
*****************************************************************************************************************/

// Struct definition of a blind control button
typedef struct
{
  int id;             // Button ID
  int pin;            // Pin the button is attached to Arduino, use a LOW active button --> connect GND to this pin when button is pressed
  String GA1;         // GA for 1-click
  String GA2;         // GA for 2-clicks
  String GA3;         // GA for 3-clicks
  String stopGA;      // GA for stop event, sent when clicked within offsetStopEvent ms
  boolean stateGA1;   // State GA1
  boolean stateGA2;   // State GA2
  boolean stateGA3;   // State GA3
  long updated;       // Timestamp of last update
} BlindControl ;


// Struct definition of a light control button
typedef struct
{
  int id;                     // Button ID
  int pin;                    // Pin the button is attached to Arduino, use a LOW active button --> connect GND to this pin when button is pressed
  String  GA1;                // GA1 for the first click
  boolean GA1_value;          // GA1 value to be send
  String  GA2;                // GA2 for the next click
  boolean GA2_value;          // GA2 value to be send
  String  GA3;                // GA3 for the next click
  boolean GA3_value;          // GA3 value to be send
  String  GADouble;           // GA when double click
  boolean GADouble_value;     // GA value to be send for double click
  String  GATriple;           // GA when triple click
  boolean GATriple_value;     // GA value to be send for triple click
  String  GALong;             // GA when long button press
  boolean GALong_value;       // GA value to be send when long button press
  int currentGA;              // Which is the current GA switched on
} LightControl ;

// Struct definition of a reed record.
typedef struct
{
  int id;              // Reed ID
  int pin;             // Pin the reed contact is attached to Arduino, use a LOW active button --> connect GND to this pin when reed is closed
  String GA;           // GA
  boolean state;       // Current state of the reed contact
} reedStruct ;


/****************************************************************************************************************
END struct definitions
*****************************************************************************************************************/



/****************************************************************************************************************
START array definitions
*****************************************************************************************************************/

// Blind control button defintions
BlindControl blindButtons[] = {
  {1, 46,  "0/4/0", "0/0/2", "0/0/3", "",      0, 0, 0, 0},
  {2, 47,  "0/0/1", "0/0/2", "0/0/3", "",      0, 0, 0, 0},
  {3, 48,  "0/0/1", "0/0/2", "0/0/3", "3/3/3", 0, 0, 0, 0},
  {4, A15, "0/0/1", "0/0/2", "0/0/3", "4/4/4", 0, 0, 0, 0},
  { -1, 0, "", "", "", "", 0, 0, 0, 0} // Last element
};

// Blind control button defintions
LightControl lightButtons[] = {
  //ID, PIN, GA1, GA1_value, GA2, GA2_value, GA3, GA3_value, GADouble, GADouble_value, GATriple, GATriple_value, GALong, GALong_value
  {1,  50,  "0/2/0", 1, "0/2/1", 1, "0/2/2", 0, "0/2/3", 0, "0/2/4", 0, "0/2/4", 0, 0},
  {2,  51,  "0/2/0", 0, "0/2/1", 0, "0/2/2", 0, "0/2/3", 0, "0/2/4", 0, "0/2/4", 0, 0},
  {-1,  0, "", 0, "", 0, "", 0, "", 0, "", 0, "", 0, 0} // Last element
};

// Reed defintions arcording the buttton structure
reedStruct reeds[] = {
  {1, A8 , "0/4/0",  1},
  {2, A13, "0/0/1", 0},
  { -1, 0, "", 0} // Last element
};

/****************************************************************************************************************
END array definitions
*****************************************************************************************************************/


/****************************************************************************************************************
START variable definitions
*****************************************************************************************************************/

// Array KNX commands from KNX sequences are getting stored in
String commandArray[20];


// Jalousie handling variables
unsigned long currentTime;              // Variable to store the current time
unsigned long offsetStopEvent = 4000;   // When jalousie button is pressed within offsetStopEvent ms, then the stop GA will be send
int diff = 0;

// Utility variables
int i = 0;

// Global variables
String GA = "";


// Initialize temperature sensor DS18S20
OneWire ds(2);  // on digital pin 2


// Instantiate blind button objects for clickButton library
ClickButton blindButton1(blindButtons[0].pin, LOW, CLICKBTN_PULLUP);
ClickButton blindButton2(blindButtons[1].pin, LOW, CLICKBTN_PULLUP);
ClickButton blindButton3(blindButtons[2].pin, LOW, CLICKBTN_PULLUP);
ClickButton blindButton4(blindButtons[3].pin, LOW, CLICKBTN_PULLUP);

// Instantiate light button objects for clickButton library
ClickButton lightButton1(lightButtons[0].pin, LOW, CLICKBTN_PULLUP);



/****************************************************************************************************************
END variable definitions
*****************************************************************************************************************/



void setup()
{
  // Open serial USB
  Serial.begin(38400);

  // Open serial KNX
  //Serial1.begin(9600);
  Serial1.begin(38400);

  // Instantiate all reeds from array
  for (i = 0; i < sizeof(reeds); i++) {
    pinMode(reeds[i].pin, INPUT_PULLUP);
  }

}


void loop()
{
  // Blind button handling
  blindButton1.Update();
  blindButton2.Update();
  blindButton3.Update();
  blindButton4.Update();
  if (blindButton1.clicks != 0) handleBlindButtonClick(1, blindButton1.clicks);
  if (blindButton2.clicks != 0) handleBlindButtonClick(2, blindButton2.clicks);
  if (blindButton3.clicks != 0) handleBlindButtonClick(3, blindButton3.clicks);
  if (blindButton4.clicks != 0) handleBlindButtonClick(4, blindButton4.clicks);


  // Light button handling
  lightButton1.Update();
  if (lightButton1.clicks != 0) handleLightButtonClick(0, lightButton1.clicks);

  // Reed contacts handling
  handleReed(1, digitalRead(reeds[0].pin));

  // Listening to KNX
  if (Serial1.available() > 0) {
    handleMessageSequence(Serial1.readString(), "KNX");
  }

}



// This method will identify a sequence of commands, and will execute the handle message method for each of these commands
void handleMessageSequence (String message, String serialSource) {
  Serial.println("***************************************************************************************************");
  message.trim();
  Serial.print("Incomming transmission from "); Serial.print(serialSource); Serial.print(": ["); Serial.print(message); Serial.println("]");

  String currentCommand;
  String currentChar;
  int pos = 0;

  if (message.indexOf("\n") != -1) { // new line detected -> tokenize command sequence
    Serial.println("Tokenizing sequence...");
    for (int i = 0; i < message.length(); i++) {
      currentChar = message.substring(i, i + 1);
      if (currentChar != "\n") {
        currentCommand += currentChar;
      } else {
        commandArray[pos] = currentCommand;
        currentCommand = "";
        pos++;
      }
    }
    handleMessageArray();
  } else {
    handleMessage(message);
  }
}

// This method will execute the handleMessage routine for every command in the array
void handleMessageArray () {
  for (int i = 0; i < 20 - 1; i++) {
    if (commandArray[i] != "") {
      Serial.print(i); Serial.println("--------------------------------------------------------");
      handleMessage(commandArray[i]);
    }
  }
}


// Main method for message handling. Message processors can be linked here!
void handleMessage (String message) {
  // Remove linebreaks
  message.trim();

  Serial.print("Message: "); Serial.println(message);

  if (message.startsWith("tdi") || message.startsWith("tdc")) {
    tdi_Processor(message);
  } else if (message.startsWith("ivo")) {
    //writeToKNXSerial_Processor(message);
  } else if (message.startsWith("updateTemp")) {
    //updateTemp_Processor(message);
  } else if (message.startsWith("sendCommand")) {
    //writeToMoteinoSerial_Processor(message);
  } else if (message.startsWith("nodeID=")) {
    //writeToRpiSerial_Processor(message);
  }
  else {
    Serial.println("No command handler found!");
  }

}

/****************************************************************************************************************
START message processor section
*****************************************************************************************************************/


// This processor will observe KNX TDI messages. If the affected GA is inside the monitored reed contact array,
// it will send a message to RPi
void tdi_Processor (String message) {
  Serial.println("Transparent Data Indication (TDI) handler");
  String source       = message.substring(6, 10);
  String destination  = message.substring(12, 16);
  String length       = message.substring(18, 20);
  String data         = message.substring(23);
  data.trim();

  Serial.print("Source: "); Serial.print(source); Serial.print("="); Serial.print(HexGA2String(source));
  Serial.print(" Destination: "); Serial.print(destination); Serial.print("="); Serial.print(HexGA2String(destination));
  Serial.print(" Length: "); Serial.print(length);
  Serial.print(" Data: "); Serial.println(data);


  // If light GA is detected, then update the current GA status
  updateLightButtonCurrentGA(HexGA2String(destination));

}



/******************************************************************************
handleLightButtonClick - This method is handling light button click events.

It will rotate between 3 GAs for single clicks according the currentGA pointer.
GA1->GA2->GA3

Double click will send telegram to GADouble adress
Triple click will send telegram to GATriple adress

Values of each telegram is either 0 or 1 as configured in the array definition

*******************************************************************************/
void handleLightButtonClick (int buttonIndex, int clickCount) {
  Serial.println("***************************************************************************************************");
  Serial.print("LightButton: "); Serial.print(buttonIndex); Serial.print(" Clicks: "); Serial.println(clickCount); //Serial.print(" ButtonType: "); Serial.print(buttonType);

  // Get current GA
  int currentGA = lightButtons[buttonIndex].currentGA;


  // When single click, do GA rotation GA1>GA2>GA3>GA1>...
  if (clickCount == 1) {

    // When current GA = 1 and GA2 is defined > send GA2 telegram
    if (currentGA == 1 and (!lightButtons[buttonIndex].GA2.equals(""))) {
      // Send GA2 telegram
      sendKNXMessage_Value(lightButtons[buttonIndex].GA2, lightButtons[buttonIndex].GA2_value);

    // When current GA = 2 and GA3 is defined > send GA3 telegram
    } else if (currentGA == 2 and (!lightButtons[buttonIndex].GA3.equals(""))) {
      // Send GA3 telegram
      sendKNXMessage_Value(lightButtons[buttonIndex].GA3, lightButtons[buttonIndex].GA3_value);

    // All other cases return rotation to GA = 1 > send GA1 telegram
    } else {
      // Send GA1 telegram
      sendKNXMessage_Value(lightButtons[buttonIndex].GA1, lightButtons[buttonIndex].GA1_value);
    }
  } 
  
  // When double click > send GADouble telegram
  else if (clickCount == 2) {
    // Send GADouble telegram
    sendKNXMessage_Value(lightButtons[buttonIndex].GADouble, lightButtons[buttonIndex].GADouble_value);
  }

  // When triple click > send GATriple telegram
  else if (clickCount == 3) {
    // Send GATriple telegram
    sendKNXMessage_Value(lightButtons[buttonIndex].GATriple, lightButtons[buttonIndex].GATriple_value);
  }

  // When long button press > send GALong telegram
  else if (clickCount == -1) {
    // Send GALong telegram
    sendKNXMessage_Value(lightButtons[buttonIndex].GALong, lightButtons[buttonIndex].GALong_value);
  }

}


// Needs rework
void handleReed (int reed, int currentState) {
  // Get last reed state from reed array
  int lastState = reeds[reed - 1].state;

  // Only send status if it changed
  if (currentState != lastState) {
    Serial.print("Reed"); Serial.print(reed); Serial.print(" changed to: "); Serial.print(currentState);

    // Get GA from reed array
    String GA = reeds[reed - 1].GA;
    Serial.print(" GA:");  Serial.println(GA);

    sendKNXMessage_Value(GA, currentState);

    // Save new state to reed array
    reeds[reed - 1].state = currentState;
  }
}




// Need rework!!
void handleBlindButtonClick (int button, int clickCount) {
  // Which button was pressed? --> button
  // How often the button was pressed? -> clickCount

  boolean stopMessage = false;
  boolean currentState;

  Serial.print("BlindButton: "); Serial.print(button); Serial.print(" Clicks: "); Serial.print(clickCount); //Serial.print(" ButtonType: "); Serial.print(buttonType);

  // Remember current time to identify jalousie stop events later
  currentTime = millis();

  // Jalousie handling
  //if (buttonType == 2) {
  if (true) {
    // diff of current time and time of last update
    diff = (int)currentTime - (int)blindButtons[button - 1].updated;
  }

  // When button is pressed within offsetStopEvent ms, send message to stop GA
  //if (buttonType == 2 && (diff < offsetStopEvent)) {
  if (diff < offsetStopEvent) {
    stopMessage = true;
    GA = blindButtons[button - 1].stopGA;
  }  else GA = getGA(button, clickCount);

  if (stopMessage) {
    // this way is will always send 1 to a stop GA
    currentState = 0;
  } else {
    currentState = getState(button, clickCount);
  }

  Serial.print(" GA: "); Serial.print(GA); Serial.print(" CurrentState: "); Serial.println(currentState);

  // Update timestamp
  if (stopMessage) {

    // forget timestamp when last message was a stop event
    blindButtons[button - 1].updated = 0;
  } else {
    blindButtons[button - 1].updated = currentTime;
  }

  sendKNXMessage_Value(GA, !currentState);

  // Update new state, unless its a stop message or the jalousie button which is not toggling states
  //if (!stopMessage && blinds[button - 1].type != 2) setState(button, clickCount, !currentState);
  if (!stopMessage) setState(button, clickCount, !currentState);


  // for testing: send temp from DS18S20 to KNX
  //Serial.println("Handling temp");
  //handleTemp();



}


/****************************************************************************************************************
END message processor section
*****************************************************************************************************************/



/****************************************************************************************************************
START KNX listener section
*****************************************************************************************************************/


// This method will update all current GA pointers with a matching light GA
void updateLightButtonCurrentGA (String GA) {
  Serial.print("Destination: "); Serial.println(GA);
  int i = 0;
  while (true) {
    if (lightButtons[i].id == -1) {
      break;
    } else if (lightButtons[i].GA1.equals(GA)) {
      lightButtons[i].currentGA = 1;
      //Serial.print("ID:"); Serial.print(lightButtons[i].id); Serial.println(" Current GA:1");
    } else if (lightButtons[i].GA2.equals(GA)) {
      lightButtons[i].currentGA = 2;
      //Serial.print("ID:"); Serial.print(lightButtons[i].id); Serial.println(" Current GA:2");
    } else if (lightButtons[i].GA3.equals(GA)) {
      lightButtons[i].currentGA = 3;
      //Serial.print("ID:"); Serial.print(lightButtons[i].id); Serial.println(" Current GA:3");
    } else if (lightButtons[i].GADouble.equals(GA)) {
      lightButtons[i].currentGA = 3;
      //Serial.print("ID:"); Serial.print(lightButtons[i].id); Serial.println(" Current GA:3");
    } else if (lightButtons[i].GATriple.equals(GA)) {
      lightButtons[i].currentGA = 3;
      //Serial.print("ID:"); Serial.print(lightButtons[i].id); Serial.println(" Current GA:3");
    }
    i++;
  }
}

/****************************************************************************************************************
END KNX listener section
*****************************************************************************************************************/


/****************************************************************************************************************
START interface section (SIMKNX)
*****************************************************************************************************************/


void sendKNXMessage_2Hex_Values(String GA, String hex1, String hex2) {
  String output = "tds (" + GA + " $02) " + hex1 + " " + hex2;
  Serial.print("Sending:"); Serial.println(output);
  Serial1.println(output);
  
}

// Send KNX telegram to SIMKNX module
void sendKNXMessage_Value(String GA, int value) {
  String output = "tds (" + GA + ") " + value;
  Serial.print("Sending:"); Serial.println(output);
  Serial1.println(output);

  //Serial.println("Setting Baud rate to 38400");
  //Serial1.println("ids (5 50) $51 $00");

}

/****************************************************************************************************************
END interface section (SIMKNX)
*****************************************************************************************************************/





/****************************************************************************************************************
START utility functions
*****************************************************************************************************************/



// Helper method for GA transformation
String HexGA2String (String hex) {
  return GroupAddr2Ets(Hex2Dec(hex));

}

// Helper method for GA transformation
int Hex2Dec(String hex) {
  char hex_char[5];
  hex.toCharArray(hex_char, 5) ;
  long dec = strtol(hex_char, NULL, 16);
  return dec;
}

// Helper method for GA transformation
String GroupAddr2Ets(int address)
{
  String ets = "";
  ets = String(int(address / 2048)) + "/" + String(int((address % 2048) / 256)) + "/" + String(int(address % 256));
  return ets;
}





// Get GA from button array according to how often the button was clicked
String getGA (int button, int clickCount) {
  if (clickCount == 1) return  blindButtons[button - 1].GA1;
  else if (clickCount == 2) return  blindButtons[button - 1].GA2;
  else if (clickCount == 3) return  blindButtons[button - 1].GA3;
  else return  blindButtons[button - 1].GA1;
}

// Get current button state from array according to how often the button was clicked
boolean getState (int button, int clickCount) {
  if (clickCount == 1) return  blindButtons[button - 1].stateGA1;
  else if (clickCount == 2) return  blindButtons[button - 1].stateGA2;
  else if (clickCount == 3) return  blindButtons[button - 1].stateGA3;
  else return  blindButtons[button - 1].stateGA1;
}

// Save new button state to array according to how often the button was clicked
void setState (int button, int clickCount, boolean state) {
  if (clickCount == 1) blindButtons[button - 1].stateGA1 = state;
  else if (clickCount == 2) blindButtons[button - 1].stateGA2 = state;
  else if (clickCount == 3) blindButtons[button - 1].stateGA3 = state;
}


void handleTemp() {
  float temperature = getTemp();
  sendKNXMessage_DPT9("0/4/3", temperature);
}


float getTemp() {
  //returns the temperature from one DS18S20 in DEG Celsius

  byte data[12];
  byte addr[8];

  if ( !ds.search(addr)) {
    //no more sensors on chain, reset search
    ds.reset_search();
    return -1000;
  }

  if ( OneWire::crc8( addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    return -1000;
  }

  if ( addr[0] != 0x10 && addr[0] != 0x28) {
    Serial.print("Device is not recognized");
    return -1000;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1); // start conversion, with parasite power on at the end

  byte present = ds.reset();
  ds.select(addr);
  ds.write(0xBE); // Read Scratchpad


  for (int i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = ds.read();
  }

  ds.reset_search();

  byte MSB = data[1];
  byte LSB = data[0];

  float tempRead = ((MSB << 8) | LSB); //using two's compliment
  float TemperatureSum = tempRead / 16;

  Serial.print("Current temperature:"); Serial.println(TemperatureSum);
  return TemperatureSum;

}


void sendKNXMessage_DPT9(String GA, float floatValue) {


  int value2DPT9 = Value2DPT9(floatValue * 100);
  String hexTemp    = Dec2Hex(value2DPT9, 4);
  String hex1 = "$" + hexTemp.substring(0, 2);
  String hex2 = "$" + hexTemp.substring(2, 4);
  sendKNXMessage_2Hex_Values(GA, hex1, hex2);

}


// Not working for negative decimal values!!!!
int Value2DPT9(int value)
{
  int eis5 = 0;
  int exponent = 0;
  if (value < 0)
  {
    eis5 = 0x08000;
    value = -value;
  }
  while (value > 0x07ff)
  {
    value >>= 1;
    exponent++;
  }
  if (eis5 != 0)
    value = - value;
  eis5 |= value & 0x7ff;
  eis5 |= (exponent << 11) & 0x07800;
  return eis5 & 0x0ffff;
}


String Dec2Hex(int dec, int hexDigits) {
  String hexNumber = "0123456789ABCDEF";
  String hex = "";
  if (dec > pow(16, hexDigits))
  {
    return "";
  }
  while (hexDigits > 0)
  {
    hex = hexNumber.substring(dec % 16, dec % 16 + 1) + hex;
    dec = dec / 16;
    hexDigits = hexDigits - 1;
  }
  return hex;
}


/****************************************************************************************************************
END utility functions
*****************************************************************************************************************/

