#include "t_lib.h"

TCB_Queue readyQueue;
TCB_Queue runningQueue;

// calling thread is put at end of ready queue and the head of readyQueue continues
// execution
void t_yield()
{
  if (readyQueue.head != NULL){
    tcb *tmp = runningQueue.head;
    addThread_ToReadyQueue(tmp);

    runningQueue.head = readyQueue.head;

    readyQueue.head = readyQueue.head->next;

    swapcontext(tmp->thread_context, runningQueue.head->thread_context);
  }
}

// initialize main thread and add to runningQueue
void t_init()
{
  tcb *tmp = (tcb *) malloc(sizeof(tcb));
  tmp->thread_id = -1;
  tmp->thread_priority = 1;
  tmp->thread_context = (ucontext_t *) malloc(sizeof(ucontext_t));
  tmp->next = NULL;

  getcontext(tmp->thread_context);

  addThread_ToRunningQueue(tmp);
}

// create a new thread and add to readyQueue
int t_create(void (*fct)(int), int id, int pri)
{
  size_t sz = 0x10000;

  ucontext_t *uc;
  uc = (ucontext_t *) malloc(sizeof(ucontext_t));

  getcontext(uc);
/***
  uc->uc_stack.ss_sp = mmap(0, sz,
       PROT_READ | PROT_WRITE | PROT_EXEC,
       MAP_PRIVATE | MAP_ANON, -1, 0);
***/
  uc->uc_stack.ss_sp = malloc(sz);  /* new statement */
  uc->uc_stack.ss_size = sz;
  uc->uc_stack.ss_flags = 0;
  uc->uc_link = runningQueue.tail->thread_context;
  makecontext(uc, (void (*)(void)) fct, 1, id);

  tcb *tmp = (tcb *) malloc(sizeof(tcb));
  tmp->thread_id = id;
  tmp->thread_priority = 1;
  tmp->thread_context = uc;
  tmp->next = NULL;

  addThread_ToReadyQueue(tmp);
}

// free all memory in running and ready queues
void t_shutdown(){
  if (runningQueue.head != NULL){
    free(runningQueue.head->thread_context->uc_stack.ss_sp);
    free(runningQueue.head->thread_context);
    free(runningQueue.head);
  }
  while (readyQueue.head != NULL){
    tcb *tmp = readyQueue.head;
    readyQueue.head = readyQueue.head->next;
    free(tmp->thread_context->uc_stack.ss_sp);
    free(tmp->thread_context);
    free(tmp);
  }
}

// terminate the calling thread and continue execution of first thread in readyQueue
void t_terminate(){
  tcb *tmp = runningQueue.head;
  free(tmp->thread_context->uc_stack.ss_sp);
  free(tmp->thread_context);
  free(tmp);
  runningQueue.head = readyQueue.head;
  readyQueue.head = readyQueue.head->next;

  setcontext(runningQueue.head->thread_context);
}

// add thread to end of readyQueue
int addThread_ToReadyQueue(tcb *t){
  if (readyQueue.head == NULL){
    t->next = NULL;
    readyQueue.head = t;
    readyQueue.tail = t;
  }
  else {
    readyQueue.tail->next = t;
    readyQueue.tail = t;
    readyQueue.tail->next = NULL;
  }
}
 // add thread to end of runningQueue
int addThread_ToRunningQueue(tcb *t){
  if (runningQueue.head == NULL){
    t->next = NULL;
    runningQueue.head = t;
    runningQueue.tail = t;
  }
  else {
    runningQueue.tail->next = t;
    runningQueue.tail = t;
    runningQueue.tail->next = NULL;
  }
}

// initialize a semaphore
int sem_init(sem_t **sp, int sem_count)
{
  *sp = malloc(sizeof(sem_t));
  (*sp)->count = sem_count;
  (*sp)->q = NULL;
}

// add a thread to a semaphore's queue
int addThread_ToSemQueue(sem_t *s, tcb *t){
  // if queue is empty, add as head
  if (s->q == NULL){
    s->q = t;
  }
  // else move to end of queue and add there
  else {
    tcb *tmp;
    tmp = s->q;
    while (tmp->next != NULL){
      tmp = tmp->next;
    }
    tmp->next = t;
    t->next = NULL;
  }
}

