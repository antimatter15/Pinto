#include <Adafruit_TinyUSB.h>
#include "nrf.h"
#include "nrf_spis.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_timer.h"
#include "nrf_twim.h"
#include "arm_math.h"
#include <bluefruit.h>
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include <Wire.h>

// IMU I2C address
#define LSM6DS3_I2C_ADDR 0x6A

// LSM6DS3 register addresses
#define LSM6DS3_WHO_AM_I_REG    0x0F
#define LSM6DS3_CTRL1_XL        0x10
#define LSM6DS3_CTRL2_G         0x11
#define LSM6DS3_CTRL3_C         0x12
#define LSM6DS3_CTRL4_C         0x13
#define LSM6DS3_TAP_CFG1        0x58
#define LSM6DS3_TAP_THS_6D      0x59
#define LSM6DS3_INT_DUR2        0x5A
#define LSM6DS3_WAKE_UP_THS     0x5B
#define LSM6DS3_MD1_CFG         0x5E
#define LSM6DS3_TAP_SRC         0x1C
#define LSM6DS3_FUNC_CFG_ACCESS 0x01
#define LSM6DS3_CTRL10_C        0x19

// WHO_AM_I expected value
#define LSM6DS3_WHO_AM_I_VALUE  0x69

// Interrupt pin configuration  
#define IMU_INT_PIN 1  // P0.01

// Global state
volatile bool tap_detected = false;


#define DEVICE_NAME   "XIAO_nRF52840_CAM"

#define TJE_PACKED_IMPLEMENTATION
#include "tiny_jpeg_packed.h"

// Convert Arduino pins to nRF pins
#define NRF_PIN_CAM_SDA    NRF_GPIO_PIN_MAP(1, 14)   // D9: P1.14
#define NRF_PIN_CAM_SCL    NRF_GPIO_PIN_MAP(1, 15)   // D10: P1.15

// IMU I2C pins (avoid conflict with SPIS pins)
#define NRF_PIN_IMU_SDA    NRF_GPIO_PIN_MAP(0, 6)    // D4 alternative: P0.06  
#define NRF_PIN_IMU_SCL    NRF_GPIO_PIN_MAP(0, 27)   // D5 alternative: P0.27

#define SPIS_SCK_PIN        NRF_GPIO_PIN_MAP(0, 4)  // D4: P0.04
#define SPIS_MOSI_PIN       NRF_GPIO_PIN_MAP(0, 5)  // D5: P0.05
#define SPIS_CS_PIN         NRF_GPIO_PIN_MAP(0, 29)  // D3: P0.29

#define CAMERA_ENABLE_PIN 6

// TWI instance pointer
#define TWI_INSTANCE        NRF_TWIM0
#define IMU_TWI_INSTANCE    NRF_TWIM1


#define SPIS1_IRQn SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQn
#define SPIS1_IRQHandler SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler


#if defined(CUSTOM_CS) && defined(CUSTOM_SPI)
Adafruit_FlashTransport_SPI flashTransport(CUSTOM_CS, CUSTOM_SPI);

#elif defined(ARDUINO_ARCH_ESP32)
// ESP32 use same flash device that store code.
// Therefore there is no need to specify the SPI and SS
Adafruit_FlashTransport_ESP32 flashTransport;

#else
// On-board external flash (QSPI or SPI) macros should already
// defined in your board variant if supported
// - EXTERNAL_FLASH_USE_QSPI
// - EXTERNAL_FLASH_USE_CS/EXTERNAL_FLASH_USE_SPI
#if defined(EXTERNAL_FLASH_USE_QSPI)
Adafruit_FlashTransport_QSPI flashTransport;

#elif defined(EXTERNAL_FLASH_USE_SPI)
Adafruit_FlashTransport_SPI flashTransport(EXTERNAL_FLASH_USE_CS, EXTERNAL_FLASH_USE_SPI);

#else
#error No QSPI/SPI flash are defined on your board variant.h !
#endif
#endif
Adafruit_SPIFlash flash(&flashTransport);


