#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TJE_PACKED_IMPLEMENTATION
#include "tiny_jpeg_packed.h"

#define WIDTH 328

typedef struct {
    uint8_t r, g, b;
} RGB;

// Timing and file I/O helper functions
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static void write_to_file(void* context, void* data, int size) {
    FILE* fd = (FILE*)context;
    fwrite(data, size, 1, fd);
}

static int encode_jpeg_to_file(const char* filename, int quality, int width, int height, 
                               const unsigned char* packed_data, double* encode_time_ms) {
    FILE* fd = fopen(filename, "wb");
    if (!fd) {
        printf("Error: Could not open %s for writing\n", filename);
        return 0;
    }
    
    double start_time = get_time_ms();
    int result = tje_encode_packed_with_func(write_to_file, fd, quality, width, height, packed_data);
    double end_time = get_time_ms();
    
    *encode_time_ms = end_time - start_time;
    
    fclose(fd);
    return result;
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
    // Case 0: (0,0) = G, Case 1: (0,1) = R, Case 2: (1,0) = B, Case 3: (1,1) = G
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

void shift_bitstream_right(uint8_t* data, int length, int shift_bits) {
    if (shift_bits == 0 || shift_bits > 7) return;

    for (int i = length - 1; i >= 0; i--) {
        data[i] >>= shift_bits;
        if (i > 0) {
            data[i] |= (data[i-1] & ((1 << shift_bits) - 1)) << (8 - shift_bits);
        }
    }
}

void quantize_to_6bit(uint8_t* data, int length) {
    for (int i = 0; i < length; i++) {
        // Keep only the most significant 6 bits (clear lower 2 bits)
        data[i] = data[i] & 0xFC;
    }
}

void pack_6bit_data(uint8_t* source, uint8_t* packed) {
    int packed_idx = 0;

    for (int row = 0; row < WIDTH; row++) {
        int row_offset = row * WIDTH;

        for (int i = 0; i < WIDTH; i += 4) {
            // Convert 8-bit values to 6-bit by right-shifting by 2
            uint8_t a = (source[row_offset + i] >> 2);         // 6 bits: aaaaaa
            uint8_t b = (source[row_offset + i + 1] >> 2);     // 6 bits: bbbbbb
            uint8_t c = (source[row_offset + i + 2] >> 2);     // 6 bits: cccccc
            uint8_t d = (source[row_offset + i + 3] >> 2);     // 6 bits: dddddd

            // Pack 4×6-bit values into 3 bytes (24 bits total)
            // Byte 1: aaaaaa + bb (upper 6 bits of a + upper 2 bits of b)
            packed[packed_idx++] = (a << 2) | (b >> 4);

            // Byte 2: bbbb + cccc (lower 4 bits of b + upper 4 bits of c)
            packed[packed_idx++] = ((b & 0x0F) << 4) | (c >> 2);

            // Byte 3: cc + dddddd (lower 2 bits of c + all 6 bits of d)
            packed[packed_idx++] = ((c & 0x03) << 6) | (d & 0x3F);
        }
    }
}


// Optimized version processing 4 bytes at a time
void shift_quantize_pack(uint8_t* source, uint8_t* packed, int length, int shift_bits) {
    // Early exit for invalid shift
    if (shift_bits < 0 || shift_bits > 7) {
        shift_bits = 0;
    }

    // Precompute masks
    const uint32_t shift_mask = (1U << shift_bits) - 1;
    const int inv_shift = 8 - shift_bits;

    int src_idx = 0;
    int dst_idx = 0;
    uint32_t carry = 0;

    // Process 4 bytes at a time (4 bytes -> 3 packed bytes)
    // 4 * 8 bits = 32 bits input -> 4 * 6 bits = 24 bits = 3 bytes output
    while (src_idx + 3 < length) {
        uint32_t b0, b1, b2, b3;

        if (shift_bits) {
            // Apply shift with carry
            b0 = source[src_idx];
            uint32_t next_carry = b0 & shift_mask;
            b0 = (b0 >> shift_bits) | (carry << inv_shift);

            b1 = source[src_idx + 1];
            carry = b1 & shift_mask;
            b1 = (b1 >> shift_bits) | (next_carry << inv_shift);

            b2 = source[src_idx + 2];
            next_carry = b2 & shift_mask;
            b2 = (b2 >> shift_bits) | (carry << inv_shift);

            b3 = source[src_idx + 3];
            carry = b3 & shift_mask;
            b3 = (b3 >> shift_bits) | (next_carry << inv_shift);
        } else {
            // No shift - just load
            b0 = source[src_idx];
            b1 = source[src_idx + 1];
            b2 = source[src_idx + 2];
            b3 = source[src_idx + 3];
        }

        // Quantize to 6 bits (keep upper 6 bits)
        b0 = (b0 >> 2) & 0x3F;
        b1 = (b1 >> 2) & 0x3F;
        b2 = (b2 >> 2) & 0x3F;
        b3 = (b3 >> 2) & 0x3F;

        // Pack 4x6 bits into 3 bytes
        packed[dst_idx]     = (b0 << 2) | (b1 >> 4);
        packed[dst_idx + 1] = (b1 << 4) | (b2 >> 2);
        packed[dst_idx + 2] = (b2 << 6) | b3;

        src_idx += 4;
        dst_idx += 3;
    }

    // Handle remaining bytes
    if (src_idx < length) {
        uint64_t accumulator = 0;
        int bits = 0;

        while (src_idx < length) {
            uint32_t value = source[src_idx++];

            if (shift_bits) {
                uint32_t next_carry = value & shift_mask;
                value = (value >> shift_bits) | (carry << inv_shift);
                carry = next_carry;
            }

            // Quantize and accumulate
            accumulator = (accumulator << 6) | ((value >> 2) & 0x3F);
            bits += 6;

            if (bits >= 8) {
                packed[dst_idx++] = (accumulator >> (bits - 8)) & 0xFF;
                bits -= 8;
            }
        }

        // Flush remaining bits
        if (bits > 0) {
            packed[dst_idx] = (accumulator << (8 - bits)) & 0xFF;
        }
    }
}

uint8_t get_pixel_value_packed(uint8_t* data, int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= WIDTH) {
        return 0;
    }

    // Calculate which group of 4 pixels this x belongs to
    int group = x / 4;
    int pixel_in_group = x % 4;

    // Calculate byte offset in the row (246 bytes per row)
    int row_offset = y * 246;
    int byte_offset = group * 3; // Each group of 4 pixels takes 3 bytes

    uint8_t value;

    // Unpack the 6-bit value based on position in group
    switch (pixel_in_group) {
        case 0: // Pixel A: upper 6 bits of first byte
            value = (data[row_offset + byte_offset] >> 2) & 0x3F;
            break;
        case 1: // Pixel B: lower 2 bits of first byte + upper 4 bits of second byte
            value = ((data[row_offset + byte_offset] & 0x03) << 4) |
                    ((data[row_offset + byte_offset + 1] >> 4) & 0x0F);
            break;
        case 2: // Pixel C: lower 4 bits of second byte + upper 2 bits of third byte
            value = ((data[row_offset + byte_offset + 1] & 0x0F) << 2) |
                    ((data[row_offset + byte_offset + 2] >> 6) & 0x03);
            break;
        case 3: // Pixel D: lower 6 bits of third byte
            value = data[row_offset + byte_offset + 2] & 0x3F;
            break;
        default:
            value = 0;
            break;
    }

    // Convert 6-bit value back to 8-bit (shift left by 2 to restore original range)
    value = value << 2;

    return value;
}

