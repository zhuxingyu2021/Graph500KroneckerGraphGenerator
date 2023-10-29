/* Copyright (C) 2009-2010 The Trustees of Indiana University.             */
/*                                                                         */
/* Use, modification and distribution is subject to the Boost Software     */
/* License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at */
/* http://www.boost.org/LICENSE_1_0.txt)                                   */
/*                                                                         */
/*  Authors: Jeremiah Willcock                                             */
/*           Andrew Lumsdaine                                              */

#ifndef GRAPH_GENERATOR_H
#define GRAPH_GENERATOR_H

#include "user_settings.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GENERATOR_USE_PACKED_EDGE_TYPE

typedef struct packed_edge {
  uint32_t v0_low;
  uint32_t v1_low;
  uint32_t high; /* v1 in high half, v0 in low half */
} packed_edge;

static inline int64_t get_v0_from_edge(const packed_edge* p) {
  return (p->v0_low | ((int64_t)((int16_t)(p->high & 0xFFFF)) << 32));
}

static inline int64_t get_v1_from_edge(const packed_edge* p) {
  return (p->v1_low | ((int64_t)((int16_t)(p->high >> 16)) << 32));
}

static inline void write_edge(packed_edge* p, int64_t v0, int64_t v1) {
  p->v0_low = (uint32_t)v0;
  p->v1_low = (uint32_t)v1;
  p->high = (uint32_t)(((v0 >> 32) & 0xFFFF) | (((v1 >> 32) & 0xFFFF) << 16));
}

#else

#include <stdlib.h>

typedef struct packed_edge {
  int64_t v0;
  int64_t v1;
} packed_edge;

static inline int64_t get_v0_from_edge(const packed_edge* p) {
  return p->v0;
}

static inline int64_t get_v1_from_edge(const packed_edge* p) {
  return p->v1;
}

static inline void write_edge(packed_edge* p, int64_t v0, int64_t v1) {
  p->v0 = v0;
  p->v1 = v1;
}

static inline int comp_edge (const void * elem1, const void * elem2)
{
    packed_edge* f = (packed_edge *)elem1;
    packed_edge* s = (packed_edge *)elem2;
    if (f->v0 > s->v0) return  1;
    if (f->v0 < s->v0) return -1;

    // v0 is equal -> compare v1
    if (f->v1 > s->v1) return  1;
    if (f->v1 < s->v1) return -1;

    return 0;
}

static inline void sort_edge(packed_edge* p, size_t size){
    qsort(p, size, sizeof(packed_edge), comp_edge);
}

static inline size_t rm_edge(packed_edge* p,packed_edge* q, size_t size){
    size_t count = 0;
    int first = 1;
    packed_edge last;
    for(size_t i = 0; i < size; ++i){
        if(p[i].v0 != p[i].v1) {
            if (first > 0 || (p[i].v0 != last.v0 || p[i].v1 != last.v1)) {
                q[count] = p[i];
                last = p[i];
                ++count;

                first = 0;
            }
        }
    }

    return count;
}

#endif

/* Generate a range of edges (from start_edge to end_edge of the total graph),
 * writing into elements [0, end_edge - start_edge) of the edges array.  This
 * code is parallel on OpenMP and XMT; it must be used with
 * separately-implemented SPMD parallelism for MPI. */
void generate_kronecker_range(
       const uint_fast32_t seed[5] /* All values in [0, 2^31 - 1) */,
       int logN /* In base 2 */,
       int64_t start_edge, int64_t end_edge /* Indices (in [0, M)) for the edges to generate */,
       packed_edge* edges /* Size >= end_edge - start_edge */
);

#ifdef __cplusplus
}
#endif

#endif /* GRAPH_GENERATOR_H */
