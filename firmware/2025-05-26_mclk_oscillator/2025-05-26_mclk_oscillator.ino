#include <Wire.h>
#include <Adafruit_TinyUSB.h>

#include "nrf.h"


#include "nrf_spis.h"
#include "nrf_gpio.h"



// Pin definitions
#define PIN_CAM_SDA    4   // D4/P0.04
#define PIN_CAM_SCL    5   // D5/P0.05
#define PIN_CAM_PCLK   8   // D8/P1.13
#define PIN_CAM_D0     9   // D9/P1.14
#define PIN_CAM_VSYNC  2   // D2/P0.28
#define PIN_CAM_HREF   3   // D3/P0.29




#define SPIS_SCK_PIN   NRF_GPIO_PIN_MAP(1, 13)  // P1.13
#define SPIS_MOSI_PIN  NRF_GPIO_PIN_MAP(1, 14)  // P1.14
#define SPIS_CS_PIN    NRF_GPIO_PIN_MAP(0, 15)  // P0.15


// This configures 8mhz oscillator on D6/P1.11 to serve as XCLK



void configure_4mhz_square_wave_p1_11(void) {

  // === 1. Configure GPIO P1.11 as output ===
  NRF_P1->PIN_CNF[11] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                        (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                        (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                        (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

  // Set initial state to low
  NRF_P1->OUTCLR = (1 << 11);

  // === 2. Configure GPIOTE channel 0 for toggle on P1.11 ===
  NRF_GPIOTE->CONFIG[0] = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
                          (11 << GPIOTE_CONFIG_PSEL_Pos) |                    // Pin 11
                          (1 << GPIOTE_CONFIG_PORT_Pos) |                     // Port 1
                          (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) |
                          (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);

  // === 3. Configure TIMER0 for 125ns intervals (4MHz) ===
  NRF_TIMER0->MODE = TIMER_MODE_MODE_Timer;           // Timer mode (not counter)
  NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_16Bit;  // 16-bit sufficient for high frequency
  NRF_TIMER0->PRESCALER = 1;                          // 16MHz / 2^1 = 8 MHz

  // For 125ns toggle interval: 8MHz × 125ns = 1 tick
  NRF_TIMER0->CC[0] = 1;

  // Clear the timer initially
  NRF_TIMER0->TASKS_CLEAR = 1;

  // === 4. Configure shortcut: auto-clear timer on COMPARE[0] ===
  NRF_TIMER0->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Enabled;

  // === 5. Configure PPI channel 0: TIMER0 COMPARE[0] -> GPIOTE OUT[0] ===
  NRF_PPI->CH[0].EEP = (uint32_t)&NRF_TIMER0->EVENTS_COMPARE[0];  // Event endpoint
  NRF_PPI->CH[0].TEP = (uint32_t)&NRF_GPIOTE->TASKS_OUT[0];       // Task endpoint

  // Enable PPI channel 0
  NRF_PPI->CHEN |= PPI_CHEN_CH0_Enabled;

  // === 6. Start the timer ===
  NRF_TIMER0->TASKS_START = 1;

  // Square wave now runs continuously at 4MHz on P1.11!
  // Timer fires every 125ns, toggling the pin via PPI
}


void configure_8mhz_square_wave_p1_11(void) {

  // === 1. Configure GPIO P1.11 as output ===
  NRF_P1->PIN_CNF[11] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                        (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                        (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                        (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

  // Set initial state to low
  NRF_P1->OUTCLR = (1 << 11);

  // === 2. Configure GPIOTE channel 0 for toggle on P1.11 ===
  NRF_GPIOTE->CONFIG[0] = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
                          (11 << GPIOTE_CONFIG_PSEL_Pos) |                    // Pin 11
                          (1 << GPIOTE_CONFIG_PORT_Pos) |                     // Port 1
                          (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) |
                          (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);

  // === 3. Configure TIMER0 for 62.5ns intervals (8MHz) ===
  NRF_TIMER0->MODE = TIMER_MODE_MODE_Timer;           // Timer mode (not counter)
  NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_16Bit;  // 16-bit sufficient for high frequency
  NRF_TIMER0->PRESCALER = 0;                          // 16MHz / 2^0 = 16 MHz

  // For 62.5ns toggle interval: 16MHz × 62.5ns = 1 tick
  NRF_TIMER0->CC[0] = 1;

  // Clear the timer initially
  NRF_TIMER0->TASKS_CLEAR = 1;

  // === 4. Configure shortcut: auto-clear timer on COMPARE[0] ===
  NRF_TIMER0->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Enabled;

  // === 5. Configure PPI channel 0: TIMER0 COMPARE[0] -> GPIOTE OUT[0] ===
  NRF_PPI->CH[0].EEP = (uint32_t)&NRF_TIMER0->EVENTS_COMPARE[0];  // Event endpoint
  NRF_PPI->CH[0].TEP = (uint32_t)&NRF_GPIOTE->TASKS_OUT[0];       // Task endpoint

  // Enable PPI channel 0
  NRF_PPI->CHEN |= PPI_CHEN_CH0_Enabled;

  // === 6. Start the timer ===
  NRF_TIMER0->TASKS_START = 1;

  // Square wave now runs continuously at 8MHz on P1.11!
  // Timer fires every 62.5ns, toggling the pin via PPI
}



// Function to configure VSYNC to CS inversion using GPIOTE and PPI
void configure_vsync_to_cs_inversion(void) {
  // === Configure CS pin as output ===
  NRF_P0->PIN_CNF[15] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                        (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
                        (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                        (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

  // === Configure VSYNC pin as input ===
  NRF_P0->PIN_CNF[28] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
                        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                        (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                        (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

  // Set CS pin initial state to HIGH (assuming VSYNC starts LOW)
  NRF_P0->OUTSET = (1 << 15);

  // === Configure GPIOTE channel 1 for VSYNC rising edge detection ===
  NRF_GPIOTE->CONFIG[1] = (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) |
                          (28 << GPIOTE_CONFIG_PSEL_Pos) |                    // Pin 28 (VSYNC)
                          (0 << GPIOTE_CONFIG_PORT_Pos) |                     // Port 0
                          (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos);

  // === Configure GPIOTE channel 2 for VSYNC falling edge detection ===
  NRF_GPIOTE->CONFIG[2] = (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) |
                          (28 << GPIOTE_CONFIG_PSEL_Pos) |                    // Pin 28 (VSYNC)
                          (0 << GPIOTE_CONFIG_PORT_Pos) |                     // Port 0
                          (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos);

  // === Configure GPIOTE channel 3 for CS pin SET task ===
  NRF_GPIOTE->CONFIG[3] = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
                          (15 << GPIOTE_CONFIG_PSEL_Pos) |                    // Pin 15 (CS)
                          (0 << GPIOTE_CONFIG_PORT_Pos) |                     // Port 0
                          (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) |
                          (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);

  // === Configure GPIOTE channel 4 for CS pin CLR task ===
  NRF_GPIOTE->CONFIG[4] = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
                          (15 << GPIOTE_CONFIG_PSEL_Pos) |                    // Pin 15 (CS)
                          (0 << GPIOTE_CONFIG_PORT_Pos) |                     // Port 0
                          (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) |
                          (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);

  // === Configure PPI channel 1: VSYNC rising edge -> CS CLR (set CS LOW when VSYNC goes HIGH) ===
  NRF_PPI->CH[1].EEP = (uint32_t)&NRF_GPIOTE->EVENTS_IN[1];     // VSYNC rising edge event
  NRF_PPI->CH[1].TEP = (uint32_t)&NRF_GPIOTE->TASKS_CLR[4];     // CS clear task

  // === Configure PPI channel 2: VSYNC falling edge -> CS SET (set CS HIGH when VSYNC goes LOW) ===
  NRF_PPI->CH[2].EEP = (uint32_t)&NRF_GPIOTE->EVENTS_IN[2];     // VSYNC falling edge event
  NRF_PPI->CH[2].TEP = (uint32_t)&NRF_GPIOTE->TASKS_SET[3];     // CS set task

  // Enable PPI channels
  NRF_PPI->CHEN |= (PPI_CHEN_CH1_Enabled | PPI_CHEN_CH2_Enabled);

  Serial.println("VSYNC to CS inversion configured!");
}


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
  for (int i = 0; i < sizeof(hm01b0_init_seq) / sizeof(hm01b0_init_seq[0]); i++) {
    writeReg16_8(hm01b0_init_seq[i].reg, hm01b0_init_seq[i].val);
    delay(1);
  }

  Serial.println("Camera initialized!");
  return true;
}




void spis_init(void)
{
  // Configure GPIO pins
  nrf_gpio_cfg_input(SPIS_SCK_PIN, NRF_GPIO_PIN_NOPULL);
  nrf_gpio_cfg_input(SPIS_MOSI_PIN, NRF_GPIO_PIN_NOPULL);
//  nrf_gpio_cfg_input(SPIS_CS_PIN, NRF_GPIO_PIN_PULLDOWN);  // CS active low by default

  // Set pin assignments
  nrf_spis_pins_set(NRF_SPIS1, SPIS_SCK_PIN, SPIS_MOSI_PIN,
                    NRF_SPIS_PIN_NOT_CONNECTED, SPIS_CS_PIN);

  // Configure SPI mode 0, MSB first
  nrf_spis_configure(NRF_SPIS1, NRF_SPIS_MODE_0, NRF_SPIS_BIT_ORDER_MSB_FIRST);

  // Set default character (used when TX buffer is empty)
  nrf_spis_def_set(NRF_SPIS1, 0x00);

  // Configure RX buffer - this is where received data will be stored
  nrf_spis_rx_buffer_set(NRF_SPIS1, frameBuffer, IMAGE_SIZE);

  // Since we're receive-only, set TX buffer to NULL with 0 length
  nrf_spis_tx_buffer_set(NRF_SPIS1, NULL, 0);

  // Clear events
  nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_END);
  nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);

  // Enable shortcut for automatic semaphore handover after transaction
  nrf_spis_shorts_enable(NRF_SPIS1, NRF_SPIS_SHORT_END_ACQUIRE);

  // Enable SPIS peripheral
  nrf_spis_enable(NRF_SPIS1);

  // The semaphore is initially with CPU after enabling
  // Trigger RELEASE to make it available for SPI slave
  nrf_spis_task_trigger(NRF_SPIS1, NRF_SPIS_TASK_RELEASE);
}


// Poll for frame completion
bool spis_frame_ready(void)
{
  // Check if END event occurred (transaction completed)
  if (nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_END))
  {
    nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_END);

    // With END_ACQUIRE shortcut enabled, semaphore was automatically acquired
    // Check if we also got ACQUIRED event
    if (nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED))
    {
      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);
    }

    // Check how many bytes were received
    uint32_t bytes_received = nrf_spis_rx_amount_get(NRF_SPIS1);

    // Check if we received the full frame
    if (bytes_received == IMAGE_SIZE)
    {
      // Full frame received
      return true;
    }

    // Transaction ended but not full frame
    // Release semaphore for next transaction
    nrf_spis_task_trigger(NRF_SPIS1, NRF_SPIS_TASK_RELEASE);
  }

  return false;
}

// Get number of bytes received in last transaction
uint32_t spis_get_received_bytes(void)
{
  return nrf_spis_rx_amount_get(NRF_SPIS1);
}

// Prepare for next frame reception
void spis_prepare_next_frame(void)
{
  // Check if we have the semaphore
  if (nrf_spis_semaphore_status_get(NRF_SPIS1) == NRF_SPIS_SEMSTAT_CPU)
  {
    nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_END);
    nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);
    
    // Reset RX buffer for next frame
    nrf_spis_rx_buffer_set(NRF_SPIS1, frameBuffer, IMAGE_SIZE);

    // Release semaphore for SPI slave
    nrf_spis_task_trigger(NRF_SPIS1, NRF_SPIS_TASK_RELEASE);
  }
}

// Check if a transaction is in progress
bool spis_busy(void)
{
  return nrf_spis_semaphore_status_get(NRF_SPIS1) == NRF_SPIS_SEMSTAT_SPIS;
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


  configure_4mhz_square_wave_p1_11();
  configure_vsync_to_cs_inversion();

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

  spis_init();


  // Green LED indicates ready
  digitalWrite(LED_GREEN, LOW);


  // Start continuous streaming mode
  writeReg16_8(0x0100, 0x01); // Set to streaming mode

  Serial.println("Setup complete! Press any key to capture a frame...");

}



void printFrameStats() {
  //  uint32_t bytesReceived = SPIS1_REG(SPIS_RXD_AMOUNT_OFFSET);

  // Print first few bytes as hex
//  Serial.print("First 16 bytes: ");
  for (int i = 0; i < IMAGE_SIZE; i++) {
    if(i % (IMAGE_WIDTH + 2) == 0){
      Serial.println();
    }
    
    if (frameBuffer[i] < 16) Serial.print("0");
    Serial.print(frameBuffer[i], HEX);
    Serial.print(" ");
    
  }
  Serial.println();

  // Calculate basic statistics
  uint32_t sum = 0;
  uint8_t minVal = 255, maxVal = 0;


}


void loop() {
  uint32_t bytes_received = nrf_spis_rx_amount_get(NRF_SPIS1);
  if(nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_END)){
    Serial.println("EVENT_END");
  }
  if(nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED)){
    Serial.println("NRF_SPIS_EVENT_ACQUIRED");
  }
  if(nrf_spis_semaphore_status_get(NRF_SPIS1) == NRF_SPIS_SEMSTAT_SPIS){
    Serial.println("TX IN PROGRESS");
  }
  if(nrf_spis_semaphore_status_get(NRF_SPIS1) == NRF_SPIS_SEMSTAT_CPU){
    Serial.println("SEMSTAT CPU");
  }
  if(nrf_spis_status_get(NRF_SPIS1) == NRF_SPIS_STATUS_OVERREAD){
    Serial.println("OVERREAD");
  }
  if(nrf_spis_status_get(NRF_SPIS1) == NRF_SPIS_STATUS_OVERFLOW){
    Serial.println("OVERFLOW");
  }
  Serial.println("Tick");
  Serial.print(bytes_received);
  

  //  captureFrame();
  //  if (data_received) {
  //    Serial.print("Received ");
  //    Serial.print(bytes_received);
  //    Serial.println(" bytes");
  //
  //    // Process frameBuffer[IMAGE_SIZE] here
  //    printFrameStats();
  //    data_received = false;
  //  }


  if (spis_frame_ready())
  {
    Serial.print("Bytes received: ");
    Serial.println(spis_get_received_bytes());
    printFrameStats();
    // Process frameBuffer here
    // All IMAGE_SIZE bytes have been received

    // Do something with the image data...

    // Prepare for next frame
    spis_prepare_next_frame();
    
  }

  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
}
