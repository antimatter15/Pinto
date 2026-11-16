/*********************************************************************
 BLE JPEG Camera for Seeed XIAO nRF52840

 Captures JPEG images from HM01B0 camera and sends via BLE UART
*********************************************************************/

#include <bluefruit.h>
#include <Adafruit_TinyUSB.h>
#include "nrf.h"
#include "nrf_spis.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_timer.h"
#include "nrf_twim.h"
#include "arm_math.h"

#define TJE_PACKED_IMPLEMENTATION
#include "tiny_jpeg_packed.h"

#define DEVICE_NAME   "XIAO_nRF52840_CAM"

BLEUart bleuart;

// Camera and GPIO pin definitions
#define NRF_PIN_CAM_SDA    NRF_GPIO_PIN_MAP(0, 4)   // P0.04
#define NRF_PIN_CAM_SCL    NRF_GPIO_PIN_MAP(0, 5)   // P0.05

#define SPIS_SCK_PIN        NRF_GPIO_PIN_MAP(1, 13)  // P1.13
#define SPIS_MOSI_PIN       NRF_GPIO_PIN_MAP(1, 14)  // P1.14
#define SPIS_CS_PIN         NRF_GPIO_PIN_MAP(1, 15)  // P1.15

#define PIN_MCLK            NRF_GPIO_PIN_MAP(1, 11)  // P1.11
#define PIN_VSYNC           NRF_GPIO_PIN_MAP(0, 29)  // P0.28
#define PIN_INV_VSYNC       NRF_GPIO_PIN_MAP(1, 12)  // P1.12

// Camera settings
#define HM01B0_I2C_ADDR 0x24
#define IMAGE_WIDTH  328
#define IMAGE_HEIGHT 328
#define IMAGE_PACKED_WIDTH  (IMAGE_WIDTH * 6 / 8)
#define IMAGE_SIZE   (IMAGE_PACKED_WIDTH * IMAGE_HEIGHT)

// TWI instance pointer
#define TWI_INSTANCE        NRF_TWIM0
#define SPIS1_IRQn SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQn
#define SPIS1_IRQHandler SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler

// Frame buffer and JPEG variables
volatile uint32_t bufferPosition = 0;
uint8_t frameBuffer[IMAGE_SIZE];
uint8_t rowBuffer[IMAGE_WIDTH];
uint8_t jpeg_buffer[8192 * 4];  // Buffer for JPEG data
uint32_t jpeg_size = 0;

// TWI buffer for transactions
static uint8_t twi_tx_buffer[8];
static uint8_t twi_rx_buffer[8];

uint32_t packet_counter = 0;
uint32_t last_send_time = 0;
const uint32_t SEND_INTERVAL = 3000;
volatile uint32_t numTx = 0;

// Helper function to get pin configuration register
static volatile uint32_t* pincfg_reg(uint32_t pin)
{
  NRF_GPIO_Type * port = nrf_gpio_pin_port_decode(&pin);
  return &port->PIN_CNF[pin];
}

// Initialize TWI/I2C peripheral
void twi_init(void) {
  // Configure SDA pin
  *pincfg_reg(NRF_PIN_CAM_SDA) = ((uint32_t)GPIO_PIN_CNF_DIR_Input         << GPIO_PIN_CNF_DIR_Pos)
                                 | ((uint32_t)GPIO_PIN_CNF_INPUT_Connect    << GPIO_PIN_CNF_INPUT_Pos)
                                 | ((uint32_t)GPIO_PIN_CNF_PULL_Pullup      << GPIO_PIN_CNF_PULL_Pos)
                                 | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0D1       << GPIO_PIN_CNF_DRIVE_Pos)
                                 | ((uint32_t)GPIO_PIN_CNF_SENSE_Disabled   << GPIO_PIN_CNF_SENSE_Pos);

  // Configure SCL pin
  *pincfg_reg(NRF_PIN_CAM_SCL) = ((uint32_t)GPIO_PIN_CNF_DIR_Input         << GPIO_PIN_CNF_DIR_Pos)
                                 | ((uint32_t)GPIO_PIN_CNF_INPUT_Connect    << GPIO_PIN_CNF_INPUT_Pos)
                                 | ((uint32_t)GPIO_PIN_CNF_PULL_Pullup      << GPIO_PIN_CNF_PULL_Pos)
                                 | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0D1       << GPIO_PIN_CNF_DRIVE_Pos)
                                 | ((uint32_t)GPIO_PIN_CNF_SENSE_Disabled   << GPIO_PIN_CNF_SENSE_Pos);

  // Configure TWIM peripheral
  nrf_twim_pins_set(TWI_INSTANCE, NRF_PIN_CAM_SCL, NRF_PIN_CAM_SDA);
  nrf_twim_frequency_set(TWI_INSTANCE, NRF_TWIM_FREQ_100K);
  nrf_twim_enable(TWI_INSTANCE);
}

