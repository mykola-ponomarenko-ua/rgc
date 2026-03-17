#ifndef RGC_H
#define RGC_H

#include <stdint.h>

#define RGC_VERSION_MAJOR 0
#define RGC_VERSION_MINOR 1
#define RGC_VERSION_PATCH 1
#define RGC_VERSION_STRING "0.1.1"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Recursive Group Coding (RGC)
 * Copyright (c) 2018-2026 Mykola Ponomarenko
 */

/*
 * Compresses source_data[0..source_size-1].
 * Allocates *compressed_data_out and writes its size to *compressed_size_out.
 * The caller must release *compressed_data_out with rgc_free().
 */
void rgc_encode(
    const uint8_t* source_data,
    int32_t source_size,
    uint8_t** compressed_data_out,
    int32_t* compressed_size_out
);

/*
 * Decompresses compressed_data[0..compressed_size-1].
 * Allocates *decoded_data_out and writes its size to *decoded_size_out.
 * The caller must release *decoded_data_out with rgc_free().
 */
void rgc_decode(
    const uint8_t* compressed_data,
    int32_t compressed_size,
    uint8_t** decoded_data_out,
    int32_t* decoded_size_out
);

/* Releases buffers allocated by rgc_encode() or rgc_decode(). */
void rgc_free(void* pointer);

#ifdef __cplusplus
}
#endif

#endif /* RGC_H */