RGB debayer_pixel_packed(uint8_t* data, int x, int y) {
    RGB pixel;
    uint8_t center = get_pixel_value_packed(data, x, y);

    // GRBG pattern: G R / B G
    // Case 0: (0,0) = G, Case 1: (0,1) = R, Case 2: (1,0) = B, Case 3: (1,1) = G
    switch (((y % 2) << 1) | (x % 2)) {
        case 0: // G position (even row, even col)
            pixel.g = center;
            pixel.r = get_pixel_value_packed(data, x + 1, y);
            pixel.b = get_pixel_value_packed(data, x, y + 1);
            break;

        case 1: // R position (even row, odd col)
            pixel.r = center;
            pixel.g = (get_pixel_value_packed(data, x - 1, y) + get_pixel_value_packed(data, x + 1, y)) / 2;
            pixel.b = (get_pixel_value_packed(data, x - 1, y - 1) + get_pixel_value_packed(data, x - 1, y + 1)) / 2;
            break;

        case 2: // B position (odd row, even col)
            pixel.b = center;
            pixel.g = (get_pixel_value_packed(data, x, y - 1) + get_pixel_value_packed(data, x, y + 1)) / 2;
            pixel.r = (get_pixel_value_packed(data, x + 1, y - 1) + get_pixel_value_packed(data, x + 1, y + 1)) / 2;
            break;

        case 3: // G position (odd row, odd col)
            pixel.g = center;
            pixel.b = get_pixel_value_packed(data, x - 1, y);
            pixel.r = get_pixel_value_packed(data, x, y - 1);
            break;
    }

    return pixel;
}

int read_hex_file(const char* filename, uint8_t* data) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open %s\n", filename);
        return 0;
    }

    char buffer[1024];
    int index = 0;

    while (fgets(buffer, sizeof(buffer), file) && index < WIDTH * WIDTH) {
        char* ptr = buffer;
        while (*ptr && index < WIDTH * WIDTH) {
            if (isxdigit(*ptr)) {
                char hex_str[3] = {0};
                int hex_len = 0;

                // Extract hex digits (up to 2)
                while (isxdigit(*ptr) && hex_len < 2) {
                    hex_str[hex_len++] = *ptr++;
                }

                // Convert to byte
                data[index++] = (uint8_t)strtol(hex_str, NULL, 16);
            } else {
                ptr++;
            }
        }
    }

    fclose(file);
    return index;
}

