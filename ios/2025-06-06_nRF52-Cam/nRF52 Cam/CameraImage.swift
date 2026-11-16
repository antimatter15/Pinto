import Foundation
import SwiftUI
import Photos

struct CameraImage: Identifiable {
    let id = UUID()
    let data: Data
    let timestamp: Date
    let size: Int
    var ghibliData: Data?
    var isGeneratingGhibli = false
    var ghibliGenerationTime: TimeInterval = 0
    
    var sizeKB: String {
        String(format: "%.1f KB", Double(size) / 1024.0)
    }
    
    var timeString: String {
        let formatter = DateFormatter()
        formatter.timeStyle = .medium
        return formatter.string(from: timestamp)
    }
    
    var uiImage: UIImage? {
        UIImage(data: data)
    }
    
    var ghibliImage: UIImage? {
        guard let ghibliData = ghibliData else { return nil }
        return UIImage(data: ghibliData)
    }
    
    var ghibliGenerationTimeString: String {
        if ghibliGenerationTime > 0 {
            return String(format: "%.1fs", ghibliGenerationTime)
        }
        return "-"
    }
}

class CameraImageStore: ObservableObject, ImageReceiver {
    @Published var images: [CameraImage] = []
    @Published var latestImage: CameraImage?
    var saveOriginalToPhotoLibrary = UserDefaults.standard.bool(forKey: "saveOriginalToPhotoLibrary")
    var saveGhibliToPhotoLibrary = UserDefaults.standard.bool(forKey: "saveGhibliToPhotoLibrary")
    var generateGhibliInBackground = true
    weak var bluetoothManager: BluetoothManager?
    
    func addImage(_ imageData: Data) {
        var newImage = CameraImage(
            data: imageData,
            timestamp: Date(),
            size: imageData.count
        )
        
        DispatchQueue.main.async {
            self.images.insert(newImage, at: 0)
            self.latestImage = newImage
            
            // Keep only last 50 images to manage memory
            if self.images.count > 50 {
                self.images = Array(self.images.prefix(50))
            }
            
            // Generate Ghibli version if enabled
            if self.generateGhibliInBackground {
                self.generateGhibliVersion(for: newImage)
            }
        }
        
        // Save original to photo library if enabled
        if saveOriginalToPhotoLibrary {
            saveImageToPhotoLibrary(imageData, isGhibli: false)
        }
    }
    
    private func generateGhibliVersion(for image: CameraImage) {
        guard let uiImage = image.uiImage else { return }
        
        let startTime = Date()
        
        // Update image to show it's generating
        if let index = images.firstIndex(where: { $0.id == image.id }) {
            DispatchQueue.main.async {
                self.images[index].isGeneratingGhibli = true
                self.objectWillChange.send()
            }
        }
        
        // Call OpenAI API
        OpenAIService.shared.generateGhibliVersion(of: image.data) { [weak self] result in
            guard let self = self else { return }
            
            let generationTime = Date().timeIntervalSince(startTime)
            
            switch result {
            case .success(let ghibliData):
                // Update the image with Ghibli data
                if let index = self.images.firstIndex(where: { $0.id == image.id }) {
                    DispatchQueue.main.async {
                        self.images[index].ghibliData = ghibliData
                        self.images[index].isGeneratingGhibli = false
                        self.images[index].ghibliGenerationTime = generationTime
                        
                        // Update latest image if it's the same
                        if self.latestImage?.id == image.id {
                            self.latestImage = self.images[index]
                        }
                        
                        // Update BluetoothManager stats
                        self.bluetoothManager?.updateGhibliGenerationTime(generationTime)
                        
                        self.objectWillChange.send()
                        
                        // Save Ghibli version to photo library if enabled
                        if self.saveGhibliToPhotoLibrary {
                            self.saveImageToPhotoLibrary(ghibliData, isGhibli: true)
                        }
                    }
                }
                
            case .failure(let error):
                print("Failed to generate Ghibli version: \(error.localizedDescription)")
                // Update to show generation failed
                if let index = self.images.firstIndex(where: { $0.id == image.id }) {
                    DispatchQueue.main.async {
                        self.images[index].isGeneratingGhibli = false
                        self.images[index].ghibliGenerationTime = generationTime
                        self.objectWillChange.send()
                    }
                }
            }
        }
    }
    
    func clear() {
        DispatchQueue.main.async {
            self.images.removeAll()
            self.latestImage = nil
        }
    }
    
    // ImageReceiver protocol implementation
    func didReceiveImage(_ imageData: Data) {
        addImage(imageData)
    }
    
    private func saveImageToPhotoLibrary(_ imageData: Data, isGhibli: Bool) {
        guard let image = UIImage(data: imageData) else { return }
        
        PHPhotoLibrary.requestAuthorization { status in
            if status == .authorized {
                PHPhotoLibrary.shared().performChanges({
                    PHAssetCreationRequest.creationRequestForAsset(from: image)
                }) { success, error in
                    if success {
                        print("\(isGhibli ? "Ghibli" : "Original") image saved to photo library")
                    } else if let error = error {
                        print("Error saving \(isGhibli ? "Ghibli" : "original") image: \(error.localizedDescription)")
                    }
                }
            } else {
                print("Photo library access not authorized")
            }
        }
    }
}