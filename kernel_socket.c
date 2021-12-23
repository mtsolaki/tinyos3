#include "tinyos.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_cc.h"
#include "kernel_sched.h"
#include "kernel_socket.h"
#include "kernel_pipe.h"
#include "tinyos.h"



int close_socket(void* socket)
{
	SCB* scb = socket;
	free(scb);
	return 0;
}
int close_listener(void* socket)
{
	SCB* scb = socket;

	PORT_MAP[scb->port] = NULL;

	kernel_signal(&scb->socket_union.listener->req_available);
	close_socket(scb);

	return 0;
}


file_ops socket_fops = {
	.Open = NULL,
	.Read = NULL,
	.Write = NULL,
	.Close = (void *)close_socket};

file_ops listener_fops = {
	.Open = NULL,
	.Read = NULL,
	.Write = NULL,
	.Close = (void *)close_listener};



Fid_t sys_Socket(port_t port)
{	
	// returns NOFILE if port doesn't exist
	if (port < 0 || port > MAX_PORT )
		return NOFILE;
	
	FCB* fcb;
	Fid_t fid;

	if(!FCB_reserve(1, &fid, &fcb))
	{
		return NOFILE;
	}
	SCB* scb = (SCB*)xmalloc(sizeof(SCB));

	/* Socket init */

	scb->fcb = fcb;
	scb->port = port;
	scb->type = UNBOUND;
	scb->refcount = 0;
	
	fcb->streamobj = scb;
	fcb->streamfunc = &socket_fops;

	return fid;
	
}

int sys_Listen(Fid_t sock)
{
	if (sock<0 || sock >MAX_FILEID)
		return -1;
	
	FCB* fcb = get_fcb(sock);
	
	if(fcb != NULL && fcb->streamobj !=NULL)
	{
		SCB* listener_scb = fcb->streamobj;
	
		if (listener_scb != NULL && listener_scb->port != NOPORT)
		{
			SCB* scb = PORT_MAP[listener_scb->port];

			if(listener_scb->type == UNBOUND && scb == NULL)
			{
				listener_scb->type = LISTENER;
				listener_scb->socket_union.listener = (listener_s* )xmalloc(sizeof(listener_s));
				rlnode_init(&listener_scb->socket_union.listener->queue, NULL);
				listener_scb->socket_union.listener->req_available = COND_INIT;
				fcb->streamfunc = &listener_fops;
				PORT_MAP[listener_scb->port] = listener_scb;
				return 0;
			}
		
		
		}
		
	}
	return -1;
	
	
}


Fid_t sys_Accept(Fid_t lsock)
{
	FCB* fcb = get_fcb(lsock);
	if (fcb == NULL || fcb->streamobj != NULL)
		return NOFILE;
	SCB* listener_scb = fcb->streamobj;

	if(listener_scb != NULL || listener_scb->type != LISTENER)
		return NOFILE;
			
	if(is_rlist_empty(&listener_scb->socket_union.listener->queue))
		kernel_wait(&listener_scb->socket_union.listener->queue, SCHED_PIPE);
	

	Fid_t peer_fid = Socket(NOPORT);
	if(peer_fid == NOFILE)
		return NOFILE;

	FCB* fcb_peer = get_fcb(peer_fid);
	SCB* peer = fcb_peer->streamobj;

	rlnode* request_node = rlist_pop_front(&listener_scb->socket_union.listener->queue);
	if(request_node == NULL)
		return NOFILE;

	listener_scb->refcount++;

	SCB* request_scb = request_node->scb;


	Pipe_CB*  pipe1, *pipe2;
	pipe1 = (Pipe_CB*)xmalloc(sizeof(Pipe_CB));
	pipe2 = (Pipe_CB*)xmalloc(sizeof(Pipe_CB));

	pipe1->has_data = COND_INIT;
	pipe1->has_space = COND_INIT;

	pipe1->w_position = 0;
	pipe1->r_position = 0;
	pipe1->reader = request_scb->fcb;
	pipe1->writer = peer->fcb;

	pipe2->has_data = COND_INIT;
	pipe2->has_space = COND_INIT;

	pipe2->w_position = 0;
	pipe2->r_position = 0;
	pipe2->reader = peer->fcb;
	pipe2->writer = request_scb->fcb;

	peer->socket_union.peer = (peer_s *)xmalloc(sizeof(peer_s));
	request_scb->socket_union.peer = (peer_s *)xmalloc(sizeof(peer_s));

	peer_s* peer1 = peer->socket_union.peer;
	peer_s* peer2 = request_scb->socket_union.peer;

	peer1->peer = peer;
	peer2->peer = request_scb;

	peer1->write_pipe = pipe2;
	peer1->read_pipe = pipe1;

	peer2->write_pipe = pipe1;
	peer2->read_pipe = pipe2;

	kernel_signal(&(request_node->crcb->connected_cv));
	
	listener_scb->refcount--;
	if(listener_scb->refcount == 0)
	{
		free(listener_scb);
	}
	
	return peer_fid;
	
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	FCB* fcb = get_fcb(sock);

	if(fcb == NULL || fcb->streamobj == NULL)
		return -1;
	
	SCB* scb = fcb->streamobj;
	if(port < 1 || port > MAX_PORT || scb->type != UNBOUND || PORT_MAP[port] == NULL || PORT_MAP[port]->type != LISTENER )
		return -1;
	
	conn_req*  cr = (conn_req*)xmalloc(sizeof(conn_req));
	cr->peer = scb;
	cr->peer->refcount++;
	cr->admitted=0;
	cr->connected_cv = COND_INIT;

	rlnode_init(&cr->queue_node, cr);

	SCB* lis_scb = PORT_MAP[port];
	rlist_push_back(&lis_scb->socket_union.listener->queue, &cr->queue_node);
	kernel_signal(&lis_scb->socket_union.listener->req_available);

	scb->refcount++;
	kernel_timedwait(&(cr->connected_cv), SCHED_PIPE, timeout);
	scb->refcount--;
	
	if (cr->admitted == 0 )
		return -1;
	
	return 0;

	
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	FCB* fcb = get_fcb(sock);
	if(fcb == NULL || fcb->streamobj == NULL)
		return -1;
	
	SCB* scb = fcb->streamobj;
	if(scb == NULL || scb->type != PEER) 
		return -1;
	
	switch (how){
		//Shut down only reader
		case SHUTDOWN_READ:
			pipe_reader_close(scb->socket_union.peer->read_pipe);
			scb->socket_union.peer->read_pipe = NULL;
			break;
		//Shut down only writer
		case SHUTDOWN_WRITE:
			pipe_writer_close(scb->socket_union.peer->write_pipe);
			scb->socket_union.peer->write_pipe = NULL;
			break;
		//Shut down both, reader and writer
		case SHUTDOWN_BOTH:
			pipe_reader_close(scb->socket_union.peer->read_pipe);
			scb->socket_union.peer->read_pipe = NULL;
			pipe_writer_close(scb->socket_union.peer->write_pipe);
			scb->socket_union.peer->write_pipe = NULL;
			break;
		default:
			return -1;
			break;
	}
	return 0;
}

