//
//  nRF52_CamApp.swift
//  nRF52 Cam
//
//  Created by Kevin Kwok on 6/6/25.
//

import SwiftUI
import AppIntents

@main
struct nRF52_CamApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
    
    init() {
        // Register App Intents for iOS 16+
        if #available(iOS 16.0, *) {
            nRF52CameraShortcutsProvider.updateAppShortcutParameters()
        }
    }
}
