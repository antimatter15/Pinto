#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

// LED configuration - based on netlist LED_DIN goes to U2.B9
#define LED_DATA_PIN 30        // Pin for LED_DIN
#define LED_ENABLE_PIN 26      // Pin for LED_CE
#define NUM_LEDS 5            // 5 SK6812-EC20 LEDs in ring

// Timing constants based on SK6812-EC20 datasheet (in microseconds)
#define T0H_TIME 0.32         // 0 code high time (0.2-0.4µs)
#define T0L_TIME 0.88         // 0 code low time (0.8µs min, total period 1.2µs)
#define T1H_TIME 0.64         // 1 code high time (0.58-1.0µs)
#define T1L_TIME 0.56         // 1 code low time (0.2µs min, total period 1.2µs)
#define RESET_TIME 100        // Reset time (>80µs)

// Color structure for SK6812-EC20 (24-bit GRB format)
struct Color {
  uint8_t green;
  uint8_t red;
  uint8_t blue;
};

// Predefined colors
Color COLOR_RED = {0, 255, 0};
Color COLOR_GREEN = {255, 0, 0};
Color COLOR_BLUE = {0, 0, 255};
Color COLOR_WHITE = {255, 255, 255};
Color COLOR_OFF = {0, 0, 0};

// Array to store LED colors
Color ledColors[NUM_LEDS];

void setup() {
  
  // Initialize serial communication for debugging
  Serial.begin(115200);
  while (!Serial) delay(10); // Wait for serial port to connect
  
  Serial.println("XIAO nRF52840 SK6812 LED Control");


  
  // Initialize pins
  pinMode(LED_ENABLE_PIN, OUTPUT);
  digitalWrite(LED_ENABLE_PIN, LOW);   // Enable LEDs (active low)
  pinMode(LED_DATA_PIN, OUTPUT);
  digitalWrite(LED_DATA_PIN, LOW);
  
  delay(100);
  
  // Initialize all LEDs to off
  clearAllLEDs();
  updateLEDs();
}

// Send a '0' bit according to SK6812-EC20 timing
void sendBit0() {
  noInterrupts();
  digitalWrite(LED_DATA_PIN, HIGH);
  delayMicroseconds(T0H_TIME);
  digitalWrite(LED_DATA_PIN, LOW);
  delayMicroseconds(T0L_TIME);
  interrupts();
}

// Send a '1' bit according to SK6812-EC20 timing
void sendBit1() {
  noInterrupts();
  digitalWrite(LED_DATA_PIN, HIGH);
  delayMicroseconds(T1H_TIME);
  digitalWrite(LED_DATA_PIN, LOW);
  delayMicroseconds(T1L_TIME);
  interrupts();
}

// Send a byte (8 bits) MSB first
void sendByte(uint8_t data) {
  for (int i = 7; i >= 0; i--) {
    if (data & (1 << i)) {
      sendBit1();
    } else {
      sendBit0();
    }
  }
}

// Send reset signal to latch data
void sendReset() {
  digitalWrite(LED_DATA_PIN, LOW);
  delayMicroseconds(RESET_TIME);
}

// Send color data for one LED (GRB format)
void sendColor(Color color) {
  sendByte(color.green);  // Green first
  sendByte(color.red);    // Red second
  sendByte(color.blue);   // Blue third
}

// Update all LEDs with current color data
void updateLEDs() {
  // Send data for all LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    sendColor(ledColors[i]);
  }
  
  // Send reset to latch the data
  sendReset();
}

// Set color for a specific LED
void setLEDColor(int ledIndex, Color color) {
  if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
    ledColors[ledIndex] = color;
  }
}

// Set all LEDs to the same color
void setAllLEDs(Color color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    ledColors[i] = color;
  }
}

// Clear all LEDs (turn them off)
void clearAllLEDs() {
  setAllLEDs(COLOR_OFF);
}

void loop() {
  pinMode(LED_ENABLE_PIN, OUTPUT);
  digitalWrite(LED_ENABLE_PIN, LOW);   // Enable LEDs (active low)
  pinMode(LED_DATA_PIN, OUTPUT);
  digitalWrite(LED_DATA_PIN, LOW);
  
  delay(100);
  
  // Clear all LEDs
  clearAllLEDs();
  updateLEDs();
  
  // Light up LEDs one by one in blue
  for (int i = 0; i < NUM_LEDS; i++) {
    setLEDColor(i, COLOR_BLUE);
    updateLEDs();
    delay(200);
  }
  
  // Clear all LEDs
  clearAllLEDs();
  updateLEDs();
  
  // Light up LEDs one by one in green
  for (int i = 0; i < NUM_LEDS; i++) {
    setLEDColor(i, COLOR_GREEN);
    updateLEDs();
    delay(200);
  }

  Serial.println("X");

  pinMode(LED_ENABLE_PIN, OUTPUT);
  digitalWrite(LED_ENABLE_PIN, HIGH);   // Enable LEDs (active low)
  pinMode(LED_DATA_PIN, OUTPUT);
  digitalWrite(LED_DATA_PIN, LOW);
  
  delay(100);

  // Clear all LEDs
  clearAllLEDs();
  updateLEDs();
  
  // Light up LEDs one by one in blue
  for (int i = 0; i < NUM_LEDS; i++) {
    setLEDColor(i, COLOR_BLUE);
    updateLEDs();
    delay(200);
  }
  
  // Clear all LEDs
  clearAllLEDs();
  updateLEDs();
  
  // Light up LEDs one by one in green
  for (int i = 0; i < NUM_LEDS; i++) {
    setLEDColor(i, COLOR_GREEN);
    updateLEDs();
    delay(200);
  }

  Serial.println("Y");
  
}
