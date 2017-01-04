/****************************************************************************************************************
CentralLink

HW: Arduino Mega 2560

Features:
  - Listenting to KNX Bus   on serial 1
  - Listenting to Moteino   on serial 2
  - Listenting to Raspberry on serial 3

  - Processing of incoming data with corresponding command handlers
    e.g. Receiving KNX state change for a predefined GA (reed contact) and informing the RPi about the state change
    e.g. Receiving a command from RPi to send value to a predifined GA (switch lamps on, move blinds up,...)
*****************************************************************************************************************/



/****************************************************************************************************************
START struct definitions
*****************************************************************************************************************/

// Struct definition of a reed record.
typedef struct
{
  int id;              // Reed ID
  String GA;           // GA
} reedStruct;


// Struct definition of a temp record.
typedef struct
{
  int id;                  // Temp ID
  String fhemID;           // FHEM ID of thermostat
  String GA_setTemp;       // Temp set from G1
  String GA_measuredTemp;  // Temp measured from thermostat sent back to G1
} tempStruct;


// Struct definition of a position record.
typedef struct
{
  int id;              // Posistion ID
  String GA;           // GA
} positionStruct;

/****************************************************************************************************************
END struct definitions
*****************************************************************************************************************/



/****************************************************************************************************************
START array definitions
*****************************************************************************************************************/

// Array KNX commands from KNX sequences are getting stored in
String commandArray[20];


// Reed defintions
reedStruct reeds[] = {
  {21,  "0/4/0"},
  {222, "0/4/6"},
  {2,   "0/4/1"},
  { -1,  "Last Element"}
};

// Temp defintions
tempStruct temps[] = {
  {1,  "Thermostat_Office_Clima",  "0/4/7", "0/4/8"},
  {2,  "Thermostat_Bath_Clima",    "0/4/7", "0/4/8"},
  {3,  "Thermostat_Living_Clima",  "0/4/7", "0/4/8"},
  {4,  "Thermostat_Kitchen_Clima", "0/4/7", "0/4/8"},
  { -1, "Last Element", "", ""}

};

// Positions defintions
positionStruct positions[] = {
  {1,   "0/7/3"},
  {2,   "0/7/4"},
  { -1, "Last Element"}
};

/****************************************************************************************************************
END array definitions
*****************************************************************************************************************/


/****************************************************************************************************************
START variable definitions
*****************************************************************************************************************/

// Clear moteino buffer when powering up
String currentMoteinoReading = "";

/****************************************************************************************************************
END variable definitions
*****************************************************************************************************************/




void setup() {
  // Open serial USB
  Serial.begin(9600);
  Serial.println("Ardunio KNX interface running...");

  // Open serial KNX
  Serial1.begin(9600);

  // Open serial Motino
  Serial2.begin(9600);

  // Open serial RPi
  Serial3.begin(9600);
  Serial3.println("Ardunio KNX interface running...");
}



