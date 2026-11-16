#include <bluefruit.h>

// BLE Service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble


void setup()
{
  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behavior, but provided
  // here in case you want to control this LED manually via PIN 19
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_LOW);
  Bluefruit.configCentralBandwidth(0);
  


  Bluefruit.begin();
  Bluefruit.setTxPower(-8);    // Very low power, ~1 meter range
  //Bluefruit.setName(getMcuUniqueID()); // useful testing with multiple central connections


  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();
  bleuart.setRxCallback(bleuart_rx_callback);


  // Set up and start advertising
  startAdv();

}


void bleuart_rx_callback(uint16_t conn_hdl)
{
  (void) conn_hdl;

  String receivedMessage = "";
  while (bleuart.available()) {
    char ch = (char) bleuart.read();
    receivedMessage += ch;
  }

  if (receivedMessage.length() > 0) {
    // Echo back with prefix
    String echoMessage = "Echo: " + receivedMessage;
    bleuart.print(echoMessage);
  }
}


void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  //  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();

  /* Start Advertising
     - Enable auto advertising if disconnected
     - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
     - Timeout for fast mode is 30 seconds
     - Start(timeout) with timeout = 0 will advertise forever (until connected)

     For recommended advertising interval
     https://developer.apple.com/library/content/qa/qa1931/_index.html
  */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(800, 1600);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

void loop()
{
  sd_app_evt_wait();
  delay(500);
}
