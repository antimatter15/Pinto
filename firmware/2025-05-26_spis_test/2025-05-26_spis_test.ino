#include <Wire.h>
#include <Adafruit_TinyUSB.h>

#include "nrf.h"
#include "nrf_spis.h"
#include "nrfx_spis.h"

#define SPIS_INSTANCE 0
static const nrfx_spis_t spis = NRFX_SPIS_INSTANCE(SPIS_INSTANCE); /**< SPIS instance. */

 
#define SPIS_SCK_PIN   NRF_GPIO_PIN_MAP(1, 13)  // P1.13
#define SPIS_MOSI_PIN  NRF_GPIO_PIN_MAP(1, 14)  // P1.14
#define SPIS_CS_PIN    NRF_GPIO_PIN_MAP(0, 15)  // P0.15



static uint8_t frameBuffer[IMAGE_SIZE];  /**< Frame buffer for image data. */

static volatile bool spis_xfer_done; /**< Flag used to indicate that SPIS instance completed the transfer. */

void spis_event_handler(nrfx_spis_evt_t const* event, void* context) {
  if ((*event).evt_type == NRFX_SPIS_XFER_DONE)
  {
    spis_xfer_done = true;
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


  // Initialize I2C
  Wire.begin();
  Wire.setClock(100000); // 100kHz

  // Green LED indicates ready
  digitalWrite(LED_GREEN, LOW);
  Serial.println("Setup complete! Press any key to capture a frame...");
}

void loop() {
  digitalWrite(LED_BLUE, LOW);
  Serial.println("\nCapturing...");
  delay(100);
  digitalWrite(LED_BLUE, HIGH);
  delay(100);
}
