#include <Adafruit_TinyUSB.h>
#include "nrf.h"
#include "nrf_spis.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_timer.h"
#include "nrf_twim.h"
#include "arm_math.h"

#define NRF_PIN_CAM_SDA    NRF_GPIO_PIN_MAP(0, 4)   // P0.04
#define NRF_PIN_CAM_SCL    NRF_GPIO_PIN_MAP(0, 5)   // P0.05

#define SPIS_SCK_PIN        NRF_GPIO_PIN_MAP(1, 13)  // P1.13
#define SPIS_MOSI_PIN       NRF_GPIO_PIN_MAP(1, 14)  // P1.14
#define SPIS_CS_PIN         NRF_GPIO_PIN_MAP(1, 15)  // P1.15

#define PIN_MCLK            NRF_GPIO_PIN_MAP(1, 11)  // P1.11
//#define PIN_VSYNC           NRF_GPIO_PIN_MAP(0, 28)  // P0.28
//#define PIN_HSYNC           NRF_GPIO_PIN_MAP(0, 29)  // P0.28

#define PIN_VSYNC           NRF_GPIO_PIN_MAP(0, 28)  // P0.28
#define PIN_INV_VSYNC       NRF_GPIO_PIN_MAP(1, 12)  // P1.12


// TWI instance pointer
#define TWI_INSTANCE        NRF_TWIM0

// Camera I2C address
#define HM01B0_I2C_ADDR 0x24


// assuming based on:     NRF_SPIS_INT_END_MASK      = SPIS_INTENSET_END_Msk,     ///< Interrupt on END event.
// i think its missing from the arduino nrf52 thing for some reason, perhaps compatibility with nrf51
#define NRF_SPIS_INT_ENDRX_MASK SPIS_INTENSET_ENDRX_Msk
#define NRF_SPIS_EVENT_ENDRX  ((nrf_spis_event_t) offsetof(NRF_SPIS_Type, EVENTS_ENDRX))



#define SPIS1_IRQn SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQn
#define SPIS1_IRQHandler SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler



#define SEGMENT_SIZE        1024    // 16KB per segment (must be <= 0xFFFF)
#define NUM_RX_SEGMENTS     128        // 7 segments = 112KB capacity (more than 100KB needed)

// Array List structure - each segment must be exactly SEGMENT_SIZE
typedef struct {
  uint8_t data[SEGMENT_SIZE];
} __attribute__((packed, aligned(4))) rx_segment_t;

// RX buffer array for Array List mode
static rx_segment_t rx_segments[NUM_RX_SEGMENTS] __attribute__((aligned(4)));

// Minimal TX buffer (required by hardware but not used)
static uint8_t dummy_tx_buffer[1] __attribute__((aligned(4))) = {0xFF};




// TWI buffer for transactions
static uint8_t twi_tx_buffer[8];
static uint8_t twi_rx_buffer[8];

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

void writeReg16_8(uint16_t reg, uint8_t val) {
  uint8_t data[3];
  data[0] = reg >> 8;        // Register address high byte
  data[1] = reg & 0xFF;      // Register address low byte
  data[2] = val;             // Value

  twi_write(HM01B0_I2C_ADDR, data, 3);
}

