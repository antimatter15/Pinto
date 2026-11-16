//
//  ContentView.swift
//  nRF52 Cam
//
//  Created by Kevin Kwok on 6/6/25.
//

import SwiftUI

struct ContentView: View {
    @StateObject private var bluetoothManager = SharedBluetoothManager.shared.bluetoothManager
    @StateObject private var imageStore = CameraImageStore()
    @State private var showingGallery = false
    @State private var selectedImage: CameraImage?
    @State private var saveOriginalToPhotoLibrary = UserDefaults.standard.bool(forKey: "saveOriginalToPhotoLibrary")
    @State private var saveGhibliToPhotoLibrary = UserDefaults.standard.bool(forKey: "saveGhibliToPhotoLibrary")
    @State private var takePictureDisabled = false
    
    var body: some View {
        NavigationView {
            ScrollView {
                VStack(spacing: 20) {
                    // Control Buttons (Connect and Take Picture)
                    VStack(spacing: 12) {
                        HStack(spacing: 12) {
                            if !bluetoothManager.isConnected {
                                Button(action: {
                                    bluetoothManager.connect()
                                }) {
                                    Label("Connect to Camera", systemImage: "wifi")
                                        .frame(maxWidth: .infinity)
                                }
                                .buttonStyle(.borderedProminent)
                                .controlSize(.large)
                            }else{
                                Button(action: {
                                    bluetoothManager.takePicture()
                                    takePictureDisabled = true
                                    
                                    // Re-enable after 5 seconds as fallback
                                    DispatchQueue.main.asyncAfter(deadline: .now() + 5.0) {
                                        takePictureDisabled = false
                                    }
                                }) {
                                    Label(takePictureDisabled ? "Taking Picture..." : "Take Picture", systemImage: takePictureDisabled ? "hourglass" : "camera.fill")
                                        .frame(maxWidth: .infinity)
                                }
                                .buttonStyle(.borderedProminent)
                                .controlSize(.large)
                                .disabled(!bluetoothManager.isConnected || takePictureDisabled)
                            }
                            
                            
                        }
                        
                    }
                    .padding(.horizontal)
                    
                    // Latest Image
                    if let latestImage = imageStore.latestImage {
                        VStack(alignment: .leading, spacing: 16) {
                            HStack {
                                Text("Latest Image")
                                    .font(.headline)
                                Spacer()
                                Button("View Gallery") {
                                    showingGallery = true
                                }
                                .font(.caption)
                            }
                            
                            // Primary Image Display (Ghibli if available, otherwise original)
                            VStack(alignment: .leading, spacing: 4) {
                                HStack {
                                    if let _ = latestImage.ghibliImage {
                                        Text("Studio Ghibli Version ✨")
                                            .font(.caption2)
                                            .foregroundColor(.green)
                                            .fontWeight(.medium)
                                    } else if latestImage.isGeneratingGhibli {
                                        Text("Original (Ghibli Generating...)")
                                            .font(.caption2)
                                            .foregroundColor(.orange)
                                    } else {
                                        Text("Original")
                                            .font(.caption2)
                                            .foregroundColor(.secondary)
                                    }
                                    
                                    if latestImage.ghibliGenerationTime > 0 {
                                        Spacer()
                                        Text("Generated in \(latestImage.ghibliGenerationTimeString)")
                                            .font(.caption2)
                                            .foregroundColor(.green)
                                    }
                                }
                                
                                // Show Ghibli version if available, otherwise original
                                if let ghibliImage = latestImage.ghibliImage {
                                    Image(uiImage: ghibliImage)
                                        .resizable()
                                        .aspectRatio(contentMode: .fit)
                                        .frame(maxHeight: 400)
                                        .cornerRadius(8)
                                        .shadow(radius: 2)
                                        .onTapGesture {
                                            selectedImage = latestImage
                                        }
                                } else if let originalImage = latestImage.uiImage {
                                    ZStack(alignment: .topTrailing) {
                                        Image(uiImage: originalImage)
                                            .resizable()
                                            .aspectRatio(contentMode: .fit)
                                            .frame(maxHeight: 400)
                                            .cornerRadius(8)
                                            .shadow(radius: 2)
                                            .onTapGesture {
                                                selectedImage = latestImage
                                            }
                                        
                                        // Show generating indicator overlay
                                        if latestImage.isGeneratingGhibli {
                                            VStack(spacing: 4) {
                                                ProgressView()
                                                    .progressViewStyle(CircularProgressViewStyle(tint: .white))
                                                    .scaleEffect(0.8)
                                                Text("✨")
                                                    .font(.caption2)
                                            }
                                            .padding(8)
                                            .background(Color.black.opacity(0.7))
                                            .cornerRadius(8)
                                            .padding(8)
                                        }
                                    }
                                }
                            }
                            
                            Text("Captured at \(latestImage.timeString)")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                        .padding()
                        .background(Color(.systemGray6))
                        .cornerRadius(12)
                    } else {
                        VStack(spacing: 12) {
                            Image(systemName: "camera.fill")
                                .font(.system(size: 48))
                                .foregroundColor(.secondary)
                            Text("No images received yet...")
                                .foregroundColor(.secondary)
                        }
                        .frame(maxWidth: .infinity)
                        .frame(height: 200)
                        .background(Color(.systemGray6))
                        .cornerRadius(12)
                    }
                    
                    // Stats Grid
                    StatsGrid(bluetoothManager: bluetoothManager)
                    
                    // Logs
                    LogsView(logs: bluetoothManager.logs)
                    
                    // Status Card
                    StatusCard(
                        isConnected: bluetoothManager.isConnected,
                        status: bluetoothManager.connectionStatus
                    )
                    
                    
                    // Save to Photo Library Toggles
                    VStack(alignment: .leading, spacing: 12) {
                        Text("Save to Photo Library")
                            .font(.headline)
                            .padding(.bottom, 4)
                        
                        Toggle(isOn: $saveOriginalToPhotoLibrary) {
                            Label("Save Original Images", systemImage: "photo")
                                .font(.body)
                        }
                        .toggleStyle(SwitchToggleStyle(tint: .accentColor))
                        
                        Toggle(isOn: $saveGhibliToPhotoLibrary) {
                            Label("Save Ghibli Versions", systemImage: "sparkles")
                                .font(.body)
                        }
                        .toggleStyle(SwitchToggleStyle(tint: .green))
                    }
                    .padding(.horizontal, 4)
                    
                    // Disconnect Button (only shown when connected)
                    if bluetoothManager.isConnected {
                        Button(action: {
                            bluetoothManager.disconnect()
                        }) {
                            Label("Disconnect", systemImage: "wifi.slash")
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.bordered)
                        .controlSize(.large)
                        .tint(.red)
                        .padding(.horizontal)
                    }
                }
                .padding()
            }
            .navigationTitle("nRF52 Camera")
            .sheet(isPresented: $showingGallery) {
                ImageGalleryView(imageStore: imageStore, selectedImage: $selectedImage)
            }
            .fullScreenCover(item: $selectedImage) { image in
                ImageDetailView(image: image)
            }
            .onAppear {
                // Ensure the shared manager knows about our instances
                SharedBluetoothManager.shared.setBluetoothManager(bluetoothManager)
                SharedBluetoothManager.shared.setImageStore(imageStore)
                bluetoothManager.imageDelegate = imageStore
                imageStore.bluetoothManager = bluetoothManager
                imageStore.saveOriginalToPhotoLibrary = saveOriginalToPhotoLibrary
                imageStore.saveGhibliToPhotoLibrary = saveGhibliToPhotoLibrary
            }
            .onChange(of: saveOriginalToPhotoLibrary) { newValue in
                imageStore.saveOriginalToPhotoLibrary = newValue
                UserDefaults.standard.set(newValue, forKey: "saveOriginalToPhotoLibrary")
            }
            .onChange(of: saveGhibliToPhotoLibrary) { newValue in
                imageStore.saveGhibliToPhotoLibrary = newValue
                UserDefaults.standard.set(newValue, forKey: "saveGhibliToPhotoLibrary")
            }
            .onChange(of: imageStore.latestImage?.id) { _ in
                // Re-enable take picture button when new image is received
                takePictureDisabled = false
            }
        }
    }
}

struct StatusCard: View {
    let isConnected: Bool
    let status: String
    