// TWI write transaction
bool twi_write(uint8_t address, const uint8_t* data, size_t length) {
  // Copy data to TX buffer
  for (size_t i = 0; i < length && i < sizeof(twi_tx_buffer); i++) {
    twi_tx_buffer[i] = data[i];
  }

  // Set address and configure TX
  nrf_twim_address_set(TWI_INSTANCE, address);
  nrf_twim_tx_buffer_set(TWI_INSTANCE, twi_tx_buffer, length);

  // Clear events
  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_ERROR);
  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED);
  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_TXSTARTED);
  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_LASTTX);

  // Start transmission
  nrf_twim_task_trigger(TWI_INSTANCE, NRF_TWIM_TASK_RESUME);
  nrf_twim_task_trigger(TWI_INSTANCE, NRF_TWIM_TASK_STARTTX);

  // Wait for TX to start
  while (!nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_TXSTARTED) &&
         !nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_ERROR));

  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_TXSTARTED);

  // Wait for last TX if we have data
  if (length > 0) {
    while (!nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_LASTTX) &&
           !nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_ERROR));
    nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_LASTTX);
  }

  // Stop transaction
  nrf_twim_task_trigger(TWI_INSTANCE, NRF_TWIM_TASK_STOP);
  while (!nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED));
  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED);

  // Check for errors
  if (nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_ERROR)) {
    nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_ERROR);
    uint32_t error = nrf_twim_errorsrc_get_and_clear(TWI_INSTANCE);
    return false;
  }

  return true;
}

// TWI read transaction
bool twi_read(uint8_t address, uint8_t* data, size_t length) {
  // Set address and configure RX
  nrf_twim_address_set(TWI_INSTANCE, address);
  nrf_twim_rx_buffer_set(TWI_INSTANCE, twi_rx_buffer, length);

  // Clear events
  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_ERROR);
  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED);
  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_RXSTARTED);
  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_LASTRX);

  // Start reception
  nrf_twim_task_trigger(TWI_INSTANCE, NRF_TWIM_TASK_RESUME);
  nrf_twim_task_trigger(TWI_INSTANCE, NRF_TWIM_TASK_STARTRX);

  // Wait for RX to start
  while (!nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_RXSTARTED) &&
         !nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_ERROR));

  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_RXSTARTED);

  // Wait for last RX
  while (!nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_LASTRX) &&
         !nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_ERROR));
  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_LASTRX);

  // Stop transaction
  nrf_twim_task_trigger(TWI_INSTANCE, NRF_TWIM_TASK_STOP);
  while (!nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED));
  nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED);

  // Check for errors
  if (nrf_twim_event_check(TWI_INSTANCE, NRF_TWIM_EVENT_ERROR)) {
    nrf_twim_event_clear(TWI_INSTANCE, NRF_TWIM_EVENT_ERROR);
    uint32_t error = nrf_twim_errorsrc_get_and_clear(TWI_INSTANCE);
    return false;
  }

  // Copy data from RX buffer
  for (size_t i = 0; i < length && i < sizeof(twi_rx_buffer); i++) {
    data[i] = twi_rx_buffer[i];
  }

  return true;
}