int main() {
    uint8_t* data = malloc(WIDTH * WIDTH);
    if (!data) {
        printf("Error: Memory allocation failed for data\n");
        return 1;
    }

    // Create a copy of data for the new implementation
    uint8_t* data_copy = malloc(WIDTH * WIDTH);
    if (!data_copy) {
        printf("Error: Memory allocation failed for data copy\n");
        free(data);
        return 1;
    }

    // Read hex data from file
    int data_length = read_hex_file("source.txt", data);
    if (data_length == 0) {
        free(data);
        free(data_copy);
        return 1;
    }

    // Copy the original data
    for (int i = 0; i < data_length; i++) {
        data_copy[i] = data[i];
    }

    printf("Read %d hex values from source.txt\n", data_length);

    // Apply 3-bit right shift to the data
    shift_bitstream_right(data, data_length, 3);
    printf("Applied 3-bit right shift\n");

    // Quantize to 6-bit (keep only most significant 6 bits)
    quantize_to_6bit(data, data_length);
    printf("Quantized to 6-bit\n");

    // Create packed 6-bit data (246 bytes per row * 328 rows)
    uint8_t* packed_data = malloc(246 * WIDTH);
    if (!packed_data) {
        printf("Error: Memory allocation failed for packed data\n");
        free(data);
        free(data_copy);
        return 1;
    }

    pack_6bit_data(data, packed_data);
    printf("Created packed 6-bit data (246 x %d = %d bytes)\n", WIDTH, 246 * WIDTH);

    // Test the new combined function
    uint8_t* packed_data_new = malloc(246 * WIDTH);
    if (!packed_data_new) {
        printf("Error: Memory allocation failed for new packed data\n");
        free(data);
        free(data_copy);
        free(packed_data);
        return 1;
    }

    shift_quantize_pack(data_copy, packed_data_new, data_length, 3);
    printf("Created packed data using combined function\n");

    // Load 6bit.txt as already packed 6-bit data
    uint8_t* sixbit_packed_data = malloc(246 * WIDTH);
    if (!sixbit_packed_data) {
        printf("Error: Memory allocation failed for 6bit packed data\n");
        free(data);
        free(data_copy);
        free(packed_data);
        free(packed_data_new);
        return 1;
    }

    int sixbit_length = read_hex_file("6bit.txt", sixbit_packed_data);
    if (sixbit_length == 0) {
        printf("Warning: Could not read 6bit.txt, skipping 6bit.bmp creation\n");
    } else {
        printf("Read %d bytes from 6bit.txt as packed 6-bit data\n", sixbit_length);
    }

    // Verify that debayer_pixel_packed results match between original and combined methods
    int mismatches = 0;
    for (int y = 0; y < WIDTH && mismatches < 10; y++) {
        for (int x = 0; x < WIDTH && mismatches < 10; x++) {
            RGB pixel_original = debayer_pixel_packed(packed_data, x, y);
            RGB pixel_combined = debayer_pixel_packed(packed_data_new, x, y);

            if (pixel_original.r != pixel_combined.r ||
                pixel_original.g != pixel_combined.g ||
                pixel_original.b != pixel_combined.b) {
                printf("Mismatch at (%d,%d): Original=(%d,%d,%d) Combined=(%d,%d,%d)\n",
                       x, y, pixel_original.r, pixel_original.g, pixel_original.b,
                       pixel_combined.r, pixel_combined.g, pixel_combined.b);
                mismatches++;
            }
        }
    }

    if (mismatches == 0) {
        printf("Verification successful: Both packing methods produce identical debayer results\n");
    } else {
        printf("Verification failed: Found %d debayer mismatches\n", mismatches);
    }

    // Create output image using packed data
    uint8_t* image_data = malloc(WIDTH * WIDTH * 3);
    if (!image_data) {
        printf("Error: Memory allocation failed\n");
        free(data);
        free(data_copy);
        free(packed_data);
        free(packed_data_new);
        free(sixbit_packed_data);
        return 1;
    }

    // Debayer with interpolation using packed data
    for (int y = 0; y < WIDTH; y++) {
        for (int x = 0; x < WIDTH; x++) {
            RGB pixel = debayer_pixel_packed(packed_data, x, y);
            int idx = (y * WIDTH + x) * 3;
            image_data[idx] = pixel.r;
            image_data[idx + 1] = pixel.g;
            image_data[idx + 2] = pixel.b;
        }
    }

    // Write BMP file
    if (stbi_write_bmp("mosaic.bmp", WIDTH, WIDTH, 3, image_data)) {
        printf("Successfully created mosaic.bmp (%dx%d)\n", WIDTH, WIDTH);
    } else {
        printf("Error: Failed to write mosaic.bmp\n");
        free(data);
        free(data_copy);
        free(packed_data);
        free(packed_data_new);
        free(sixbit_packed_data);
        free(image_data);
        return 1;
    }

    // Write JPEG files at different quality levels using packed data directly
    int quality_levels[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 100};
    int num_qualities = sizeof(quality_levels) / sizeof(quality_levels[0]);
    
    printf("\nMosaic JPEG encoding times:\n");
    printf("Quality\tTime (ms)\tFilename\n");
    printf("-------\t---------\t--------\n");
    
    double mosaic_total_time = 0.0;
    double mosaic_min_time = 1000.0;
    double mosaic_max_time = 0.0;
    
    for (int q = 0; q < num_qualities; q++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "mosaic-q%d.jpg", quality_levels[q]);
        
        double encode_time;
        if (encode_jpeg_to_file(filename, quality_levels[q], WIDTH, WIDTH, packed_data, &encode_time)) {
            printf("%d\t%.2f\t\t%s\n", quality_levels[q], encode_time, filename);
            mosaic_total_time += encode_time;
            if (encode_time < mosaic_min_time) mosaic_min_time = encode_time;
            if (encode_time > mosaic_max_time) mosaic_max_time = encode_time;
        } else {
            printf("Error: Failed to write %s\n", filename);
        }
    }
    
    printf("\nMosaic encoding summary:\n");
    printf("Total time: %.2f ms\n", mosaic_total_time);
    printf("Average time: %.2f ms\n", mosaic_total_time / num_qualities);
    printf("Min time: %.2f ms\n", mosaic_min_time);
    printf("Max time: %.2f ms\n", mosaic_max_time);

    // Create 6bit.bmp from 6bit.txt data if it was loaded successfully
    if (sixbit_length > 0) {
        uint8_t* sixbit_image_data = malloc(WIDTH * WIDTH * 3);
        if (!sixbit_image_data) {
            printf("Error: Memory allocation failed for 6bit image data\n");
            free(data);
            free(data_copy);
            free(packed_data);
            free(packed_data_new);
            free(sixbit_packed_data);
            free(image_data);
            return 1;
        }

        // Debayer with interpolation using 6bit packed data
        for (int y = 0; y < WIDTH; y++) {
            for (int x = 0; x < WIDTH; x++) {
                RGB pixel = debayer_pixel_packed(sixbit_packed_data, x, y);
                int idx = (y * WIDTH + x) * 3;
                sixbit_image_data[idx] = pixel.r;
                sixbit_image_data[idx + 1] = pixel.g;
                sixbit_image_data[idx + 2] = pixel.b;
            }
        }

        // Write 6bit BMP file
        if (stbi_write_bmp("6bit.bmp", WIDTH, WIDTH, 3, sixbit_image_data)) {
            printf("Successfully created 6bit.bmp (%dx%d)\n", WIDTH, WIDTH);
        } else {
            printf("Error: Failed to write 6bit.bmp\n");
        }

        // Write 6bit JPEG files at different quality levels
        int quality_levels[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 100};
        int num_qualities = sizeof(quality_levels) / sizeof(quality_levels[0]);
        
        printf("\n6bit JPEG encoding times:\n");
        printf("Quality\tTime (ms)\tFilename\n");
        printf("-------\t---------\t--------\n");
        
        double total_time = 0.0;
        double min_time = 1000.0;
        double max_time = 0.0;
        
        for (int q = 0; q < num_qualities; q++) {
            char filename[64];
            snprintf(filename, sizeof(filename), "6bit-q%d.jpg", quality_levels[q]);
            
            double encode_time;
            if (encode_jpeg_to_file(filename, quality_levels[q], WIDTH, WIDTH, sixbit_packed_data, &encode_time)) {
                printf("%d\t%.2f\t\t%s\n", quality_levels[q], encode_time, filename);
                total_time += encode_time;
                if (encode_time < min_time) min_time = encode_time;
                if (encode_time > max_time) max_time = encode_time;
            } else {
                printf("Error: Failed to write %s\n", filename);
            }
        }
        
        printf("\n6bit encoding summary:\n");
        printf("Total time: %.2f ms\n", total_time);
        printf("Average time: %.2f ms\n", total_time / num_qualities);
        printf("Min time: %.2f ms\n", min_time);
        printf("Max time: %.2f ms\n", max_time);

        free(sixbit_image_data);
    }

    free(data);
    free(data_copy);
    free(packed_data);
    free(packed_data_new);
    free(sixbit_packed_data);
    free(image_data);
    return 0;
}
