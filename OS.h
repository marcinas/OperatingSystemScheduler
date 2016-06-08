/*
 * Final Project - Operating System
 * TCSS 422 A Spring 2016
 * Mark Peters, Luis Solis-Bruno, Chris Ottersen
 */

#ifndef OS_H
#define OS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#include "FIFOq.h"
#include "PCB.h"

//RENAMES
#define thread pthread_t
#define mutex pthread_mutex_t
#define cond pthread_cond_t

//OUTPUT SETTINGS
#define DEBUG false
#define THREAD_DEBUG false
#define STACK_DEBUG false
#define CREATEPCB_DEBUG false
#define MUTEX_DEBUG false
#define EXIT_STATUS_MESSAGE true
#define OUTPUT true
#define OUTPUT_CONTEXT_SWITCH 1
#define FIRST_IO 1

//PCB SETTINGS
#define EXIT_ON_MAX_PROCESSES false
#define MAX_PROCESSES ULONG_MAX
#define MAX_NEW_PCB 5
#define START_IDLE false
#define PCB_CREATE_EVERY false
#define PCB_CREATE_FIRST false
#define PCB_SCHEDULE_EVERY false
#define PCB_CREATE_CHANCE (TIME_QUANTUM * 50)
#define RUN_MIN_TIME 3000
#define RUN_TIME_RANGE 1000
#define STARVATION_CHECK_FREQUENCY (TIME_QUANTUM / 15)
#define STARVATION_CLOCK_LIMIT (TIME_QUANTUM * 50)
#define PROMOTION_ALLOWANCE 1

//INTERRUPT CODES
#define NO_INTERRUPT 9999
#define INTERRUPT_TIMER 5555
#define INTERRUPT_TERMINATE 8888
#define INTERRUPT_IOCOMPLETE 4444

//SYSTEM DETAILS
#define SHUTDOWN 300000
#define TIME_QUANTUM 300
#define TIMER_SLEEP (TIME_QUANTUM * 1000)
#define IO_MAX_SLEEP (TIME_QUANTUM * 2000)
#define IO_MIN_SLEEP (TIME_QUANTUM * 10)
#define SYSSIZE 256
#define SYSTEM_RUNS 1

//ERRORS
#define OS_NO_ERROR 0//EXIT_SUCCESS
#define STACK_ERROR_DEFAULT 0
#define OS_MUTEX_ERROR 83
#define CPU_NULL_ERROR 71
#define CPU_STACK_ERROR 73
#define OS_UNKOWN_INTERRUPT_ERROR 79


//note on style: ALLCAPS_lowercase is either a MUTEX_ variable or the data
//               protected by the MUTEX_

struct CPU
{
    REG_p regs;
};

struct shared_resource
{
    int members;
    bool flag[MUTUAL_MAX_RESOURCES];
    word resource[MUTUAL_MAX_RESOURCES];
    FIFOq_p fmutex[MUTUAL_MAX_RESOURCES];
    FIFOq_p fcond[MUTUAL_MAX_RESOURCES];
};

struct io_thread_type
{
    thread THREAD_io;
    mutex MUTEX_io;
    cond COND_io;
    bool INTERRUPT_iocomplete;
    bool SHUTOFF_io;
    FIFOq_p waitingQ;
};

typedef struct io_thread_type *io_thread;
typedef struct shared_resource *PCB_r;

//extern word SsyStack[SYSSIZE];
//extern int SysPointer;

int      bootOS          ();
int      mainLoopOS      (int *error);
void*    timer           (void*);
void*    io              (void*);
void     trap_terminate  (int* error);
void     trap_iohandler  (const int T, int* error);
void     trap_mutexhandler(const int T, int* error);
void     trap_requehandler(const int T, int* error);
void     interrupt       (const int INTERRUPT, void*, int* error);
void     isr_timer       (int* error);
void     isr_iocomplete  (const int IO, int* error);
void     scheduler       (int* error);
void     dispatcher      (int* error);
int      createPCBs  	 (int *error); 
PCB_r    mutexPair       (int* error);
void     mutexEmpty      (PCB_r, int* error);
void      sysStackPush    (REG_p, int* error);
void      sysStackPop     (REG_p, int* error);
void     cleanup         (int* error);
void     queueCleanup    (FIFOq_p, char*, int* error);
void     stackCleanup    ();
void     nanosleeptest   ();
void awakeStarvationDaemon(int* error); 

//static void     run             (word *pc, int *error);

#endif