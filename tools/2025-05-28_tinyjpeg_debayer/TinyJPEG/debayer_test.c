#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define WIDTH 330
#define HEIGHT 330

// Different Bayer patterns to test
typedef enum {
    BAYER_GRGB = 0,
    BAYER_RGGB = 1,
    BAYER_BGGR = 2,
    BAYER_GBRG = 3
} BayerPattern;

void debayer_bilinear(uint8_t* raw_data, uint8_t* rgb_output, int width, int height, BayerPattern pattern) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int raw_idx = y * width + x;
            int rgb_idx = (y * width + x) * 3;
            
            uint8_t r, g, b;
            
            // Determine what color this pixel is based on pattern and position
            int color;
            switch (pattern) {
                case BAYER_GRGB: // G R G R ... / B G B G ...
                    if (y % 2 == 0) color = (x % 2 == 0) ? 1 : 0; // G or R
                    else color = (x % 2 == 0) ? 2 : 1; // B or G
                    break;
                case BAYER_RGGB: // R G R G ... / G B G B ...
                    if (y % 2 == 0) color = (x % 2 == 0) ? 0 : 1; // R or G
                    else color = (x % 2 == 0) ? 1 : 2; // G or B
                    break;
                case BAYER_BGGR: // B G B G ... / G R G R ...
                    if (y % 2 == 0) color = (x % 2 == 0) ? 2 : 1; // B or G
                    else color = (x % 2 == 0) ? 1 : 0; // G or R
                    break;
                case BAYER_GBRG: // G B G B ... / R G R G ...
                    if (y % 2 == 0) color = (x % 2 == 0) ? 1 : 2; // G or B
                    else color = (x % 2 == 0) ? 0 : 1; // R or G
                    break;
            }
            
            if (color == 0) { // Red pixel
                r = raw_data[raw_idx];
                
                // Interpolate green
                int g_sum = 0, g_count = 0;
                if (x > 0) { g_sum += raw_data[raw_idx - 1]; g_count++; }
                if (x < width - 1) { g_sum += raw_data[raw_idx + 1]; g_count++; }
                if (y > 0) { g_sum += raw_data[(y - 1) * width + x]; g_count++; }
                if (y < height - 1) { g_sum += raw_data[(y + 1) * width + x]; g_count++; }
                g = g_count > 0 ? g_sum / g_count : 0;
                
                // Interpolate blue
                int b_sum = 0, b_count = 0;
                if (y > 0 && x > 0) { b_sum += raw_data[(y - 1) * width + (x - 1)]; b_count++; }
                if (y > 0 && x < width - 1) { b_sum += raw_data[(y - 1) * width + (x + 1)]; b_count++; }
                if (y < height - 1 && x > 0) { b_sum += raw_data[(y + 1) * width + (x - 1)]; b_count++; }
                if (y < height - 1 && x < width - 1) { b_sum += raw_data[(y + 1) * width + (x + 1)]; b_count++; }
                b = b_count > 0 ? b_sum / b_count : 0;
                
            } else if (color == 1) { // Green pixel
                g = raw_data[raw_idx];
                
                // Interpolate red and blue based on neighbors
                int r_sum = 0, r_count = 0;
                int b_sum = 0, b_count = 0;
                
                // Check horizontal neighbors
                if (x > 0) {
                    int left_color;
                    switch (pattern) {
                        case BAYER_GRGB: left_color = (y % 2 == 0) ? 1 : 2; break;
                        case BAYER_RGGB: left_color = (y % 2 == 0) ? 0 : 1; break;
                        case BAYER_BGGR: left_color = (y % 2 == 0) ? 2 : 1; break;
                        case BAYER_GBRG: left_color = (y % 2 == 0) ? 1 : 0; break;
                    }
                    if (left_color == 0) { r_sum += raw_data[raw_idx - 1]; r_count++; }
                    else if (left_color == 2) { b_sum += raw_data[raw_idx - 1]; b_count++; }
                }
                if (x < width - 1) {
                    int right_color;
                    switch (pattern) {
                        case BAYER_GRGB: right_color = (y % 2 == 0) ? 0 : 1; break;
                        case BAYER_RGGB: right_color = (y % 2 == 0) ? 1 : 2; break;
                        case BAYER_BGGR: right_color = (y % 2 == 0) ? 1 : 0; break;
                        case BAYER_GBRG: right_color = (y % 2 == 0) ? 2 : 1; break;
                    }
                    if (right_color == 0) { r_sum += raw_data[raw_idx + 1]; r_count++; }
                    else if (right_color == 2) { b_sum += raw_data[raw_idx + 1]; b_count++; }
                }
                
                // Check vertical neighbors
                if (y > 0) {
                    int top_color;
                    switch (pattern) {
                        case BAYER_GRGB: top_color = (x % 2 == 0) ? 1 : 0; break;
                        case BAYER_RGGB: top_color = (x % 2 == 0) ? 1 : 2; break;
                        case BAYER_BGGR: top_color = (x % 2 == 0) ? 1 : 0; break;
                        case BAYER_GBRG: top_color = (x % 2 == 0) ? 0 : 1; break;
                    }
                    if (top_color == 0) { r_sum += raw_data[(y - 1) * width + x]; r_count++; }
                    else if (top_color == 2) { b_sum += raw_data[(y - 1) * width + x]; b_count++; }
                }
                if (y < height - 1) {
                    int bottom_color;
                    switch (pattern) {
                        case BAYER_GRGB: bottom_color = (x % 2 == 0) ? 2 : 1; break;
                        case BAYER_RGGB: bottom_color = (x % 2 == 0) ? 0 : 1; break;
                        case BAYER_BGGR: bottom_color = (x % 2 == 0) ? 1 : 2; break;
                        case BAYER_GBRG: bottom_color = (x % 2 == 0) ? 1 : 0; break;
                    }
                    if (bottom_color == 0) { r_sum += raw_data[(y + 1) * width + x]; r_count++; }
                    else if (bottom_color == 2) { b_sum += raw_data[(y + 1) * width + x]; b_count++; }
                }
                
                r = r_count > 0 ? r_sum / r_count : 0;
                b = b_count > 0 ? b_sum / b_count : 0;
                
            } else { // Blue pixel
                b = raw_data[raw_idx];
                
                // Interpolate green
                int g_sum = 0, g_count = 0;
                if (x > 0) { g_sum += raw_data[raw_idx - 1]; g_count++; }
                if (x < width - 1) { g_sum += raw_data[raw_idx + 1]; g_count++; }
                if (y > 0) { g_sum += raw_data[(y - 1) * width + x]; g_count++; }
                if (y < height - 1) { g_sum += raw_data[(y + 1) * width + x]; g_count++; }
                g = g_count > 0 ? g_sum / g_count : 0;
                
                // Interpolate red
                int r_sum = 0, r_count = 0;
                if (y > 0 && x > 0) { r_sum += raw_data[(y - 1) * width + (x - 1)]; r_count++; }
                if (y > 0 && x < width - 1) { r_sum += raw_data[(y - 1) * width + (x + 1)]; r_count++; }
                if (y < height - 1 && x > 0) { r_sum += raw_data[(y + 1) * width + (x - 1)]; r_count++; }
                if (y < height - 1 && x < width - 1) { r_sum += raw_data[(y + 1) * width + (x + 1)]; r_count++; }
                r = r_count > 0 ? r_sum / r_count : 0;
            }
            
            rgb_output[rgb_idx] = r;
            rgb_output[rgb_idx + 1] = g;
            rgb_output[rgb_idx + 2] = b;
        }
    }
}