// Configure 8MHz oscillator on P1.11 to serve as XCLK
void configure_8mhz_square_wave_p1_11(void) {
  nrf_gpio_cfg_output(PIN_MCLK);
  nrf_gpio_pin_clear(PIN_MCLK);

  nrf_gpiote_task_configure(NRF_GPIOTE, 0, PIN_MCLK, NRF_GPIOTE_POLARITY_TOGGLE, NRF_GPIOTE_INITIAL_VALUE_LOW);
  nrf_gpiote_task_enable(NRF_GPIOTE, 0);

  nrf_timer_mode_set(NRF_TIMER0, NRF_TIMER_MODE_TIMER);
  nrf_timer_bit_width_set(NRF_TIMER0, NRF_TIMER_BIT_WIDTH_16);
  nrf_timer_frequency_set(NRF_TIMER0, NRF_TIMER_FREQ_16MHz);

  nrf_timer_cc_set(NRF_TIMER0, NRF_TIMER_CC_CHANNEL0, 1);
  nrf_timer_task_trigger(NRF_TIMER0, NRF_TIMER_TASK_CLEAR);
  nrf_timer_shorts_enable(NRF_TIMER0, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK);

  NRF_PPI->CH[0].EEP = nrf_timer_event_address_get(NRF_TIMER0, NRF_TIMER_EVENT_COMPARE0);
  NRF_PPI->CH[0].TEP = nrf_gpiote_task_address_get(NRF_GPIOTE, NRF_GPIOTE_TASK_OUT_0);

  NRF_PPI->CHEN |= PPI_CHEN_CH0_Enabled;

  nrf_timer_task_trigger(NRF_TIMER0, NRF_TIMER_TASK_START);
}

// HM01B0 initialization sequence for 328x328, 6-bit mode
const struct {
  uint16_t reg;
  uint8_t val;
} hm01b0_init_seq[] = {
  {0x0103, 0x00}, // Software reset
  {0x0100, 0x00}, // Power up
  
  // Analog settings
  {0x1003, 0x08}, {0x1007, 0x08}, {0x3044, 0x0A}, {0x3045, 0x00},
  {0x3047, 0x0A}, {0x3050, 0xC0}, {0x3051, 0x42}, {0x3052, 0x50},
  {0x3053, 0x00}, {0x3054, 0x03}, {0x3055, 0xF7}, {0x3056, 0xF8},
  {0x3057, 0x29}, {0x3058, 0x1F}, {0x3059, 0x22}, {0x3064, 0x00},
  {0x3065, 0x04},
  
  // Digital functions
  {0x1000, 0x43}, {0x1001, 0x40}, {0x1002, 0x32}, {0x0350, 0x7F},
  {0x1006, 0x01}, {0x1008, 0x00}, {0x1009, 0xA0}, {0x100A, 0x60},
  {0x100B, 0x90}, {0x100C, 0x40}, {0x3022, 0x02}, {0x3023, 0x02},
  {0x1012, 0x07}, // VSync/HSYNC/PixelShift enable
  
  // ROI and AE settings
  {0x2000, 0x07}, {0x2003, 0x00}, {0x2004, 0x1C}, {0x2007, 0x00},
  {0x2008, 0x58}, {0x200B, 0x00}, {0x200C, 0x7A}, {0x200F, 0x00},
  {0x2010, 0xB8}, {0x2013, 0x00}, {0x2014, 0x58}, {0x2017, 0x00},
  {0x2018, 0x9B}, {0x2100, 0x01}, {0x2101, 0x5F}, {0x2102, 0x0A},
  {0x2103, 0x03}, {0x2104, 0x05}, {0x2105, 0x02}, {0x2106, 0x14},
  {0x2107, 0x02}, {0x2108, 0x03}, {0x2109, 0x03}, {0x210A, 0x00},
  {0x210B, 0x80}, {0x210C, 0x40}, {0x210D, 0x20}, {0x210E, 0x03},
  {0x210F, 0x00}, {0x2110, 0x85}, {0x2111, 0x00}, {0x2112, 0xA0},
  {0x2150, 0x03},
  
  // Frame timing
  {0x0340, 0x01}, {0x0341, 0x7A}, {0x0342, 0x01}, {0x0343, 0x77},
  
  // Resolution settings
  {0x3010, 0x00}, {0x3011, 0x70}, {0x3060, 0x30}, {0x0101, 0x01},
  {0x0104, 0x01}
};

