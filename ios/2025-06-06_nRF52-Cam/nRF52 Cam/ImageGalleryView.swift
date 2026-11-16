import SwiftUI

struct ImageGalleryView: View {
    @ObservedObject var imageStore: CameraImageStore
    @Binding var selectedImage: CameraImage?
    @Environment(\.dismiss) var dismiss
    
    let columns = [
        GridItem(.adaptive(minimum: 150))
    ]
    
    var body: some View {
        NavigationView {
            ScrollView {
                if imageStore.images.isEmpty {
                    VStack(spacing: 20) {
                        Image(systemName: "photo.stack")
                            .font(.system(size: 60))
                            .foregroundColor(.secondary)
                        
                        Text("No images in gallery")
                            .font(.headline)
                            .foregroundColor(.secondary)
                    }
                    .frame(maxWidth: .infinity, minHeight: 400)
                } else {
                    LazyVGrid(columns: columns, spacing: 16) {
                        ForEach(imageStore.images) { image in
                            GalleryItemView(image: image) {
                                selectedImage = image
                                dismiss()
                            }
                        }
                    }
                    .padding()
                }
            }
            .navigationTitle("Image Gallery")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Done") {
                        dismiss()
                    }
                }
                
                ToolbarItem(placement: .navigationBarLeading) {
                    if !imageStore.images.isEmpty {
                        Button("Clear All") {
                            imageStore.clear()
                        }
                        .foregroundColor(.red)
                    }
                }
            }
        }
    }
}

struct GalleryItemView: View {
    let image: CameraImage
    let onTap: () -> Void
    
    // Prioritize Ghibli version if available, otherwise show original
    var displayImage: UIImage? {
        if let ghibliImage = image.ghibliImage {
            return ghibliImage
        }
        return image.uiImage
    }
    
    var body: some View {
        VStack(alignment: .center, spacing: 4) {
            if let uiImage = displayImage {
                ZStack(alignment: .topLeading) {
                    Image(uiImage: uiImage)
                        .resizable()
                        .aspectRatio(contentMode: .fill)
                        .frame(height: 150)
                        .clipped()
                        .cornerRadius(8)
                        .onTapGesture {
                            onTap()
                        }
                    
                    // Badge to indicate image type
                    if image.ghibliData != nil {
                        Text("✨")
                            .font(.caption)
                            .padding(4)
                            .background(Color.green.opacity(0.8))
                            .cornerRadius(4)
                            .padding(4)
                    } else if image.isGeneratingGhibli {
                        ProgressView()
                            .progressViewStyle(CircularProgressViewStyle(tint: .white))
                            .scaleEffect(0.7)
                            .padding(4)
                            .background(Color.orange.opacity(0.8))
                            .cornerRadius(4)
                            .padding(4)
                    } else {
                        Text("📷")
                            .font(.caption)
                            .padding(4)
                            .background(Color.blue.opacity(0.8))
                            .cornerRadius(4)
                            .padding(4)
                    }
                }
            }
            
            VStack(spacing: 2) {
                Text(image.timeString)
                    .font(.caption2)
                    .foregroundColor(.secondary)
                
                Text(image.sizeKB)
                    .font(.caption2)
                    .fontWeight(.medium)
                    .foregroundColor(.secondary)
            }
        }
        .frame(maxWidth: .infinity)
        .padding(8)
        .background(Color(.systemGray6))
        .cornerRadius(12)
    }
}

struct ImageDetailView: View {
    let image: CameraImage
    @Environment(\.dismiss) var dismiss
    @State private var scale: CGFloat = 1.0
    @State private var lastScale: CGFloat = 1.0
    @State private var offset: CGSize = .zero
    @State private var lastOffset: CGSize = .zero
    @State private var showingGhibli: Bool
    
    init(image: CameraImage) {
        self.image = image
        // Default to showing Ghibli version if available
        self._showingGhibli = State(initialValue: image.ghibliData != nil)
    }
    
    var displayedImage: UIImage? {
        showingGhibli && image.ghibliImage != nil ? image.ghibliImage : image.uiImage
    }
    
    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            
            if let uiImage = displayedImage {
                Image(uiImage: uiImage)
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .scaleEffect(scale)
                    .offset(offset)
                    .gesture(
                        MagnificationGesture()
                            .onChanged { value in
                                scale = lastScale * value
                            }
                            .onEnded { value in
                                lastScale = scale
                                withAnimation(.spring()) {
                                    if scale < 1 {
                                        scale = 1
                                        lastScale = 1
                                        offset = .zero
                                        lastOffset = .zero
                                    } else if scale > 4 {
                                        scale = 4
                                        lastScale = 4
                                    }
                                }
                            }
                    )
                    .simultaneousGesture(
                        DragGesture()
                            .onChanged { value in
                                if scale > 1 {
                                    offset = CGSize(
                                        width: lastOffset.width + value.translation.width,
                                        height: lastOffset.height + value.translation.height
                                    )
                                }
                            }
                            .onEnded { value in
                                lastOffset = offset
                            }
                    )
                    .onTapGesture(count: 2) {
                        withAnimation(.spring()) {
                            if scale > 1 {
                                scale = 1
                                lastScale = 1
                                offset = .zero
                                lastOffset = .zero
                            } else {
                                scale = 2
                                lastScale = 2
                            }
                        }
                    }
            }
            
            VStack {
                HStack {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(image.timeString)
                            .font(.caption)
                            .fontWeight(.medium)
                        
                        Text(image.sizeKB)
                            .font(.caption)
                        
                        if showingGhibli && image.ghibliGenerationTime > 0 {
                            Text("Ghibli Version (\(image.ghibliGenerationTimeString))")
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        } else {
                            Text(showingGhibli ? "Ghibli Version" : "Original")
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }
                    }
                    .padding(12)
                    .background(.ultraThinMaterial)
                    .cornerRadius(8)
                    
                    Spacer()
                    
                    // Toggle button for Ghibli version
                    if image.ghibliData != nil {
                        Button(action: {
                            withAnimation(.spring()) {
                                showingGhibli.toggle()
                                scale = 1
                                lastScale = 1
                                offset = .zero
                                lastOffset = .zero
                            }
                        }) {
                            Image(systemName: showingGhibli ? "photo" : "sparkles")
                                .font(.title2)
                                .foregroundStyle(.white)
                                .padding(8)
                                .background(.ultraThinMaterial)
                                .cornerRadius(8)
                        }
                    }
                    
                    Button(action: {
                        dismiss()
                    }) {
                        Image(systemName: "xmark.circle.fill")
                            .font(.title)
                            .symbolRenderingMode(.hierarchical)
                            .foregroundStyle(.white)
                    }
                    .padding(12)
                }
                .padding()
                
                Spacer()
                
                HStack(spacing: 20) {
                    if let uiImage = displayedImage {
                        ShareLink(item: Image(uiImage: uiImage), preview: SharePreview("Camera Image", image: Image(uiImage: uiImage))) {
                            Label("Share", systemImage: "square.and.arrow.up")
                                .padding(.horizontal, 20)
                                .padding(.vertical, 12)
                                .background(.ultraThinMaterial)
                                .cornerRadius(25)
                        }
                    }
                }
                .padding(.bottom, 40)
            }
        }
    }
}