// Current Thread ddoes a wait (P) on the specified semaphore
void sem_wait(sem_t *s){
  sigrelse(SIGINT);
  s->count--;
  if (s->count < 0){
    addThread_ToSemQueue(s, runningQueue.head);
    // block the running thread
    tcb *tmp = runningQueue.head;
    runningQueue.head = readyQueue.head;
    readyQueue.head = readyQueue.head->next;
    swapcontext(tmp->thread_context, runningQueue.head->thread_context);

    sighold(SIGINT);
  }
  else {
    sighold(SIGINT);
  }
}

// calling thread does a signal (V) on the specified semaphore
void sem_signal(sem_t *s){
  sigrelse(SIGINT);
  s->count++;
  if (s->count <= 0){
    tcb *tmp = s->q;
    s->q = s->q->next;
    addThread_ToReadyQueue(tmp);
  }
  sighold(SIGINT);
}

// destroy any state related to specified semaphore
void sem_destroy(sem_t **s){
  // free the semaphore's queue
  while ((*s)->q != NULL){
    tcb *tmp = (*s)->q;
    (*s)->q = (*s)->q->next;
    free(tmp->thread_context->uc_stack.ss_sp);
    free(tmp->thread_context);
    free(tmp);
  }
  // free the semaphore itself
  free(*s);
}

int mbox_create(mbox **mb){
  *mb = malloc(sizeof(mbox));
  (*mb)->msg = NULL;
  sem_t *sp = malloc(sizeof(sem_t));
  sp->count = 1;
  sp->q = NULL;
  (*mb)->mbox_sem = sp;
}

void mbox_deposit(mbox *mb, char *msg, int len){
  sem_wait(mb->mbox_sem);
  // create new messageNode
  messageNode *msgNode = malloc(sizeof(messageNode));
  msgNode->message = malloc(sizeof(char) * len);
  strcpy(msgNode->message, msg);
  msgNode->sender = -1;
  msgNode->receiver = -1;
  // insert at end of mailbox message queue
  if (mb->msg == NULL){
    mb->msg = msgNode;
  }
  else {
    messageNode *tmp;
    tmp = mb->msg;
    while (tmp->next != NULL){
      tmp = tmp->next;
    }
    tmp->next = msgNode;
    msgNode->next = NULL;
  }
  sem_signal(mb->mbox_sem);
}

void mbox_withdraw(mbox *mb, char *msg, int *len){
  sem_wait(mb->mbox_sem);
  messageNode *tmp = mb->msg;
  mb->msg = mb->msg->next;
  if (tmp == NULL){
    len = 0;
  }
  else {
    len = &(tmp->len);
  }
  strcpy(msg, tmp->message);
  free(tmp->message);
  free(tmp);
  sem_signal(mb->mbox_sem);
}

void mbox_destroy(mbox **mb){
  sem_destroy(&(*mb)->mbox_sem);
  free(*mb);
}

void enqueue(tcb *t, messageNode *msgNode){
  if (t->msgQueue == NULL){
    t->msgQueue  = msgNode;
  }
  // else move to end of queue and add there
  else {
    messageNode *tmp;
    tmp = t->msgQueue;
    while (tmp->next != NULL){
      tmp = tmp->next;
    }
    tmp->next = msgNode;
    msgNode->next = NULL;
  }
}

void send(int tid, char *msg, int len){
  tcb *tmp = readyQueue.head;
  while (tmp != NULL){
    if (tmp->thread_id == tid){
      // create messageNode
      messageNode *msgNode = malloc(sizeof(messageNode));
      msgNode->message = malloc(sizeof(char) * len);
      strcpy(msgNode->message, msg);
      msgNode->sender = runningQueue.head->thread_id;
      msgNode->receiver = tid;
      // enqueue in the specified thread's msgQueue
      enqueue(tmp, msgNode);
    }
    tmp = tmp->next;
  }
}

void receive(int *tid, char *msg, int *len){
  if (*tid == 0){
    len = &(runningQueue.head->msgQueue->len);
    strcpy(msg, runningQueue.head->msgQueue->message);
  }
  else {
    tcb *tmp = readyQueue.head;
    while (tmp != NULL){
      if (tmp->thread_id == *tid){
        len = &(tmp->msgQueue->len);
        strcpy(msg, tmp->msgQueue->message);
        tmp->msgQueue = tmp->msgQueue->next;
        return;
      }
      tmp = tmp->next;
    }
  }
}