    var body: some View {
        HStack {
            Circle()
                .fill(isConnected ? Color.green : Color.red)
                .frame(width: 12, height: 12)
            
            Text("Status: \(status)")
                .font(.headline)
            
            Spacer()
        }
        .padding()
        .background(isConnected ? Color.green.opacity(0.1) : Color.red.opacity(0.1))
        .overlay(
            RoundedRectangle(cornerRadius: 8)
                .stroke(isConnected ? Color.green : Color.red, lineWidth: 1)
        )
        .cornerRadius(8)
    }
}

struct StatsGrid: View {
    @ObservedObject var bluetoothManager: BluetoothManager
    
    var body: some View {
        LazyVGrid(columns: [
            GridItem(.flexible()),
            GridItem(.flexible()),
            GridItem(.flexible())
        ], spacing: 12) {
            StatBox(
                value: "\(bluetoothManager.imagesReceived)",
                label: "Images Received",
                systemImage: "photo.stack"
            )
            
            StatBox(
                value: formatBytes(bluetoothManager.bytesReceived),
                label: "Bytes Received",
                systemImage: "arrow.down.circle"
            )
            
            StatBox(
                value: String(format: "%.2f", bluetoothManager.transferRate),
                label: "KB/s",
                systemImage: "speedometer"
            )
            
            StatBox(
                value: bluetoothManager.lastTransmissionTime > 0 ? "\(bluetoothManager.lastTransmissionTime)" : "-",
                label: "Last Transmission (ms)",
                systemImage: "timer"
            )
            
            StatBox(
                value: bluetoothManager.lastImageSize > 0 ? String(format: "%.1f KB", Double(bluetoothManager.lastImageSize) / 1024) : "-",
                label: "Last Image Size",
                systemImage: "doc"
            )
            
            StatBox(
                value: bluetoothManager.lastGhibliGenerationTime > 0 ? String(format: "%.1fs", bluetoothManager.lastGhibliGenerationTime) : "-",
                label: "Last Ghibli Gen Time",
                systemImage: "sparkles"
            )
        }
    }
    
