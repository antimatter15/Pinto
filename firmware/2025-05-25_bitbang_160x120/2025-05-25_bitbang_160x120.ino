#include <Wire.h>
#include <Adafruit_TinyUSB.h>

// Pin definitions
#define PIN_CAM_SDA    4   // D4/P0.04
#define PIN_CAM_SCL    5   // D5/P0.05
#define PIN_CAM_PCLK   8   // D8/P1.13
#define PIN_CAM_D0     9   // D9/P1.14
#define PIN_CAM_VSYNC  2   // D2/P0.28
#define PIN_CAM_HREF   3   // D3/P0.29

// Camera I2C address
#define HM01B0_I2C_ADDR 0x24

// Image dimensions for 160x120 mode
#define IMAGE_WIDTH  160
#define IMAGE_HEIGHT 120
#define IMAGE_SIZE   (IMAGE_WIDTH * IMAGE_HEIGHT)

// Frame buffer
uint8_t frameBuffer[IMAGE_SIZE];

// HM01B0 initialization sequence for 160x120, 1-bit serial mode
const struct {
  uint16_t reg;
  uint8_t val;
} hm01b0_init_seq[] = {
  {0x0103, 0x00}, // Software reset
  {0x0100, 0x00}, // Power up
  
  // Analog settings
  {0x1003, 0x08},
  {0x1007, 0x08},
  {0x3044, 0x0A},
  {0x3045, 0x00},
  {0x3047, 0x0A},
  {0x3050, 0xC0},
  {0x3051, 0x42},
  {0x3052, 0x50},
  {0x3053, 0x00},
  {0x3054, 0x03},
  {0x3055, 0xF7},
  {0x3056, 0xF8},
  {0x3057, 0x29},
  {0x3058, 0x1F},
  {0x3059, 0x22}, // 1-bit serial mode, MSB first
  {0x3064, 0x00},
  {0x3065, 0x04},
  
  // Digital functions
  {0x1000, 0x43}, // BLC on
  {0x1001, 0x40},
  {0x1002, 0x32},
  {0x0350, 0x7F}, // Dgain control
  {0x1006, 0x01}, // BLI enable
  {0x1008, 0x00}, // DPC off
  {0x1009, 0xA0},
  {0x100A, 0x60},
  {0x100B, 0x90},
  {0x100C, 0x40},
  {0x3022, 0x01}, // Advance VSYNC by 1 row
  {0x1012, 0x01}, // Sync enable
  
  // ROI and AE settings
  {0x2000, 0x07},
  {0x2003, 0x00},
  {0x2004, 0x1C},
  {0x2007, 0x00},
  {0x2008, 0x58},
  {0x200B, 0x00},
  {0x200C, 0x7A},
  {0x200F, 0x00},
  {0x2010, 0xB8},
  {0x2013, 0x00},
  {0x2014, 0x58},
  {0x2017, 0x00},
  {0x2018, 0x9B},
  {0x2100, 0x01}, // AE enable
  {0x2101, 0x5F}, // AE target mean
  {0x2102, 0x0A}, // AE min mean
  {0x2103, 0x03}, // Converge in threshold
  {0x2104, 0x05}, // Converge out threshold
  {0x2105, 0x02},
  {0x2106, 0x14},
  {0x2107, 0x02},
  {0x2108, 0x03},
  {0x2109, 0x03},
  {0x210A, 0x00},
  {0x210B, 0x80},
  {0x210C, 0x40},
  {0x210D, 0x20},
  {0x210E, 0x03},
  {0x210F, 0x00},
  {0x2110, 0x85},
  {0x2111, 0x00},
  {0x2112, 0xA0},
  {0x2150, 0x03},
  
  // Frame timing for 160x120
  {0x0340, 0x01}, // Frame length
  {0x0341, 0x7A},
  {0x0342, 0x01}, // Line length
  {0x0343, 0x77},
  
  // Resolution settings for 160x120
  {0x3010, 0x01}, // QVGA window mode
  {0x0383, 0x03}, // X subsample
  {0x0387, 0x03}, // Y subsample
  {0x0390, 0x03}, // Binning mode
  
  // Bit width and output format
  {0x3011, 0x70},
  {0x3060, 0x30}, // Gated clock, MSB first
  
  // Mirror/flip
  {0x0101, 0x01},
  
  // Update CMU
  {0x0104, 0x01}
};

void writeReg16_8(uint16_t reg, uint8_t val) {
  Wire.beginTransmission(HM01B0_I2C_ADDR);
  Wire.write(reg >> 8);    // Register address high byte
  Wire.write(reg & 0xFF);  // Register address low byte
  Wire.write(val);         // Value
  Wire.endTransmission();
}