void writeReg16_8(uint16_t reg, uint8_t val) {
  uint8_t data[3];
  data[0] = reg >> 8;
  data[1] = reg & 0xFF;
  data[2] = val;
  twi_write(HM01B0_I2C_ADDR, data, 3);
}

uint8_t readReg16_8(uint16_t reg) {
  uint8_t reg_data[2];
  uint8_t value = 0xFF;
  reg_data[0] = reg >> 8;
  reg_data[1] = reg & 0xFF;
  if (twi_write(HM01B0_I2C_ADDR, reg_data, 2)) {
    twi_read(HM01B0_I2C_ADDR, &value, 1);
  }
  return value;
}

uint16_t readReg16_16(uint16_t reg) {
  uint8_t reg_data[2];
  uint8_t value_data[2] = {0xFF, 0xFF};
  uint16_t value = 0;
  reg_data[0] = reg >> 8;
  reg_data[1] = reg & 0xFF;
  if (twi_write(HM01B0_I2C_ADDR, reg_data, 2)) {
    if (twi_read(HM01B0_I2C_ADDR, value_data, 2)) {
      value = (value_data[0] << 8) | value_data[1];
    }
  }
  return value;
}

bool initCamera() {
  uint16_t modelId = readReg16_16(0x0000);
  Serial.print("Camera Model ID: 0x");
  Serial.println(modelId, HEX);
  
  if (modelId != 0x01B0) {
    Serial.println("ERROR: Invalid model ID!");
    return false;
  }
  
  Serial.println("Initializing camera...");
  for (int i = 0; i < sizeof(hm01b0_init_seq) / sizeof(hm01b0_init_seq[0]); i++) {
    writeReg16_8(hm01b0_init_seq[i].reg, hm01b0_init_seq[i].val);
    delay(1);
  }
  
  Serial.println("Camera initialized!");
  return true;
}

void spis_init(void) {
  nrf_gpio_cfg_input(SPIS_SCK_PIN, NRF_GPIO_PIN_PULLDOWN);
  nrf_gpio_cfg_input(SPIS_MOSI_PIN, NRF_GPIO_PIN_NOPULL);
  nrf_gpio_cfg_input(SPIS_CS_PIN, NRF_GPIO_PIN_PULLUP);
  
  nrf_spis_pins_set(NRF_SPIS1, SPIS_SCK_PIN, SPIS_MOSI_PIN,
                    NRF_SPIS_PIN_NOT_CONNECTED, SPIS_CS_PIN);
  
  nrf_spis_configure(NRF_SPIS1, NRF_SPIS_MODE_3, NRF_SPIS_BIT_ORDER_MSB_FIRST);
  nrf_spis_def_set(NRF_SPIS1, 0x00);
  
  bufferPosition = 0;
  nrf_spis_rx_buffer_set(NRF_SPIS1, rowBuffer, IMAGE_WIDTH);
  nrf_spis_tx_buffer_set(NRF_SPIS1, NULL, 0);
  
  nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_END);
  nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);
  nrf_spis_shorts_enable(NRF_SPIS1, NRF_SPIS_SHORT_END_ACQUIRE);
  nrf_spis_int_enable(NRF_SPIS1, NRF_SPIS_INT_END_MASK);
  
  NVIC_SetPriority(SPIS1_IRQn, 3);
  NVIC_ClearPendingIRQ(SPIS1_IRQn);
  NVIC_EnableIRQ(SPIS1_IRQn);
  
  nrf_spis_enable(NRF_SPIS1);
  nrf_spis_task_trigger(NRF_SPIS1, NRF_SPIS_TASK_RELEASE);
}