/*  If you want to use a specific flash device, for example for a custom built board, first look for it in Adafruit_SPIFlash\src\flash_devices.h
    If it isn't in there you need to create your own definition like the W25Q80DLX_EXAMPLE example below.
    These definitions need to be edited to match information on the data sheet of the flash device that you want to use.
    If you are not sure what the manufacture ID, memory type and capacity values should be, try running the sketch anyway and look at the serial output
    The flash device will report these values to you as a single hexadecimal value (the JDEC ID)
    For example, the first device on the list - the W25Q80DLX - will report its JDEC ID as 0xef4014, which is made of these three values:
    manufacturer_id = 0xef
    memory_type     = 0x40
    capacity        = 0x14
    With this macro properly defined you can then create an array of device definitions as shown below, this can include any from the list of devices in flash_devices.h, and any you define yourself here
    You need to update the variable on line 71 to reflect the number of items in the array
    You also need to uncomment line 84 and comment out line 81 so this array will be passed to the flash memory driver.
*/
//Example of a user defined flash memory device:
//#define W25Q80DLX_EXAMPLE                                                               \
//  {                                                                            \
//    .total_size = (1 << 20), /* 1 MiB */                                       \
//        .start_up_time_us = 5000, .manufacturer_id = 0xef,                     \
//    .memory_type = 0x40, .capacity = 0x14, .max_clock_speed_mhz = 80,         \
//    .quad_enable_bit_mask = 0x02, .has_sector_protection = false,              \
//    .supports_fast_read = true, .supports_qspi = true,                         \
//    .supports_qspi_writes = false, .write_status_register_split = false,       \
//    .single_status_byte = false, .is_fram = false,                             \
//  }
#define P25Q16H                                                                \
  {                                                                            \
    .total_size = (1 << 21), /* 2 MiB */                                       \
                  .start_up_time_us = 5000, .manufacturer_id = 0x85,                     \
                                      .memory_type = 0x60, .capacity = 0x15, .max_clock_speed_mhz = 104,         \
                                          .quad_enable_bit_mask = 0x02, .has_sector_protection = false,              \
                                              .supports_fast_read = true, .supports_qspi = true,                         \
                                                  .supports_qspi_writes = true, .write_status_register_split = false,        \
                                                      .single_status_byte = false, .is_fram = false,                             \
  }

/*
   Create an array of data structures and fill it with the settings we defined above.
   We are using two devices, but more can be added if you want.
*/
//static const SPIFlash_Device_t my_flash_devices[] = {
//    W25Q80DLX_EXAMPLE,
//};

static const SPIFlash_Device_t my_flash_devices[] = {
  P25Q16H,
};
/*
   Specify the number of different devices that are listed in the array we just created. If you add more devices to the array, update this value to match.
*/
const int flashDevices = 1;


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

// IMU TWI functions
bool imu_twi_write(uint8_t reg, uint8_t value) {
  uint8_t data[2] = {reg, value};
  
  // Set address and configure TX
  nrf_twim_address_set(IMU_TWI_INSTANCE, LSM6DS3_I2C_ADDR);
  nrf_twim_tx_buffer_set(IMU_TWI_INSTANCE, data, 2);

  // Clear events
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR);
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED);
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_TXSTARTED);
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_LASTTX);

  // Start transmission
  nrf_twim_task_trigger(IMU_TWI_INSTANCE, NRF_TWIM_TASK_RESUME);
  nrf_twim_task_trigger(IMU_TWI_INSTANCE, NRF_TWIM_TASK_STARTTX);

  // Wait for TX to start
  while (!nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_TXSTARTED) &&
         !nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR));

  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_TXSTARTED);

  // Wait for last TX
  while (!nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_LASTTX) &&
         !nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR));
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_LASTTX);

  // Stop transaction
  nrf_twim_task_trigger(IMU_TWI_INSTANCE, NRF_TWIM_TASK_STOP);
  while (!nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED));
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED);

  // Check for errors
  if (nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR)) {
    nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR);
    nrf_twim_errorsrc_get_and_clear(IMU_TWI_INSTANCE);
    return false;
  }

  return true;
}

bool imu_twi_read(uint8_t reg, uint8_t* value) {
  uint8_t tx_data = reg;
  
  // Write register address
  nrf_twim_address_set(IMU_TWI_INSTANCE, LSM6DS3_I2C_ADDR);
  nrf_twim_tx_buffer_set(IMU_TWI_INSTANCE, &tx_data, 1);

  // Clear events
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR);
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED);
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_TXSTARTED);
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_LASTTX);

  // Start transmission
  nrf_twim_task_trigger(IMU_TWI_INSTANCE, NRF_TWIM_TASK_RESUME);
  nrf_twim_task_trigger(IMU_TWI_INSTANCE, NRF_TWIM_TASK_STARTTX);

  // Wait for TX to complete
  while (!nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_LASTTX) &&
         !nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR));
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_LASTTX);

  if (nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR)) {
    nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR);
    nrf_twim_errorsrc_get_and_clear(IMU_TWI_INSTANCE);
    return false;
  }

  // Now read the data
  nrf_twim_rx_buffer_set(IMU_TWI_INSTANCE, value, 1);

  // Clear events
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR);
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED);
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_RXSTARTED);
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_LASTRX);

  // Start reception
  nrf_twim_task_trigger(IMU_TWI_INSTANCE, NRF_TWIM_TASK_STARTRX);

  // Wait for RX to complete with timeout
  uint32_t timeout = 1000;  // 1000 iterations timeout
  while (!nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_LASTRX) &&
         !nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR) && 
         --timeout > 0);
  
  if (timeout == 0) {
    Serial.println("I2C read timeout!");
    return false;
  }
  
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_LASTRX);

  // Stop transaction
  nrf_twim_task_trigger(IMU_TWI_INSTANCE, NRF_TWIM_TASK_STOP);
  while (!nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED));
  nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_STOPPED);

  // Check for errors
  if (nrf_twim_event_check(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR)) {
    nrf_twim_event_clear(IMU_TWI_INSTANCE, NRF_TWIM_EVENT_ERROR);
    nrf_twim_errorsrc_get_and_clear(IMU_TWI_INSTANCE);
    return false;
  }

  return true;
}

