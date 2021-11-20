
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"

void init_thread()
{
  int exitval;

  TCB* tcb = cur_thread();
  PTCB* ptcb = tcb->owner_ptcb;

  //assert(ptcb != NULL);
  
  Task call = ptcb->task;
  int argl = ptcb->argl;
  void* args = ptcb->args;

  exitval = call(argl, args);

  ThreadExit(exitval);
}

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  if (task != NULL) 
  {
    PTCB* ptcb = (PTCB *)xmalloc(sizeof(PTCB));
    TCB* tcb = CURPROC ->main_thread;

    // Init ptcb

    ptcb->task = task;
    ptcb->argl = argl;
    if(args != NULL)
      ptcb->args = args;
    else
      ptcb->args = NULL;
    
    ptcb->exited = 0;
    ptcb->detached = 0;

    ptcb->exitval = CURPROC->exitval;
    ptcb->exit_cv = COND_INIT;

    CURPROC->thread_count++;
    cur_thread()->owner_ptcb = ptcb;

    TCB* new_tcb = spawn_thread(CURPROC, init_thread);
    new_tcb->owner_ptcb = ptcb;
    ptcb->tcb = new_tcb;
    
    rlnode_init(&ptcb->ptcb_list_node, ptcb);
    rlist_push_back(&CURPROC->ptcb_list, &ptcb->ptcb_list_node);

    wakeup(ptcb->tcb);

    return(Tid_t)ptcb;


  }
  else
	  return NOTHREAD;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread();
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
 
  PTCB* ptcb = (PTCB*)tid;
  if(!rlist_find(&CURPROC->ptcb_list, ptcb, NULL))
  {
    ptcb = NULL;
  }

  if((Tid_t)cur_thread() == tid || ptcb ==NULL || ptcb->detached == 1)
  {
    return -1;
  }
	
  if(ptcb->exited == 1 && ptcb->detached == 1)
  {
    return -1;
  }
 

  ptcb->refcount++;

  while(ptcb->exited != 1 && ptcb->detached != 1)
  {
    kernel_wait(&ptcb->exit_cv,SCHED_USER);
  }

  ptcb->refcount--;

  if(ptcb->exited == 1)
  {
    ptcb->refcount = 0;

    if(exitval!= NULL)
      *exitval = ptcb->exitval;
  }

  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{ 
   PTCB* ptcb = (PTCB*) tid;

  if(!rlist_find(&CURPROC->ptcb_list, ptcb, NULL))
  { 
    ptcb = NULL;
    return -1;
  }

  if(ptcb->exited == 1)
  {
    return -1;
  }



  ptcb->detached = 1;
  kernel_broadcast(&ptcb->exit_cv);
  ptcb->refcount = 0;


  return 0;
  
  
  
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  PTCB* ptcb = cur_thread()->owner_ptcb;

  ptcb->exited = 1;
  ptcb->exitval = exitval;

  kernel_broadcast(& ptcb->exit_cv);

  if (ptcb->tcb->owner_pcb->thread_count == 0)
  {
    sys_Exit(exitval);
  }

  ptcb->refcount--;

  rlist_remove(& ptcb->ptcb_list_node);
  free(ptcb);

  kernel_sleep(EXITED, SCHED_USER);
}

