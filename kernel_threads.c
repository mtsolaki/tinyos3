
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
  
    PTCB* ptcb = (PTCB *)xmalloc(sizeof(PTCB));

    // Init ptcb

    ptcb->task = task;
    ptcb->argl = argl;
    if(args != NULL)
      ptcb->args = args;
    else
      ptcb->args = NULL;
    
    ptcb->exited = 0;
    ptcb->detached = 0;

    ptcb->exitval = 0;
    ptcb->refcount = 0;
    ptcb->exit_cv = COND_INIT;

    CURPROC->thread_count++;
    cur_thread()->owner_ptcb = ptcb;

    TCB* new_tcb = spawn_thread(CURPROC, init_thread);
    new_tcb->owner_ptcb = ptcb;
    ptcb->tcb = new_tcb;
    
    assert(task!=NULL);
    rlnode_init(&ptcb->ptcb_list_node, ptcb);
    rlist_push_back(&CURPROC->ptcb_list, &ptcb->ptcb_list_node);

    wakeup(ptcb->tcb);

    return(Tid_t)ptcb;


  
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
  TCB* tcb = cur_thread();
	return (Tid_t) tcb->owner_ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
 
  PTCB* ptcb = (PTCB*)tid;
  rlnode* i = rlist_find(&CURPROC->ptcb_list, ptcb, NULL);
  //no thread with this tid  - join itself - detached 
  if( ptcb ==NULL || i == NULL || CURPROC->thread_count == 1)
  {
    return -1;
  }

  ptcb->refcount++;

  while(ptcb->exited != 1 && ptcb->detached != 1)
  {
    kernel_wait(&ptcb->exit_cv,SCHED_USER);
  }


  if(ptcb->detached == 1)
  {
    kernel_broadcast(&ptcb->exit_cv);
    ptcb->refcount--;
    return -1;
  }

  if(ptcb->exited == 1)
  {
    if(exitval!= NULL)
    {
      ptcb->refcount--;
      *exitval = ptcb->exitval;
    }
    else
    {
      return -1;
    }
  }
    if (ptcb->refcount == 0)
    {
      rlist_remove(&ptcb->ptcb_list_node);
      free(ptcb);
      CURPROC->thread_count--;
    }

  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{ 
  PTCB* ptcb = (PTCB*) tid;
  rlnode* i;

  i = rlist_find(&CURPROC->ptcb_list, ptcb, NULL);

  if(i == NULL|| ptcb == NULL || ptcb->exited == 1)
  {
    return -1;
  }

  ptcb->detached = 1;
  kernel_broadcast(&ptcb->exit_cv);


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
  CURPROC->thread_count--;
  kernel_broadcast(& ptcb->exit_cv); //broadcast gia wake up oswn perimenoun

  if (ptcb->tcb->owner_pcb->thread_count == 0)
  {
    sys_Exit(exitval);
  }

  ptcb->refcount--;

  if(ptcb->refcount == 0)
  {
    rlist_remove(& ptcb->ptcb_list_node);
    free(ptcb);
  }

  
  kernel_sleep(EXITED, SCHED_USER);
}

