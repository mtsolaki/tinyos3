#ifndef __KERNEL_PIPE_H
#define __KERNEL_PIPE_H
#include "tinyos.h"
#include "kernel_streams.h"

#define PIPE_BUFFER_SIZE 4096   /* Size of Buffer */

/**
  @brief Pipe Control Block.
  This structure holds all information pertaining to a Pipe.
 */

typedef struct pipe_control_block {

    FCB *reader, *writer; /* Pointers to read/write from buffer*/

    CondVar has_space; /* For blocking writer if no space is available*/
 
    CondVar has_data; /* For blocking reader until data are available*/ 

    int w_position, r_position; /* Write and Read position in buffer*/

    char buffer[PIPE_BUFFER_SIZE]; /* Bounded (cyclic) byte buffer*/

    int word_length;
    
} Pipe_CB;

int sys_Pipe(pipe_t* pipe);

int pipe_write(void* pipecb_t, const char *buf, unsigned int n);

int pipe_read(void* pipecb_t, char *buf, unsigned int n);

int pipe_writer_close(void* pipecb_t);

int pipe_reader_close(void* pipecb_t);

#endif