double compare_with_sample(uint8_t* rgb_data, const char* pattern_name) {
    int sample_width, sample_height, sample_channels;
    unsigned char* sample_data = stbi_load("sample.bmp", &sample_width, &sample_height, &sample_channels, 0);
    if (!sample_data) {
        printf("Could not load sample.bmp\n");
        return 0.0;
    }
    
    if (sample_width != WIDTH || sample_height != HEIGHT) {
        printf("Dimension mismatch\n");
        stbi_image_free(sample_data);
        return 0.0;
    }
    
    long long total_diff = 0;
    int total_pixels = WIDTH * HEIGHT;
    
    for (int i = 0; i < total_pixels; i++) {
        unsigned char r1 = sample_data[i * sample_channels];
        unsigned char g1 = sample_data[i * sample_channels + 1];
        unsigned char b1 = sample_data[i * sample_channels + 2];
        
        unsigned char r2 = rgb_data[i * 3];
        unsigned char g2 = rgb_data[i * 3 + 1];
        unsigned char b2 = rgb_data[i * 3 + 2];
        
        total_diff += abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2);
    }
    
    double avg_diff = (double)total_diff / (total_pixels * 3);
    double similarity = 100.0 * (255.0 - avg_diff) / 255.0;
    
    printf("%s: %.2f%% similar (avg diff: %.2f)\n", pattern_name, similarity, avg_diff);
    
    stbi_image_free(sample_data);
    return similarity;
}

int main() {
    FILE* file = fopen("source.txt", "r");
    if (!file) {
        printf("Could not open source.txt\n");
        return 1;
    }
    
    char line[4096];
    fgets(line, sizeof(line), file); // Skip first empty line
    
    uint8_t* raw_data = malloc(WIDTH * HEIGHT);
    uint8_t* rgb_data = malloc(WIDTH * HEIGHT * 3);
    
    // Test different bit shifts
    for (int shift = 0; shift <= 5; shift++) {
        printf("\n=== Testing bit shift: %d ===\n", shift);
        
        // Reset file position
        rewind(file);
        fgets(line, sizeof(line), file); // Skip first line
        
        int pixel_count = 0;
        
        // Read and process data with current shift
        while (fgets(line, sizeof(line), file) && pixel_count < WIDTH * HEIGHT) {
            char* ptr = line;
            while (*ptr && pixel_count < WIDTH * HEIGHT) {
                while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') ptr++;
                if (*ptr == '\0') break;
                
                if (ptr[0] && ptr[1]) {
                    unsigned int value;
                    if (sscanf(ptr, "%2x", &value) == 1) {
                        raw_data[pixel_count] = (uint8_t)(value >> shift);
                        pixel_count++;
                    }
                    ptr += 2;
                } else {
                    break;
                }
            }
        }
        
        // Test different Bayer patterns
        const char* pattern_names[] = {"GRGB", "RGGB", "BGGR", "GBRG"};
        double best_similarity = 0.0;
        
        for (int pattern = 0; pattern < 4; pattern++) {
            debayer_bilinear(raw_data, rgb_data, WIDTH, HEIGHT, (BayerPattern)pattern);
            double similarity = compare_with_sample(rgb_data, pattern_names[pattern]);
            if (similarity > best_similarity) {
                best_similarity = similarity;
            }
        }
        
        printf("Best similarity for shift %d: %.2f%%\n", shift, best_similarity);
        
        if (best_similarity > 99.0) {
            printf("*** FOUND SOLUTION: shift=%d, similarity=%.2f%% ***\n", shift, best_similarity);
            break;
        }
    }
    
    fclose(file);
    free(raw_data);
    free(rgb_data);
    
    return 0;
}