uint8_t readReg16_8(uint16_t reg) {
  uint8_t reg_data[2];
  uint8_t value = 0xFF;

  reg_data[0] = reg >> 8;
  reg_data[1] = reg & 0xFF;

  // Write register address
  if (twi_write(HM01B0_I2C_ADDR, reg_data, 2)) {
    // Read value
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

  // Write register address
  if (twi_write(HM01B0_I2C_ADDR, reg_data, 2)) {
    // Read value
    if (twi_read(HM01B0_I2C_ADDR, value_data, 2)) {
      value = (value_data[0] << 8) | value_data[1];
    }
  }

  return value;
}
// This configures 8MHz oscillator on D6/P1.11 to serve as XCLK using HAL functions
void configure_8mhz_square_wave_p1_11(void) {
  // === 1. Configure GPIO P1.11 as output using HAL ===
  nrf_gpio_cfg_output(PIN_MCLK);
  nrf_gpio_pin_clear(PIN_MCLK);  // Set initial state to low

  nrf_gpiote_task_configure(NRF_GPIOTE, 0, PIN_MCLK, NRF_GPIOTE_POLARITY_TOGGLE, NRF_GPIOTE_INITIAL_VALUE_LOW);
  nrf_gpiote_task_enable(NRF_GPIOTE, 0);

  nrf_timer_mode_set(NRF_TIMER0, NRF_TIMER_MODE_TIMER);           // Timer mode (not counter)
  nrf_timer_bit_width_set(NRF_TIMER0, NRF_TIMER_BIT_WIDTH_16);    // 16-bit sufficient for high frequency
  nrf_timer_frequency_set(NRF_TIMER0, NRF_TIMER_FREQ_16MHz);      // 16MHz base frequency

  nrf_timer_cc_set(NRF_TIMER0, NRF_TIMER_CC_CHANNEL0, 2); // maybe go for 4mhz?
  nrf_timer_task_trigger(NRF_TIMER0, NRF_TIMER_TASK_CLEAR);
  nrf_timer_shorts_enable(NRF_TIMER0, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK);

  // === 5. Configure PPI channel 0: TIMER0 COMPARE[0] -> GPIOTE OUT[0] using HAL ===
  NRF_PPI->CH[0].EEP = nrf_timer_event_address_get(NRF_TIMER0, NRF_TIMER_EVENT_COMPARE0);  // Event endpoint
  NRF_PPI->CH[0].TEP = nrf_gpiote_task_address_get(NRF_GPIOTE, NRF_GPIOTE_TASK_OUT_0);     // Task endpoint

  NRF_PPI->CHEN |= PPI_CHEN_CH0_Enabled;

  nrf_timer_task_trigger(NRF_TIMER0, NRF_TIMER_TASK_START);
}





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
  {0x3010, 0x00}, // QVGA window mode
  //  {0x0383, 0x03}, // X subsample
  //  {0x0387, 0x03}, // Y subsample
  //  {0x0390, 0x03}, // Binning mode

  // Bit width and output format
  {0x3011, 0x70},
  {0x3060, 0x30}, // Gated clock, MSB first

  // Mirror/flip
  {0x0101, 0x01},

  // Update CMU
  {0x0104, 0x01}
};

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
  nrf_gpio_cfg_input(SPIS_CS_PIN, NRF_GPIO_PIN_PULLDOWN);  // CS active low by default

  // Set pin assignments
  nrf_spis_pins_set(NRF_SPIS1, SPIS_SCK_PIN, SPIS_MOSI_PIN,
                    NRF_SPIS_PIN_NOT_CONNECTED, SPIS_CS_PIN);

  // Configure SPI mode 0, MSB first
  nrf_spis_configure(NRF_SPIS1, NRF_SPIS_MODE_0, NRF_SPIS_BIT_ORDER_MSB_FIRST);

  // Set default character (used when TX buffer is empty)
  nrf_spis_def_set(NRF_SPIS1, 0x00);

  // Configure RX buffer - this is where received data will be stored
  //  nrf_spis_rx_buffer_set(NRF_SPIS1, frameBuffer, 1024);
  //  nrf_spis_rx_buffer_set(NRF_SPIS1, frameBuffer, 0);
  //    nrf_spis_rx_list_enable(NRF_SPIS1);




  // Since we're receive-only, set TX buffer to NULL with 0 length
  //  nrf_spis_tx_buffer_set(NRF_SPIS1, NULL, 0);


  // Configure RX Array List
  // PTR points to first segment
  nrf_spis_rx_buffer_set(NRF_SPIS1, (uint8_t*)&rx_segments[0], SEGMENT_SIZE);

  // Enable Array List mode for RX
  nrf_spis_rx_list_enable(NRF_SPIS1);

  // Setup minimal TX (required by hardware)
  nrf_spis_tx_buffer_set(NRF_SPIS1, dummy_tx_buffer, sizeof(dummy_tx_buffer));



  // Clear events
  nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_END);
  nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ENDRX);
  nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);

  // Enable shortcut for automatic semaphore handover after transaction
  nrf_spis_shorts_enable(NRF_SPIS1, NRF_SPIS_SHORT_END_ACQUIRE);



  // Enable interrupts
  nrf_spis_int_enable(NRF_SPIS1, NRF_SPIS_INT_END_MASK | NRF_SPIS_INT_ACQUIRED_MASK | NRF_SPIS_INT_ENDRX_MASK);

  // Enable SPIS interrupt in NVIC
  NVIC_SetPriority(SPIS1_IRQn, 3);
  NVIC_ClearPendingIRQ(SPIS1_IRQn);
  NVIC_EnableIRQ(SPIS1_IRQn);

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
    nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ENDRX);

    // With END_ACQUIRE shortcut enabled, semaphore was automatically acquired
    // Check if we also got ACQUIRED event
    if (nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED))
    {
      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);
    }

    // Check how many bytes were received
    uint32_t bytes_received = nrf_spis_rx_amount_get(NRF_SPIS1);

    // Check if we received the full frame
