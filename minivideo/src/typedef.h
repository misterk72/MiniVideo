/*!
 * COPYRIGHT (C) 2010 Emeric Grange - All Rights Reserved
 *
 * This file is part of MiniVideo.
 *
 * MiniVideo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MiniVideo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with MiniVideo.  If not, see <http://www.gnu.org/licenses/>.
 *
 * \file      typedef.h
 * \author    Emeric Grange <emeric.grange@gmail.com>
 * \date      2010
 */

#ifndef TYPEDEF_H
#define TYPEDEF_H

/* ************************************************************************** */

// Contains some settings generated during the CMake configure step
#include "minivideo_settings.h"

/* ************************************************************************** */

// CLI color output
#include "colors.h"

/* ************************************************************************** */

// Custom return codes
#define UNSUPPORTED -1
#define FAILURE      0
#define SUCCESS      1

/* ************************************************************************** */

// Custom types
#if ENABLE_STDINT
    #include <stdint.h>
#else
    // Defines custom types (if C99 <stdint.h> is not available)
    // These types may have already been defined by your compiler

    typedef signed char        int8_t;
    typedef signed short       int16_t;
    typedef signed int         int32_t;
    typedef signed long long   int64_t;

    typedef unsigned char      uint8_t;
    typedef unsigned short     uint16_t;
    typedef unsigned int       uint32_t;
    typedef unsigned long long uint64_t;
#endif // ENABLE_STDINT

/* ************************************************************************** */

// Boolean support
#if ENABLE_STDBOOL
    #include <stdbool.h>
#else
    // Software boolean implementation (if C99 <stdbool.h> is not available)

    typedef unsigned int   bool;
    #define true       1
    #define false      0
#endif // ENABLE_STDBOOL

/* ************************************************************************** */
/*
#if ENABLE_STDALIGN
    #include <stdlib.h>
    #include <stdalign.h>
#else
    // Aligned malloc from POSIX implementation (if C11 <stdalign.h> is not available)
    #include <stdlib.h>

    void *aligned_alloc( size_t alignment, size_t size )
    {
        void *__memptr = NULL;
        posix_memalign(&__memptr, alignment, size);
        return __memptr;
    }
#endif // ENABLE_STDALIGN
*/
/* ************************************************************************** */
#endif // TYPEDEF_H
