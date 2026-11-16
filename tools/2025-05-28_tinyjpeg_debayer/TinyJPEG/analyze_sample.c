#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
    int width, height, channels;
    unsigned char* data = stbi_load("sample.bmp", &width, &height, &channels, 0);
    if (!data) {
        printf("Could not load sample.bmp\n");
        return 1;
    }
    
    printf("Sample.bmp: %dx%d, %d channels\n", width, height, channels);
    
    // Analyze the image characteristics
    long long r_sum = 0, g_sum = 0, b_sum = 0;
    int r_min = 255, r_max = 0;
    int g_min = 255, g_max = 0;
    int b_min = 255, b_max = 0;
    
    // Sample first few pixels for pattern analysis
    printf("\nFirst 20 pixels (RGB values):\n");
    for (int i = 0; i < 20 && i < width * height; i++) {
        unsigned char r = data[i * channels];
        unsigned char g = data[i * channels + 1];
        unsigned char b = data[i * channels + 2];
        
        printf("Pixel %d: R=%d G=%d B=%d\n", i, r, g, b);
        
        r_sum += r; g_sum += g; b_sum += b;
        if (r < r_min) r_min = r; if (r > r_max) r_max = r;
        if (g < g_min) g_min = g; if (g > g_max) g_max = g;
        if (b < b_min) b_min = b; if (b > b_max) b_max = b;
    }
    
    // Analyze entire image
    r_sum = g_sum = b_sum = 0;
    r_min = g_min = b_min = 255;
    r_max = g_max = b_max = 0;
    
    for (int i = 0; i < width * height; i++) {
        unsigned char r = data[i * channels];
        unsigned char g = data[i * channels + 1];
        unsigned char b = data[i * channels + 2];
        
        r_sum += r; g_sum += g; b_sum += b;
        if (r < r_min) r_min = r; if (r > r_max) r_max = r;
        if (g < g_min) g_min = g; if (g > g_max) g_max = g;
        if (b < b_min) b_min = b; if (b > b_max) b_max = b;
    }
    
    int total_pixels = width * height;
    printf("\nOverall statistics:\n");
    printf("Red   - Min: %3d, Max: %3d, Avg: %.1f\n", r_min, r_max, (double)r_sum / total_pixels);
    printf("Green - Min: %3d, Max: %3d, Avg: %.1f\n", g_min, g_max, (double)g_sum / total_pixels);
    printf("Blue  - Min: %3d, Max: %3d, Avg: %.1f\n", b_min, b_max, (double)b_sum / total_pixels);
    
    // Check if it's grayscale (R=G=B)
    int grayscale_count = 0;
    for (int i = 0; i < width * height; i++) {
        unsigned char r = data[i * channels];
        unsigned char g = data[i * channels + 1];
        unsigned char b = data[i * channels + 2];
        
        if (r == g && g == b) {
            grayscale_count++;
        }
    }
    
    printf("\nGrayscale analysis:\n");
    printf("Pixels where R=G=B: %d out of %d (%.1f%%)\n", 
           grayscale_count, total_pixels, 
           100.0 * grayscale_count / total_pixels);
    
    // If mostly grayscale, extract the grayscale values
    if (grayscale_count > total_pixels * 0.9) {
        printf("\nImage appears to be grayscale. Extracting grayscale values:\n");
        
        FILE* gray_file = fopen("sample_grayscale.txt", "w");
        if (gray_file) {
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    int idx = y * width + x;
                    unsigned char gray = data[idx * channels]; // Use red channel
                    fprintf(gray_file, "%02X ", gray);
                    if ((x + 1) % 16 == 0) fprintf(gray_file, "\n");
                }
                if (width % 16 != 0) fprintf(gray_file, "\n");
            }
            fclose(gray_file);
            printf("Saved grayscale data to sample_grayscale.txt\n");
        }
    }
    
    stbi_image_free(data);
    return 0;
}