//    if (bytes_received == IMAGE_SIZE)
//    {
//      // Full frame received
//      return true;
//    }

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
  if (nrf_spis_semaphore_status_get(NRF_SPIS1) == NRF_SPIS_SEMSTAT_CPU || nrf_spis_semaphore_status_get(NRF_SPIS1) == NRF_SPIS_SEMSTAT_FREE)
  {
    nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_END);
    nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ENDRX);
    nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);

    // Reset RX buffer for next frame
//    nrf_spis_rx_buffer_set(NRF_SPIS1, frameBuffer, 1024);




  // Configure RX Array List
  // PTR points to first segment
  nrf_spis_rx_buffer_set(NRF_SPIS1, (uint8_t*)&rx_segments[0], SEGMENT_SIZE);

  // Enable Array List mode for RX
  nrf_spis_rx_list_enable(NRF_SPIS1);

  // Setup minimal TX (required by hardware)
  nrf_spis_tx_buffer_set(NRF_SPIS1, dummy_tx_buffer, sizeof(dummy_tx_buffer));



    // Release semaphore for SPI slave
    nrf_spis_task_trigger(NRF_SPIS1, NRF_SPIS_TASK_RELEASE);

    // get single frame
    writeReg16_8(0x3020, 0x01); // Set frame count to 1
    writeReg16_8(0x0100, 0x03); // Stream N frames mode
  } else {
    Serial.println("cant prepare because task still in process");
    Serial.print(nrf_spis_semaphore_status_get(NRF_SPIS1));
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

  //  pinMode(PIN_CAM_HREF, INPUT);

  configure_8mhz_square_wave_p1_11();

  //  pinMode(2, INPUT_PULLDOWN);
  //  pinMode(7, OUTPUT);
  //  attachInterrupt(2, digital_callback,  CHANGE);
  //  configure_gpiote_inverter_sdk();


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


  // Green LED indicates ready
  digitalWrite(LED_GREEN, LOW);


  // Start continuous streaming mode
  //  writeReg16_8(0x0100, 0x01); // Set to streaming mode

  // get single frame
  writeReg16_8(0x3020, 0x01); // Set frame count to 1
  writeReg16_8(0x0100, 0x03); // Stream N frames mode

  Serial.println("Setup complete! Press any key to capture a frame...");


  spis_prepare_next_frame();


}



