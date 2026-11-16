import Foundation
import AppIntents

@available(iOS 16.0, *)
struct nRF52CameraShortcutsProvider: AppShortcutsProvider {
    static var appShortcuts: [AppShortcut] {
        AppShortcut(
            intent: TakePictureIntent(),
            phrases: [
                "Take a picture with \(.applicationName)",
                "Capture photo with \(.applicationName)",
                "Trigger camera with \(.applicationName)"
            ],
            shortTitle: "Take Picture",
            systemImageName: "camera"
        )
    }
}