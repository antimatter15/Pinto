#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    int width, height, num_components;
    unsigned char* data = stbi_load("sample.bmp", &width, &height, &num_components, 0);
    if (!data) {
        puts("Could not find sample.bmp");
        return EXIT_FAILURE;
    }

    printf("Loaded image: %dx%d with %d components\n", width, height, num_components);

    // Encode with quality 1 (lowest quality, highest compression)
    if (!tje_encode_to_file_at_quality("sample_quality1.jpg", 1, width, height, num_components, data)) {
        fprintf(stderr, "Could not write quality 1 JPEG\n");
        return EXIT_FAILURE;
    }
    printf("Encoded sample_quality1.jpg\n");

    // Encode with quality 2 (medium quality)
    if (!tje_encode_to_file_at_quality("sample_quality2.jpg", 2, width, height, num_components, data)) {
        fprintf(stderr, "Could not write quality 2 JPEG\n");
        return EXIT_FAILURE;
    }
    printf("Encoded sample_quality2.jpg\n");

    // Encode with quality 3 (highest quality, lowest compression)
    if (!tje_encode_to_file_at_quality("sample_quality3.jpg", 3, width, height, num_components, data)) {
        fprintf(stderr, "Could not write quality 3 JPEG\n");
        return EXIT_FAILURE;
    }
    printf("Encoded sample_quality3.jpg\n");

    // Also encode with default quality (which is quality 3)
    if (!tje_encode_to_file("sample_default.jpg", width, height, num_components, data)) {
        fprintf(stderr, "Could not write default quality JPEG\n");
        return EXIT_FAILURE;
    }
    printf("Encoded sample_default.jpg\n");

    stbi_image_free(data);
    return EXIT_SUCCESS;
}