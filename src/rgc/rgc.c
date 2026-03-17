#include "rgc.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * Recursive Group Coding (RGC)
 * Copyright (c) 2009-2026 Mykola Ponomarenko
 *
 * Reference:
 * N. Ponomarenko, V. Lukin, K. Egiazarian, J. Astola,
 * "Fast recursive coding based on grouping of symbols",
 * Telecommunications and Radio Engineering, Vol. 68, No. 20, 2009,
 * pp. 1857-1863.
 */

#define RGC_SYMBOL_COUNT 256
#define RGC_MAX_GROUP_COUNT 16
#define RGC_RECURSION_STOP_SIZE 200
#define RGC_GROUP_THRESHOLD 1.02

static double rgc_log2_double(double value) {
    return log(value) / log(2.0);
}

static uint8_t rgc_log2_power_of_two(uint32_t value) {
    uint8_t bit_count = 0;

    while (value > 1U) {
        value >>= 1U;
        bit_count++;
    }

    return bit_count;
}

static void rgc_write_u32_le(uint8_t* destination, uint32_t value) {
    destination[0] = (uint8_t)(value & 0xffU);
    destination[1] = (uint8_t)((value >> 8U) & 0xffU);
    destination[2] = (uint8_t)((value >> 16U) & 0xffU);
    destination[3] = (uint8_t)((value >> 24U) & 0xffU);
}

static uint32_t rgc_read_u32_le(const uint8_t* source) {
    return ((uint32_t)source[0]) |
           ((uint32_t)source[1] << 8U) |
           ((uint32_t)source[2] << 16U) |
           ((uint32_t)source[3] << 24U);
}

static void rgc_sort_symbols_by_frequency(
    int32_t* symbol_frequencies,
    uint8_t* sorted_symbols,
    int32_t left_index,
    int32_t right_index
) {
    int32_t pivot_frequency;
    int32_t lower_index;
    int32_t upper_index;

    lower_index = left_index;
    upper_index = right_index;
    pivot_frequency = symbol_frequencies[(left_index + right_index) / 2];

    while (lower_index <= upper_index) {
        int32_t frequency_value;
        uint8_t symbol_value;

        while (symbol_frequencies[lower_index] > pivot_frequency) {
            lower_index++;
        }

        while (pivot_frequency > symbol_frequencies[upper_index]) {
            upper_index--;
        }

        if (lower_index <= upper_index) {
            frequency_value = symbol_frequencies[lower_index];
            symbol_frequencies[lower_index] = symbol_frequencies[upper_index];
            symbol_frequencies[upper_index] = frequency_value;

            symbol_value = sorted_symbols[lower_index];
            sorted_symbols[lower_index] = sorted_symbols[upper_index];
            sorted_symbols[upper_index] = symbol_value;

            lower_index++;
            upper_index--;
        }
    }

    if (left_index < upper_index) {
        rgc_sort_symbols_by_frequency(symbol_frequencies, sorted_symbols, left_index, upper_index);
    }

    if (lower_index < right_index) {
        rgc_sort_symbols_by_frequency(symbol_frequencies, sorted_symbols, lower_index, right_index);
    }
}

void rgc_free(void* pointer) {
    free(pointer);
}

