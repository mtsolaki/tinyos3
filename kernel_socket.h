#ifndef __KERNEL_SOCKET_H
#define __KERNEL_SOCKET_H

#include "tinyos.h"
#include "kernel_dev.h"
#include "kernel_streams.h"
#include "kernel_pipe.h"
#include "util.h"

typedef struct listener_socket listener_s;
typedef struct unbound_socket unbound_s;
typedef struct peer_socket peer_s;


typedef enum
{
    UNBOUND,
    LISTENER,
    PEER

}socket_type;


typedef struct socket_control_block
{
    uint refcount;

    FCB* fcb;
    socket_type type;

    port_t port;

    union{
        listener_s* listener;
        unbound_s* unbound;
        peer_s* peer;
    }socket_union;

}SCB;

typedef struct listener_socket
{
    rlnode queue;
    CondVar req_available;

}listener_s;

typedef struct unbound_socket
{
    rlnode unbound_socket;
    
}unbound_s;

typedef struct peer_socket
{
    SCB* peer;
    Pipe_CB* write_pipe;
    Pipe_CB* read_pipe;
    
}peer_s; 



typedef struct connection_request
{
    int admitted;
    SCB* peer;

    CondVar connected_cv;
    rlnode queue_node;
}conn_req;

SCB* PORT_MAP[MAX_PORT + 1];
#endif