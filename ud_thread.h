#include "string.h"
typedef void sem_t;
typedef void mbox;
typedef void messageNode;

/*
 * thread library function prototypes
 */
void t_create(void(*function)(int), int thread_id, int priority);
void t_yield(void);
void t_init(void);
void t_shutdown(void);
void t_terminate(void);
int sem_init(sem_t **sp, unsigned int count);
void sem_wait(sem_t *sp);
void sem_signal(sem_t *sp);
void sem_destroy(sem_t **sp);
int mbox_create(mbox **mb);
void mbox_deposit(mbox *mb, char *msg, int len);
void mbox_withdraw(mbox *mb, char *msg, int *len);
void mbox_destroy(mbox **mb);
void send(int tid, char *msg, int len);
void receive(int *tid, char *msg, int *len);