void rgc_encode(
    const uint8_t* source_data,
    int32_t source_size,
    uint8_t** compressed_data_out,
    int32_t* compressed_size_out
) {
    int32_t symbol_frequencies[RGC_SYMBOL_COUNT];
    int32_t group_indices[RGC_SYMBOL_COUNT];
    int32_t suffix_bit_lengths[RGC_SYMBOL_COUNT];
    uint8_t sorted_symbols[RGC_SYMBOL_COUNT];
    uint8_t group_sizes[RGC_MAX_GROUP_COUNT];
    uint8_t group_symbols[RGC_MAX_GROUP_COUNT][RGC_SYMBOL_COUNT];
    uint8_t symbol_suffix_bits[RGC_SYMBOL_COUNT][8];
    uint8_t* compressed_buffer;
    uint8_t* suffix_bit_buffer;
    uint8_t* prefix_buffer;
    uint8_t* working_buffer;
    int32_t compressed_size = 0;
    int32_t current_size;
    size_t compressed_capacity;
    size_t suffix_buffer_capacity;
    size_t prefix_buffer_capacity;
    size_t working_buffer_capacity;
    int32_t suffix_bit_count;
    int32_t index;
    int32_t group_count;
    int32_t distinct_symbol_count;
    int32_t group_size;
    int32_t group_index;
    int32_t group_symbol_index;
    int32_t packed_suffix_byte_count;
    int32_t padded_suffix_bit_count;
    int32_t merged_index;
    double code_size_ratio;
    double grouped_code_bits;
    double separate_code_bits;
    double group_probability_sum;
    uint8_t suffix_bit_width;

    if (compressed_data_out == NULL || compressed_size_out == NULL) {
        return;
    }

    *compressed_data_out = NULL;
    *compressed_size_out = 0;

    if (source_size < 0 || (source_size > 0 && source_data == NULL)) {
        return;
    }

    current_size = source_size;
    compressed_capacity = (size_t)current_size * 9U + 5000U;
    suffix_buffer_capacity = (size_t)current_size * 9U;
    prefix_buffer_capacity = (size_t)current_size + 1U;
    working_buffer_capacity = (size_t)current_size;

    compressed_buffer = (uint8_t*)malloc(compressed_capacity);
    suffix_bit_buffer = (uint8_t*)malloc(suffix_buffer_capacity > 0U ? suffix_buffer_capacity : 1U);
    prefix_buffer = (uint8_t*)malloc(prefix_buffer_capacity > 0U ? prefix_buffer_capacity : 1U);
    working_buffer = (uint8_t*)malloc(working_buffer_capacity > 0U ? working_buffer_capacity : 1U);

    if (compressed_buffer == NULL ||
        suffix_bit_buffer == NULL ||
        prefix_buffer == NULL ||
        working_buffer == NULL) {
        free(working_buffer);
        free(prefix_buffer);
        free(suffix_bit_buffer);
        free(compressed_buffer);
        return;
    }

    if (current_size > 0) {
        memcpy(working_buffer, source_data, (size_t)current_size);
    }

    rgc_write_u32_le(compressed_buffer, (uint32_t)current_size);
    compressed_size = 4;

    while (current_size >= RGC_RECURSION_STOP_SIZE) {
        for (index = 0; index < RGC_SYMBOL_COUNT; index++) {
            symbol_frequencies[index] = 0;
            group_indices[index] = 0;
            suffix_bit_lengths[index] = 0;
            sorted_symbols[index] = (uint8_t)index;
        }

        for (index = 0; index < current_size; index++) {
            symbol_frequencies[working_buffer[index]]++;
        }

        rgc_sort_symbols_by_frequency(symbol_frequencies, sorted_symbols, 0, RGC_SYMBOL_COUNT - 1);

        distinct_symbol_count = 0;
        for (index = 0; index < RGC_SYMBOL_COUNT; index++) {
            if (symbol_frequencies[index] > 0) {
                distinct_symbol_count++;
            }
        }

        group_count = 0;
        while (distinct_symbol_count > 0) {
            group_size = RGC_SYMBOL_COUNT;
            while (group_size > distinct_symbol_count) {
                group_size /= 2;
            }

            code_size_ratio = RGC_GROUP_THRESHOLD + 1.0;
            while (code_size_ratio > RGC_GROUP_THRESHOLD) {
                group_probability_sum = 0.0;
                for (index = distinct_symbol_count - 1; index >= distinct_symbol_count - group_size; index--) {
                    group_probability_sum += (double)symbol_frequencies[index];
                }

                grouped_code_bits =
                    -group_probability_sum *
                    (-rgc_log2_double(group_probability_sum / (double)current_size) +
                     rgc_log2_double((double)group_size));

                separate_code_bits = 0.0;
                for (index = distinct_symbol_count - 1; index >= distinct_symbol_count - group_size; index--) {
                    separate_code_bits +=
                        (double)symbol_frequencies[index] *
                        rgc_log2_double((double)symbol_frequencies[index] / (double)current_size);
                }

                if (separate_code_bits == 0.0) {
                    code_size_ratio = 1.0;
                } else {
                    code_size_ratio = grouped_code_bits / separate_code_bits;
                }

                if (code_size_ratio > RGC_GROUP_THRESHOLD) {
                    group_size /= 2;
                }
            }

            group_sizes[group_count] = (uint8_t)(group_size - 1);
            suffix_bit_width = rgc_log2_power_of_two((uint32_t)group_size);

            for (index = 0; index < group_size; index++) {
                group_symbols[group_count][index] = sorted_symbols[distinct_symbol_count - group_size + index];
            }

            for (index = distinct_symbol_count - group_size; index < distinct_symbol_count; index++) {
                uint8_t symbol;
                uint8_t suffix_value;

                symbol = sorted_symbols[index];
                group_indices[symbol] = group_count;
                suffix_value = (uint8_t)(index - distinct_symbol_count + group_size);
                suffix_bit_lengths[symbol] = suffix_bit_width;

                for (group_symbol_index = 0; group_symbol_index < suffix_bit_width; group_symbol_index++) {
                    symbol_suffix_bits[symbol][group_symbol_index] = (uint8_t)(suffix_value & 1U);
                    suffix_value >>= 1U;
                }
            }

            distinct_symbol_count -= group_size;
            group_count++;
        }

        group_count--;
        suffix_bit_count = 0;

        for (index = 0; index < current_size; index++) {
            uint8_t symbol;
            int32_t bit_index;

            symbol = working_buffer[index];
            prefix_buffer[index] = (uint8_t)group_indices[symbol];

            for (bit_index = 0; bit_index < suffix_bit_lengths[symbol]; bit_index++) {
                suffix_bit_buffer[suffix_bit_count++] = symbol_suffix_bits[symbol][bit_index];
            }
        }

        for (group_index = 0; group_index <= group_count; group_index++) {
            int32_t stored_group_size;

            stored_group_size = (int32_t)group_sizes[group_index] + 1;
            memcpy(compressed_buffer + compressed_size, group_symbols[group_index], (size_t)stored_group_size);
            compressed_size += stored_group_size;
        }

        memcpy(compressed_buffer + compressed_size, group_sizes, (size_t)(group_count + 1));
        compressed_size += group_count + 1;

        compressed_buffer[compressed_size++] = (uint8_t)group_count;

        padded_suffix_bit_count = suffix_bit_count;
        if ((padded_suffix_bit_count % 8) != 0) {
            padded_suffix_bit_count = ((padded_suffix_bit_count / 8) + 1) * 8;
        }

        for (index = suffix_bit_count; index < padded_suffix_bit_count; index++) {
            suffix_bit_buffer[index] = 0;
        }

        packed_suffix_byte_count = padded_suffix_bit_count / 8;
        for (index = 0; index < packed_suffix_byte_count; index++) {
            const uint8_t* suffix_source = suffix_bit_buffer + (size_t)index * 8U;

            compressed_buffer[compressed_size + index] =
                (uint8_t)((suffix_source[0] << 7) |
                          (suffix_source[1] << 6) |
                          (suffix_source[2] << 5) |
                          (suffix_source[3] << 4) |
                          (suffix_source[4] << 3) |
                          (suffix_source[5] << 2) |
                          (suffix_source[6] << 1) |
                          suffix_source[7]);
        }
        compressed_size += packed_suffix_byte_count;

        rgc_write_u32_le(compressed_buffer + compressed_size, (uint32_t)packed_suffix_byte_count);
        compressed_size += 4;

        if ((current_size % 2) == 1) {
            current_size++;
            prefix_buffer[current_size - 1] = (uint8_t)group_count;
        }

        current_size /= 2;
        merged_index = 0;
        for (index = 0; index < current_size; index++) {
            working_buffer[index] = (uint8_t)((prefix_buffer[merged_index] << 4) | prefix_buffer[merged_index + 1]);
            merged_index += 2;
        }
    }

    if (current_size > 0) {
        memcpy(compressed_buffer + compressed_size, working_buffer, (size_t)current_size);
    }
    compressed_size += current_size;

    rgc_write_u32_le(compressed_buffer + compressed_size, (uint32_t)current_size);
    compressed_size += 4;

    *compressed_data_out = (uint8_t*)malloc((size_t)compressed_size > 0U ? (size_t)compressed_size : 1U);
    if (*compressed_data_out != NULL) {
        memcpy(*compressed_data_out, compressed_buffer, (size_t)compressed_size);
        *compressed_size_out = compressed_size;
    }

    free(working_buffer);
    free(prefix_buffer);
    free(suffix_bit_buffer);
    free(compressed_buffer);
}

