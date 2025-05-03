#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>             // SPI library for communication
#include <FastLED.h>         // LED library for controlling WS2812B LEDs

// TFT pin definitions
#define TFT_CS   10
#define TFT_RST  8
#define TFT_DC   9
#define TFT_LED  6  // Backlight

// FastLED definitions
#define LED_PIN     7
#define NUM_LEDS    255
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

// Line coding schemes
#define NRZ 1
#define NRZI 2
#define MANCHESTER 3
#define AMI 4

// Define custom color for dark grey (not included in standard library)
#define ST7735_DARKGREY 0x4208  // Dark grey color in RGB565 format

// Create display object
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Variables to store received data
String original_message = "";
String binary_data = "";
String encoded_data = "";
int current_scheme = NRZ;
String scheme_name = "NRZ";

// LED control variables
bool ledActive = false;
unsigned long ledStartTime = 0;
const unsigned long LED_DURATION = 10000; // 10 seconds in milliseconds
uint8_t gHue = 0; // Global hue for rainbow cycling

void setup() {
  // Initialize backlight
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH); // Full brightness
  
  // Start serial communication
  Serial.begin(9600);
  delay(2000); // Wait for serial to stabilize
  
  // Initialize TFT display
  tft.initR(INITR_BLACKTAB); // Confirmed working
  tft.setRotation(1);        // Landscape mode
  tft.fillScreen(ST7735_BLACK);
  
  // Set text properties
  tft.setTextColor(ST7735_GREEN);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.println("Line Coding Demo");
  tft.println("Waiting for message...");
  
  // Initialize FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();
}

void loop() {
  // Check if LED animation is active and needs to be updated
  if (ledActive) {
    // Check if the LED duration has elapsed
    if (millis() - ledStartTime >= LED_DURATION) {
      // Turn off LEDs after duration
      FastLED.clear();
      FastLED.show();
      ledActive = false;
    } else {
      // Continue LED animation
      fill_rainbow(leds, NUM_LEDS, gHue, 5); // 5 = hue step between LEDs
      FastLED.show();
      gHue++; // Slowly cycle the global hue
      delay(20); // Short delay to control speed of rainbow movement
    }
  }

  // Check for serial data
  if (Serial.available()) {
    // Read the incoming message
    String message = Serial.readStringUntil('\n');
    
    // Parse the message if it's in the correct format: S<scheme>:<original>:<binary>:<encoded>E
    if (message.startsWith("S") && message.indexOf('E') > 0) {
      int firstColon = message.indexOf(':');
      int secondColon = message.indexOf(':', firstColon + 1);
      int thirdColon = message.indexOf(':', secondColon + 1);
      int endMarker = message.indexOf('E');
      
      if (firstColon > 0 && secondColon > 0 && thirdColon > 0 && endMarker > 0) {
        current_scheme = message.substring(1, firstColon).toInt();
        original_message = message.substring(firstColon + 1, secondColon);
        binary_data = message.substring(secondColon + 1, thirdColon);
        encoded_data = message.substring(thirdColon + 1, endMarker);
        
        // Set scheme name
        switch (current_scheme) {
          case NRZ:
            scheme_name = "NRZ";
            break;
          case NRZI:
            scheme_name = "NRZI";
            break;
          case MANCHESTER:
            scheme_name = "Manchester";
            break;
          case AMI:
            scheme_name = "AMI";
            break;
          default:
            scheme_name = "Unknown";
        }
        
        // Check if the message is "LED" to activate the LED sequence
        if (original_message == "LED") {
          // Activate LED sequence
          ledActive = true;
          ledStartTime = millis();
          Serial.println("ACK: LED activated for 10 seconds");
        }
        
        // Display the received data
        updateDisplay();
        
        // Send acknowledgment
        Serial.println("ACK: Message received and displayed");
      }
    }
  }
}

// Function to convert binary string back to text
String binaryToText(String binary) {
  String text = "";
  
  // Process 8 bits at a time (one ASCII character)
  for (int i = 0; i < binary.length(); i += 8) {
    // Extract 8 bits
    String byte_str = binary.substring(i, i + 8);
    
    // Convert binary string to integer
    int ascii_value = 0;
    for (int j = 0; j < 8; j++) {
      if (byte_str.charAt(j) == '1') {
        ascii_value |= (1 << (7 - j));
      }
    }
    
    // Convert integer to character and append to result
    text += (char)ascii_value;
  }
  
  return text;
}

// Function to decode NRZ data back to binary
String decode_nrz(String encoded) {
  // For NRZ, the encoded data is the same as binary data
  return encoded;
}

// Function to decode NRZI data back to binary
String decode_nrzi(String encoded) {
  String binary = "";
  char prev_state = encoded.charAt(0);
  
  // First bit is special (we assume it's 0 since we started with '0')
  binary += '0';
  
  // Process the rest of the bits
  for (int i = 1; i < encoded.length(); i++) {
    char current_state = encoded.charAt(i);
    
    if (current_state == prev_state) {
      // No change in state means '0'
      binary += '0';
    } else {
      // Change in state means '1'
      binary += '1';
    }
    
    prev_state = current_state;
  }
  
  return binary;
}

