import Foundation
import AppIntents
import SwiftUI

@available(iOS 16.0, *)
struct TakePictureIntent: AppIntent {
    static var title: LocalizedStringResource = "Take Picture with nRF52 Camera"
    static var description = IntentDescription("Remotely trigger the nRF52 camera to take a picture via Bluetooth")
    
    static var openAppWhenRun: Bool = false
    static var isDiscoverable: Bool = true
    
    @Parameter(title: "Save Original to Photos", description: "Automatically save original images to Photos app", default: false)
    var saveOriginal: Bool
    
    @Parameter(title: "Save Ghibli to Photos", description: "Automatically save Ghibli versions to Photos app", default: false)
    var saveGhibli: Bool
    
    @Parameter(title: "Generate Ghibli Version", description: "Automatically generate Studio Ghibli style version", default: true)
    var generateGhibli: Bool
    
    @MainActor
    func perform() async throws -> some IntentResult & ProvidesDialog {
        // Get the shared Bluetooth manager instance
        let bluetoothManager = SharedBluetoothManager.shared
        
        // Check if connected
        guard bluetoothManager.isConnected else {
            return .result(dialog: "❌ nRF52 Camera is not connected. Please open the app and connect to the camera first.")
        }
        
        // Set preferences
        SharedBluetoothManager.shared.setSavePreferences(saveOriginal: saveOriginal, saveGhibli: saveGhibli)
        SharedBluetoothManager.shared.setGhibliGenerationPreference(generateGhibli)
        
        // Trigger take picture
        bluetoothManager.takePicture()
        
        var messages: [String] = []
        if saveOriginal { messages.append("Original will be saved to Photos") }
        if saveGhibli { messages.append("Ghibli version will be saved to Photos") }
        if generateGhibli { messages.append("Ghibli version will be generated") }
        
        let additionalMessage = messages.isEmpty ? "" : ". " + messages.joined(separator: ". ") + "."
        return .result(dialog: "📸 Picture capture triggered on nRF52 camera!\(additionalMessage)")
    }
}

@available(iOS 16.0, *)
struct GetCameraStatusIntent: AppIntent {
    static var title: LocalizedStringResource = "Get nRF52 Camera Status"
    static var description = IntentDescription("Check the connection status of the nRF52 camera")
    
    static var openAppWhenRun: Bool = false
    static var isDiscoverable: Bool = true
    
    @MainActor
    func perform() async throws -> some IntentResult & ProvidesDialog {
        let bluetoothManager = SharedBluetoothManager.shared
        
        if bluetoothManager.isConnected {
            let stats = bluetoothManager.getStats()
            return .result(dialog: "✅ nRF52 Camera is connected!\n📊 Images received: \(stats.imagesReceived)\n📁 Bytes received: \(stats.bytesReceived)")
        } else {
            return .result(dialog: "❌ nRF52 Camera is not connected. Open the app to connect.")
        }
    }
}

// Singleton to share BluetoothManager across the app and Shortcuts
class SharedBluetoothManager {
    static let shared = SharedBluetoothManager()
    
    private var _bluetoothManager: BluetoothManager?
    private var saveOriginalPreference = false
    private var saveGhibliPreference = false
    
    private init() {}
    
    var bluetoothManager: BluetoothManager {
        if let manager = _bluetoothManager {
            return manager
        }
        let manager = BluetoothManager()
        _bluetoothManager = manager
        return manager
    }
    
    var isConnected: Bool {
        return bluetoothManager.isConnected
    }
    
    func takePicture() {
        bluetoothManager.takePicture()
    }
    
    func setBluetoothManager(_ manager: BluetoothManager) {
        _bluetoothManager = manager
    }
    
    private var ghibliGenerationPreference = true
    
    func setSavePreferences(saveOriginal: Bool, saveGhibli: Bool) {
        saveOriginalPreference = saveOriginal
        saveGhibliPreference = saveGhibli
        // Apply to image store if available
        if let imageStore = getImageStore() {
            imageStore.saveOriginalToPhotoLibrary = saveOriginal
            imageStore.saveGhibliToPhotoLibrary = saveGhibli
        }
    }
    
    func setGhibliGenerationPreference(_ generateGhibli: Bool) {
        ghibliGenerationPreference = generateGhibli
        // Apply to image store if available
        if let imageStore = getImageStore() {
            imageStore.generateGhibliInBackground = generateGhibli
        }
    }
    
    private var _imageStore: CameraImageStore?
    
    func setImageStore(_ imageStore: CameraImageStore) {
        _imageStore = imageStore
    }
    
    func getImageStore() -> CameraImageStore? {
        return _imageStore
    }
    
    func getStats() -> (imagesReceived: Int, bytesReceived: Int) {
        return (
            imagesReceived: bluetoothManager.imagesReceived,
            bytesReceived: bluetoothManager.bytesReceived
        )
    }
}