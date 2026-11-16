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

void shift_bitstream(uint8_t* input, uint8_t* output, int num_bytes, int shift_bits) {
    if (shift_bits == 0) {
        memcpy(output, input, num_bytes);
        return;
    }
    
    // Convert bytes to bit array
    int total_bits = num_bytes * 8;
    uint8_t* bit_array = malloc(total_bits);
    
    for (int i = 0; i < num_bytes; i++) {
        uint8_t byte = input[i];
        for (int bit = 0; bit < 8; bit++) {
            bit_array[i * 8 + bit] = (byte >> (7 - bit)) & 1;
        }
    }
    
    // Create shifted bit array
    uint8_t* shifted_bits = calloc(total_bits, 1);
    
    if (shift_bits > 0) {
        // Right shift
        for (int i = 0; i < total_bits - shift_bits; i++) {
            shifted_bits[i + shift_bits] = bit_array[i];
        }
    } else {
        // Left shift
        int left_shift = -shift_bits;
        for (int i = left_shift; i < total_bits; i++) {
            shifted_bits[i - left_shift] = bit_array[i];
        }
    }
    
    // Convert back to bytes
    for (int i = 0; i < total_bits; i += 8) {
        uint8_t byte = 0;
        for (int bit = 0; bit < 8 && i + bit < total_bits; bit++) {
            if (shifted_bits[i + bit]) {
                byte |= (1 << (7 - bit));
            }
        }
        output[i / 8] = byte;
    }
    
    free(bit_array);
    free(shifted_bits);
}

void debayer_grgb(uint8_t* raw_data, uint8_t* rgb_output, int width, int height) {
    // GRGB Bayer pattern:
    // G R G R ...
    // B G B G ...
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            int rgb_idx = idx * 3;
            
            uint8_t r, g, b;
            uint8_t center = raw_data[idx];
            
            if (y % 2 == 0) { // Even rows: G R G R ...
                if (x % 2 == 0) { // Green pixel
                    g = center;
                    // Get R from right neighbor
                    r = (x < width - 1) ? raw_data[idx + 1] : center;
                    // Get B from bottom neighbor
                    b = (y < height - 1) ? raw_data[(y + 1) * width + x] : center;
                } else { // Red pixel
                    r = center;
                    // Get G from neighbors (average)
                    int g_sum = 0, g_count = 0;
                    if (x > 0) { g_sum += raw_data[idx - 1]; g_count++; }
                    if (x < width - 1) { g_sum += raw_data[idx + 1]; g_count++; }
                    if (y > 0) { g_sum += raw_data[(y - 1) * width + x]; g_count++; }
                    if (y < height - 1) { g_sum += raw_data[(y + 1) * width + x]; g_count++; }
                    g = g_count > 0 ? g_sum / g_count : center;
                    // Get B from diagonal neighbors
                    int b_sum = 0, b_count = 0;
                    if (y > 0 && x > 0) { b_sum += raw_data[(y - 1) * width + (x - 1)]; b_count++; }
                    if (y > 0 && x < width - 1) { b_sum += raw_data[(y - 1) * width + (x + 1)]; b_count++; }
                    if (y < height - 1 && x > 0) { b_sum += raw_data[(y + 1) * width + (x - 1)]; b_count++; }
                    if (y < height - 1 && x < width - 1) { b_sum += raw_data[(y + 1) * width + (x + 1)]; b_count++; }
                    b = b_count > 0 ? b_sum / b_count : center;
                }
            } else { // Odd rows: B G B G ...
                if (x % 2 == 0) { // Blue pixel
                    b = center;
                    // Get G from neighbors
                    int g_sum = 0, g_count = 0;
                    if (x > 0) { g_sum += raw_data[idx - 1]; g_count++; }
                    if (x < width - 1) { g_sum += raw_data[idx + 1]; g_count++; }
                    if (y > 0) { g_sum += raw_data[(y - 1) * width + x]; g_count++; }
                    if (y < height - 1) { g_sum += raw_data[(y + 1) * width + x]; g_count++; }
                    g = g_count > 0 ? g_sum / g_count : center;
                    // Get R from diagonal neighbors
                    int r_sum = 0, r_count = 0;
                    if (y > 0 && x > 0) { r_sum += raw_data[(y - 1) * width + (x - 1)]; r_count++; }
                    if (y > 0 && x < width - 1) { r_sum += raw_data[(y - 1) * width + (x + 1)]; r_count++; }
                    if (y < height - 1 && x > 0) { r_sum += raw_data[(y + 1) * width + (x - 1)]; r_count++; }
                    if (y < height - 1 && x < width - 1) { r_sum += raw_data[(y + 1) * width + (x + 1)]; r_count++; }
                    r = r_count > 0 ? r_sum / r_count : center;
                } else { // Green pixel
                    g = center;
                    // Get B from left neighbor
                    b = (x > 0) ? raw_data[idx - 1] : center;
                    // Get R from top neighbor
                    r = (y > 0) ? raw_data[(y - 1) * width + x] : center;
                }
            }
            
            rgb_output[rgb_idx] = r;
            rgb_output[rgb_idx + 1] = g;
            rgb_output[rgb_idx + 2] = b;
        }
    }
}

