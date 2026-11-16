#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define WIDTH 330

typedef struct {
    uint8_t r, g, b;
} RGB;

// Forward declaration
int find_line_alignment_full(uint8_t* line, int length);

void shift_line_right(uint8_t* line, int length, int shift_bits) {
    if (shift_bits == 0 || shift_bits > 7) return;

    for (int i = length - 1; i >= 0; i--) {
        line[i] >>= shift_bits;
        if (i > 0) {
            line[i] |= (line[i-1] & ((1 << shift_bits) - 1)) << (8 - shift_bits);
        }
    }
}

int fast_find_alignment(uint8_t* line, int length) {
    // For 6-bit data, there must be at least 2 consecutive zero bits
    // Check bytes at specific indices: 10, 100, 200
    if (length < 201) return find_line_alignment_full(line, length);
    
    // Sample at fixed positions
    int positions[3] = {10, 100, 200};
    
    // Count zeros at each bit position across the samples
    int bit_zeros[8] = {0};
    
    // Check the byte at each specified position
    for (int i = 0; i < 3; i++) {
        uint8_t byte_val = line[positions[i]];
        
        // Count zeros at each bit position
        for (int bit = 0; bit < 8; bit++) {
            if ((byte_val & (1 << bit)) == 0) {
                bit_zeros[bit]++;
            }
        }
    }
    
    // Find the rightmost bit position that has all zeros
    // We sampled 3 bytes, so look for positions with 3 zeros
    for (int bit = 7; bit >= 0; bit--) {
        if (bit_zeros[bit] == 3) {
            return bit;
        }
    }
    
    // If no perfect match, look for positions with 2 zeros
    for (int bit = 7; bit >= 0; bit--) {
        if (bit_zeros[bit] >= 2) {
            return bit;
        }
    }
    
    // Fallback to full scan
    return find_line_alignment_full(line, length);
}

int find_line_alignment_full(uint8_t* line, int length) {
    // Array to count 1's at each bit offset (0-7)
    int bit_counts[8] = {0};
    
    for (int i = 0; i < length; i++) {
        uint8_t byte_val = line[i];
        // Unrolled loop to count 1's at each bit position (LSB to MSB)
        if (byte_val & 0x01) bit_counts[0]++;  // bit 0
        if (byte_val & 0x02) bit_counts[1]++;  // bit 1
        if (byte_val & 0x04) bit_counts[2]++;  // bit 2
        if (byte_val & 0x08) bit_counts[3]++;  // bit 3
        if (byte_val & 0x10) bit_counts[4]++;  // bit 4
        if (byte_val & 0x20) bit_counts[5]++;  // bit 5
        if (byte_val & 0x40) bit_counts[6]++;  // bit 6
        if (byte_val & 0x80) bit_counts[7]++;  // bit 7
    }
    
    // Find rightmost zero (last zero bit position from right)
    int line_alignment = 0;
    for (int i = 7; i >= 0; i--) {
        if (bit_counts[i] == 0) {
            line_alignment = i;
            break;
        }
    }
    
    return line_alignment;
}

int find_line_alignment_full_6b(uint8_t* line, int length) {
    int pair_zero_counts[8] = {0};
    
    for (int i = 0; i < length; i++) {
        uint8_t byte_val = line[i];
        
        if ((byte_val & 0x03) == 0) pair_zero_counts[0]++;
        if ((byte_val & 0x06) == 0) pair_zero_counts[1]++;
        if ((byte_val & 0x0C) == 0) pair_zero_counts[2]++;
        if ((byte_val & 0x18) == 0) pair_zero_counts[3]++;
        if ((byte_val & 0x30) == 0) pair_zero_counts[4]++;
        if ((byte_val & 0x60) == 0) pair_zero_counts[5]++;
        if ((byte_val & 0xC0) == 0) pair_zero_counts[6]++;
        if ((byte_val & 0x81) == 0) pair_zero_counts[7]++;
    }
    
    int max_zeros = 0;
    int best_alignment = 0;
    
    for (int i = 7; i >= 0; i--) {
        if (pair_zero_counts[i] > max_zeros) {
            max_zeros = pair_zero_counts[i];
            best_alignment = i;
        }
    }
    
    return best_alignment;
}