// Optimized version processing 4 bytes at a time
void shift_quantize_pack(uint8_t* source, uint8_t* packed, int length, int shift_bits) {
  if (shift_bits < 0 || shift_bits > 7) {
    shift_bits = 0;
  }
  
  const uint32_t shift_mask = (1U << shift_bits) - 1;
  const int inv_shift = 8 - shift_bits;
  
  int src_idx = 0;
  int dst_idx = 0;
  uint32_t carry = 0;
  
  while (src_idx + 3 < length) {
    uint32_t b0, b1, b2, b3;
    
    if (shift_bits) {
      b0 = source[src_idx];
      uint32_t next_carry = b0 & shift_mask;
      b0 = (b0 >> shift_bits) | (carry << inv_shift);
      
      b1 = source[src_idx + 1];
      carry = b1 & shift_mask;
      b1 = (b1 >> shift_bits) | (next_carry << inv_shift);
      
      b2 = source[src_idx + 2];
      next_carry = b2 & shift_mask;
      b2 = (b2 >> shift_bits) | (carry << inv_shift);
      
      b3 = source[src_idx + 3];
      carry = b3 & shift_mask;
      b3 = (b3 >> shift_bits) | (next_carry << inv_shift);
    } else {
      b0 = source[src_idx];
      b1 = source[src_idx + 1];
      b2 = source[src_idx + 2];
      b3 = source[src_idx + 3];
    }
    
    b0 = (b0 >> 2) & 0x3F;
    b1 = (b1 >> 2) & 0x3F;
    b2 = (b2 >> 2) & 0x3F;
    b3 = (b3 >> 2) & 0x3F;
    
    packed[dst_idx]     = (b0 << 2) | (b1 >> 4);
    packed[dst_idx + 1] = (b1 << 4) | (b2 >> 2);
    packed[dst_idx + 2] = (b2 << 6) | b3;
    
    src_idx += 4;
    dst_idx += 3;
  }
  
  if (src_idx < length) {
    uint64_t accumulator = 0;
    int bits = 0;
    
    while (src_idx < length) {
      uint32_t value = source[src_idx++];
      
      if (shift_bits) {
        uint32_t next_carry = value & shift_mask;
        value = (value >> shift_bits) | (carry << inv_shift);
        carry = next_carry;
      }
      
      accumulator = (accumulator << 6) | ((value >> 2) & 0x3F);
      bits += 6;
      
      if (bits >= 8) {
        packed[dst_idx++] = (accumulator >> (bits - 8)) & 0xFF;
        bits -= 8;
      }
    }
    
    if (bits > 0) {
      packed[dst_idx] = (accumulator << (8 - bits)) & 0xFF;
    }
  }
}

// JPEG write callback function
void jpeg_write_callback(void* context, void* data, int size) {
  if (jpeg_size + size <= sizeof(jpeg_buffer)) {
    memcpy(jpeg_buffer + jpeg_size, data, size);
    jpeg_size += size;
  }
}

void printFrameStats() {
  jpeg_size = 0;
  Serial.println("Encoding JPEG");
  int result = tje_encode_packed_with_func(
                 jpeg_write_callback,
                 NULL,
                 70,
                 IMAGE_WIDTH,
                 IMAGE_HEIGHT,
                 frameBuffer
               );
  Serial.println("Done Encoding JPEG");
  
  if (result && jpeg_size > 0) {
    Serial.printf("JPEG encoded: %d bytes\\n", jpeg_size);
  } else {
    Serial.println("ERROR: JPEG encoding failed");
  }
}