    func formatBytes(_ bytes: Int) -> String {
        if bytes < 1024 {
            return "\(bytes)"
        } else if bytes < 1_048_576 {
            return String(format: "%.1fK", Double(bytes) / 1024)
        } else {
            return String(format: "%.1fM", Double(bytes) / 1_048_576)
        }
    }
}

struct StatBox: View {
    let value: String
    let label: String
    let systemImage: String
    
    var body: some View {
        VStack(spacing: 8) {
            Image(systemName: systemImage)
                .font(.title2)
                .foregroundColor(.accentColor)
            
            Text(value)
                .font(.title2)
                .fontWeight(.bold)
                .foregroundColor(.accentColor)
            
            Text(label)
                .font(.caption)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(12)
    }
}

struct LogsView: View {
    let logs: [String]
    
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Logs")
                .font(.headline)
            
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 4) {
                        ForEach(Array(logs.enumerated()), id: \.offset) { index, log in
                            Text(log)
                                .font(.system(.caption, design: .monospaced))
                                .id(index)
                        }
                    }
                    .padding(8)
                }
                .frame(height: 200)
                .background(Color(.systemGray6))
                .cornerRadius(8)
                .onChange(of: logs.count) { _ in
                    withAnimation {
                        proxy.scrollTo(logs.count - 1, anchor: .bottom)
                    }
                }
            }
        }
    }
}

#Preview {
    ContentView()
}
