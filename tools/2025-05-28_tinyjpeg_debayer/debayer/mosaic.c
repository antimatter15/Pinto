#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef struct {
    uint8_t r, g, b;
} RGB;

typedef enum {
    PATTERN_GRAYSCALE,
    PATTERN_RGGB_RAW,
    PATTERN_GRBG_RAW,
    PATTERN_GBRG_RAW,
    PATTERN_BGGR_RAW,
    PATTERN_RGGB_DEBAYER,
    PATTERN_GRBG_DEBAYER,
    PATTERN_GBRG_DEBAYER,
    PATTERN_BGGR_DEBAYER,
    PATTERN_RGGB_SIMPLE,
    PATTERN_GRBG_SIMPLE,
    PATTERN_GBRG_SIMPLE,
    PATTERN_BGGR_SIMPLE
} BayerPattern;

// Global bayer patterns (2x2 repeating patterns)
char bayer_patterns[4][2][2] = {
    {{'R', 'G'}, {'G', 'B'}}, // RGGB
    {{'G', 'R'}, {'B', 'G'}}, // GRBG
    {{'G', 'B'}, {'R', 'G'}}, // GBRG
    {{'B', 'G'}, {'G', 'R'}}  // BGGR
};

void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -w, --width WIDTH        Set image width (default: 330)\n");
    printf("  -p, --pattern PATTERN    Set bayer pattern:\n");
    printf("                           grayscale, rggb_raw, grbg_raw, gbrg_raw, bggr_raw,\n");
    printf("                           rggb_debayer, grbg_debayer, gbrg_debayer, bggr_debayer,\n");
    printf("                           rggb_simple, grbg_simple, gbrg_simple, bggr_simple\n");
    printf("                           (default: grayscale)\n");
    printf("  -s, --bitshift SHIFT     Bit shift amount (-32 to 32, default: 0)\n");
    printf("  -6, --6bit               Enable 6-bit quantization\n");
    printf("  -4, --4bit               Enable 4-bit quantization (overrides 6-bit)\n");
    printf("  -h, --help               Show this help message\n");
}

BayerPattern parse_pattern(const char* pattern_str) {
    if (strcmp(pattern_str, "grayscale") == 0) return PATTERN_GRAYSCALE;
    if (strcmp(pattern_str, "rggb_raw") == 0) return PATTERN_RGGB_RAW;
    if (strcmp(pattern_str, "grbg_raw") == 0) return PATTERN_GRBG_RAW;
    if (strcmp(pattern_str, "gbrg_raw") == 0) return PATTERN_GBRG_RAW;
    if (strcmp(pattern_str, "bggr_raw") == 0) return PATTERN_BGGR_RAW;
    if (strcmp(pattern_str, "rggb_debayer") == 0) return PATTERN_RGGB_DEBAYER;
    if (strcmp(pattern_str, "grbg_debayer") == 0) return PATTERN_GRBG_DEBAYER;
    if (strcmp(pattern_str, "gbrg_debayer") == 0) return PATTERN_GBRG_DEBAYER;
    if (strcmp(pattern_str, "bggr_debayer") == 0) return PATTERN_BGGR_DEBAYER;
    if (strcmp(pattern_str, "rggb_simple") == 0) return PATTERN_RGGB_SIMPLE;
    if (strcmp(pattern_str, "grbg_simple") == 0) return PATTERN_GRBG_SIMPLE;
    if (strcmp(pattern_str, "gbrg_simple") == 0) return PATTERN_GBRG_SIMPLE;
    if (strcmp(pattern_str, "bggr_simple") == 0) return PATTERN_BGGR_SIMPLE;
    return PATTERN_GRAYSCALE;
}

void shift_bitstream(uint8_t* data, int length, int shift_bits) {
    if (shift_bits == 0) return;

    if (shift_bits > 0) {
        // Right shift
        for (int i = length - 1; i >= 0; i--) {
            data[i] >>= shift_bits;
            if (i > 0) {
                data[i] |= (data[i-1] & ((1 << shift_bits) - 1)) << (8 - shift_bits);
            }
        }
    } else {
        // Left shift
        int left_shift = -shift_bits;
        for (int i = 0; i < length; i++) {
            data[i] <<= left_shift;
            if (i < length - 1) {
                data[i] |= (data[i+1] >> (8 - left_shift)) & ((1 << left_shift) - 1);
            }
        }
    }
}

void quantize_data(uint8_t* data, int length, int use_4bit, int use_6bit) {
    if (use_4bit) {
        for (int i = 0; i < length; i++) {
            data[i] = (data[i] / 16) * 16;
            if (data[i] > 240) data[i] = 240;
        }
    } else if (use_6bit) {
        for (int i = 0; i < length; i++) {
            data[i] = (data[i] / 4) * 4;
            if (data[i] > 252) data[i] = 252;
        }
    }
}