extern "C" {
  void SPIS1_IRQHandler(void) {
    if (nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_END)) {
      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_END);
      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);
      
      if (bufferPosition >= IMAGE_SIZE) {
        bufferPosition = 0;
      }
      nrf_spis_rx_buffer_set(NRF_SPIS1, rowBuffer, IMAGE_WIDTH);
      nrf_spis_task_trigger(NRF_SPIS1, NRF_SPIS_TASK_RELEASE);
      
      shift_quantize_pack(rowBuffer,
                          &frameBuffer[bufferPosition],
                          IMAGE_WIDTH,
                          3);
      
      bufferPosition += IMAGE_PACKED_WIDTH;
      numTx++;
    }
    
    if (nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED)) {
      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial && millis() < 5000); // Wait for serial or timeout
  Serial.println("Starting BLE JPEG Camera");

  // Initialize LEDs
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_GREEN, HIGH);

  // Initialize camera hardware
  configure_8mhz_square_wave_p1_11();
  twi_init();
  
  // Initialize camera
  if (!initCamera()) {
    Serial.println("Camera initialization failed!");
    while (1) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(500);
    }
  }
  
  spis_init();
  
  // Green LED indicates camera ready
  digitalWrite(LED_GREEN, LOW);

  // Initialize BLE
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(DEVICE_NAME);

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  Bluefruit.Periph.setConnInterval(6, 12);

  bleuart.begin();
  startAdv();

  Serial.println("Setup complete. Camera ready, BLE advertising...");

  delay(1000);
  // Start first frame capture
  writeReg16_8(0x3020, 0x01); // Set frame count to 1
  writeReg16_8(0x0100, 0x03); // Stream N frames mode
  bufferPosition = 0;
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void loop()
{
  Serial.print("TX: ");
  Serial.println(numTx);
  numTx = 0;

  Serial.print("Buffer position: ");
  Serial.println(bufferPosition);

  if (bufferPosition > 0) {
    // Wait for frame to complete
    while (nrf_spis_semaphore_status_get(NRF_SPIS1) != NRF_SPIS_SEMSTAT_CPU &&
           nrf_spis_semaphore_status_get(NRF_SPIS1) != NRF_SPIS_SEMSTAT_FREE) {
      Serial.println("Waiting for frame finish");
      delay(10);
    }
    
    // Encode and send JPEG if BLE connected
    if (Bluefruit.connected()) {
      encodeAndSendJPEG();
    } else {
      printFrameStats(); // Still print for debugging
    }

    // Reset for next frame
    bufferPosition = 0;
    memset(frameBuffer, 0, sizeof(frameBuffer));
    writeReg16_8(0x3020, 0x01); // Set frame count to 1
    writeReg16_8(0x0100, 0x03); // Stream N frames mode
  }

  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
}

void encodeAndSendJPEG()
{
  // Reset JPEG buffer
  jpeg_size = 0;

  Serial.println("Encoding JPEG");
  // Encode packed frame data to JPEG with quality 70
  int result = tje_encode_packed_with_func(
                 jpeg_write_callback,
                 NULL,
                 70,  // Quality
                 IMAGE_WIDTH,
                 IMAGE_HEIGHT,
                 frameBuffer
               );
  Serial.println("Done Encoding JPEG");

  if (result && jpeg_size > 0) {
    Serial.printf("Sending JPEG #%d (%d bytes)\n", packet_counter, jpeg_size);
    
    uint32_t start_time = millis();
    
    // Send JPEG data in chunks over BLE
    const int chunk_size = 200;
    for (int i = 0; i < jpeg_size; i += chunk_size) {
      int remaining = min(chunk_size, (int)(jpeg_size - i));
      bleuart.write(&jpeg_buffer[i], remaining);
      delay(5); // Small delay between chunks
    }

    uint32_t send_time = millis() - start_time;
    Serial.printf("Sent JPEG #%d (%d bytes) in %d ms\n", packet_counter, jpeg_size, send_time);
    packet_counter++;
  } else {
    Serial.println("ERROR: JPEG encoding failed");
  }
}

void connect_callback(uint16_t conn_handle)
{
  Serial.println("Device Connected!");
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  Serial.printf("Connection Info: PHY = %d Mbps, Conn Interval = %.2f ms, Data Length = %d, MTU = %d\n",
                conn->getPHY(), conn->getConnectionInterval()*1.25f, conn->getDataLength(), conn->getMtu());
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
  Serial.println("Device Disconnected!");
  packet_counter = 0;
}
