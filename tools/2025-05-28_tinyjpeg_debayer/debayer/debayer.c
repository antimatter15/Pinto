#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define WIDTH 330
#define HEIGHT 330
#define BMP_HEADER_SIZE 54

typedef struct {
    uint8_t r, g, b;
} RGB;

void write_bmp_header(FILE *file, int width, int height) {
    uint32_t file_size = BMP_HEADER_SIZE + (width * height * 3);
    uint32_t data_offset = BMP_HEADER_SIZE;
    uint32_t image_size = width * height * 3;
    
    // BMP file header
    fwrite("BM", 1, 2, file);
    fwrite(&file_size, 4, 1, file);
    uint32_t reserved = 0;
    fwrite(&reserved, 4, 1, file);
    fwrite(&data_offset, 4, 1, file);
    
    // DIB header (BITMAPINFOHEADER)
    uint32_t header_size = 40;
    fwrite(&header_size, 4, 1, file);
    fwrite(&width, 4, 1, file);
    fwrite(&height, 4, 1, file);
    uint16_t planes = 1;
    fwrite(&planes, 2, 1, file);
    uint16_t bits_per_pixel = 24;
    fwrite(&bits_per_pixel, 2, 1, file);
    uint32_t compression = 0;
    fwrite(&compression, 4, 1, file);
    fwrite(&image_size, 4, 1, file);
    uint32_t x_pixels_per_meter = 2835;
    fwrite(&x_pixels_per_meter, 4, 1, file);
    uint32_t y_pixels_per_meter = 2835;
    fwrite(&y_pixels_per_meter, 4, 1, file);
    uint32_t colors_used = 0;
    fwrite(&colors_used, 4, 1, file);
    uint32_t colors_important = 0;
    fwrite(&colors_important, 4, 1, file);
}

RGB debayer_pixel(uint8_t *data, int x, int y, int width, int height) {
    RGB pixel = {0, 0, 0};
    int idx = y * width + x;
    
    // Bayer pattern (RGGB):
    // R G
    // G B
    
    if (y % 2 == 0) {  // Even row
        if (x % 2 == 0) {  // Red pixel
            pixel.r = data[idx];
            
            // Green interpolation
            int g_count = 0, g_sum = 0;
            if (x > 0) { g_sum += data[idx - 1]; g_count++; }
            if (x < width - 1) { g_sum += data[idx + 1]; g_count++; }
            if (y > 0) { g_sum += data[(y-1) * width + x]; g_count++; }
            if (y < height - 1) { g_sum += data[(y+1) * width + x]; g_count++; }
            pixel.g = g_count > 0 ? g_sum / g_count : 0;
            
            // Blue interpolation
            int b_count = 0, b_sum = 0;
            if (x > 0 && y > 0) { b_sum += data[(y-1) * width + (x-1)]; b_count++; }
            if (x < width - 1 && y > 0) { b_sum += data[(y-1) * width + (x+1)]; b_count++; }
            if (x > 0 && y < height - 1) { b_sum += data[(y+1) * width + (x-1)]; b_count++; }
            if (x < width - 1 && y < height - 1) { b_sum += data[(y+1) * width + (x+1)]; b_count++; }
            pixel.b = b_count > 0 ? b_sum / b_count : 0;
            
        } else {  // Green pixel
            pixel.g = data[idx];
            
            // Red interpolation
            int r_count = 0, r_sum = 0;
            if (x > 0) { r_sum += data[idx - 1]; r_count++; }
            if (x < width - 1) { r_sum += data[idx + 1]; r_count++; }
            pixel.r = r_count > 0 ? r_sum / r_count : 0;
            
            // Blue interpolation
            int b_count = 0, b_sum = 0;
            if (y > 0) { b_sum += data[(y-1) * width + x]; b_count++; }
            if (y < height - 1) { b_sum += data[(y+1) * width + x]; b_count++; }
            pixel.b = b_count > 0 ? b_sum / b_count : 0;
        }
    } else {  // Odd row
        if (x % 2 == 0) {  // Green pixel
            pixel.g = data[idx];
            
            // Blue interpolation
            int b_count = 0, b_sum = 0;
            if (x > 0) { b_sum += data[idx - 1]; b_count++; }
            if (x < width - 1) { b_sum += data[idx + 1]; b_count++; }
            pixel.b = b_count > 0 ? b_sum / b_count : 0;
            
            // Red interpolation
            int r_count = 0, r_sum = 0;
            if (y > 0) { r_sum += data[(y-1) * width + x]; r_count++; }
            if (y < height - 1) { r_sum += data[(y+1) * width + x]; r_count++; }
            pixel.r = r_count > 0 ? r_sum / r_count : 0;
            
        } else {  // Blue pixel
            pixel.b = data[idx];
            
            // Green interpolation
            int g_count = 0, g_sum = 0;
            if (x > 0) { g_sum += data[idx - 1]; g_count++; }
            if (x < width - 1) { g_sum += data[idx + 1]; g_count++; }
            if (y > 0) { g_sum += data[(y-1) * width + x]; g_count++; }
            if (y < height - 1) { g_sum += data[(y+1) * width + x]; g_count++; }
            pixel.g = g_count > 0 ? g_sum / g_count : 0;
            
            // Red interpolation
            int r_count = 0, r_sum = 0;
            if (x > 0 && y > 0) { r_sum += data[(y-1) * width + (x-1)]; r_count++; }
            if (x < width - 1 && y > 0) { r_sum += data[(y-1) * width + (x+1)]; r_count++; }
            if (x > 0 && y < height - 1) { r_sum += data[(y+1) * width + (x-1)]; r_count++; }
            if (x < width - 1 && y < height - 1) { r_sum += data[(y+1) * width + (x+1)]; r_count++; }
            pixel.r = r_count > 0 ? r_sum / r_count : 0;
        }
    }
    
    return pixel;
}

