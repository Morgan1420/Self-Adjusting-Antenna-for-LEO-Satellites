

// Define the pin for the built-in LED (usually pin 13)
const int ledPin = LED_BUILTIN; 

// Track the current state of the LED
bool ledState = LOW;

void setup() {
  // Initialize the built-in LED pin as an output
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledState); // Ensure it starts LOW

  // Start serial communication at 9600 baud
  Serial.begin(9600);
  Serial.println("Arduino Ready. Waiting for command '1'...");
}

void loop() {
  // Check if any data is available to be read from the serial buffer
  if (Serial.available() > 0) {
    // Read the incoming byte/character
    char incomingByte = Serial.read();

    // Check if the received character is our command '1'
    if (incomingByte == '1') {
      // Toggle the LED state
      ledState = !ledState;
      digitalWrite(ledPin, ledState);

      // Send a confirmation message back to the computer (optional but good practice)
      Serial.print("Command received: '");
      Serial.print(incomingByte);
      Serial.print("'. LED is now ");
      
      if (ledState == HIGH) {
        Serial.println("ON.");
      } else {
        Serial.println("OFF.");
      }
    } else {
      // Print anything else received
      Serial.print("Received unknown command: ");
      Serial.println(incomingByte);
    }
  }
  // Small delay to prevent flooding the CPU/serial port (optional)
  delay(10); 
}