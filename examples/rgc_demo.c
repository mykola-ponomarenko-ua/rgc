#include "../src/rgc/rgc.h"
#include "../src/rgc/rgc.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char* load_file(const char* path, int32_t* size) {
    FILE* file = fopen(path, "rb");
    unsigned char* data;

    fseek(file, 0, SEEK_END);
    *size = (int32_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    data = (unsigned char*)malloc((size_t)(*size));
    fread(data, 1, (size_t)(*size), file);
    fclose(file);

    return data;
}

static void save_file(const char* path, const unsigned char* data, int32_t size) {
    FILE* file = fopen(path, "wb");
    fwrite(data, 1, (size_t)size, file);
    fclose(file);
}

int main(void) {
    unsigned char* source_data;
    unsigned char* compressed_data;
    unsigned char* compressed_file_data;
    unsigned char* decoded_data;
    int32_t source_size;
    int32_t compressed_size;
    int32_t compressed_file_size;
    int32_t decoded_size;

    source_data = load_file("tst.dat", &source_size);
    rgc_encode(source_data, source_size, &compressed_data, &compressed_size);
    save_file("tst.rgc", compressed_data, compressed_size);
    printf("Compressed file size: %d bytes\n", compressed_size);
    rgc_free(compressed_data);

    compressed_file_data = load_file("tst.rgc", &compressed_file_size);
    rgc_decode(compressed_file_data, compressed_file_size, &decoded_data, &decoded_size);

    if (decoded_size == source_size && memcmp(decoded_data, source_data, (size_t)source_size) == 0) {
        printf("Decoded data is identical to the source\n");
    }

    rgc_free(decoded_data);
    free(compressed_file_data);
    free(source_data);
    return 0;
}
