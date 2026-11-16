#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main() {
    int width1, height1, channels1;
    int width2, height2, channels2;
    
    // Load sample.bmp (reference image)
    unsigned char* sample_data = stbi_load("sample.bmp", &width1, &height1, &channels1, 0);
    if (!sample_data) {
        printf("Could not load sample.bmp\n");
        return 1;
    }
    
    // Load debayer.bmp (our generated image)
    unsigned char* debayer_data = stbi_load("debayer.bmp", &width2, &height2, &channels2, 0);
    if (!debayer_data) {
        printf("Could not load debayer.bmp\n");
        stbi_image_free(sample_data);
        return 1;
    }
    
    printf("Sample image: %dx%d, %d channels\n", width1, height1, channels1);
    printf("Debayer image: %dx%d, %d channels\n", width2, height2, channels2);
    
    if (width1 != width2 || height1 != height2) {
        printf("Images have different dimensions!\n");
        stbi_image_free(sample_data);
        stbi_image_free(debayer_data);
        return 1;
    }
    
    // Compare pixels
    long long total_diff_r = 0, total_diff_g = 0, total_diff_b = 0;
    int max_diff_r = 0, max_diff_g = 0, max_diff_b = 0;
    int total_pixels = width1 * height1;
    
    for (int i = 0; i < total_pixels; i++) {
        // Get RGB values, handling different channel counts
        unsigned char r1 = sample_data[i * channels1];
        unsigned char g1 = sample_data[i * channels1 + 1];
        unsigned char b1 = sample_data[i * channels1 + 2];
        
        unsigned char r2 = debayer_data[i * channels2];
        unsigned char g2 = debayer_data[i * channels2 + 1];
        unsigned char b2 = debayer_data[i * channels2 + 2];
        
        int diff_r = abs(r1 - r2);
        int diff_g = abs(g1 - g2);
        int diff_b = abs(b1 - b2);
        
        total_diff_r += diff_r;
        total_diff_g += diff_g;
        total_diff_b += diff_b;
        
        if (diff_r > max_diff_r) max_diff_r = diff_r;
        if (diff_g > max_diff_g) max_diff_g = diff_g;
        if (diff_b > max_diff_b) max_diff_b = diff_b;
    }
    
    double avg_diff_r = (double)total_diff_r / total_pixels;
    double avg_diff_g = (double)total_diff_g / total_pixels;
    double avg_diff_b = (double)total_diff_b / total_pixels;
    
    printf("\nComparison Results:\n");
    printf("Red channel   - Average diff: %.2f, Max diff: %d\n", avg_diff_r, max_diff_r);
    printf("Green channel - Average diff: %.2f, Max diff: %d\n", avg_diff_g, max_diff_g);
    printf("Blue channel  - Average diff: %.2f, Max diff: %d\n", avg_diff_b, max_diff_b);
    
    double total_avg_diff = (avg_diff_r + avg_diff_g + avg_diff_b) / 3.0;
    printf("Overall average difference: %.2f (out of 255)\n", total_avg_diff);
    printf("Overall similarity: %.1f%%\n", 100.0 * (255.0 - total_avg_diff) / 255.0);
    
    if (total_avg_diff < 10.0) {
        printf("✓ Images are very close!\n");
    } else if (total_avg_diff < 30.0) {
        printf("~ Images are reasonably similar\n");
    } else {
        printf("✗ Images are quite different\n");
    }
    
    stbi_image_free(sample_data);
    stbi_image_free(debayer_data);
    
    return 0;
}