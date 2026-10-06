/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file types.h
 * @brief Typové aliasy pro přehlednost a přenositelnost.
 */

#ifndef MZ800_TYPES_H
#define MZ800_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** 8bitový bezznaménkový typ */
typedef uint8_t  u8;
/** 16bitový bezznaménkový typ */
typedef uint16_t u16;
/** 32bitový bezznaménkový typ */
typedef uint32_t u32;
/** 64bitový bezznaménkový typ */
typedef uint64_t u64;
/** 8bitový znaménkový typ */
typedef int8_t   s8;
/** 16bitový znaménkový typ */
typedef int16_t  s16;
/** 32bitový znaménkový typ */
typedef int32_t  s32;

#endif /* MZ800_TYPES_H */
