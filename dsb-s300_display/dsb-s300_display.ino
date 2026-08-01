/*
 * Samsung DSB-S300 Front Panel Control
 * ESP32 + 2x 74HC595 + TOF-2411BG-B3-TS Display
 * 
 * Pinout:
 * 1 - GND
 * 2 - SRCLK (SH_CP) - Shift Register Clock
 * 3 - DATA (DS) - Data Input
 * 4 - NC
 * 5 - RCLK (ST_CP) - Latch Clock
 * 6 - NC
 * 7 - IR (not used)
 * 8 - +3.3V
 * 
 * Hardware: Two 74HC595 shift registers in daisy chain
 * - First register (low byte): Controls digit selection (bits 0-3) and LEDs (bits 5-6)
 * - Second register (high byte): Controls segment data (bits 8-15)
 * 
 * Display Type: Common Anode (active LOW for segments)
 */

#include <Arduino.h>

// ==================== PIN DEFINITIONS ====================

#define PIN_SRCLK  18  // Shift register clock (SH_CP)
#define PIN_RCLK   19  // Latch clock (ST_CP)
#define PIN_DATA   23  // Serial data input (DS)

// ==================== SEGMENT DATA (YOUR EXACT VALUES) ====================
// High byte (bits 8-15): Segment control (active LOW for common anode)
// Low byte (bits 0-3): Digit selection (active HIGH)
// Low byte (bits 5-6): LED control (active HIGH)

// Digit 0: segments a,b,c,d,e,f (all except g)
byte seg0_high = 0b00000010;
byte seg0_low  = 0b00001111;  // All digits ON for testing

// Digit 1: segments a,b
byte seg1_high = 0b11110011;
byte seg1_low  = 0b00001111;

// Digit 2: segments a,b,d,e,g
byte seg2_high = 0b00100101;
byte seg2_low  = 0b00001111;

// Digit 3: segments a,b,c,d,g
byte seg3_high = 0b01100001;
byte seg3_low  = 0b00001111;

// Digit 4: segments b,c,f,g
byte seg4_high = 0b11010001;
byte seg4_low  = 0b00001111;

// Digit 5: segments a,c,d,f,g
byte seg5_high = 0b01001001;
byte seg5_low  = 0b00001111;

// Digit 6: segments a,c,d,e,f,g
byte seg6_high = 0b00001001;
byte seg6_low  = 0b00001111;

// Digit 7: segments a,b,c
byte seg7_high = 0b11100011;
byte seg7_low  = 0b00001111;

// Digit 8: segments a,b,c,d,e,f,g (all segments)
byte seg8_high = 0b00000001;
byte seg8_low  = 0b00001111;

// Digit 9: segments a,b,c,d,f,g
byte seg9_high = 0b01000001;
byte seg9_low  = 0b00001111;

// ==================== DATA STRUCTURE ====================

/*
 * Structure to hold segment data for each digit
 * high: Segment data (bits 8-15) - controls which segments light up
 * low:  Digit/LED data (bits 0-7) - controls which digit is active
 */
struct DigitData {
  byte high;  // Segment selection byte
  byte low;   // Digit selection byte
};

// Array of all 10 digits (0-9)
DigitData digits[10] = {
  {seg0_high, seg0_low}, // 0
  {seg1_high, seg1_low}, // 1
  {seg2_high, seg2_low}, // 2
  {seg3_high, seg3_low}, // 3
  {seg4_high, seg4_low}, // 4
  {seg5_high, seg5_low}, // 5
  {seg6_high, seg6_low}, // 6
  {seg7_high, seg7_low}, // 7
  {seg8_high, seg8_low}, // 8
  {seg9_high, seg9_low}  // 9
};

// ==================== LED CONTROL ====================
// LED bits in the low byte
#define LED_GREEN 0b00100000  // Bit 5
#define LED_RED   0b01000000  // Bit 6

// ==================== CORE FUNCTIONS ====================

/*
 * Send 16 bits of data to the two 74HC595 shift registers
 * 
 * The data is sent LSB first (bit 0 first, bit 15 last)
 * 
 * @param highByte: Data for the second shift register (segments)
 * @param lowByte:  Data for the first shift register (digits + LEDs)
 * 
 * Timing: All delays are removed for maximum speed to reduce flickering
 */
void sendBytes(byte highByte, byte lowByte) {
  // Prepare latch for data transfer
  digitalWrite(PIN_RCLK, LOW);
  
  // Send low byte first (digits and LEDs) - this goes to the first 74HC595
  for (int i = 0; i < 8; i++) {
    digitalWrite(PIN_DATA, (lowByte >> i) & 1);  // Write one bit
    digitalWrite(PIN_SRCLK, HIGH);                // Clock pulse - rising edge
    digitalWrite(PIN_SRCLK, LOW);                 // Clock pulse - falling edge
  }
  
  // Send high byte next (segments) - this goes to the second 74HC595
  for (int i = 0; i < 8; i++) {
    digitalWrite(PIN_DATA, (highByte >> i) & 1); // Write one bit
    digitalWrite(PIN_SRCLK, HIGH);               // Clock pulse - rising edge
    digitalWrite(PIN_SRCLK, LOW);                // Clock pulse - falling edge
  }
  
  // Latch the data - both registers update simultaneously
  digitalWrite(PIN_RCLK, HIGH);
  digitalWrite(PIN_RCLK, LOW);
}

