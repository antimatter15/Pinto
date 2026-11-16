import Foundation

class GhibliQueue {
    static let shared = GhibliQueue()
    
    private let userDefaults = UserDefaults.standard
    private let pendingKey = "PendingGhibliGenerations"
    
    private init() {}
    
    func addPendingGeneration(_ imageData: Data) {
        var pending = getPendingGenerations()
        
        // Convert to base64 string for storage
        let base64String = imageData.base64EncodedString()
        let timestamp = Date().timeIntervalSince1970
        
        let pendingItem: [String: Any] = [
            "imageData": base64String,
            "timestamp": timestamp
        ]
        
        pending.append(pendingItem)
        
        // Keep only last 5 pending items to manage storage
        if pending.count > 5 {
            pending = Array(pending.suffix(5))
        }
        
        userDefaults.set(pending, forKey: pendingKey)
        print("Added pending Ghibli generation. Total pending: \(pending.count)")
    }
    
    func getPendingGenerations() -> [[String: Any]] {
        return userDefaults.array(forKey: pendingKey) as? [[String: Any]] ?? []
    }
    
    func processPendingGenerations(completion: @escaping (Int) -> Void) {
        let pending = getPendingGenerations()
        guard !pending.isEmpty else {
            completion(0)
            return
        }
        
        print("Processing \(pending.count) pending Ghibli generations...")
        
        var processed = 0
        let group = DispatchGroup()
        
        for item in pending {
            guard let base64String = item["imageData"] as? String,
                  let imageData = Data(base64Encoded: base64String) else {
                continue
            }
            
            group.enter()
            OpenAIService.shared.performGhibliGeneration(imageData: imageData) { result in
                switch result {
                case .success(let ghibliData):
                    print("Successfully processed pending Ghibli generation")
                    // Could save to photo library or notify user
                    processed += 1
                case .failure(let error):
                    print("Failed to process pending Ghibli generation: \(error.localizedDescription)")
                }
                group.leave()
            }
        }
        
        group.notify(queue: .main) {
            // Clear processed items
            self.clearPendingGenerations()
            completion(processed)
        }
    }
    
    func clearPendingGenerations() {
        userDefaults.removeObject(forKey: pendingKey)
        print("Cleared pending Ghibli generations")
    }
    
    func getPendingCount() -> Int {
        return getPendingGenerations().count
    }
}