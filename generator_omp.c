/* Copyright (C) 2009-2010 The Trustees of Indiana University.             */
/*                                                                         */
/* Use, modification and distribution is subject to the Boost Software     */
/* License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at */
/* http://www.boost.org/LICENSE_1_0.txt)                                   */
/*                                                                         */
/*  Authors: Jeremiah Willcock                                             */
/*           Andrew Lumsdaine                                              */

/* Modified by Xin Yi                                                      */

#include <math.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>
#include <stdio.h>
#include <omp.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <string.h>
#include <stdlib.h>

#include "make_graph.h"

void printError() {
    fprintf(stderr,
            "usage: ./generator_omp <# of vertices (log 2 base)> <-e intNumber [optional: average # of edges per vertex, defualt to be 16> <-o outputFileName [optional: default to stdout]> <-s randomSeedInt [optional: default to use the current time]> <-b [optional: default is ascii version, -b for binary version]>\n");
    exit(0);
}


int main(int argc, char *argv[]) {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    int seed = currentTime.tv_sec ^currentTime.tv_usec;
    seed ^= seed >> 12;
    seed ^= seed << 25;
    seed ^= seed >> 27;

    FILE *fout;
    int fd;
    if (argc < 2 || argc > 10) {
        printError();
    }

    // define all the variables
    int log_numverts = -1;
    char *filename = "";
    int64_t numEdges;
    double start, time_taken;
    double start_write, time_taken_write;
    int64_t nedges;
    packed_edge *result;
    int binary = 0; // set default to be not binary, normal tsv

    numEdges = 16;  // default 16
    fout = stdout;  // default the stdout

    int opt;
    int r = 0;
    while (optind < argc) {
        if ((opt = getopt(argc, argv, "+e:o:s:b:r")) != -1) {
            switch (opt) {
                case 'e':
                    numEdges = atoi(optarg);
                    break;
                case 'o':
                    filename = optarg;
                    fd = open(optarg, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

                    fout = fopen(optarg, "wb");
                    if (fout == NULL) {
                        fprintf(stderr, "%s -- ", optarg);
                        perror("fopen for write failed");
                        exit(1);
                    }
                    break;
                case 's':
                    seed = atoi(optarg);
                    break;
                case 'b':
                    binary = 1;
                    break;
                case 'r':
                    r = 1;
                    break;
                default:
                    printError();
                    break;
            }
        } else {
            if (argv[optind] == NULL) {
                printError();
            } else {
                log_numverts = atoi(argv[optind]); // In base 2
                optind++;
            }
        }
    }

    if (log_numverts < 0) {
        printError();
    }

    /* Start of graph generation timing */
    start = omp_get_wtime();
    make_graph(log_numverts, numEdges << log_numverts, seed, seed, &nedges, &result);
    time_taken = omp_get_wtime() - start;

    /* End of graph generation timing */
    printf("For 2^%d\n", log_numverts);
    printf("\t%f seconds for making graph\n", time_taken);

//  start_write = omp_get_wtime();
//  produce_graph(numEdges << log_numverts, &result, fout, binary);
//  time_taken_write = omp_get_wtime() - start_write;

//  if (binary == 0) {
//    printf("\t%f seconds for writing ascii version\n", time_taken_write);
//  } else {
//  	printf("\t%f seconds for writing binary version\n", time_taken_write);
//  }
    if(r == 1){
        start_write = omp_get_wtime();

        size_t nv = 1 << log_numverts;
        size_t num_edges = (numEdges << log_numverts);
        packed_edge *result2 = (packed_edge*)malloc(sizeof(packed_edge) * num_edges * 2);
        memcpy(result2, result, sizeof(packed_edge) * num_edges);
        free(result);

#pragma omp parallel for
        for (size_t i = 0; i < num_edges; i++) {
            result2[num_edges + i].v0 = result2[i].v1;
            result2[num_edges + i].v1 = result2[i].v0;
        }

        sort_edge(result2, num_edges * 2);
        packed_edge *result3 = (packed_edge*)malloc(sizeof(packed_edge) * num_edges * 2);
        size_t new_edges = rm_edge(result2, result3, num_edges * 2);

        free(result2);

        size_t size_header = 4*sizeof(uint64_t);
        size_t size_xadj = (nv+1) * sizeof(uint64_t);
        size_t size_adjncy = new_edges * sizeof(uint32_t);

        size_t file_size = size_header + size_xadj + size_adjncy;

        int ret = ftruncate(fd, file_size);
        printf("%d\n", ret);
        printf("%zu, %zu\n", new_edges, file_size);

        uint64_t *mmap_arr_ = (uint64_t *) mmap(NULL, file_size, PROT_WRITE | PROT_READ,
                                                MAP_SHARED, fd, 0);


        mmap_arr_[0] = nv;
        mmap_arr_[1] = new_edges;
        mmap_arr_[2] = 0;

        uint64_t *xadj = &mmap_arr_[4];
        uint32_t *adjncy = (uint32_t*)&mmap_arr_[nv + 5];

        uint64_t *xadj_mem = (uint64_t*) malloc(sizeof(uint64_t) * (nv+1));
        memset(xadj_mem, 0, (nv+1) * sizeof(uint64_t));

        uint32_t last_from;
        size_t edge_count = 0;
        for (size_t i = 0; i < new_edges; i++) {
            uint32_t from = get_v0_from_edge(result3 + i);

            if(from != last_from && i != 0){
                xadj_mem[last_from] += edge_count;

                edge_count = 0;
            }

            ++edge_count;
            last_from = from;
        }

        xadj_mem[last_from] += edge_count;

        for(int i = 1; i < nv+1; ++i){
            xadj_mem[i] += xadj_mem[i-1];
        }

        memcpy(xadj, xadj_mem, sizeof(uint64_t) * (nv+1));
        free(xadj_mem);

#pragma omp parallel for
        for (size_t i = 0; i < new_edges; i++) {
            uint32_t to = get_v1_from_edge(result3 + i);

            adjncy[i] = to;
        }

        munmap(mmap_arr_, file_size);
        close(fd);

        time_taken_write = omp_get_wtime() - start_write;
        printf("\t%f seconds for writing binary version\n", time_taken_write);
    }
    else if (binary == 0) {
        // print to the file
        start_write = omp_get_wtime();

        size_t num_edges = (numEdges << log_numverts);
        packed_edge *result2 = (packed_edge*)malloc(sizeof(packed_edge) * num_edges * 2);
        memcpy(result2, result, sizeof(packed_edge) * num_edges);
        free(result);

        for (size_t i = 0; i < num_edges; i++) {
            result2[num_edges + i].v0 = result2[i].v1;
            result2[num_edges + i].v1 = result2[i].v0;
        }

        sort_edge(result2, num_edges * 2);
        packed_edge *result3 = (packed_edge*)malloc(sizeof(packed_edge) * num_edges * 2);
        size_t new_edges = rm_edge(result2, result3, num_edges * 2);

        free(result2);
        for (int i = 0; i < new_edges; i++) {
            fprintf(fout, "%lu\t%lu\n", get_v0_from_edge(result3 + i), get_v1_from_edge(result3 + i));
        }
        time_taken_write = omp_get_wtime() - start_write;
        printf("\t%f seconds for writing ascii version\n", time_taken_write);
    } else {
        // need to print binary
        printf("fd: %d\n", fd);
        start_write = omp_get_wtime();
        size_t num_edges = (numEdges << log_numverts);
        size_t file_size = num_edges * sizeof(uint32_t) * 2;
        int ret = ftruncate(fd, file_size);
        printf("%d\n", ret);
        printf("%zu, %zu\n", num_edges, file_size);

        uint32_t *mmap_arr_ = (uint32_t *) mmap(NULL, file_size, PROT_WRITE | PROT_READ,
                                                MAP_SHARED, fd, 0);

#pragma omp parallel for
        for (size_t i = 0; i < num_edges; i++) {
            uint32_t from = get_v0_from_edge(result + i);
            uint32_t to = get_v1_from_edge(result + i);
            // add the check for not exceed the uint32_t max
//            printf("%d, %d\n", from, to);
            mmap_arr_[i * 2] = from;
            mmap_arr_[i * 2 + 1] = to;
        }
        munmap(mmap_arr_, file_size);
        close(fd);
        time_taken_write = omp_get_wtime() - start_write;
        printf("\t%f seconds for writing binary version\n", time_taken_write);
    }

    // used to check if fclose works well
    int check_correctness;
    check_correctness = fclose(fout);
    if (check_correctness == EOF) {
        fprintf(stderr, "%s -- ", filename);
        perror("fclose for failed");
        exit(1);
    }

    return 0;
}
