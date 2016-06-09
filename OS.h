/**
 * Final Project - Operating System
 * TCSS 422 A Spring 2016
 * Mark Peters and Luis Solis-Bruno, who stayed up all night, when they should have
 *                                  been studying for finals before and enjoying themselves
 *                                  afterwards, because the other group members
 *                                  waited until two fucking days before due date
 *                                  (the code we wrote was finished weeks ago)
 *                                  to turn in their buggy, unusable code that had
 *                                  to be completely thrown away and redone into
 *                                  the fleeting moments in dark.
 * 
 * Chris Ottersen:  At least he tried, unlike the other two. Submitted buggy code on Sunday, but forgot to upload half of it. Submitted the rest and kind of fixed it over the next couple days. "Guys, I promise I'll get it in time!" Spent most of his time formatting brackets.
 *
 * 
 * And these folks (at least they tried... kind of): 
 * Daniel Bayless:  Submitted about 20 lines about two hours after the final. Through email instead of the Github we use. We threw it away because it didn't compile even with half of it commented out.
 * Bun Kak       :  Didn't do shit. "Oh I'm graduating and I don't need to do any work, hehe."
 */

#ifndef OS_H
#define OS_H


#include <pthread.h>
#include "FIFOq.h"
#include "PCB.h"

//RENAMES
#define thread pthread_t
#define mutex pthread_mutex_t
#define cond pthread_cond_t

//OUTPUT SETTINGS
#define WRITE_TO_FILE false
#define DEFAULT_TRACE "scheduleTrace.txt"
#define DEBUG false
#define THREAD_DEBUG false
#define STACK_DEBUG false
#define CREATEPCB_DEBUG false
#define MUTEX_DEBUG false
#define EXIT_STATUS_MESSAGE true
#define CLEANUP_MESSAGE true
#define THREAD_EMPTY_MESSAGE true
#define ERROR_MESSAGE false
#define HELP_CHRIS_UNDERSTAND_WHAT_PC_VALUE_IS false
#define MAX_FIELD_WIDTH 204
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
#define SHUTDOWN 100000
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

//PRAGMAS
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

//note on style: ALLCAPS_lowercase is either a MUTEX_ variable or the data
//               protected by the MUTEX_ only for pthreads, thought, not our internal process threads

struct CPU { REG_p regs; };

struct shared_resource {
    int members;
    bool flag[MUTUAL_MAX_RESOURCES];
    word resource[MUTUAL_MAX_RESOURCES];
    FIFOq_p fmutex[MUTUAL_MAX_RESOURCES];
    FIFOq_p fcond[MUTUAL_MAX_RESOURCES];
};

struct io_thread_type {
    thread THREAD_io;
    mutex MUTEX_io;
    cond COND_io;
    bool INTERRUPT_iocomplete;
    bool SHUTOFF_io;
    FIFOq_p waitingQ;
};

typedef struct io_thread_type *io_thread;
typedef struct shared_resource *PCB_r;

int      bootOS                 ();
int      mainLoopOS             (int *error);

void*    timer                  (void*);
void*    io                     (void*);
PCB_r    mutexPair              (int* error);
void     mutexEmpty             (PCB_r, int* error);

void     trap_terminate         (int* error);
void     trap_iohandler         (const int T, int* error);
void     trap_mutexhandler      (const int T, int* error);
void     trap_requehandler      (const int T, int* error);

void     interrupt              (const int INTERRUPT, void*, int* error);
void     isr_timer              (int* error);
void     isr_iocomplete         (const int IO, int* error);

void     scheduler              (int* error);
void     awakeStarvationDaemon  (int* error); 
void     dispatcher             (int* error);
int      createPCBs             (int *error); 

void     sysStackPush           (REG_p, int* error);
void     sysStackPop            (REG_p, int* error);

void     cleanup                (int* error);
void     queueCleanup           (FIFOq_p, char*, int* error);
void     stackCleanup           ();

void     nanosleeptest          ();
void*    codeLikeBunDoes        ();

#endif