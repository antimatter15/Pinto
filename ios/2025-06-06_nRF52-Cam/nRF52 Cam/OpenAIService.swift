import Foundation
import UIKit
import BackgroundTasks

class OpenAIService {
    static let shared = OpenAIService()
    private let apiKey = Secrets.openAIAPIKey
    
    private init() {}
    
    func generateGhibliVersion(of imageData: Data, completion: @escaping (Result<Data, Error>) -> Void) {
        // Start background task to ensure completion even if app goes to background
        var backgroundTask: UIBackgroundTaskIdentifier = .invalid
        var isCompleted = false
        
        backgroundTask = UIApplication.shared.beginBackgroundTask(withName: "GhibliGeneration") {
            // If time expires, save the image data for later processing
            if !isCompleted {
                print("Background task expired - Ghibli generation may continue when app becomes active")
                // Could save to UserDefaults for retry when app becomes active
                GhibliQueue.shared.addPendingGeneration(imageData)
            }
            
            UIApplication.shared.endBackgroundTask(backgroundTask)
            backgroundTask = .invalid
        }
        
        let originalCompletion = completion
        let wrappedCompletion: (Result<Data, Error>) -> Void = { result in
            isCompleted = true
            originalCompletion(result)
            
            // End background task when done
            if backgroundTask != .invalid {
                UIApplication.shared.endBackgroundTask(backgroundTask)
                backgroundTask = .invalid
            }
        }
        
        performGhibliGeneration(imageData: imageData, completion: wrappedCompletion)
    }
    
    func performGhibliGeneration(imageData: Data, completion: @escaping (Result<Data, Error>) -> Void) {
        let editURL = "https://api.openai.com/v1/images/edits"
        
        guard let url = URL(string: editURL) else {
            completion(.failure(NSError(domain: "OpenAIService", code: 1, userInfo: [NSLocalizedDescriptionKey: "Invalid URL"])))
            return
        }
        
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("Bearer \(apiKey)", forHTTPHeaderField: "Authorization")
        
        let boundary = UUID().uuidString
        request.setValue("multipart/form-data; boundary=\(boundary)", forHTTPHeaderField: "Content-Type")
        
        var body = Data()
        
        // Add image data (using array format for gpt-image-1)
        body.append("--\(boundary)\r\n".data(using: .utf8)!)
        body.append("Content-Disposition: form-data; name=\"image[]\"; filename=\"image.png\"\r\n".data(using: .utf8)!)
        body.append("Content-Type: image/png\r\n\r\n".data(using: .utf8)!)
        body.append(imageData)
        body.append("\r\n".data(using: .utf8)!)
        
        // Add prompt
        body.append("--\(boundary)\r\n".data(using: .utf8)!)
        body.append("Content-Disposition: form-data; name=\"prompt\"\r\n\r\n".data(using: .utf8)!)
        body.append("make a studio ghibli version of this image".data(using: .utf8)!)
        body.append("\r\n".data(using: .utf8)!)
        
        // Add model
        body.append("--\(boundary)\r\n".data(using: .utf8)!)
        body.append("Content-Disposition: form-data; name=\"model\"\r\n\r\n".data(using: .utf8)!)
        body.append("gpt-image-1".data(using: .utf8)!)
        body.append("\r\n".data(using: .utf8)!)
        
        // Add size
        body.append("--\(boundary)\r\n".data(using: .utf8)!)
        body.append("Content-Disposition: form-data; name=\"size\"\r\n\r\n".data(using: .utf8)!)
        body.append("1024x1024".data(using: .utf8)!)
        body.append("\r\n".data(using: .utf8)!)
        
        body.append("--\(boundary)--\r\n".data(using: .utf8)!)
        
        request.httpBody = body
        
        URLSession.shared.dataTask(with: request) { data, response, error in
            if let error = error {
                completion(.failure(error))
                return
            }
            
            guard let data = data else {
                completion(.failure(NSError(domain: "OpenAIService", code: 2, userInfo: [NSLocalizedDescriptionKey: "No data received"])))
                return
            }
            
            do {
                if let json = try JSONSerialization.jsonObject(with: data) as? [String: Any],
                   let dataArray = json["data"] as? [[String: Any]],
                   let firstResult = dataArray.first,
                   let base64String = firstResult["b64_json"] as? String,
                   let imageData = Data(base64Encoded: base64String) {
                    completion(.success(imageData))
                } else {
                    // Try to parse error response
                    if let errorJson = try JSONSerialization.jsonObject(with: data) as? [String: Any],
                       let error = errorJson["error"] as? [String: Any],
                       let message = error["message"] as? String {
                        completion(.failure(NSError(domain: "OpenAIService", code: 3, userInfo: [NSLocalizedDescriptionKey: message])))
                    } else {
                        // Print raw response for debugging
                        let responseString = String(data: data, encoding: .utf8) ?? "Unable to decode response"
                        print("Raw API response: \(responseString)")
                        completion(.failure(NSError(domain: "OpenAIService", code: 3, userInfo: [NSLocalizedDescriptionKey: "Failed to parse response: \(responseString)"])))
                    }
                }
            } catch {
                completion(.failure(error))
            }
        }.resume()
    }
}