void imu_twi_init(void) {
  // Configure SDA pin
  *pincfg_reg(NRF_PIN_IMU_SDA) = ((uint32_t)GPIO_PIN_CNF_DIR_Input         << GPIO_PIN_CNF_DIR_Pos)
                                | ((uint32_t)GPIO_PIN_CNF_INPUT_Connect    << GPIO_PIN_CNF_INPUT_Pos)
                                | ((uint32_t)GPIO_PIN_CNF_PULL_Pullup      << GPIO_PIN_CNF_PULL_Pos)
                                | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0D1       << GPIO_PIN_CNF_DRIVE_Pos)
                                | ((uint32_t)GPIO_PIN_CNF_SENSE_Disabled   << GPIO_PIN_CNF_SENSE_Pos);

  // Configure SCL pin
  *pincfg_reg(NRF_PIN_IMU_SCL) = ((uint32_t)GPIO_PIN_CNF_DIR_Input         << GPIO_PIN_CNF_DIR_Pos)
                                | ((uint32_t)GPIO_PIN_CNF_INPUT_Connect    << GPIO_PIN_CNF_INPUT_Pos)
                                | ((uint32_t)GPIO_PIN_CNF_PULL_Pullup      << GPIO_PIN_CNF_PULL_Pos)
                                | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0D1       << GPIO_PIN_CNF_DRIVE_Pos)
                                | ((uint32_t)GPIO_PIN_CNF_SENSE_Disabled   << GPIO_PIN_CNF_SENSE_Pos);

  // Configure TWIM peripheral for IMU
  nrf_twim_pins_set(IMU_TWI_INSTANCE, NRF_PIN_IMU_SCL, NRF_PIN_IMU_SDA);
  nrf_twim_frequency_set(IMU_TWI_INSTANCE, NRF_TWIM_FREQ_100K);
  nrf_twim_enable(IMU_TWI_INSTANCE);
}

