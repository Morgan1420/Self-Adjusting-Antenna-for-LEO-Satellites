#include <SoftwareSerial.h>

// Constants
#define ARDUINO_LED 13

#define PUL_AZIMUTH 10
#define DIR_AZIMUTH 9
#define ENA_AZIMUTH 8

#define PUL_ELEVATION 7
#define DIR_ELEVATION 6
#define ENA_ELEVATION 5

// Global Variables
String command = "";
bool completeCommandDetected = false; // check 

int AzSpeed = 600; // Miliseconds between on and off, tune for more precision
int AzStepsPerDegree = 152; // 32000 steps per revolution

int ElSpeed = 600;
int ElStepsPerDegree = 87; // TBD

// Functions
void moveAzimuth(bool dir, int deg){
  // Change stepper direction
  if(dir) digitalWrite(DIR_AZIMUTH, LOW);
  else digitalWrite(DIR_AZIMUTH, HIGH);

  // CountStepsPerDegree
  long totalSteps = (long) AzStepsPerDegree * deg;
  
  // Count number of repetitions
  for (long i = 0; i < totalSteps; i++) {
    // 1. Send the HIGH signal (Start of Pulse)
    digitalWrite(PUL_AZIMUTH, HIGH);
    delayMicroseconds(AzSpeed); 
    
    // 2. Send the LOW signal (End of Pulse - THIS IS THE CRITICAL PART!)
    digitalWrite(PUL_AZIMUTH, LOW);
    delayMicroseconds(AzSpeed); 
  }
}

void moveElevation(bool dir, int deg){
  // Change stepper direction
  if(dir) digitalWrite(DIR_ELEVATION, LOW);
  else digitalWrite(DIR_ELEVATION, HIGH);

  // CountStepsPerDegree
  int totalSteps = ElStepsPerDegree * deg;
  
  for (long i = 0; i < totalSteps; i++) {
    // 1. Send the HIGH signal (Start of Pulse)
    digitalWrite(PUL_ELEVATION, HIGH);
    delayMicroseconds(ElSpeed); 
    
    // 2. Send the LOW signal (End of Pulse - THIS IS THE CRITICAL PART!)
    digitalWrite(PUL_ELEVATION, LOW);
    delayMicroseconds(ElSpeed); 
  }   
}

void calibrate(){
  
}

void test(){
  // Function that checks all movements.
  // Move 360 degrees Azimuth Left
  moveAzimuth(true, 360);
  // Move 90 degrees Elevation Up
  moveElevation(true, 90);
  // Move 360 degrees Azimuth Right
  moveAzimuth(false, 360);
  // Move 90 degrees Elevation Down
  moveElevation(false, 90);
    
}


// Void Setup
void setup() {
  // Initialize Serial
  Serial.begin(9600);

  // Initialize vairiables
  command.reserve(30); // Prevent memmory fragmentation

  // Initialize pins
  pinMode(ARDUINO_LED, OUTPUT);
  pinMode(PUL_AZIMUTH, OUTPUT);
  pinMode(DIR_AZIMUTH, OUTPUT);
  pinMode(ENA_AZIMUTH, OUTPUT);
  pinMode(PUL_ELEVATION, OUTPUT);
  pinMode(DIR_ELEVATION, OUTPUT);
  pinMode(ENA_ELEVATION, OUTPUT);

  // Set default outputs
  digitalWrite(ARDUINO_LED, HIGH);
  digitalWrite(ENA_AZIMUTH, HIGH); // Enable high
  digitalWrite(ENA_ELEVATION, HIGH); // Enable high
  

  // Notify the user the arduino is ready to take in commands
  Serial.print("Arduino ready!\n");
}





// Void Loop
void loop() {

  // 1. Check command from serial (this loop runs repeatedly whenever data is available)
  while (Serial.available()) {
    // 1.1. Read the next character
    char inChar = (char)Serial.read();
    
    // 1.2. Stop reading characters when we reach a \n or a \r
    if (inChar == '\n' || inChar == '\r') {
      // 1.2.1. If we detect a termination character we know the command is complete and exit the loop
      completeCommandDetected = true; 
    } else {
      // 1.2.2. Add the detected char to the command string
      command += inChar;
    }
    
    
  }
  
  // 2. If we have a new command we handle it
  if(completeCommandDetected){    
    String commandStatus = "ERROR"; // Can be "ERROR", "DONE or "UNKNOWN"


    
    // 2.1. Sanitize the command
    command.trim(); // To avoid empty spaces or unwanted characters.    
    
    // 2.2. Handle the commands
    /* The commands can look like one of the following: 
     * - MALXXX (Move Azimuth Left XXX degrees)
     * - MARXXX (Move Azimuth Right XXX degrees)
     * - MEUXX (Move Elevation Up XX degrees)
     * - MEDXX (Move Elevation Down XX degrees)
     */

    // Movement commands
    if(command.startsWith("M")){
      if(command.indexOf("A") == 1){
        // Command MAR or MAL
        // Extract direction and degrees
        bool dir = command.indexOf("L") == 2 ? true : false;
        int deg = command.substring(3, 6).toInt(); // From index 3 (inclusive) to 6 (exclusive)
        // Move
        moveAzimuth(dir, deg);         
      }
      else { 
        // Command MEU or MED
        // Extract direction and degrees
        bool dir = command.indexOf("U") == 2 ? false : true;
        int deg = command.substring(3, 6).toInt(); // From index 3 (inclusive) to 5 (exclusive)
        // Move
        moveElevation(dir, deg); 
      }

      commandStatus = "DONE";

    } else{
      // Unknown command recieved
      commandStatus = "UNKNOWN";
    }

    // Resset the command logic for the next command
    command = "";
    completeCommandDetected = false;
    
    // Notify the user
    if(commandStatus == "DONE") Serial.print("DONE\n");
    else if (commandStatus = "UNKNOWN") Serial.print("UNKNOWN\n");
    else if (commandStatus = "ERROR") Serial.print("ERROR\n");
    
    
  }
}