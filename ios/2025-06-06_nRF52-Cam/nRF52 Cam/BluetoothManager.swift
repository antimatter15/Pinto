import Foundation
import CoreBluetooth
import SwiftUI
import UIKit

class BluetoothManager: NSObject, ObservableObject {
    // UART Service and Characteristic UUIDs
    private let UART_SERVICE_UUID = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    private let UART_RX_CHARACTERISTIC_UUID = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
    private let UART_TX_CHARACTERISTIC_UUID = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
    
    private var centralManager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var rxCharacteristic: CBCharacteristic?
    private var txCharacteristic: CBCharacteristic?
    
    // Published properties for UI binding
    @Published var isConnected = false
    @Published var connectionStatus = "Disconnected"
    @Published var logs: [String] = []
    @Published var imagesReceived = 0
    @Published var bytesReceived = 0
    @Published var transferRate: Double = 0
    @Published var lastTransmissionTime: Int = 0
    @Published var lastImageSize: Int = 0
    @Published var lastGhibliGenerationTime: Double = 0
    
    // Image data handling
    private var imageData = Data()
    private var imageStartTime: Date?
    private var startTime: Date?
    private var imageTimeout: Timer?
    
    // Delegate for image reception
    weak var imageDelegate: ImageReceiver?
    
    // Auto-reconnection
    private var shouldReconnect = false
    private var lastConnectedPeripheralIdentifier: UUID?
    