void rgc_decode(
    const uint8_t* compressed_data,
    int32_t compressed_size,
    uint8_t** decoded_data_out,
    int32_t* decoded_size_out
) {
    int32_t iteration_sizes[100];
    int32_t original_size;
    int32_t current_size;
    int32_t compressed_offset;
    int32_t iteration_count;
    int32_t iteration_index;
    int32_t prefix_count;
    int32_t packed_suffix_byte_count;
    int32_t suffix_bit_index;
    int32_t group_bit_lengths[RGC_SYMBOL_COUNT];
    uint8_t group_sizes[RGC_MAX_GROUP_COUNT];
    uint8_t group_symbols[RGC_MAX_GROUP_COUNT][RGC_SYMBOL_COUNT];
    uint8_t* suffix_bit_buffer;
    uint8_t* prefix_buffer;
    uint8_t* decoded_buffer;
    uint8_t group_count;
    int32_t index;

    if (decoded_data_out == NULL || decoded_size_out == NULL) {
        return;
    }

    *decoded_data_out = NULL;
    *decoded_size_out = 0;

    if (compressed_data == NULL || compressed_size < 8) {
        return;
    }

    original_size = (int32_t)rgc_read_u32_le(compressed_data);
    if (original_size < 0) {
        return;
    }

    *decoded_size_out = original_size;
    *decoded_data_out = (uint8_t*)malloc((size_t)original_size > 0U ? (size_t)original_size : 1U);
    if (*decoded_data_out == NULL) {
        *decoded_size_out = 0;
        return;
    }

    decoded_buffer = *decoded_data_out;
    suffix_bit_buffer = (uint8_t*)malloc((size_t)original_size * 9U + 1U);
    prefix_buffer = (uint8_t*)malloc((size_t)original_size + 1U);

    if (suffix_bit_buffer == NULL || prefix_buffer == NULL) {
        free(prefix_buffer);
        free(suffix_bit_buffer);
        free(decoded_buffer);
        *decoded_data_out = NULL;
        *decoded_size_out = 0;
        return;
    }

    current_size = original_size;
    iteration_count = 0;
    while (current_size >= RGC_RECURSION_STOP_SIZE) {
        iteration_sizes[iteration_count] = current_size;
        if ((current_size % 2) == 1) {
            current_size++;
        }
        current_size /= 2;
        iteration_count++;
    }
    iteration_sizes[iteration_count] = current_size;

    current_size = (int32_t)rgc_read_u32_le(compressed_data + compressed_size - 4);
    if (current_size > 0) {
        memcpy(decoded_buffer, compressed_data + compressed_size - 4 - current_size, (size_t)current_size);
    }
    compressed_offset = compressed_size - 4 - current_size;
    iteration_index = iteration_count - 1;

    while (iteration_index >= 0) {
        prefix_count = 0;
        for (index = 0; index < current_size; index++) {
            uint8_t packed_prefixes = decoded_buffer[index];

            prefix_buffer[prefix_count] = (uint8_t)(packed_prefixes >> 4);
            prefix_buffer[prefix_count + 1] = (uint8_t)(packed_prefixes & 0x0fU);
            prefix_count += 2;
        }

        current_size = iteration_sizes[iteration_index];
        packed_suffix_byte_count = (int32_t)rgc_read_u32_le(compressed_data + compressed_offset - 4);
        memcpy(suffix_bit_buffer, compressed_data + compressed_offset - 4 - packed_suffix_byte_count, (size_t)packed_suffix_byte_count);

        for (index = packed_suffix_byte_count - 1; index >= 0; index--) {
            uint8_t packed_byte = suffix_bit_buffer[index];
            int32_t bit_offset = index * 8;

            suffix_bit_buffer[bit_offset] = (uint8_t)((packed_byte & 0x80U) >> 7);
            suffix_bit_buffer[bit_offset + 1] = (uint8_t)((packed_byte & 0x40U) >> 6);
            suffix_bit_buffer[bit_offset + 2] = (uint8_t)((packed_byte & 0x20U) >> 5);
            suffix_bit_buffer[bit_offset + 3] = (uint8_t)((packed_byte & 0x10U) >> 4);
            suffix_bit_buffer[bit_offset + 4] = (uint8_t)((packed_byte & 0x08U) >> 3);
            suffix_bit_buffer[bit_offset + 5] = (uint8_t)((packed_byte & 0x04U) >> 2);
            suffix_bit_buffer[bit_offset + 6] = (uint8_t)((packed_byte & 0x02U) >> 1);
            suffix_bit_buffer[bit_offset + 7] = (uint8_t)(packed_byte & 0x01U);
        }

        group_count = compressed_data[compressed_offset - 4 - packed_suffix_byte_count - 1];
        memcpy(
            group_sizes,
            compressed_data + compressed_offset - 4 - packed_suffix_byte_count - 1 - group_count - 1,
            (size_t)(group_count + 1)
        );
        compressed_offset -= 4 + packed_suffix_byte_count + 1 + group_count + 1;

        for (index = group_count; index >= 0; index--) {
            int32_t stored_group_size;

            group_bit_lengths[index] = rgc_log2_power_of_two((uint32_t)group_sizes[index] + 1U);
            stored_group_size = (int32_t)group_sizes[index] + 1;
            memcpy(group_symbols[index], compressed_data + compressed_offset - stored_group_size, (size_t)stored_group_size);
            compressed_offset -= stored_group_size;
        }

        suffix_bit_index = 0;
        for (index = 0; index < current_size; index++) {
            int32_t bit_count;
            int32_t suffix_value;
            int32_t bit_index;
            uint8_t group_id;

            group_id = prefix_buffer[index];
            bit_count = group_bit_lengths[group_id];
            suffix_value = 0;
            suffix_bit_index += bit_count;

            for (bit_index = 1; bit_index <= bit_count; bit_index++) {
                suffix_value = (suffix_value << 1) + suffix_bit_buffer[suffix_bit_index - bit_index];
            }

            decoded_buffer[index] = group_symbols[group_id][suffix_value];
        }

        iteration_index--;
    }

    free(prefix_buffer);
    free(suffix_bit_buffer);
}