/*
 * Display a single digit at a specific position
 * 
 * @param digit:    The digit to display (0-9)
 * @param position: The digit position (0 = leftmost, 3 = rightmost)
 */
void displayDigit(byte digit, byte position) {
  if (digit > 9) return;      // Invalid digit
  if (position > 3) return;   // Invalid position
  
  byte highByte = digits[digit].high;    // Get segment data for this digit
  byte lowByte = 1 << position;          // Select only one digit position (bit 0-3)
  
  sendBytes(highByte, lowByte);
}

/*
 * Display a 4-digit number with dynamic multiplexing
 * 
 * This uses time-division multiplexing to display all 4 digits.
 * Each digit is turned on for a short time, then the next digit.
 * The cycle repeats fast enough that the eye sees all digits lit.
 * 
 * @param number: The number to display (0-9999)
 */
void displayNumber(int number) {
  if (number > 9999) number = 9999;
  if (number < 0) number = 0;
  
  // Extract individual digits
  byte d0 = number / 1000;        // Thousands digit (leftmost)
  byte d1 = (number / 100) % 10;  // Hundreds digit
  byte d2 = (number / 10) % 10;   // Tens digit
  byte d3 = number % 10;          // Units digit (rightmost)
  
  // Turn off all digits first - prevents ghosting
  sendBytes(0x00, 0x00);
  delayMicroseconds(100);
  
  /*
   * Each digit is turned on for 1000 microseconds.
   * This creates a 25% duty cycle for each digit (4 digits total).
   * The refresh rate is approximately 250Hz (1000us * 4 = 4ms period)
   */
  
  // Digit 0 (leftmost) - thousands
  sendBytes(digits[d0].high, 0b00000001);
  delayMicroseconds(1000);
  
  // Digit 1 - hundreds
  sendBytes(digits[d1].high, 0b00000010);
  delayMicroseconds(1000);
  
  // Digit 2 - tens
  sendBytes(digits[d2].high, 0b00000100);
  delayMicroseconds(1000);
  
  // Digit 3 (rightmost) - units
  sendBytes(digits[d3].high, 0b00001000);
  delayMicroseconds(1000);
}

/*
 * Clear the display (turn off all segments and digits)
 */
void displayClear() {
  sendBytes(0x00, 0x00);
}

/*
 * Control the status LEDs
 * 
 * @param green: true = green LED ON, false = OFF
 * @param red:   true = red LED ON,   false = OFF
 */
void setLEDs(bool green, bool red) {
  byte leds = 0;
  if (green) leds |= LED_GREEN;
  if (red) leds |= LED_RED;
  
  // Send only LED data (no digits, no segments)
  sendBytes(0x00, leds);
}

/*
 * Display the same digit on all 4 positions (for testing)
 * 
 * @param digit: The digit to display (0-9)
 */
void showDigitOnAll(byte digit) {
  if (digit > 9) return;
  sendBytes(digits[digit].high, 0b00001111);  // All 4 digits enabled
}

// ==================== SETUP ====================

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("========================================");
  Serial.println("Samsung DSB-S300 Front Panel Controller");
  Serial.println("========================================");
  Serial.println("Hardware: 2x 74HC595 + 4-digit 7-segment display");
  Serial.println("Display type: Common Anode (active LOW)");
  Serial.println();
  
  // Configure GPIO pins
  pinMode(PIN_SRCLK, OUTPUT);
  pinMode(PIN_RCLK, OUTPUT);
  pinMode(PIN_DATA, OUTPUT);
  
  // Initialize all pins to LOW (inactive state)
  digitalWrite(PIN_SRCLK, LOW);
  digitalWrite(PIN_RCLK, LOW);
  digitalWrite(PIN_DATA, LOW);
  
  delay(100);
  
  // Clear the display
  displayClear();
  
  // ==================== TEST SEQUENCE ====================
  
  Serial.println("Starting test sequence...");
  Serial.println();
  
  // Test 1: Display all digits 0-9 on all positions
  Serial.println("Test 1: Displaying digits 0-9 on all positions");
  for (byte d = 0; d < 10; d++) {
    Serial.printf("Digit: %d\n", d);
    showDigitOnAll(d);
    delay(1000);
  }
  
  // Clear display between tests
  displayClear();
  delay(500);
  
  // Test 2: Test individual digit positions
  Serial.println("\nTest 2: Testing individual digit positions");
  for (byte pos = 0; pos < 4; pos++) {
    Serial.printf("Position %d (showing '8')\n", pos + 1);
    displayDigit(8, pos);  // Show '8' at each position
    delay(1000);
    displayClear();
    delay(200);
  }
  
  // Test 3: LED test
  Serial.println("\nTest 3: Testing LEDs");
  Serial.println("Green LED ON");
  setLEDs(true, false);
  delay(1000);
  
  Serial.println("Red LED ON");
  setLEDs(false, true);
  delay(1000);
  
  Serial.println("Both LEDs ON");
  setLEDs(true, true);
  delay(1000);
  
  Serial.println("LEDs OFF");
  setLEDs(false, false);
  delay(500);
  
  // Clear display
  displayClear();
  
  Serial.println("\n========================================");
  Serial.println("Setup complete! Starting counter...");
  Serial.println("========================================\n");
}