void loop() {

  // Listening to KNX
  if (Serial1.available() > 0) {
    //Serial.println(SerialMotino.readString() );
    handleMessageSequence(Serial1.readString(), "KNX");
  }

  // Listening to Motino
  if (Serial2.available() > 0) {
    currentMoteinoReading = Serial2.readString();
    if (!currentMoteinoReading.startsWith("serialMessage"))
      handleMessageSequence(currentMoteinoReading, "Moteino");

  }

  // Listening to RPi
  if (Serial3.available() > 0) {
    //Serial.println(SerialMotino.readString() );
    handleMessageSequence(Serial3.readString(), "RPi");
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
    writeToKNXSerial_Processor(message);
  } else if (message.startsWith("updateTemp")) {
    updateTemp_Processor(message);
  } else if (message.startsWith("sendCommand")) {
    writeToMoteinoSerial_Processor(message);
  } else if (message.startsWith("nodeID=")) {
    writeToRpiSerial_Processor(message);
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

  int currentPositionIndex = insideArray(HexGA2String(destination), 3);
  int currentReedIndex     = insideArray(HexGA2String(destination), 1);
  int currentTempIndex     = insideArray(HexGA2String(destination), 2);

  if (currentReedIndex != -1) {

    Serial.print("This reed contact with ID="); Serial.print(reeds[currentReedIndex].id); Serial.println(" is monitored -> send notification to RPi");

    String nodeID        = "nodeID=" + String(reeds[currentReedIndex].id);
    String restOfMessage;

    if (data.equals("00")) {
      restOfMessage = " messageType=Reed vcc=29000 status=0 signal=1 KNX=true";
    } else {
      restOfMessage = " messageType=Reed vcc=29000 status=1 signal=1 KNX=true";
    }

    writeToRpiSerial_Processor(nodeID + restOfMessage);


  }

  else if (currentTempIndex != -1) {
    String data1 = data.substring(0, 2);
    String data2 = data.substring(4, 6);

    int dec = Hex2Dec(data1 + data2);

    float temp = (float)DPT9ToValue(dec) / 100;
    Serial.print("This temperature object with ID="); Serial.print(temps[currentTempIndex].id);
    Serial.print(" is monitored -> Temperature: "); Serial.print(temp); Serial.println(" degree Celsius");

    // Send temperatur value to Raspberry
    writeToRpiSerial_Processor("SetTemp " + temps[currentTempIndex].fhemID + " " + temp);

    // Beispiel für die Umrechnung von decimal Wert zu DPT9 und versenden an KNX GA
    //float temp2 = temp + 1;

    //temp2 = 34.9;

    //int value2DPT9 = Value2DPT9(temp2 * 100);
    //Serial.print("value2DPT9: "); Serial.println(value2DPT9);
    //String hexTemp    = Dec2Hex(value2DPT9, 4);
    //Serial.print("hexTemp: "); Serial.println(hexTemp);



    //Serial.print(temp2); Serial.print(" to DPT9="); Serial.println(hexTemp);

    //String hex1 = "$" + hexTemp.substring(0, 2);
    //String hex2 = "$" + hexTemp.substring(2, 4);

    //Serial.print("Hex1: "); Serial.println(hex1);
    //Serial.print("Hex2: "); Serial.println(hex2);

    //sendKNXMessage_2Hex_Values("0/4/8", hex1, hex2);

  }

  else if (currentPositionIndex != -1) {
    String data1 = data.substring(0, 2);
    String data2 = data.substring(4, 6);

    int dec = Hex2Dec(data1 + data2);

    int pos        = DPT9ToValue(dec);
    int posPercent = (DPT9ToValue(dec) * 100) / 255 + 1;

    Serial.print("This position object with ID="); Serial.print(positions[currentPositionIndex].id);
    Serial.print(" is monitored -> Position: "); Serial.print(pos); Serial.print(" "); Serial.print(posPercent); Serial.println("%");

  }

  else {
    Serial.println("This KNX address is NOT supervised");

  }

}



// This method will handle thermostat updates received from FHEM and will forward it to the KNX bus serial
void updateTemp_Processor(String message) {
  Serial.println("Update temperature handler");

  String thermostatID = message.substring(message.indexOf("NAME:") + 5,     message.indexOf("_TEMP:"));
  String tempValue    = message.substring(message.indexOf("TEMP:") + 5,     message.length());

  String GA_measuredTemp = "";


  for (int i = 0; i < sizeof(temps) - 1; i++) {
    if (temps[i].fhemID.equals(thermostatID)) {
      GA_measuredTemp = temps[i].GA_measuredTemp;
    }
  }

  Serial.print("Sending thermostat "); Serial.print(thermostatID); Serial.print(" change "); Serial.print(tempValue); Serial.print(" degree"); Serial.print(" to "); Serial.println(GA_measuredTemp);

  float newTemp = tempValue.toFloat();
  int value2DPT9 = Value2DPT9(newTemp * 100);
  String hexTemp    = Dec2Hex(value2DPT9, 4);
  String hex1 = "$" + hexTemp.substring(0, 2);
  String hex2 = "$" + hexTemp.substring(2, 4);
  sendKNXMessage_2Hex_Values(GA_measuredTemp, hex1, hex2);

}


/****************************************************************************************************************
END message processor section
*****************************************************************************************************************/

/****************************************************************************************************************
START interface section (SIMKNX, Raspberry Pi, Moteino)
*****************************************************************************************************************/




void sendKNXMessage_DPT9(String GA, float floatValue) {
  int value2DPT9 = Value2DPT9(floatValue * 100);
  String hexTemp    = Dec2Hex(value2DPT9, 4);
  String hex1 = "$" + hexTemp.substring(0, 2);
  String hex2 = "$" + hexTemp.substring(2, 4);
  sendKNXMessage_2Hex_Values(GA, hex1, hex2);
}



// Method to write something to the KNX bus serial
void writeToKNXSerial_Processor(String output) {
  // encode RPi command here and send out KNX message
  sendKNXMessage_Value("1/1/1", 1);
}


// Send KNX telegram to SIMKNX module
void sendKNXMessage_Value(String GA, int value) {
}


void sendKNXMessage_2Hex_Values(String GA, String hex1, String hex2) {
  String output = "tds (" + GA + " $02) " + hex1 + " " + hex2;
  Serial1.println(output);
}


// Method to write something to the RPi serial
void writeToRpiSerial_Processor(String output) {
  //nodeID=21 messageType=Reed vcc=4110 status=1 signal=-53
  //Serial.print("Sending to Rpi: "); Serial.println(output);
  Serial3.println(output);
}

// Method to write something to the Moteino serial
void writeToMoteinoSerial_Processor(String output) {
  Serial.print("Sending to Moteino serial:"); Serial.println(output);
  Serial2.println(output);
  //Serial2.print("sendCommand_TO:181_RETRIES:10_RETRYWAITTIME:5000_COMMAND:4_VALUE:HIGH@");

}

/****************************************************************************************************************
END interface section (SIMKNX, Raspberry Pi, Moteino)
*****************************************************************************************************************/



/****************************************************************************************************************
START utility functions
*****************************************************************************************************************/


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




int DPT9ToValue(int dec) {
  int value = dec & 0x07ff;
  if ((dec & 0x08000) != 0) {
    value |= 0xfffff800;
    value = -value;
  }
  value <<=  ((dec & 0x07800) >> 11);
  if ((dec & 0x08000) != 0) {
    value = -value;
  }
  return value;
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



// Method to identify whether GA is in reed array or not
int insideArray(String destination, int whichArray) {
  int i = 0;
  switch (whichArray) {
    case 1:
      while (true) {
        if (reeds[i].id == -1) {
          return -1;
          break;
        } else if (reeds[i].GA.equals(destination)) {
          return i;
          break;
        }
        i++;
      }

    case 2:
      while (true) {
        if (temps[i].id == -1) {
          return -1;
          break;
        } else if (temps[i].GA_setTemp.equals(destination)) {
          return i;
          break;
        }
        i++;
      }

    case 3:
      while (true) {
        if (positions[i].id == -1) {
          return -1;
          break;
        } else if (positions[i].GA.equals(destination)) {
          return i;
          break;
        }
        i++;
      }

    default:
      break;


  }


}



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


String decToHex(byte decValue, byte desiredStringLength) {
  String hexString = String(decValue, HEX);
  while (hexString.length() < desiredStringLength) hexString = "0" + hexString;
  return hexString;
}



// Helper method for GA transformation
String GroupAddr2Ets(int address)
{
  String ets = "";
  ets = String(int(address / 2048)) + "/" + String(int((address % 2048) / 256)) + "/" + String(int(address % 256));
  return ets;
}

/****************************************************************************************************************
END utility functions
*****************************************************************************************************************/