bool init_imu(void) {
  uint8_t who_am_i;
  
  Serial.println("Initializing IMU TWI...");
  Serial.flush();
  imu_twi_init();
  Serial.println("IMU TWI initialized");
  Serial.flush();
  
  // Read WHO_AM_I register
  Serial.println("Reading WHO_AM_I...");
  Serial.flush();
  if (!imu_twi_read(LSM6DS3_WHO_AM_I_REG, &who_am_i)) {
    Serial.println("Failed to read WHO_AM_I");
    return false;
  }
  Serial.println("WHO_AM_I read successful");
  Serial.flush();
  
  if (who_am_i != LSM6DS3_WHO_AM_I_VALUE && who_am_i != 0x6A) {  // 0x6A is LSM6DS3-C
    Serial.printf("Invalid WHO_AM_I: 0x%02X (expected 0x%02X or 0x6A)\n", who_am_i, LSM6DS3_WHO_AM_I_VALUE);
    return false;
  }
  
  Serial.printf("LSM6DS3 detected! WHO_AM_I: 0x%02X\n", who_am_i);
  
  // Software reset first
  if (!imu_twi_write(LSM6DS3_CTRL3_C, 0x01)) {
    Serial.println("Failed to reset IMU");
    return false;
  }
  delay(100);  // Wait for reset
  
  // Configure accelerometer for tap detection
  // Set accelerometer to 416 Hz, ±2g, bandwidth 200Hz
  if (!imu_twi_write(LSM6DS3_CTRL1_XL, 0x62)) {  // ODR_XL = 416Hz, FS_XL = ±2g, BW_XL = 200Hz
    Serial.println("Failed to configure accelerometer");
    return false;
  }
  
  // Enable block data update and auto-increment
  if (!imu_twi_write(LSM6DS3_CTRL3_C, 0x44)) {  // BDU=1, IF_INC=1
    Serial.println("Failed to configure CTRL3_C");
    return false;
  }
  
  // Enable embedded functions
  if (!imu_twi_write(LSM6DS3_CTRL10_C, 0x04)) {  // FUNC_EN = 1
    Serial.println("Failed to enable embedded functions");
    return false;
  }
  
  // Configure tap detection - start with more sensitive settings
  // TAP_CFG1: Enable tap detection on X, Y, Z axes with timer
  if (!imu_twi_write(LSM6DS3_TAP_CFG1, 0x8E)) {  // TIMER_EN=1, TAP_X_EN=1, TAP_Y_EN=1, TAP_Z_EN=1
    Serial.println("Failed to configure tap detection");
    return false;
  }
  
  // TAP_THS_6D: Set tap threshold (lower = more sensitive)
  if (!imu_twi_write(LSM6DS3_TAP_THS_6D, 0x08)) {  // TAP_THS = 8 (more sensitive)
    Serial.println("Failed to set tap threshold");
    return false;
  }
  
  // INT_DUR2: Set shock and quiet time (lower values)
  if (!imu_twi_write(LSM6DS3_INT_DUR2, 0x32)) {  // SHOCK=2, QUIET=3, DUR=3
    Serial.println("Failed to set tap timing");
    return false;
  }
  
  // WAKE_UP_THS: Configure for single tap (not double)
  if (!imu_twi_write(LSM6DS3_WAKE_UP_THS, 0x80)) {  // SINGLE_DOUBLE_TAP = 1 (single tap)
    Serial.println("Failed to configure single tap");
    return false;
  }
  
  // MD1_CFG: Route tap interrupt to INT1 pin
  if (!imu_twi_write(LSM6DS3_MD1_CFG, 0x08)) {  // INT1_TAP = 1 (general tap, not just single)
    Serial.println("Failed to route tap interrupt");
    return false;
  }
  
  Serial.println("IMU tap detection configured!");
  
  // Test by reading back some registers
  uint8_t test_val;
  imu_twi_read(LSM6DS3_CTRL1_XL, &test_val);
  Serial.printf("CTRL1_XL readback: 0x%02X\n", test_val);
  imu_twi_read(LSM6DS3_TAP_CFG1, &test_val);
  Serial.printf("TAP_CFG1 readback: 0x%02X\n", test_val);
  imu_twi_read(LSM6DS3_MD1_CFG, &test_val);
  Serial.printf("MD1_CFG readback: 0x%02X\n", test_val);
  
  return true;
}

bool check_tap_interrupt(void) {
  uint8_t tap_src;
  
  if (!imu_twi_read(LSM6DS3_TAP_SRC, &tap_src)) {
    return false;
  }
  
  // Check if single tap was detected (bit 5)
  return (tap_src & 0x20) != 0;
}

void imu_interrupt_handler() {
  tap_detected = true;
}

void setup_imu_interrupt(void) {
  // Configure INT pin as input with pullup (IMU INT is active high)
  pinMode(IMU_INT_PIN, INPUT_PULLDOWN);
  
  // Attach interrupt for rising edge (tap detection)
  attachInterrupt(digitalPinToInterrupt(IMU_INT_PIN), imu_interrupt_handler, RISING);
}


// Camera I2C address
#define HM01B0_I2C_ADDR 0x24

// Image dimensions for 160x120 mode
#define IMAGE_WIDTH  328
#define IMAGE_HEIGHT 328

#define IMAGE_PACKED_WIDTH  (IMAGE_WIDTH * 6 / 8)
#define IMAGE_SIZE   (IMAGE_PACKED_WIDTH * IMAGE_HEIGHT)

// Frame buffer
volatile uint32_t bufferPosition = 0;
uint8_t frameBuffer[IMAGE_SIZE];
uint8_t rowBuffer[IMAGE_WIDTH];

