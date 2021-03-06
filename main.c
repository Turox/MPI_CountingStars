//
// Created by Turox on 7/17/2018.
//

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "src/png_manager.h"
#include "src/matrix.h"
#include "src/image_processing.h"
#include <sys/time.h>
#include <bits/time.h>
#include <time.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Please provide a block size and a file\n");
        exit(1);
    }

    int block_size = atoi(argv[1]);
    // Initialize the MPI environment
    MPI_Init(NULL, NULL);

    // Get the number of processes
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // Get the rank of the process
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Get the name of the processor
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);

    // Print off a hello world message
    printf("Hello world from processor %s, rank %d out of %d processors\n",
           processor_name, world_rank, world_size);

    if (world_rank == 0) {//Main process
        int k = 0;
        char hostname[256];
        gethostname(hostname, sizeof(hostname));
        printf("PID %d on %s ready for attach\n", getpid(), hostname);
        fflush(stdout);
        /*while (0 == k)
            sleep(5);*/

        printf("RANK 0: Reading PNG\n");
        struct timespec end, start;
        __time_t total_time = 0;
        long int total_starts_count = 0;
        for (int i = 2; i < argc; ++i) {
            struct png_file file = read_png(argv[i]);

            int total = 0;
            int received_subtotals[world_size - 1];
            MPI_Request receive_requests[world_size - 1];
            // MPI_Request send_requests[world_size - 1];

            //Send the first blocks
            printf("RANK 0: Sending the first blocks\n");
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            int i;
            int iter_num = 0;
            int current_I = 0, current_J = 0;

            int max_working_slaves = 0;
            while (current_I < file.width) {
                while (current_J < file.height) {
                    printf("RANK 0: Sending %d bytes POS{X: %d,Y: %d} to RANK %d\n", block_size * block_size, current_I,
                           current_J, iter_num + 1);
                    max_working_slaves++;
                    int *extracted = extract_zf(file.data,
                                                current_I, current_J, file.height, file.width, block_size,
                                                block_size);
                    MPI_Send(extracted, block_size * block_size, MPI_INT, iter_num + 1, 0, MPI_COMM_WORLD);
                    free(extracted);
                    current_J += block_size;

                    if (++iter_num >= world_size - 1)
                        goto leave1;
                }
                current_J = 0;
                current_I += block_size;
            }

            leave1:
            printf("RANK 0: Instantiating receive routines\n");
            //Receive the results
            for (i = 1; i < world_size; ++i) {
                MPI_Irecv(&received_subtotals[i - 1], 1, MPI_INT, i, 0, MPI_COMM_WORLD, &receive_requests[i - 1]);
            }

            printf("RANK 0: Entering receive loop routine\n");
            //While the whole matrix isn't consumed

            int num_done;
            int indices[world_size - 1];
            MPI_Status statuses[world_size - 1];
            int total_done = 0;
            int last_iter_done = 0;
            while (1) {
                printf("RANK 0: Waiting for slaves\n");
                MPI_Waitsome(world_size - 1, receive_requests, &num_done, indices, statuses);
                printf("RANK 0: %d slaves finished\n", num_done);

                total_done += num_done * (block_size * block_size);

                iter_num = 0;

                //Collect results
                for (int j = 0; j < num_done; ++j) {
                    total += received_subtotals[indices[j]];
                }

                while (current_I < file.width) {
                    while (current_J < file.height) {
                        printf("RANK 0: Received result from RANK %d\n", indices[iter_num] + 1);

                        printf("RANK 0: Sending %d bytes POS{X: %d,Y: %d} to RANK %d\n", block_size * block_size, current_I,
                               current_J, indices[iter_num] + 1);
                        int *extracted = extract_zf(file.data,
                                                    current_I, current_J, file.height, file.width, block_size,
                                                    block_size);
                        MPI_Send(extracted, block_size * block_size, MPI_INT, indices[iter_num] + 1, 0, MPI_COMM_WORLD);
                        free(extracted);
                        MPI_Irecv(&received_subtotals[indices[iter_num]], 1, MPI_INT, indices[iter_num] + 1, 0,
                                  MPI_COMM_WORLD,
                                  &receive_requests[indices[iter_num]]);
                        current_J += block_size;

                        if (++iter_num > num_done - 1)
                            goto leave2;
                    }
                    current_J = 0;
                    current_I += block_size;
                }
                leave2:
                printf("RANK 0: Counted stars: %d, bytes processed: %d/%d\n", total, total_done,
                       (file.width * file.height));

                // Wait for lazy workers
                if(current_I >= file.width){
                    last_iter_done += num_done;
                    if(last_iter_done == max_working_slaves){
                        break;
                    }
                }
            }
            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            __time_t i1 = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
            printf("RANK 0: Finished in %lu microseconds \n", i1);
            total_time += i1;
            total_starts_count += total;
            free(file.data);
        }


        printf("RANK 0: Total time %lu microseconds \n", total_time);
        printf("RANK 0: %li stars counted \n", total_starts_count);
        MPI_Abort(MPI_COMM_WORLD, 0);
        MPI_Finalize();
        exit(0);

    } else {
        //Slave
        //int sub_image[block_size*block_size];
        int *sub_image = (int *) malloc(sizeof(int) * block_size * block_size);
        MPI_Status status;

        while (1) {
            printf("RANK %d: Waiting block from RANK 0\n", world_rank);
            fflush(stdout);
            MPI_Recv(sub_image, block_size * block_size, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
            printf("RANK %d: Received block from RANK 0\n", world_rank);
            fflush(stdout);
            binarize(sub_image, block_size, block_size, 128);
            int res = count_objects(sub_image, block_size, block_size);
            printf("RANK %d: Sending result to RANK 0\n", world_rank);
            fflush(stdout);
            MPI_Send(&res, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }
    }

}
