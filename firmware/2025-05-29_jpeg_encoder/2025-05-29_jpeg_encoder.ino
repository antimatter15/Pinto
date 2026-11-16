#include <Adafruit_TinyUSB.h>
#include "nrf.h"
#include "data.h"
#define TJE_PACKED_IMPLEMENTATION
#include "tiny_jpeg_packed.h"

// Image dimensions - full size
#define IMAGE_WIDTH  328
#define IMAGE_HEIGHT 328

#define IMAGE_PACKED_WIDTH  (IMAGE_WIDTH * 6 / 8)
#define IMAGE_SIZE   (IMAGE_PACKED_WIDTH * IMAGE_HEIGHT)




// JPEG encoding variables
uint8_t jpeg_buffer[16384];  // Reduced buffer size for JPEG data
uint32_t jpeg_size = 0;

// Base64 encoding table
const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


void setup() {
  // put your setup code here, to run once:
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
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);

  // Reset JPEG buffer
  jpeg_size = 0;

  int result = tje_encode_packed_with_func(
                 jpeg_write_callback,
                 NULL,
                 70,  // Quality
                 IMAGE_WIDTH,
                 IMAGE_HEIGHT,
                 packed_data
               );
               
  if (result && jpeg_size > 0) {
    // Calculate base64 output size (4/3 of input + padding)
    size_t base64_size = ((jpeg_size + 2) / 3) * 4 + 1;
    char* base64_output = (char*)malloc(base64_size);

    if (base64_output) {
      encode_base64(jpeg_buffer, jpeg_size, base64_output);

      // Print data URL
      Serial.print("data:image/jpeg;base64,");
      Serial.println(base64_output);

      free(base64_output);
    } else {
      Serial.println("ERROR: Failed to allocate base64 buffer");
    }
  } else {
    Serial.println("ERROR: JPEG encoding failed");
  }
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
}


// JPEG write callback function
void jpeg_write_callback(void* context, void* data, int size) {
  if (jpeg_size + size <= sizeof(jpeg_buffer)) {
    memcpy(jpeg_buffer + jpeg_size, data, size);
    //    Serial.println(size);
    jpeg_size += size;
  }
}

// Base64 encoding function
void encode_base64(const uint8_t* data, size_t len, char* output) {
  size_t i = 0, j = 0;
  for (i = 0; i < len - 2; i += 3) {
    uint32_t octet_a = data[i];
    uint32_t octet_b = data[i + 1];
    uint32_t octet_c = data[i + 2];
    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    output[j++] = base64_chars[(triple >> 3 * 6) & 0x3F];
    output[j++] = base64_chars[(triple >> 2 * 6) & 0x3F];
    output[j++] = base64_chars[(triple >> 1 * 6) & 0x3F];
    output[j++] = base64_chars[(triple >> 0 * 6) & 0x3F];
  }

  // Handle remaining bytes
  if (i < len) {
    uint32_t octet_a = data[i];
    uint32_t octet_b = (i + 1 < len) ? data[i + 1] : 0;
    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08);

    output[j++] = base64_chars[(triple >> 3 * 6) & 0x3F];
    output[j++] = base64_chars[(triple >> 2 * 6) & 0x3F];
    output[j++] = (i + 1 < len) ? base64_chars[(triple >> 1 * 6) & 0x3F] : '=';
    output[j++] = '=';
  }

  output[j] = '\0';
}
