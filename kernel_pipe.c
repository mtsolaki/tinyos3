#include "kernel_pipe.h"
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_cc.h"
#include "kernel_sched.h"

static file_ops writeOperations = {
  .Open = NULL,
  .Read = NULL,
  .Write = pipe_write,
  .Close = pipe_writer_close
};

static file_ops readOperations = {
  .Open = NULL,
  .Read = pipe_read,
  .Write = NULL,
  .Close = pipe_reader_close
};


int pipe_write(void* pipecb_t, const char *buf, unsigned int n){
	Pipe_CB* pipe_cb = (Pipe_CB*)pipecb_t;

	if(pipe_cb == NULL || pipe_cb->reader == NULL || pipe_cb->writer == NULL)
		return -1;

	int buffer_counter=0;

	while(buffer_counter<n && pipe_cb->reader != NULL){
		pipe_cb->buffer[pipe_cb->w_position] = buf[buffer_counter]; /* Store byte from our given buffer to the pipe_CB buffer */
		pipe_cb->w_position++; 
		pipe_cb->word_length++;
		//piphs

	    /* If writer wrote all buffer and reader is sleeping
        until new data is read from the pipe, to free space */
        while(pipe_cb->word_length == (int)PIPE_BUFFER_SIZE)
            kernel_wait(&pipe_cb->has_space,SCHED_PIPE);

        /* When writer pointer reaches end of buffer, cycle to the beginning */
        if(pipe_cb->w_position == ((int)PIPE_BUFFER_SIZE - 1)){
            pipe_cb->w_position = 0;
        }

		kernel_broadcast(&pipe_cb->has_data);
		buffer_counter++;
	}
		if(pipe_cb->reader == NULL && pipe_cb->word_length == n)
        	return -1;
  		return buffer_counter;
	
}

int pipe_read(void* pipecb_t, char *buf, unsigned int n){
	
	Pipe_CB* pipe_cb = (Pipe_CB*)pipecb_t;
	if(pipe_cb == NULL || pipe_cb->reader == NULL)
		return -1;

	int buffer_counter=0;

	while(buffer_counter < n){ /* No data to Read */

        while(pipe_cb->word_length==0){
            if(pipe_cb->writer == NULL)
                /*  In case there is no more data stored and writer is closed,
                    return how much data has already been read.
                    If writer was already closed when pipe_read() was called
                    then it will return 0.
                */
                return buffer_counter;
            /* if we expect someone to write, sleep till then*/
            kernel_wait(&pipe_cb->has_data, SCHED_PIPE);
        }

		buf[buffer_counter]=pipe_cb->buffer[pipe_cb->r_position]; 
		pipe_cb->r_position++;
		pipe_cb->word_length--;
		//pipa

		/* When reader pointer reaches end of buffer, cycle to the beginning (Bounded Buffer) */
        if(pipe_cb->r_position == ((int)PIPE_BUFFER_SIZE - 1)){
            pipe_cb->r_position = 0;
        }

		if(pipe_cb->word_length<PIPE_BUFFER_SIZE)
		kernel_broadcast(&pipe_cb->has_space);

		buffer_counter++;
	}
  	return buffer_counter;

}

int pipe_writer_close(void* pipecb_t){
	Pipe_CB* pipe_cb = (Pipe_CB*)pipecb_t;

	if(pipe_cb == NULL)
		return -1; 

	pipe_cb->writer = NULL; 

	if(pipe_cb->reader == NULL)
		free(pipe_cb);

	else
		kernel_broadcast(&pipe_cb->has_data); /* Wake up the reader */

	return 0;
}

int pipe_reader_close(void* pipecb_t){
	Pipe_CB* pipe_cb = (Pipe_CB*)pipecb_t;

	if(pipe_cb == NULL)
		return -1;

	pipe_cb->reader = NULL;

	if(pipe_cb->writer == NULL)
		free(pipe_cb);

	else
		kernel_broadcast(&pipe_cb->has_space);

	return 0;
}

int sys_Pipe(pipe_t* pipe)
{
	Fid_t fid[2];
    FCB* fcb[2];

	int status = FCB_reserve(2,fid,fcb);

	if(status == 1){
		Pipe_CB* new_pipe_cb;
		new_pipe_cb = (Pipe_CB*)xmalloc(sizeof(Pipe_CB)); /* Space allocation of the new pipe control block */

		/* Return read and write fid */
		pipe->read = fid[0]; 
		pipe->write = fid[1];

		/* Reader, Writer FCB's */
    	new_pipe_cb->reader = fcb[0];
    	new_pipe_cb->writer = fcb[1];

    	/* Reader and writer position of the buffer */
    	new_pipe_cb->w_position = 0;
    	new_pipe_cb->r_position = 0;

		/* Current word length */
    	new_pipe_cb->word_length = 0;

    	/* Condition variables initialized */
    	new_pipe_cb->has_space = COND_INIT;
    	new_pipe_cb->has_data = COND_INIT;

		/* Set streams to point to the pipe_cb objects */
		fcb[0]->streamobj = new_pipe_cb;
    	fcb[1]->streamobj = new_pipe_cb;

		/* Set the functions for read/write */
    	fcb[0]->streamfunc = &readOperations;
    	fcb[1]->streamfunc = &writeOperations;
	}
	return 0;
}