    func updateGhibliGenerationTime(_ time: Double) {
        DispatchQueue.main.async {
            self.lastGhibliGenerationTime = time
        }
    }
    
    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
        log("Bluetooth manager initialized")
        setupBackgroundHandling()
    }
    
    private func setupBackgroundHandling() {
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(appDidEnterBackground),
            name: UIApplication.didEnterBackgroundNotification,
            object: nil
        )
        
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(appWillEnterForeground),
            name: UIApplication.willEnterForegroundNotification,
            object: nil
        )
        
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(appWillTerminate),
            name: UIApplication.willTerminateNotification,
            object: nil
        )
    }
    
    @objc private func appDidEnterBackground() {
        log("App entered background - maintaining Bluetooth connection")
        // iOS will maintain the connection automatically for subscribed characteristics
        // We just need to ensure we don't do unnecessary work
    }
    
    @objc private func appWillEnterForeground() {
        log("App entering foreground - checking connection status")
        DispatchQueue.main.async {
            if let peripheral = self.peripheral {
                self.isConnected = (peripheral.state == .connected)
                if self.isConnected {
                    self.connectionStatus = "Connected"
                    self.log("Connection maintained while in background")
                } else {
                    self.connectionStatus = "Disconnected"
                    self.log("Connection lost while in background")
                    // Attempt reconnection if we should be connected
                    if self.shouldReconnect {
                        self.attemptReconnection()
                    }
                }
            } else if self.shouldReconnect {
                // No peripheral but we should be connected - attempt reconnection
                self.attemptReconnection()
            }
            
            // Process any pending Ghibli generations
            let pendingCount = GhibliQueue.shared.getPendingCount()
            if pendingCount > 0 {
                self.log("Processing \(pendingCount) pending Ghibli generations...")
                GhibliQueue.shared.processPendingGenerations { processed in
                    self.log("Processed \(processed) pending Ghibli generations")
                }
            }
        }
    }
    
    @objc private func appWillTerminate() {
        log("App terminating - cleaning up Bluetooth")
        if let peripheral = peripheral, peripheral.state == .connected {
            centralManager.cancelPeripheralConnection(peripheral)
        }
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
    
    func connect() {
        if isConnected {
            disconnect()
        } else {
            shouldReconnect = true
            log("Scanning for devices...")
            // Use service UUID for more efficient scanning
            centralManager.scanForPeripherals(withServices: [UART_SERVICE_UUID], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
        }
    }
    
    func disconnect() {
        shouldReconnect = false
        if let peripheral = peripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
    }
    
    private func attemptReconnection() {
        guard shouldReconnect else { return }
        
        // Try to reconnect to the last known peripheral first
        if let identifier = lastConnectedPeripheralIdentifier {
            let knownPeripherals = centralManager.retrievePeripherals(withIdentifiers: [identifier])
            if let knownPeripheral = knownPeripherals.first {
                log("Attempting to reconnect to known device: \(knownPeripheral.name ?? "Unknown")")
                self.peripheral = knownPeripheral
                centralManager.connect(knownPeripheral, options: nil)
                return
            }
        }
        
        // If that fails, start scanning
        log("Starting scan for reconnection...")
        centralManager.scanForPeripherals(withServices: [UART_SERVICE_UUID], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }
    
    func takePicture() {
        guard let rxCharacteristic = rxCharacteristic else {
            log("Error: Not connected to device")
            return
        }
        
        log("Sending take picture command...")
        resetImageBuffer()
        
        let command = "P".data(using: .utf8)!
        peripheral?.writeValue(command, for: rxCharacteristic, type: .withResponse)
        log("Picture command sent")
    }
    
    private func log(_ message: String) {
        let timestamp = DateFormatter.localizedString(from: Date(), dateStyle: .none, timeStyle: .medium)
        let logMessage = "[\(timestamp)] \(message)"
        DispatchQueue.main.async {
            self.logs.append(logMessage)
            // Keep only last 100 logs
            if self.logs.count > 100 {
                self.logs.removeFirst()
            }
        }
    }
    
    private func updateStats() {
        if let startTime = startTime {
            let elapsed = Date().timeIntervalSince(startTime)
            if elapsed > 0 && bytesReceived > 0 {
                transferRate = Double(bytesReceived) / 1024.0 / elapsed
            }
        }
    }
    
    private func resetImageBuffer() {
        imageTimeout?.invalidate()
        imageTimeout = nil
        imageData = Data()
        imageStartTime = nil
    }
    
    private func findJPEGStart(in data: Data, startIndex: Int = 0) -> Int? {
        for i in startIndex..<(data.count - 1) {
            if data[i] == 0xFF && data[i + 1] == 0xD8 {
                return i
            }
        }
        return nil
    }
    
    private func findJPEGEnd(in data: Data, startIndex: Int = 0) -> Int? {
        for i in startIndex..<(data.count - 1) {
            if data[i] == 0xFF && data[i + 1] == 0xD9 {
                return i + 2 // Include the end marker
            }
        }
        return nil
    }
    
    private func isValidJPEG(_ data: Data) -> Bool {
        guard data.count >= 10 else { return false }
        
        // Must start with JPEG marker
        guard data[0] == 0xFF && data[1] == 0xD8 else { return false }
        
        // Must end with JPEG marker
        guard data[data.count - 2] == 0xFF && data[data.count - 1] == 0xD9 else { return false }
        
        // Look for at least one valid JPEG segment after SOI
        for i in 2..<min(data.count - 1, 50) {
            if data[i] == 0xFF {
                let marker = data[i + 1]
                if (marker >= 0xE0 && marker <= 0xEF) || // APP segments
                   marker == 0xDB || // DQT
                   marker == 0xC0 || // SOF
                   marker == 0xC4 {  // DHT
                    return true
                }
            }
        }
        
        return false
    }
    
    private func handleImageData(_ data: Data) {
        bytesReceived += data.count
        
        // Look for JPEG start in this packet
        if let jpegStartIndex = findJPEGStart(in: data) {
            // Found JPEG start - reset and begin new image
            resetImageBuffer()
            
            // Extract the JPEG data starting from the marker
            imageData = data.subdata(in: jpegStartIndex..<data.count)
            imageStartTime = Date()
            
            log("New JPEG started at offset \(jpegStartIndex), \(imageData.count) bytes")
            
            // Set timeout for incomplete images
            imageTimeout = Timer.scheduledTimer(withTimeInterval: 15.0, repeats: false) { _ in
                self.log("Image timeout - resetting buffer")
                self.resetImageBuffer()
            }
        } else if imageStartTime != nil && !imageData.isEmpty {
            // Continue building current image
            imageData.append(data)
        } else {
            // No active image and no start marker - ignore
            log("Ignoring \(data.count) bytes (no active image)")
            updateStats()
            return
        }
        
        // Check for JPEG end marker
        if let jpegEndIndex = findJPEGEnd(in: imageData) {
            // Extract complete JPEG
            let completeJPEG = imageData.subdata(in: 0..<jpegEndIndex)
            
            imageTimeout?.invalidate()
            imageTimeout = nil
            
            let transmissionTime = Int((Date().timeIntervalSince(imageStartTime!) * 1000).rounded())
            
            log("JPEG end found at \(jpegEndIndex), total size: \(completeJPEG.count) bytes")
            
            // Validate the complete JPEG
            if isValidJPEG(completeJPEG) {
                DispatchQueue.main.async {
                    self.imagesReceived += 1
                    self.lastImageSize = completeJPEG.count
                    self.lastTransmissionTime = transmissionTime
                }
                
                log("Valid JPEG received: \(completeJPEG.count) bytes (Image #\(imagesReceived + 1), \(transmissionTime)ms)")
                
                // Notify delegate
                DispatchQueue.main.async {
                    self.imageDelegate?.didReceiveImage(completeJPEG)
                }
            } else {
                log("Corrupt JPEG detected (\(completeJPEG.count) bytes) - discarding")
            }
            
            // Check if there's more data after this JPEG
            if jpegEndIndex < imageData.count {
                let remainingData = imageData.subdata(in: jpegEndIndex..<imageData.count)
                resetImageBuffer()
                
                // Process remaining data recursively
                bytesReceived -= remainingData.count // Don't double count
                handleImageData(remainingData)
                return
            } else {
                resetImageBuffer()
            }
        }
        
        // Safety check for oversized buffers
        if imageData.count > 1_000_000 {
            log("Buffer too large (\(imageData.count) bytes) - resetting")
            resetImageBuffer()
        }
        
        updateStats()
    }
}

// MARK: - CBCentralManagerDelegate
extension BluetoothManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            log("Bluetooth is powered on")
        case .poweredOff:
            log("Bluetooth is powered off")
            connectionStatus = "Bluetooth is off"
        case .unauthorized:
            log("Bluetooth unauthorized")
            connectionStatus = "Bluetooth unauthorized"
        default:
            log("Bluetooth state: \(central.state.rawValue)")
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        if peripheral.name == "XIAO_nRF52840_CAM" {
            log("Found XIAO_nRF52840_CAM")
            self.peripheral = peripheral
            centralManager.stopScan()
            centralManager.connect(peripheral, options: nil)
        }
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        log("Connected to \(peripheral.name ?? "Unknown")")
        lastConnectedPeripheralIdentifier = peripheral.identifier
        peripheral.delegate = self
        peripheral.discoverServices([UART_SERVICE_UUID])
        startTime = Date()
        centralManager.stopScan()
    }
    
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        if let error = error {
            log("Disconnected from peripheral with error: \(error.localizedDescription)")
        } else {
            log("Disconnected from peripheral")
        }
        
        DispatchQueue.main.async {
            self.isConnected = false
            self.connectionStatus = "Disconnected"
            self.peripheral = nil
            self.rxCharacteristic = nil
            self.txCharacteristic = nil
            self.resetImageBuffer()
            self.startTime = nil
            self.lastImageSize = 0
            self.lastGhibliGenerationTime = 0
        }
        
        // Attempt automatic reconnection if desired
        if shouldReconnect && error != nil {
            log("Attempting automatic reconnection...")
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                self.attemptReconnection()
            }
        }
    }
    
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        log("Failed to connect: \(error?.localizedDescription ?? "Unknown error")")
        DispatchQueue.main.async {
            self.connectionStatus = "Connection failed"
        }
    }
}

// MARK: - CBPeripheralDelegate
extension BluetoothManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        
        for service in services {
            if service.uuid == UART_SERVICE_UUID {
                log("Found UART service")
                peripheral.discoverCharacteristics([UART_RX_CHARACTERISTIC_UUID, UART_TX_CHARACTERISTIC_UUID], for: service)
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        
        for characteristic in characteristics {
            if characteristic.uuid == UART_RX_CHARACTERISTIC_UUID {
                log("Found RX characteristic")
                rxCharacteristic = characteristic
            } else if characteristic.uuid == UART_TX_CHARACTERISTIC_UUID {
                log("Found TX characteristic")
                txCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            }
        }
        
        if rxCharacteristic != nil && txCharacteristic != nil {
            DispatchQueue.main.async {
                self.isConnected = true
                self.connectionStatus = "Connected"
            }
            log("Successfully connected to camera!")
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == UART_TX_CHARACTERISTIC_UUID,
              let data = characteristic.value else { return }
        
        handleImageData(data)
    }
}

// Protocol for image reception
protocol ImageReceiver: AnyObject {
    func didReceiveImage(_ imageData: Data)
}