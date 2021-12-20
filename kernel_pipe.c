#include "kernel_pipe.h"
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_cc.h"
#include "kernel_sched.h"

static file_ops writeOperations = {
  //.Open = serial_open,
  .Read = NULL,
  .Write = pipe_write,
  .Close = pipe_writer_close
};

static file_ops readOperations = {
  //.Open = serial_open,
  .Read = pipe_read,
  .Write = NULL,
  .Close = pipe_reader_close
};

Pipe_CB* pipe_init() {

    Pipe_CB* new_pipe_cb = xmalloc(sizeof(Pipe_CB)); /* Space allocation of the new pipe control block */    
    
	/* Reader, Writer FCB's */
    new_pipe_cb->reader = NULL;
    new_pipe_cb->writer = NULL;

    /* Reader and writer position of the buffer */
    new_pipe_cb->w_position = 0;
    new_pipe_cb->r_position = 0;

    /* Condition variables initialized */
    new_pipe_cb->has_space = COND_INIT;
    new_pipe_cb->has_data = COND_INIT;

    /* Current word length */
    new_pipe_cb->write_data = 0;
	new_pipe_cb->read_data = 0;

    return new_pipe_cb;
}

int sys_Pipe(pipe_t* pipe)
{
	Fid_t fid[2];
    FCB* fcb[2];

	int status = FCB_reserve(2,&fcb[0],&fid[0]);

	if(status == 1){
		Pipe_CB* new_pipe_cb = pipe_init();

		/* Return read and write fid */
		new_pipe_cb->reader = fid[0]; 
		new_pipe_cb->writer = fid[1];

		/* Set streams to point to the pipe_cb objects */
		fcb[0]->streamobj = new_pipe_cb;
    	fcb[1]->streamobj = new_pipe_cb;

		/* Set the functions for read/write */
    	fcb[0]->streamfunc = &readOperations ;
    	fcb[1]->streamfunc = &writeOperations;
	}
	return 0;
}

int pipe_write(void* pipecb_t, const char *buf, unsigned int n){
	Pipe_CB* pipe_cb = (Pipe_CB*)pipecb_t;

	if(pipe_cb == NULL || pipe_cb->reader == NULL)
		return -1;

	int buffer_counter=0;
	int write_data;

	for(int i=0; i<n; i++){
		pipe_cb->buffer[pipe_cb->w_position] = buf[i]; /* Store byte from our given buffer to the pipe_CB buffer */
		pipe_cb->w_position++; 
		pipe_cb->write_data++;
		//piphs

	    /* If writer wrote all buffer and reader is sleeping
        until new data is read from the pipe, to free space */
        while(pipe_cb->write_data == (int)PIPE_BUFFER_SIZE)
            kernel_wait(&pipe_cb->has_space,SCHED_PIPE);

        /* When writer pointer reaches end of buffer, cycle to the beginning */
        if(pipe_cb->w_position == ((int)PIPE_BUFFER_SIZE - 1)){
            pipe_cb->w_position = 0;
        }

		kernel_broadcast(&pipe_cb->has_data);
	}
  		return buffer_counter;
	
}

int pipe_read(void* pipecb_t, char *buf, unsigned int n){
	
	Pipe_CB* pipe_cb = (Pipe_CB*)pipecb_t;
	if(pipe_cb == NULL || pipe_cb->writer == NULL)
		return -1;

	int buffer_counter=0;
	int read_data;

	while(buffer_counter < n){ /* No data to Read */

        while(pipe_cb->write_data==0){
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
	}
	for (int i = 0; i < buffer_counter; i++){
		buf[i]=pipe_cb->buffer[pipe_cb->r_position]; 
		pipe_cb->r_position++;
		pipe_cb->read_data--;
		//pipa

		/* When reader pointer reaches end of buffer, cycle to the beginning (Bounded Buffer) */
        if(pipe_cb->r_position == ((int)PIPE_BUFFER_SIZE - 1)){
            pipe_cb->r_position = 0;
        }

		if(pipe_cb->read_data<PIPE_BUFFER_SIZE)
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

	kernel_broadcast(&pipe_cb->has_data); /* Wake up the reader */ 

	if(pipe_cb->reader == NULL)
		free(pipe_cb);

	return 0;
}

int pipe_reader_close(void* pipecb_t){
	Pipe_CB* pipe_cb = (Pipe_CB*)pipecb_t;

	if(pipe_cb == NULL)
		return -1;

	pipe_cb->reader = NULL;

	kernel_broadcast(&pipe_cb->has_space); /* Wake up the writer */ 

	if(pipe_cb->writer == NULL)
		free(pipe_cb);

	return 0;

}



