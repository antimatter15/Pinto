#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define WIDTH 328
#define BYTES_PER_ROW 246

typedef struct {
    uint8_t r, g, b;
} RGB;


uint8_t get_pixel_value(uint8_t* data, int x, int y, int shift) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= WIDTH) {
        return 0;
    }

    // Calculate which group of 4 pixels this x belongs to
    int group = x / 4;
    int pixel_in_group = x % 4;

    // Calculate byte offset in the row
    int row_offset = y * BYTES_PER_ROW;
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

RGB debayer_pixel(uint8_t* data, int x, int y, int shift) {
    RGB pixel;
    uint8_t center = get_pixel_value(data, x, y, shift);

    // GRBG pattern: G R / B G
    // Case 0: (0,0) = G, Case 1: (0,1) = R, Case 2: (1,0) = B, Case 3: (1,1) = G
    switch (((y % 2) << 1) | (x % 2)) {
        case 0: // G position (even row, even col)
            pixel.g = center;
            pixel.r = get_pixel_value(data, x + 1, y, shift);
            pixel.b = get_pixel_value(data, x, y + 1, shift);
            break;

        case 1: // R position (even row, odd col)
            pixel.r = center;
            pixel.g = (get_pixel_value(data, x - 1, y, shift) + get_pixel_value(data, x + 1, y, shift)) / 2;
            pixel.b = (get_pixel_value(data, x - 1, y - 1, shift) + get_pixel_value(data, x - 1, y + 1, shift)) / 2;
            break;

        case 2: // B position (odd row, even col)
            pixel.b = center;
            pixel.g = (get_pixel_value(data, x, y - 1, shift) + get_pixel_value(data, x, y + 1, shift)) / 2;
            pixel.r = (get_pixel_value(data, x + 1, y - 1, shift) + get_pixel_value(data, x + 1, y + 1, shift)) / 2;
            break;

        case 3: // G position (odd row, odd col)
            pixel.g = center;
            pixel.b = get_pixel_value(data, x - 1, y, shift);
            pixel.r = get_pixel_value(data, x, y - 1, shift);
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
    int max_bytes = WIDTH * BYTES_PER_ROW; // 328 rows * 246 bytes per row

    while (fgets(buffer, sizeof(buffer), file) && index < max_bytes) {
        char* ptr = buffer;
        while (*ptr && index < max_bytes) {
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
    uint8_t data[WIDTH * BYTES_PER_ROW];

    // Read hex data from file
    int data_length = read_hex_file("6bit.txt", data);
    if (data_length == 0) {
        return 1;
    }

    printf("Read %d hex values from 6bit.txt\n", data_length);

    // Create output image
    uint8_t* image_data = malloc(WIDTH * WIDTH * 3);
    if (!image_data) {
        printf("Error: Memory allocation failed\n");
        return 1;
    }

    // Debayer with interpolation
    for (int y = 0; y < WIDTH; y++) {
        for (int x = 0; x < WIDTH; x++) {
            RGB pixel = debayer_pixel(data, x, y, 0);
            int idx = (y * WIDTH + x) * 3;
            image_data[idx] = pixel.r;
            image_data[idx + 1] = pixel.g;
            image_data[idx + 2] = pixel.b;
        }
    }

    // Write BMP file
    if (stbi_write_bmp("6bitmosaic.bmp", WIDTH, WIDTH, 3, image_data)) {
        printf("Successfully created 6bitmosaic.bmp (%dx%d)\n", WIDTH, WIDTH);
    } else {
        printf("Error: Failed to write mosaic.bmp\n");
        free(image_data);
        return 1;
    }

    free(image_data);
    return 0;
}