int main() {
    FILE* file = fopen("source.txt", "r");
    if (!file) {
        printf("Could not open source.txt\n");
        return 1;
    }
    
    // Skip the first empty line
    char line[4096];
    fgets(line, sizeof(line), file);
    
    // Allocate memory for raw data
    uint8_t* raw_data = malloc(WIDTH * HEIGHT);
    uint8_t* shifted_data = malloc(WIDTH * HEIGHT);
    uint8_t* rgb_data = malloc(WIDTH * HEIGHT * 3);
    
    if (!raw_data || !shifted_data || !rgb_data) {
        printf("Memory allocation failed\n");
        return 1;
    }
    
    int pixel_count = 0;
    
    // Read hex data from file
    while (fgets(line, sizeof(line), file) && pixel_count < WIDTH * HEIGHT) {
        char* ptr = line;
        while (*ptr && pixel_count < WIDTH * HEIGHT) {
            // Skip whitespace
            while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') {
                ptr++;
            }
            
            if (*ptr == '\0') break;
            
            // Read two hex characters
            if (ptr[0] && ptr[1]) {
                unsigned int value;
                if (sscanf(ptr, "%2x", &value) == 1) {
                    raw_data[pixel_count] = (uint8_t)value;
                    pixel_count++;
                }
                ptr += 2;
            } else {
                break;
            }
        }
    }
    
    fclose(file);
    
    printf("Read %d pixels from source.txt\n", pixel_count);
    
    if (pixel_count != WIDTH * HEIGHT) {
        printf("Warning: Expected %d pixels, got %d\n", WIDTH * HEIGHT, pixel_count);
    }
    
    // Apply bitstream shift of 3 bits (the optimal value found)
    shift_bitstream(raw_data, shifted_data, WIDTH * HEIGHT, 3);
    
    // Apply GRGB debayering
    debayer_grgb(shifted_data, rgb_data, WIDTH, HEIGHT);
    
    // Save as BMP
    if (stbi_write_bmp("debayer.bmp", WIDTH, HEIGHT, 3, rgb_data)) {
        printf("Successfully saved debayer.bmp (%dx%d)\n", WIDTH, HEIGHT);
    } else {
        printf("Failed to save debayer.bmp\n");
        free(raw_data);
        free(shifted_data);
        free(rgb_data);
        return 1;
    }
    
    // Compare with sample.bmp
    int sample_width, sample_height, sample_channels;
    unsigned char* sample_data = stbi_load("sample.bmp", &sample_width, &sample_height, &sample_channels, 0);
    if (!sample_data) {
        printf("Could not load sample.bmp for comparison\n");
        free(raw_data);
        free(shifted_data);
        free(rgb_data);
        return 1;
    }
    
    printf("\nComparison with sample.bmp:\n");
    printf("Sample image: %dx%d, %d channels\n", sample_width, sample_height, sample_channels);
    printf("Debayer image: %dx%d, 3 channels\n", WIDTH, HEIGHT);
    
    if (sample_width != WIDTH || sample_height != HEIGHT) {
        printf("Images have different dimensions!\n");
        stbi_image_free(sample_data);
        free(raw_data);
        free(shifted_data);
        free(rgb_data);
        return 1;
    }
    
    // Compare pixels
    long long total_diff_r = 0, total_diff_g = 0, total_diff_b = 0;
    int max_diff_r = 0, max_diff_g = 0, max_diff_b = 0;
    int total_pixels = WIDTH * HEIGHT;
    
    for (int i = 0; i < total_pixels; i++) {
        // Get RGB values from sample
        unsigned char r1 = sample_data[i * sample_channels];
        unsigned char g1 = sample_data[i * sample_channels + 1];
        unsigned char b1 = sample_data[i * sample_channels + 2];
        
        // Get RGB values from our debayered image
        unsigned char r2 = rgb_data[i * 3];
        unsigned char g2 = rgb_data[i * 3 + 1];
        unsigned char b2 = rgb_data[i * 3 + 2];
        
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
    
    if (total_avg_diff < 2.5) {
        printf("✓ Images are very close!\n");
    } else if (total_avg_diff < 10.0) {
        printf("~ Images are reasonably similar\n");
    } else {
        printf("✗ Images are quite different\n");
    }
    
    stbi_image_free(sample_data);
    free(raw_data);
    free(shifted_data);
    free(rgb_data);
    
    return 0;
}