// Function to decode Manchester data back to binary
String decode_manchester(String encoded) {
  String binary = "";
  
  // Process 2 bits at a time
  for (int i = 0; i < encoded.length(); i += 2) {
    if (i + 1 < encoded.length()) {
      String pair = encoded.substring(i, i + 2);
      
      if (pair == "10") {
        binary += '1';  // High-to-low means '1'
      } else if (pair == "01") {
        binary += '0';  // Low-to-high means '0'
      }
    }
  }
  
  return binary;
}

// Function to decode AMI data back to binary
String decode_ami(String encoded) {
  String binary = "";
  
  for (int i = 0; i < encoded.length(); i++) {
    char bit = encoded.charAt(i);
    
    if (bit == '0') {
      binary += '0';  // Zero voltage means '0'
    } else {
      binary += '1';  // Any non-zero voltage (1 or 2) means '1'
    }
  }
  
  return binary;
}

// Function to decode the received data based on the current scheme
String decodeMessage() {
  String decodedBinary = "";
  
  // Decode based on the current scheme
  switch (current_scheme) {
    case NRZ:
      decodedBinary = decode_nrz(encoded_data);
      break;
    case NRZI:
      decodedBinary = decode_nrzi(encoded_data);
      break;
    case MANCHESTER:
      decodedBinary = decode_manchester(encoded_data);
      break;
    case AMI:
      decodedBinary = decode_ami(encoded_data);
      break;
  }
  
  // Compare with the original binary data to verify decoding accuracy
  if (decodedBinary == binary_data) {
    // Convert the binary back to text
    return binaryToText(decodedBinary);
  } else {
    return "ERROR: Decoding mismatch!";
  }
}

void updateDisplay() {
  // Clear display area
  tft.fillRect(0, 20, 160, 128, ST7735_BLACK);
  
  // Display coding scheme
  tft.setTextColor(ST7735_CYAN);
  tft.setTextSize(1);
  tft.setCursor(5, 20);
  tft.print("Scheme: ");
  tft.println(scheme_name);
  
  // Display original message
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 35);
  tft.print("Sent: ");
  tft.println(original_message);
  
  // Draw separator line
  tft.drawFastHLine(5, 45, 150, ST7735_DARKGREY); // Now using our custom defined color
  
  // Display encoded data (truncated if too long)
  tft.setCursor(5, 50);
  tft.setTextColor(ST7735_YELLOW);
  tft.print("Encoded: ");
  
  // If encoded data is too long, truncate it for display
  String displayEncoded = encoded_data;
  if (displayEncoded.length() > 16) {
    displayEncoded = displayEncoded.substring(0, 16) + "...";
  }
  tft.println(displayEncoded);
  
  // Display decoded message
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(5, 65);
  tft.print("Decoded: ");
  tft.println(decodeMessage());
  
  // Draw the graphical representation of the line code
  drawLineCode();
}

void drawLineCode() {
  int startX = 5;
  int startY = 90;
  int lineHeight = 15;
  int bitWidth = 8;
  int maxBits = 16; // Display at most 16 bits to avoid overflow
  
  // Draw baseline
  tft.drawFastHLine(startX, startY, min(encoded_data.length(), maxBits) * bitWidth + 5, ST7735_YELLOW);
  
  // Draw bit values, but limit to maxBits
  for (int i = 0; i < min(encoded_data.length(), maxBits); i++) {
    char bit = encoded_data.charAt(i);
    int x = startX + i * bitWidth;
    
    if (current_scheme == AMI) {
      // For AMI: '0' = baseline, '1' = high, '2' = low
      if (bit == '0') {
        // Draw a dot on the baseline
        tft.fillCircle(x + bitWidth/2, startY, 2, ST7735_GREEN);
      } else if (bit == '1') {
        // Draw high level
        tft.drawLine(x, startY, x, startY - lineHeight, ST7735_GREEN);
        tft.drawFastHLine(x, startY - lineHeight, bitWidth, ST7735_GREEN);
        tft.drawLine(x + bitWidth, startY - lineHeight, x + bitWidth, startY, ST7735_GREEN);
      } else if (bit == '2') {
        // Draw low level (negative voltage)
        tft.drawLine(x, startY, x, startY + lineHeight, ST7735_GREEN);
        tft.drawFastHLine(x, startY + lineHeight, bitWidth, ST7735_GREEN);
        tft.drawLine(x + bitWidth, startY + lineHeight, x + bitWidth, startY, ST7735_GREEN);
      }
    } else {
      // For other coding schemes: '0' = low, '1' = high
      if (bit == '0') {
        // Draw low level
        tft.drawFastHLine(x, startY + lineHeight/2, bitWidth, ST7735_GREEN);
      } else if (bit == '1') {
        // Draw high level
        tft.drawFastHLine(x, startY - lineHeight/2, bitWidth, ST7735_GREEN);
        
        // Connect with vertical lines
        if (i > 0 && encoded_data.charAt(i-1) == '0') {
          tft.drawLine(x, startY + lineHeight/2, x, startY - lineHeight/2, ST7735_GREEN);
        }
        if (i < encoded_data.length()-1 && encoded_data.charAt(i+1) == '0') {
          tft.drawLine(x + bitWidth, startY - lineHeight/2, x + bitWidth, startY + lineHeight/2, ST7735_GREEN);
        }
      }
    }
  }
  
  // Draw a note if we truncated the display
  if (encoded_data.length() > maxBits) {
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(5, 115);
    tft.println("(display truncated)");
  }
}