// JPEG encoding variables
uint32_t jpeg_size = 0;

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

  {0x3022, 0x02}, // Advance VSYNC by 1 row
  {0x3023, 0x02}, // Advance HSYNC (NEW)

  //  {0x1012, 0x01}, // Sync enable
  //  {0x1012, 0x02}, // HSYNC enable
  {0x1012, 0x07}, // VSync/HSYNC/PixelShift enable

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

  // Frame timing
  {0x0340, 0x01}, // Frame length
  {0x0341, 0x7A},
  {0x0342, 0x02}, // Line length
  {0x0343, 0x00},

  //  {0x0340, 0x02}, // Frame length
  //  {0x0341, 0x7A},
  //  {0x0342, 0x02}, // Line length
  //  {0x0343, 0x77},

  // Resolution settings for 324x324
  {0x3010, 0x00}, // full frame
  //  {0x0383, 0x03}, // X subsample
  //  {0x0387, 0x03}, // Y subsample
  //  {0x0390, 0x03}, // Binning mode

  // Bit width and output format
  //  {0x3011, 0x70}, // 8 bit
  {0x3011, 0x71}, // 6 bit enable (bitstream is still bytewise but there's more zeros)
  {0x3060, 0x30}, // Gated clock, MSB first
  //  {0x3060, 0x10}, // Non-Gated clock, MSB first

  // Mirror/flip
  {0x0101, 0x01},

  // Update CMU
  {0x0104, 0x01}
};

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
  nrf_gpio_cfg_input(SPIS_SCK_PIN, NRF_GPIO_PIN_PULLDOWN);
  nrf_gpio_cfg_input(SPIS_MOSI_PIN, NRF_GPIO_PIN_NOPULL);
  nrf_gpio_cfg_input(SPIS_CS_PIN, NRF_GPIO_PIN_PULLUP);  // CS active low by default

  // Set pin assignments
  nrf_spis_pins_set(NRF_SPIS1, SPIS_SCK_PIN, SPIS_MOSI_PIN,
                    NRF_SPIS_PIN_NOT_CONNECTED, SPIS_CS_PIN);

  // Configure SPI mode 0, MSB first
  // NRF_SPIS_MODE_3 = 3 bit offset
  // NRF_SPIS_MODE_2 = 2 bit offset
  nrf_spis_configure(NRF_SPIS1, NRF_SPIS_MODE_0, NRF_SPIS_BIT_ORDER_MSB_FIRST);

  // Set default character (used when TX buffer is empty)
  nrf_spis_def_set(NRF_SPIS1, 0x00);

  // Configure RX buffer - this is where received data will be stored
  bufferPosition = 0;
  nrf_spis_rx_buffer_set(NRF_SPIS1, rowBuffer, IMAGE_WIDTH);
  //  nrf_spis_rx_buffer_set(NRF_SPIS1, frameBuffer, IMAGE_SIZE);

  // Since we're receive-only, set TX buffer to NULL with 0 length
  nrf_spis_tx_buffer_set(NRF_SPIS1, NULL, 0);

  // Clear events
  nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_END);
  nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);

  // Enable shortcut for automatic semaphore handover after transaction
  nrf_spis_shorts_enable(NRF_SPIS1, NRF_SPIS_SHORT_END_ACQUIRE);

  // Enable interrupts
  nrf_spis_int_enable(NRF_SPIS1, NRF_SPIS_INT_END_MASK);

  // Enable SPIS interrupt in NVIC
  NVIC_SetPriority(SPIS1_IRQn, 2);
  NVIC_ClearPendingIRQ(SPIS1_IRQn);
  NVIC_EnableIRQ(SPIS1_IRQn);


  // Enable SPIS peripheral
  nrf_spis_enable(NRF_SPIS1);

  // The semaphore is initially with CPU after enabling
  // Trigger RELEASE to make it available for SPI slave
  nrf_spis_task_trigger(NRF_SPIS1, NRF_SPIS_TASK_RELEASE);
}

// Get number of bytes received in last transaction
uint32_t spis_get_received_bytes(void)
{
  return nrf_spis_rx_amount_get(NRF_SPIS1);
}

// Check if a transaction is in progress
bool spis_busy(void)
{
  return nrf_spis_semaphore_status_get(NRF_SPIS1) == NRF_SPIS_SEMSTAT_SPIS;
}


void twi_stop(void) {
  nrf_twim_disable(TWI_INSTANCE);
}

void spis_stop(void) {
  // Disable SPIS peripheral
  nrf_spis_disable(NRF_SPIS1);

  // Disable interrupts
  NVIC_DisableIRQ(SPIS1_IRQn);
  nrf_spis_int_disable(NRF_SPIS1, NRF_SPIS_INT_END_MASK);
}


BLEUart bleuart;
int packet_counter = 0;