uint8_t get_pixel_value(uint8_t* data, int width, int height, int x, int y) {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return 0;
    }
    int index = y * width + x;
    return data[index];
}

RGB simple_debayer_2x2(uint8_t* data, int width, int height, int x, int y, char pattern[2][2]) {
    RGB pixel = {0, 0, 0};

    // Get 2x2 block
    uint8_t top_left = get_pixel_value(data, width, height, x, y);
    uint8_t top_right = get_pixel_value(data, width, height, x + 1, y);
    uint8_t bottom_left = get_pixel_value(data, width, height, x, y + 1);
    uint8_t bottom_right = get_pixel_value(data, width, height, x + 1, y + 1);

    uint8_t r_val = 0, b_val = 0;
    uint8_t g_vals[4] = {0};
    int g_count = 0;

    // Top-left
    if (pattern[0][0] == 'R') r_val = top_left;
    else if (pattern[0][0] == 'G') g_vals[g_count++] = top_left;
    else if (pattern[0][0] == 'B') b_val = top_left;

    // Top-right
    if (pattern[0][1] == 'R') r_val = top_right;
    else if (pattern[0][1] == 'G') g_vals[g_count++] = top_right;
    else if (pattern[0][1] == 'B') b_val = top_right;

    // Bottom-left
    if (pattern[1][0] == 'R') r_val = bottom_left;
    else if (pattern[1][0] == 'G') g_vals[g_count++] = bottom_left;
    else if (pattern[1][0] == 'B') b_val = bottom_left;

    // Bottom-right
    if (pattern[1][1] == 'R') r_val = bottom_right;
    else if (pattern[1][1] == 'G') g_vals[g_count++] = bottom_right;
    else if (pattern[1][1] == 'B') b_val = bottom_right;

    // Average green values
    uint8_t avg_green = 0;
    if (g_count > 0) {
        int sum = 0;
        for (int i = 0; i < g_count; i++) {
            sum += g_vals[i];
        }
        avg_green = sum / g_count;
    }

    pixel.r = r_val;
    pixel.g = avg_green;
    pixel.b = b_val;

    return pixel;
}

uint8_t interpolate_channel(uint8_t* data, int width, int height, int x, int y, char target_channel, char pattern[2][2]) {
    int sum = 0;
    int count = 0;

    // Look at neighboring pixels in a 3x3 grid
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;

            // Check bounds
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                // Check if this neighbor has the target channel
                char nchannel = pattern[ny % 2][nx % 2];

                if (nchannel == target_channel) {
                    sum += get_pixel_value(data, width, height, nx, ny);
                    count++;
                }
            }
        }
    }


    return sum / count;
}

RGB debayer_pixel_interpolate(uint8_t* data, int width, int height, int x, int y, char pattern[2][2]) {
    RGB pixel = {0, 0, 0};

    // Get current pixel's channel
    char current_channel = pattern[y % 2][x % 2];
    uint8_t current_value = get_pixel_value(data, width, height, x, y);

    // Assign known value
    if (current_channel == 'R') pixel.r = current_value;
    else if (current_channel == 'G') pixel.g = current_value;
    else if (current_channel == 'B') pixel.b = current_value;

    // Interpolate missing channels
    if (current_channel != 'R') {
        pixel.r = interpolate_channel(data, width, height, x, y, 'R', pattern);
    }
    if (current_channel != 'G') {
        pixel.g = interpolate_channel(data, width, height, x, y, 'G', pattern);
    }
    if (current_channel != 'B') {
        pixel.b = interpolate_channel(data, width, height, x, y, 'B', pattern);
    }

    return pixel;
}

int read_hex_file(const char* filename, uint8_t** data, int* length) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open %s\n", filename);
        return 0;
    }

    // Count hex values first
    char buffer[1024];
    int count = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        char* ptr = buffer;
        while (*ptr) {
            if (isxdigit(*ptr)) {
                // Found start of hex value
                while (isxdigit(*ptr)) ptr++;
                count++;
                if (*ptr && !isxdigit(*ptr)) ptr++;
            } else {
                ptr++;
            }
        }
    }

    if (count == 0) {
        printf("Error: No hex values found in file\n");
        fclose(file);
        return 0;
    }

    // Allocate memory
    *data = malloc(count);
    if (!*data) {
        printf("Error: Memory allocation failed\n");
        fclose(file);
        return 0;
    }

    // Read hex values
    rewind(file);
    int index = 0;
    while (fgets(buffer, sizeof(buffer), file) && index < count) {
        char* ptr = buffer;
        while (*ptr && index < count) {
            if (isxdigit(*ptr)) {
                char hex_str[3] = {0};
                int hex_len = 0;

                // Extract hex digits (up to 2)
                while (isxdigit(*ptr) && hex_len < 2) {
                    hex_str[hex_len++] = *ptr++;
                }

                // Convert to byte
                (*data)[index++] = (uint8_t)strtol(hex_str, NULL, 16);
            } else {
                ptr++;
            }
        }
    }

    fclose(file);
    *length = index;
    return 1;
}