int main() {
    FILE *input_file = fopen("source.txt", "rb");
    if (!input_file) {
        printf("Error: Cannot open source.txt\n");
        return 1;
    }
    
    // Get file size
    fseek(input_file, 0, SEEK_END);
    long file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);
    
    // Allocate memory for the byte array
    uint8_t *data = malloc(file_size);
    if (!data) {
        printf("Error: Memory allocation failed\n");
        fclose(input_file);
        return 1;
    }
    
    // Read file into byte array
    size_t bytes_read = fread(data, 1, file_size, input_file);
    fclose(input_file);
    
    if (bytes_read != file_size) {
        printf("Error: Failed to read complete file\n");
        free(data);
        return 1;
    }
    
    // Shift data 3 bits to the right across byte boundaries
    for (long i = file_size - 1; i >= 0; i--) {
        data[i] >>= 3;
        if (i > 0) {
            data[i] |= (data[i-1] & 0x07) << 5;
        }
    }
    
    // Expected image size
    if (file_size < WIDTH * HEIGHT) {
        printf("Error: File too small for 330x330 image\n");
        free(data);
        return 1;
    }
    
    // Open output BMP file
    FILE *output_file = fopen("output.bmp", "wb");
    if (!output_file) {
        printf("Error: Cannot create output.bmp\n");
        free(data);
        return 1;
    }
    
    // Write BMP header
    write_bmp_header(output_file, WIDTH, HEIGHT);
    
    // Debayer and write pixel data (BMP stores bottom-to-top)
    for (int y = HEIGHT - 1; y >= 0; y--) {
        for (int x = 0; x < WIDTH; x++) {
            RGB pixel = debayer_pixel(data, x, y, WIDTH, HEIGHT);
            fwrite(&pixel.b, 1, 1, output_file);  // BMP stores BGR
            fwrite(&pixel.g, 1, 1, output_file);
            fwrite(&pixel.r, 1, 1, output_file);
        }
    }
    
    fclose(output_file);
    free(data);
    
    printf("Successfully created output.bmp\n");
    return 0;
}