int read_and_align_hex_file(const char* filename, uint8_t* data) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open %s\n", filename);
        return 0;
    }

    char buffer[4096];
    int line_num = 0;
    int total_index = 0;
    uint8_t line_data[WIDTH];

    while (fgets(buffer, sizeof(buffer), file)) {
        line_num++;
        
        // Skip empty lines
        if (strlen(buffer) < 10) continue;
        
        // Read hex values into line buffer
        int line_index = 0;
        char* ptr = buffer;
        while (*ptr && line_index < WIDTH) {
            if (isxdigit(*ptr)) {
                char hex_str[3] = {0};
                int hex_len = 0;

                // Extract hex digits (up to 2)
                while (isxdigit(*ptr) && hex_len < 2) {
                    hex_str[hex_len++] = *ptr++;
                }

                // Convert to byte
                line_data[line_index++] = (uint8_t)strtol(hex_str, NULL, 16);
            } else {
                ptr++;
            }
        }
        
        // Find alignment for this line using all methods for comparison
        int fast_alignment = fast_find_alignment(line_data, line_index);
        int full_alignment = find_line_alignment_full(line_data, line_index);
        int sixb_alignment = find_line_alignment_full_6b(line_data, line_index);
        
        // Use the 6-bit alignment method
        int alignment = sixb_alignment;
        
        // Apply shift to this line
        shift_line_right(line_data, line_index, alignment);
        
        // Copy shifted line to output data
        for (int i = 0; i < line_index && total_index < WIDTH * WIDTH; i++) {
            data[total_index++] = line_data[i];
        }
        
        // Print line info
        if (fast_alignment != full_alignment || sixb_alignment != full_alignment) {
            printf("Line %d: %d bytes, fast: %d, full: %d, 6b: %d (using 6b)\n", 
                   line_num, line_index, fast_alignment, full_alignment, sixb_alignment);
        } else {
            printf("Line %d: %d bytes, alignment: %d bits\n", line_num, line_index, alignment);
        }
    }

    fclose(file);
    return total_index;
}

uint8_t get_pixel_value(uint8_t* data, int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= WIDTH) {
        return 0;
    }
    int index = y * WIDTH + x;
    return data[index];
}

RGB debayer_pixel(uint8_t* data, int x, int y) {
    RGB pixel;
    uint8_t center = get_pixel_value(data, x, y);

    // GRBG pattern: G R / B G
    switch (((y % 2) << 1) | (x % 2)) {
        case 0: // G position (even row, even col)
            pixel.g = center;
            pixel.r = get_pixel_value(data, x + 1, y);
            pixel.b = get_pixel_value(data, x, y + 1);
            break;

        case 1: // R position (even row, odd col)
            pixel.r = center;
            pixel.g = (get_pixel_value(data, x - 1, y) + get_pixel_value(data, x + 1, y)) / 2;
            pixel.b = (get_pixel_value(data, x - 1, y - 1) + get_pixel_value(data, x - 1, y + 1)) / 2;
            break;

        case 2: // B position (odd row, even col)
            pixel.b = center;
            pixel.g = (get_pixel_value(data, x, y - 1) + get_pixel_value(data, x, y + 1)) / 2;
            pixel.r = (get_pixel_value(data, x + 1, y - 1) + get_pixel_value(data, x + 1, y + 1)) / 2;
            break;

        case 3: // G position (odd row, odd col)
            pixel.g = center;
            pixel.b = get_pixel_value(data, x - 1, y);
            pixel.r = get_pixel_value(data, x, y - 1);
            break;
    }

    return pixel;
}

int main() {
    printf("Reading and aligning data line by line...\n");
    
    // Allocate data buffer
    uint8_t* data = malloc(WIDTH * WIDTH);
    if (!data) {
        printf("Error: Memory allocation failed for data\n");
        return 1;
    }

    // Read and align data line by line
    int data_length = read_and_align_hex_file("align.txt", data);
    if (data_length == 0) {
        free(data);
        return 1;
    }

    printf("\nTotal bytes processed: %d\n", data_length);

    // Create output image
    uint8_t* image_data = malloc(WIDTH * WIDTH * 3);
    if (!image_data) {
        printf("Error: Memory allocation failed\n");
        free(data);
        return 1;
    }

    // Debayer with interpolation
    for (int y = 0; y < WIDTH; y++) {
        for (int x = 0; x < WIDTH; x++) {
            RGB pixel = debayer_pixel(data, x, y);
            int idx = (y * WIDTH + x) * 3;
            image_data[idx] = pixel.r;
            image_data[idx + 1] = pixel.g;
            image_data[idx + 2] = pixel.b;
        }
    }

    // Write BMP file
    if (stbi_write_bmp("align.bmp", WIDTH, WIDTH, 3, image_data)) {
        printf("Successfully created align.bmp (%dx%d) with per-line alignment\n", WIDTH, WIDTH);
    } else {
        printf("Error: Failed to write align.bmp\n");
        free(data);
        free(image_data);
        return 1;
    }

    free(data);
    free(image_data);
    return 0;
}