void setup() {

  sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);

  Serial.begin(115200);
  while (!Serial && millis() < 5000); // Wait for serial or timeout

  Serial.println("\n=== HM01B0 Camera Test ===");
  Serial.println("Serial communication working!");
  Serial.flush();
  
  
  Serial.println("About to initialize IMU...");
  Serial.flush();
  
  // Initialize IMU and tap detection
  if (init_imu()) {
    Serial.println("IMU init successful, setting up interrupt...");
    Serial.flush();
    setup_imu_interrupt();
    Serial.println("IMU initialization complete!");
  } else {
    Serial.println("IMU initialization failed!");
  }
  
  Serial.println("Continuing with setup...");
  Serial.flush();


  flash.begin(my_flash_devices, flashDevices);
  if (flashTransport.runCommand(0xB9) == false) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    while (1) {
      yield();
    }
  }
  flash.end();

  // Initialize LEDs
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  pinMode(CAMERA_ENABLE_PIN, OUTPUT);




  // Start continuous streaming mode
  //  writeReg16_8(0x0100, 0x01); // Set to streaming mode


  //  Bluefruit.autoConnLed(false);

  // Disable central mode completely (peripheral only)
  Bluefruit.configCentralBandwidth(0);



  // Initialize BLE
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(DEVICE_NAME);

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  Bluefruit.Periph.setConnInterval(6, 12);

  bleuart.begin();
  bleuart.setRxCallback(bleuart_rx_callback);

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);


  Serial.println("Setup complete! Press any key to capture a frame...");


}

uint8_t unsent_buffer[1024];  // Buffer for unsent data
int unsent_amount = 0;
int chunk_size = 200;

// JPEG write callback function
void jpeg_write_callback(void* context, void* data, int size) {
  jpeg_size += size;

  uint8_t* src = (uint8_t*)data;
  int remaining = size;

  while (remaining > 0) {
    // Calculate how much we can add to current buffer
    int space_available = chunk_size - unsent_amount;
    int to_copy = min(remaining, space_available);

    // Copy data to buffer
    memcpy(unsent_buffer + unsent_amount, src, to_copy);
    unsent_amount += to_copy;
    src += to_copy;
    remaining -= to_copy;

    // If buffer is full, send it
    if (unsent_amount >= chunk_size) {
      bleuart.write(unsent_buffer, chunk_size);
      unsent_amount = 0;
      delay(5); // Small delay between chunks
    }
  }
}

// Function to flush any remaining data in the buffer
void flush_jpeg() {
  if (unsent_amount > 0) {
    bleuart.write(unsent_buffer, unsent_amount);
    unsent_amount = 0;
    delay(5); // Small delay
  }
}



uint8_t integ_h = 0x01;  // Integration high byte
uint8_t integ_l = 0x08;  // Integration low byte
uint8_t again_reg = 0x00;
uint8_t dgain_h = 0x01;
uint8_t dgain_l = 0x00;


void send_jpeg() {

  // Green LED indicates camera taking
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(CAMERA_ENABLE_PIN, LOW);
  delay(100);


  twi_init();

  // Initialize camera
  if (!initCamera()) {
    Serial.println("Camera initialization failed!");
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BUILTIN, LOW);
    delay(5000);
    return;
  }

  writeReg16_8(0x0100, 0x01); // Continuous streaming
  delay(1000); // Allow convergence time
  writeReg16_8(0x0100, 0x00); // MODE_SELECT = 000 (Standby)
  delay(200); // Finish frame i guess

  spis_init();
  //
  //  writeReg16_8(0x2100, 0); // AE off
  //  writeReg16_8(0x0202, integ_h);  // Integration high byte
  //  writeReg16_8(0x0203, integ_l);  // Integration low byte
  //  writeReg16_8(0x0205, again_reg);
  //  writeReg16_8(0x020E, dgain_h);
  //  writeReg16_8(0x020F, dgain_l);
  //

  //  Serial.printf("Integration Time %02X %02X, Analog Gain %02X, Digital Gain %02X %02X\n", integ_h, integ_l, again_reg, dgain_h, dgain_l);

  writeReg16_8(0x3020, 0x01); // Set frame count to 1
  writeReg16_8(0x0100, 0x03); // Stream N frames mode
  bufferPosition = 0;

  uint32_t capture_start = millis();
  while (bufferPosition < IMAGE_PACKED_WIDTH * 324 && millis() - capture_start < 1000) {
    delay(10);
  }

  Serial.print("Bytes received: ");
  Serial.println(spis_get_received_bytes());
  Serial.print("Buffer position: ");
  Serial.println(bufferPosition);

  digitalWrite(LED_GREEN, HIGH);
  spis_stop();


  twi_stop();
  digitalWrite(CAMERA_ENABLE_PIN, HIGH);

  //  writeReg16_8(0x2100, 0x01); // AE on
  //  writeReg16_8(0x0100, 0x01); // Continuous streaming

  if (bufferPosition > 0) {
    digitalWrite(LED_BLUE, LOW);
    jpeg_size = 0;
    Serial.println("Encoding JPEG");
    uint32_t start_time = millis();
    // Encode packed frame data to JPEG with quality 70
    int result = tje_encode_packed_with_func(
                   jpeg_write_callback,
                   NULL,
                   70,  // Quality
                   IMAGE_WIDTH,
                   IMAGE_HEIGHT,
                   frameBuffer
                 );
    flush_jpeg();
    Serial.println("Done Encoding JPEG");
    uint32_t send_time = millis() - start_time;
    Serial.printf("Sent JPEG #%d (%d bytes) in %d ms\n", packet_counter, jpeg_size, send_time);
    digitalWrite(LED_BLUE, HIGH);
  }

  //  integ_h = readReg16_8(0x0202);  // Integration high byte
  //  integ_l = readReg16_8(0x0203);  // Integration low byte
  //  again_reg = readReg16_8(0x0205);
  //  dgain_h = readReg16_8(0x020E);
  //  dgain_l = readReg16_8(0x020F);
  //
  //  Serial.printf("Integration Time %02X %02X, Analog Gain %02X, Digital Gain %02X %02X\n", integ_h, integ_l, again_reg, dgain_h, dgain_l);
  //
}