void printFrameStats() {
int c = 0;
for (int i = 0; i < NUM_RX_SEGMENTS; i++) {
  if(rx_segments[i].data[0]){
    c++;
  }
}
Serial.print("number of nonzero segments:");
  Serial.println(c);
//  for (int i = 0; i < IMAGE_SIZE; i++) {
//    if (i % (IMAGE_WIDTH + 2) == 0) {
//      Serial.println();
//    }
//
//    if (frameBuffer[i] < 16) Serial.print("0");
//    Serial.print(frameBuffer[i], HEX);
//    Serial.print(" ");
//
//  }
//  Serial.println();


}

volatile uint32_t numTx = 0;  // Current position in frame buffer
volatile uint32_t numOverflow = 0;  // Current position in frame buffer

extern "C"
{
  void SPIS1_IRQHandler(void)
  {

    if (nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_ENDRX))
    {
      //      if (!nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_END)) {
      //
      //      }

      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ENDRX);
      //      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_END);
      //      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);

      nrf_spis_task_trigger(NRF_SPIS1, NRF_SPIS_TASK_ACQUIRE);
      // Wait briefly for semaphore acquisition
      while (nrf_spis_semaphore_status_get(NRF_SPIS1) != NRF_SPIS_SEMSTAT_CPU) {
        // Small delay to ensure semaphore is acquired
        __NOP();
      }
      numOverflow++;

      
  // Configure RX Array List
  // PTR points to first segment
  nrf_spis_rx_buffer_set(NRF_SPIS1, (uint8_t*)&rx_segments[0], SEGMENT_SIZE);

  // Enable Array List mode for RX
  nrf_spis_rx_list_enable(NRF_SPIS1);

  // Setup minimal TX (required by hardware)
  nrf_spis_tx_buffer_set(NRF_SPIS1, dummy_tx_buffer, sizeof(dummy_tx_buffer));


//      nrf_spis_rx_buffer_set(NRF_SPIS1, frameBuffer, 1024);
      // set it to reset it
      if (nrf_spis_status_get(NRF_SPIS1) & NRF_SPIS_STATUS_OVERFLOW) {
        NRF_SPIS1->STATUS = NRF_SPIS_STATUS_OVERFLOW;
      }
      nrf_spis_task_trigger(NRF_SPIS1, NRF_SPIS_TASK_RELEASE);
    }

    // Check if END event occurred (transaction completed)
    if (nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_END))
    {
      // Clear the event
      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_END);
      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);

      //      bufferPosition += IMAGE_WIDTH;
      //      if (bufferPosition >= IMAGE_SIZE) {
      //        bufferPosition = 0;
      //      }
      //      nrf_spis_rx_buffer_set(NRF_SPIS1, &frameBuffer[bufferPosition], IMAGE_WIDTH);
      numTx++;


      nrf_spis_task_trigger(NRF_SPIS1, NRF_SPIS_TASK_RELEASE);
    }

    // Clear any other events that might be pending
    if (nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED))
    {
      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);
    }
  }
}


void loop() {

  Serial.print("TX: ");
  Serial.println(numTx);
  numTx = 0;

  Serial.print("numOver: ");
  Serial.println(numOverflow);
  numOverflow = 0;


  if (nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_ENDRX))
  {
    Serial.println("OVERFLOW");
  }
  //  uint32_t vsync_state = nrf_gpio_pin_read(PIN_VSYNC);
  //  if (vsync_state) {
  //    nrf_gpio_pin_clear(PIN_INV_VSYNC);  // VSYNC high -> INV_VSYNC low
  //  } else {
  //    nrf_gpio_pin_set(PIN_INV_VSYNC);    // VSYNC low -> INV_VSYNC high
  //  }
  //
  Serial.print("Bytes received: ");
  Serial.println(spis_get_received_bytes());


  printFrameStats();
  
  if (spis_frame_ready())
  {

    printFrameStats();
    // Process frameBuffer here
    // All IMAGE_SIZE bytes have been received

    // Do something with the image data...

    // Prepare for next frame


  }

  spis_prepare_next_frame();

  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
}