// ==================== MAIN LOOP ====================

void loop() {
  static int counter = 0;              // Current number to display
  static unsigned long lastUpdate = 0; // Last time counter was updated
  
  /*
   * Continuously refresh the display with dynamic multiplexing.
   * This must run constantly to keep the digits lit.
   * The displayNumber() function handles the timing for each digit.
   */
  displayNumber(counter);
  
  /*
   * Update the counter value once per second.
   * The counter will cycle from 0000 to 9999 and repeat.
   */
  if (millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    counter = (counter + 1) % 10000;  // Increment and wrap at 9999
    
    // Print current value to Serial Monitor for debugging
    Serial.printf("%04d\n", counter);
    
    /*
     * Blink LEDs for visual feedback:
     * Even numbers = Green LED
     * Odd numbers  = Red LED
     */
    if ((counter / 10) % 2 == 0) {
      setLEDs(true, false);   // Green ON, Red OFF
    } else {
      setLEDs(false, true);   // Green OFF, Red ON
    }
  }
  
  /*
   * Small delay to control the refresh rate.
   * The total loop time is approximately:
   * - 4 digits * 1000us = 4ms for display update
   * - Plus 5ms delay = ~9ms total
   * - Refresh rate ≈ 111Hz (good for no flickering)
   */
  delay(5);
}

// ==================== ADDITIONAL HELPER FUNCTIONS ====================

/*
 * Display a number with custom digit positions
 * Useful for displaying numbers with leading zeros or specific formatting
 * 
 * @param number: The number to display (0-9999)
 * @param showLeadingZeros: If true, displays leading zeros (e.g., 0042)
 *                          If false, blanks leading zeros
 */
void displayNumberFormatted(int number, bool showLeadingZeros) {
  if (number > 9999) number = 9999;
  if (number < 0) number = 0;
  
  byte d0 = number / 1000;
  byte d1 = (number / 100) % 10;
  byte d2 = (number / 10) % 10;
  byte d3 = number % 10;
  
  // If not showing leading zeros, blank them
  if (!showLeadingZeros) {
    if (d0 == 0) d0 = 10;  // 10 = blank (not in digit array)
    if (d0 == 0 && d1 == 0) d1 = 10;
    if (d0 == 0 && d1 == 0 && d2 == 0) d2 = 10;
  }
  
  // Turn off all digits
  sendBytes(0x00, 0x00);
  delayMicroseconds(100);
  
  // Display each digit with equal timing
  if (d0 < 10) {
    sendBytes(digits[d0].high, 0b00000001);
  } else {
    sendBytes(0x00, 0x00);  // Blank digit
  }
  delayMicroseconds(1000);
  
  if (d1 < 10) {
    sendBytes(digits[d1].high, 0b00000010);
  } else {
    sendBytes(0x00, 0x00);
  }
  delayMicroseconds(1000);
  
  if (d2 < 10) {
    sendBytes(digits[d2].high, 0b00000100);
  } else {
    sendBytes(0x00, 0x00);
  }
  delayMicroseconds(1000);
  
  if (d3 < 10) {
    sendBytes(digits[d3].high, 0b00001000);
  } else {
    sendBytes(0x00, 0x00);
  }
  delayMicroseconds(1000);
}

/*
 * Set the brightness of the display by adjusting the delay time
 * 
 * @param brightness: 0-100 (0 = off, 100 = maximum brightness)
 * 
 * Note: This changes the duty cycle of the multiplexing.
 * Higher brightness = longer on-time per digit = more current draw.
 */
void setBrightness(int brightness) {
  // Constrain brightness to valid range
  if (brightness < 0) brightness = 0;
  if (brightness > 100) brightness = 100;
  
  // Map brightness (0-100) to delay time (100-2000 microseconds)
  int delayTime = map(brightness, 0, 100, 100, 2000);
  
  // This would need to be implemented in displayNumber()
  // by using a variable delay instead of fixed 1000us
  // For simplicity, this is left as a placeholder
  Serial.printf("Brightness set to %d%% (delay: %dus)\n", brightness, delayTime);
}

/*
 * Display a scrolling message across the 4-digit display
 * 
 * @param message: String to display (will be truncated to 4 characters)
 */
void displayMessage(const char* message) {
  // Simple implementation - just display first 4 characters
  // This would need a character map for letters to be fully functional
  // For now, just displays as numbers if digits are passed
  Serial.println("Message display: (requires character mapping)");
}