void connect_callback(uint16_t conn_handle)
{
  Serial.println("Device Connected!");
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  Serial.printf("Connection Info: PHY = %d Mbps, Conn Interval = %.2f ms, Data Length = %d, MTU = %d\n",
                conn->getPHY(), conn->getConnectionInterval() * 1.25f, conn->getDataLength(), conn->getMtu());

  uint32_t connect_time = millis();
  while (millis() - connect_time < 3000 && conn->getMtu() < 30) {
    delay(50);
  };
  Serial.printf("Waited for MTU increase %d\n", millis() - connect_time);

  //  delay(1000);

  Serial.printf("Connection Info: PHY = %d Mbps, Conn Interval = %.2f ms, Data Length = %d, MTU = %d\n",
                conn->getPHY(), conn->getConnectionInterval() * 1.25f, conn->getDataLength(), conn->getMtu());


  chunk_size = conn->getMtu() - 3; // theres 3 bytes of ATT overhead for uart
  send_jpeg();

}



void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
  Serial.println("Device Disconnected!");
  //  packet_counter = 0;
}


volatile uint32_t numTx = 0;  // Current position in frame buffer
// Optimized version processing 4 bytes at a time
void shift_quantize_pack(uint8_t* source, uint8_t* packed, int length, int shift_bits) {
  // Early exit for invalid shift
  if (shift_bits < 0 || shift_bits > 7) {
    shift_bits = 0;
  }

  // Precompute masks
  const uint32_t shift_mask = (1U << shift_bits) - 1;
  const int inv_shift = 8 - shift_bits;

  int src_idx = 0;
  int dst_idx = 0;
  uint32_t carry = 0;

  // Process 4 bytes at a time (4 bytes -> 3 packed bytes)
  // 4 * 8 bits = 32 bits input -> 4 * 6 bits = 24 bits = 3 bytes output
  while (src_idx + 3 < length) {
    uint32_t b0, b1, b2, b3;

    if (shift_bits) {
      // Apply shift with carry
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
      // No shift - just load
      b0 = source[src_idx];
      b1 = source[src_idx + 1];
      b2 = source[src_idx + 2];
      b3 = source[src_idx + 3];
    }

    // Quantize to 6 bits (keep upper 6 bits)
    b0 = (b0 >> 2) & 0x3F;
    b1 = (b1 >> 2) & 0x3F;
    b2 = (b2 >> 2) & 0x3F;
    b3 = (b3 >> 2) & 0x3F;

    // Pack 4x6 bits into 3 bytes
    packed[dst_idx]     = (b0 << 2) | (b1 >> 4);
    packed[dst_idx + 1] = (b1 << 4) | (b2 >> 2);
    packed[dst_idx + 2] = (b2 << 6) | b3;

    src_idx += 4;
    dst_idx += 3;
  }

  // Handle remaining bytes
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

      // Quantize and accumulate
      accumulator = (accumulator << 6) | ((value >> 2) & 0x3F);
      bits += 6;

      if (bits >= 8) {
        packed[dst_idx++] = (accumulator >> (bits - 8)) & 0xFF;
        bits -= 8;
      }
    }

    // Flush remaining bits
    if (bits > 0) {
      packed[dst_idx] = (accumulator << (8 - bits)) & 0xFF;
    }
  }
}