int main(int argc, char* argv[]) {
    int width = 330;
    BayerPattern pattern = PATTERN_GRAYSCALE;
    int bit_shift = 0;
    int use_6bit = 0;
    int use_4bit = 0;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--width") == 0) {
            if (i + 1 < argc) {
                width = atoi(argv[++i]);
                if (width <= 0) {
                    printf("Error: Width must be positive\n");
                    return 1;
                }
            } else {
                printf("Error: Width value missing\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pattern") == 0) {
            if (i + 1 < argc) {
                pattern = parse_pattern(argv[++i]);
            } else {
                printf("Error: Pattern value missing\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--bitshift") == 0) {
            if (i + 1 < argc) {
                bit_shift = atoi(argv[++i]);
                if (bit_shift < -32 || bit_shift > 32) {
                    printf("Error: Bit shift must be between -32 and 32\n");
                    return 1;
                }
            } else {
                printf("Error: Bit shift value missing\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-6") == 0 || strcmp(argv[i], "--6bit") == 0) {
            use_6bit = 1;
        } else if (strcmp(argv[i], "-4") == 0 || strcmp(argv[i], "--4bit") == 0) {
            use_4bit = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            printf("Error: Unknown option %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Read hex data from file
    uint8_t* data;
    int data_length;
    if (!read_hex_file("source.txt", &data, &data_length)) {
        return 1;
    }

    printf("Read %d hex values from source.txt\n", data_length);

    // Apply bit shift
    if (bit_shift != 0) {
        shift_bitstream(data, data_length, bit_shift);
        printf("Applied bit shift: %d\n", bit_shift);
    }

    // Apply quantization
    quantize_data(data, data_length, use_4bit, use_6bit);
    if (use_4bit) {
        printf("Applied 4-bit quantization\n");
    } else if (use_6bit) {
        printf("Applied 6-bit quantization\n");
    }

    // Calculate image dimensions
    int height = (data_length + width - 1) / width; // Ceiling division

    // Create output image
    uint8_t* image_data;
    int output_width, output_height;

    if (pattern >= PATTERN_RGGB_SIMPLE) {
        // Simple debayer mode - output is half size
        output_width = width / 2;
        output_height = height / 2;
        image_data = malloc(output_width * output_height * 3);

        int pattern_index = pattern - PATTERN_RGGB_SIMPLE;

        for (int y = 0; y < output_height; y++) {
            for (int x = 0; x < output_width; x++) {
                RGB pixel = simple_debayer_2x2(data, width, height, x * 2, y * 2, bayer_patterns[pattern_index]);
                int idx = (y * output_width + x) * 3;
                image_data[idx] = pixel.r;
                image_data[idx + 1] = pixel.g;
                image_data[idx + 2] = pixel.b;
            }
        }
    } else if (pattern >= PATTERN_RGGB_DEBAYER && pattern <= PATTERN_BGGR_DEBAYER) {
        // Interpolated debayer mode - full size output
        output_width = width;
        output_height = height;
        image_data = malloc(output_width * output_height * 3);

        int pattern_index = pattern - PATTERN_RGGB_DEBAYER;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                RGB pixel = debayer_pixel_interpolate(data, width, height, x, y, bayer_patterns[pattern_index]);
                int idx = (y * width + x) * 3;
                image_data[idx] = pixel.r;
                image_data[idx + 1] = pixel.g;
                image_data[idx + 2] = pixel.b;
            }
        }
    } else {
        // Regular mode (grayscale or raw bayer)
        output_width = width;
        output_height = height;
        image_data = malloc(output_width * output_height * 3);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = y * width + x;
                uint8_t value = (idx < data_length) ? data[idx] : 0;

                int img_idx = (y * width + x) * 3;

                if (pattern == PATTERN_GRAYSCALE) {
                    // Grayscale
                    image_data[img_idx] = value;
                    image_data[img_idx + 1] = value;
                    image_data[img_idx + 2] = value;
                } else {
                    // Raw bayer pattern
                    int pattern_index = pattern - PATTERN_RGGB_RAW;
                    char channel = bayer_patterns[pattern_index][y % 2][x % 2];

                    image_data[img_idx] = (channel == 'R') ? value : 0;
                    image_data[img_idx + 1] = (channel == 'G') ? value : 0;
                    image_data[img_idx + 2] = (channel == 'B') ? value : 0;
                }
            }
        }
    }

    // Write BMP file
    if (stbi_write_bmp("mosaic.bmp", output_width, output_height, 3, image_data)) {
        printf("Successfully created mosaic.bmp (%dx%d)\n", output_width, output_height);
    } else {
        printf("Error: Failed to write mosaic.bmp\n");
        free(data);
        free(image_data);
        return 1;
    }

    free(data);
    free(image_data);
    return 0;
}
