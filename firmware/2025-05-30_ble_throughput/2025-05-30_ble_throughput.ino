/*********************************************************************
 BLE Data Sender for Seeed XIAO nRF52840

 Sends 1KB of data periodically via BLE UART
*********************************************************************/

#include <bluefruit.h>

#define DEVICE_NAME   "XIAO_nRF52840"

BLEUart bleuart;

uint8_t data_buffer[1024];
uint32_t packet_counter = 0;
uint32_t last_send_time = 0;
const uint32_t SEND_INTERVAL = 2000;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE Data Sender");

  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(DEVICE_NAME);

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  Bluefruit.Periph.setConnInterval(6, 12);

  bleuart.begin();

  startAdv();

  for (int i = 0; i < 1024; i++) {
    data_buffer[i] = i % 256;
  }

  Serial.println("Setup complete. Advertising...");
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
  if (Bluefruit.connected() && (millis() - last_send_time > SEND_INTERVAL)) {
    send_data_packet();
    last_send_time = millis();
  }
}

void send_data_packet()
{
  for (int i = 0; i < 1024; i++) {
    data_buffer[i] = (packet_counter + i) % 256;
  }

  uint32_t start_time = millis();
  
  const int chunk_size = 200;
  for (int i = 0; i < 1024; i += chunk_size) {
    int remaining = min(chunk_size, 1024 - i);
    bleuart.write(&data_buffer[i], remaining);
    delay(5);
  }

  uint32_t send_time = millis() - start_time;
  
  Serial.printf("Sent packet #%d (1024 bytes) in %d ms\n", packet_counter, send_time);
  packet_counter++;
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