//int find_line_alignment_full_6b(uint8_t* line, int length) {
//  int count_2 = 0;  // Count for alignment 2 (bits 2-3 zero)
//  int count_3 = 0;  // Count for alignment 3 (bits 3-4 zero)
//  for (int i = 0; i < length; i++) {
//    uint8_t byte_val = line[i];
//    if ((byte_val & 0x0C) == 0) count_2++;
//    if ((byte_val & 0x18) == 0) count_3++;
//  }
//  return (count_3 >= count_2) ? 3 : 2;
//}


int find_line_alignment_full_6b(uint8_t* line, int length) {
    int pair_zero_counts[8] = {0};
    
    for (int i = 0; i < length; i++) {
        uint8_t byte_val = line[i];
        
        if ((byte_val & 0x03) == 0) pair_zero_counts[0]++;
        if ((byte_val & 0x06) == 0) pair_zero_counts[1]++;
        if ((byte_val & 0x0C) == 0) pair_zero_counts[2]++;
        if ((byte_val & 0x18) == 0) pair_zero_counts[3]++;
        if ((byte_val & 0x30) == 0) pair_zero_counts[4]++;
        if ((byte_val & 0x60) == 0) pair_zero_counts[5]++;
        if ((byte_val & 0xC0) == 0) pair_zero_counts[6]++;
        if ((byte_val & 0x81) == 0) pair_zero_counts[7]++;
    }
    
    int max_zeros = 0;
    int best_alignment = 0;
    
    for (int i = 2; i < 6; i++) {
        if (pair_zero_counts[i] >= max_zeros) {
            max_zeros = pair_zero_counts[i];
            best_alignment = i;
        }
    }
    
    return best_alignment;
}



extern "C"
{
  void SPIS1_IRQHandler(void)
  {

    // Check if END event occurred (transaction completed)
    if (nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_END))
    {
      // Clear the event
      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_END);
      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);

      if (bufferPosition + IMAGE_PACKED_WIDTH < IMAGE_SIZE) {

        nrf_spis_rx_buffer_set(NRF_SPIS1, rowBuffer, IMAGE_WIDTH);
        nrf_spis_task_trigger(NRF_SPIS1, NRF_SPIS_TASK_RELEASE);

        // By using HSYNC as our SPI CS, we have about 1us of dead time which
        // is also somewhat nondeterministic, so we might start reading with the
        // wrong bit alignment. The timing is tight enough that we know the offset
        // is either 2 or 3 bits.
        int bit_alignment = find_line_alignment_full_6b(rowBuffer, IMAGE_WIDTH);

        shift_quantize_pack(rowBuffer,
                            &frameBuffer[bufferPosition],
                            IMAGE_WIDTH,
                            bit_alignment);

        bufferPosition += IMAGE_PACKED_WIDTH;
      }
      numTx++;


    } else if (nrf_spis_event_check(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED))
    {
      nrf_spis_event_clear(NRF_SPIS1, NRF_SPIS_EVENT_ACQUIRED);
    }


  }
  
}


void bleuart_rx_callback(uint16_t conn_hdl)
{
  (void) conn_hdl;

  if (bleuart.available()) {
    uint8_t ch = bleuart.read();

    // If we receive 'P' command, take a picture
    if (ch == 'P' || ch == 'p') {
      Serial.println("Picture command received via BLE");
      packet_counter++;
      send_jpeg();
    }
  }

}



void loop() {
  // Check for tap detection
  if (tap_detected) {
    tap_detected = false;
    
    Serial.println("Interrupt detected! Checking tap source...");
    
    // Verify tap by reading tap source register
    if (check_tap_interrupt()) {
      Serial.println("Tap confirmed! Taking photo in 1 second...");
      
      // Wait 1 second
      delay(1000);
      
      // Take photo
      packet_counter++;
      send_jpeg();
      
      Serial.println("Photo captured!");
    } else {
      Serial.println("False interrupt - no tap detected in source register");
    }
  }
  
  // Also periodically check tap source register for debugging
  static uint32_t last_check = 0;
  if (millis() - last_check > 5000) {  // Every 5 seconds
    last_check = millis();
    Serial.println("Loop running...");
    Serial.flush();
    
    uint8_t tap_src;
    if (imu_twi_read(LSM6DS3_TAP_SRC, &tap_src)) {
      Serial.printf("TAP_SRC: 0x%02X\n", tap_src);
      if (tap_src != 0) {
        Serial.printf("TAP_SRC status: 0x%02X\n", tap_src);
      }
    } else {
      Serial.println("Failed to read TAP_SRC");
    }
    
    // Check if interrupt pin is high
    int pin_state = digitalRead(IMU_INT_PIN);
    Serial.printf("INT pin state: %d\n", pin_state);
    Serial.flush();
  }
  
  sd_app_evt_wait();
  delay(100);
}