uint8_t readReg16_8(uint16_t reg) {
  Wire.beginTransmission(HM01B0_I2C_ADDR);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.endTransmission();
  
  Wire.requestFrom(HM01B0_I2C_ADDR, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0xFF;
}

uint16_t readReg16_16(uint16_t reg) {
  uint16_t val = 0;
  Wire.beginTransmission(HM01B0_I2C_ADDR);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.endTransmission();
  
  Wire.requestFrom(HM01B0_I2C_ADDR, 2);
  if (Wire.available() >= 2) {
    val = Wire.read() << 8;
    val |= Wire.read();
  }
  return val;
}

bool initCamera() {
  // Check camera model ID
  uint16_t modelId = readReg16_16(0x0000);
  Serial.print("Camera Model ID: 0x");
  Serial.println(modelId, HEX);
  
  if (modelId != 0x01B0) {
    Serial.println("ERROR: Invalid model ID!");
    return false;
  }
  
  // Load initialization sequence
  Serial.println("Initializing camera...");
  for (int i = 0; i < sizeof(hm01b0_init_seq)/sizeof(hm01b0_init_seq[0]); i++) {
    writeReg16_8(hm01b0_init_seq[i].reg, hm01b0_init_seq[i].val);
    delay(1);
  }
  
  Serial.println("Camera initialized!");
  return true;
}

void captureFrame() {
  Serial.println("Starting frame capture...");
  
  // Clear frame buffer
  memset(frameBuffer, 0, IMAGE_SIZE);
  
  // Set streaming mode for 1 frame
  writeReg16_8(0x3020, 0x01); // Set frame count to 1
  writeReg16_8(0x0100, 0x03); // Stream N frames mode
  
  // Wait for VSYNC high (start of frame)
//  while (digitalRead(PIN_CAM_VSYNC) == LOW){
//    delay(1);
//  }
  
  while (digitalRead(PIN_CAM_VSYNC) == HIGH){
//    delay(1);
  }
  
  uint32_t pixelIndex = 0;
  uint8_t currentByte = 0;
  uint8_t bitIndex = 0;
  
  // Read frame data
  for (int y = 0; y < IMAGE_HEIGHT; y++) {
    // Wait for HREF high (start of line)
    while (digitalRead(PIN_CAM_HREF) == LOW) {
      if (digitalRead(PIN_CAM_VSYNC) == HIGH) {
        // Frame ended early
        Serial.println("Frame ended early!");
        goto frame_done;
      }
    }
    
    // Read line data (8 pixels per byte in 1-bit mode)
    for (int x = 0; x < IMAGE_WIDTH; x++) {
      // Wait for PCLK high
      while (digitalRead(PIN_CAM_PCLK) == LOW);
      
      // Read bit from D0
      if (digitalRead(PIN_CAM_D0)) {
        currentByte |= (1 << (7 - bitIndex));
      }
      
      bitIndex++;
      if (bitIndex >= 8) {
        frameBuffer[pixelIndex++] = currentByte;
        currentByte = 0;
        bitIndex = 0;
      }
      
      // Wait for PCLK low
      while (digitalRead(PIN_CAM_PCLK) == HIGH);
    }
    
    // Wait for HREF low (end of line)
    while (digitalRead(PIN_CAM_HREF) == HIGH);
  }
  
frame_done:
  // Stop streaming
  writeReg16_8(0x0100, 0x00);
  
  Serial.print("Captured ");
  Serial.print(pixelIndex);
  Serial.println(" bytes");
}

void printFrame() {
  Serial.println("\n=== FRAME DATA ===");
  Serial.print("Width: ");
  Serial.print(IMAGE_WIDTH);
  Serial.print(", Height: ");
  Serial.println(IMAGE_HEIGHT);
  
  // Print as hex dump
  Serial.println("\nHex dump:");
  for (int i = 0; i < IMAGE_SIZE; i += 16) {
//    Serial.print(i, HEX);
//    Serial.print(": ");
    for (int j = 0; j < 16 && (i + j) < IMAGE_SIZE; j++) {
      if (frameBuffer[i + j] < 0x10) Serial.print("0");
      Serial.print(frameBuffer[i + j], HEX);
      Serial.print(" ");
    }
//    Serial.println();
  }
  
  // Print as ASCII art (very simple threshold)
  Serial.println("\nASCII preview:");
  for (int y = 0; y < IMAGE_HEIGHT; y += 2) { // Skip every other row for display
    for (int x = 0; x < IMAGE_WIDTH; x += 2) { // Skip every other column
      int idx = y * IMAGE_WIDTH + x;
      uint8_t pixel = frameBuffer[idx / 8] & (1 << (7 - (idx % 8)));
      Serial.print(pixel ? "#" : " ");
    }
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000); // Wait for serial or timeout
  
  Serial.println("\n=== HM01B0 Camera Test ===");
  
  // Initialize LEDs
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  
  // Initialize camera pins
  pinMode(PIN_CAM_PCLK, INPUT);
  pinMode(PIN_CAM_D0, INPUT);
  pinMode(PIN_CAM_VSYNC, INPUT);
  pinMode(PIN_CAM_HREF, INPUT);
  
  // Initialize I2C
  Wire.begin();
  Wire.setClock(100000); // 100kHz
  
  // Initialize camera
  if (!initCamera()) {
    Serial.println("Camera initialization failed!");
    while (1) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(500);
    }
  }
  
  // Green LED indicates ready
  digitalWrite(LED_GREEN, LOW);
  Serial.println("Setup complete! Press any key to capture a frame...");
}

void loop() {
  if (Serial.available()) {
    // Clear serial buffer
    while (Serial.available()) Serial.read();
    
    // Blue LED during capture
    digitalWrite(LED_BLUE, LOW);
    Serial.println("\nCapturing...");
    delay(100);
    
    
    // Capture frame
    captureFrame();
    
    // Blue LED off
    digitalWrite(LED_BLUE, HIGH);
    
    // Print frame data
    //printFrame();
    
    Serial.println("\nPress any key to capture another frame...");
  }
  
  delay(100);
}
