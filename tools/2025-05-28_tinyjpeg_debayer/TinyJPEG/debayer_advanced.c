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

// Try exact pixel copy without interpolation
void debayer_nearest(uint8_t* raw_data, uint8_t* rgb_output, int width, int height) {
    // Try treating raw data as already RGB
    for (int i = 0; i < width * height; i++) {
        rgb_output[i * 3] = raw_data[i];     // R = raw
        rgb_output[i * 3 + 1] = raw_data[i]; // G = raw
        rgb_output[i * 3 + 2] = raw_data[i]; // B = raw
    }
}

// Try different scaling factors
void debayer_scaled(uint8_t* raw_data, uint8_t* rgb_output, int width, int height, double scale) {
    for (int i = 0; i < width * height; i++) {
        int scaled_val = (int)(raw_data[i] * scale);
        if (scaled_val > 255) scaled_val = 255;
        if (scaled_val < 0) scaled_val = 0;
        
        rgb_output[i * 3] = scaled_val;
        rgb_output[i * 3 + 1] = scaled_val;
        rgb_output[i * 3 + 2] = scaled_val;
    }
}

double compare_with_sample(uint8_t* rgb_data, const char* method_name) {
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
    
    printf("%s: %.2f%% similar (avg diff: %.2f)\n", method_name, similarity, avg_diff);
    
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
    
    double best_similarity = 0.0;
    int best_shift = -1;
    double best_scale = 1.0;
    
    // Test more bit shifts and scaling combinations
    for (int shift = 0; shift <= 8; shift++) {
        printf("\n=== Testing bit shift: %d ===\n", shift);
        
        // Reset file position
        rewind(file);
        fgets(line, sizeof(line), file);
        
        int pixel_count = 0;
        
        // Read data with current shift
        while (fgets(line, sizeof(line), file) && pixel_count < WIDTH * HEIGHT) {
            char* ptr = line;
            while (*ptr && pixel_count < WIDTH * HEIGHT) {
                while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') ptr++;
                if (*ptr == '\0') break;
                
                if (ptr[0] && ptr[1]) {
                    unsigned int value;
                    if (sscanf(ptr, "%2x", &value) == 1) {
                        if (shift == 0) {
                            raw_data[pixel_count] = (uint8_t)value;
                        } else {
                            raw_data[pixel_count] = (uint8_t)(value >> shift);
                        }
                        pixel_count++;
                    }
                    ptr += 2;
                } else {
                    break;
                }
            }
        }
        
        printf("Read %d pixels\n", pixel_count);
        
        // Test direct copy (grayscale)
        debayer_nearest(raw_data, rgb_data, WIDTH, HEIGHT);
        double similarity = compare_with_sample(rgb_data, "Direct copy");
        if (similarity > best_similarity) {
            best_similarity = similarity;
            best_shift = shift;
            best_scale = 1.0;
        }
        
        // Test different scaling factors
        for (double scale = 0.5; scale <= 8.0; scale += 0.5) {
            debayer_scaled(raw_data, rgb_data, WIDTH, HEIGHT, scale);
            char scale_name[64];
            snprintf(scale_name, sizeof(scale_name), "Scale %.1fx", scale);
            similarity = compare_with_sample(rgb_data, scale_name);
            if (similarity > best_similarity) {
                best_similarity = similarity;
                best_shift = shift;
                best_scale = scale;
            }
        }
        
        if (best_similarity > 99.0) {
            printf("*** FOUND SOLUTION: shift=%d, scale=%.1f, similarity=%.2f%% ***\n", 
                   best_shift, best_scale, best_similarity);
            break;
        }
    }
    
    printf("\n=== BEST RESULT ===\n");
    printf("Best similarity: %.2f%% with shift=%d, scale=%.1f\n", best_similarity, best_shift, best_scale);
    
    // Generate final image with best parameters
    if (best_similarity > 85.0) {
        rewind(file);
        fgets(line, sizeof(line), file);
        
        int pixel_count = 0;
        while (fgets(line, sizeof(line), file) && pixel_count < WIDTH * HEIGHT) {
            char* ptr = line;
            while (*ptr && pixel_count < WIDTH * HEIGHT) {
                while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') ptr++;
                if (*ptr == '\0') break;
                
                if (ptr[0] && ptr[1]) {
                    unsigned int value;
                    if (sscanf(ptr, "%2x", &value) == 1) {
                        if (best_shift == 0) {
                            raw_data[pixel_count] = (uint8_t)value;
                        } else {
                            raw_data[pixel_count] = (uint8_t)(value >> best_shift);
                        }
                        pixel_count++;
                    }
                    ptr += 2;
                } else {
                    break;
                }
            }
        }
        
        debayer_scaled(raw_data, rgb_data, WIDTH, HEIGHT, best_scale);
        
        if (stbi_write_bmp("debayer_best.bmp", WIDTH, HEIGHT, 3, rgb_data)) {
            printf("Saved best result as debayer_best.bmp\n");
        }
    }
    
    fclose(file);
    free(raw_data);
    free(rgb_data);
    
    return 0;
}