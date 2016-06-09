/*
 * Final Project - Operating System
 * TCSS 422 A Spring 2016
 * Mark Peters, Luis Solis-Bruno, Chris Ottersen
 */

#ifndef PCB_H
#define PCB_H

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>
//#include "fthread.h"

#define DEADLOCK true

#define created created_

#define max(x, y) (  ((x) > (y)) ? (x) : (y)  )
#define min(x, y) (  ((x) < (y)) ? (x) : (y)  )

#define REGNUM (REG_COUNT + IO_NUMBER*IO_CALLS + CALL_NUMBER*2)
#define PRIORITIES_TOTAL 4
#define LOWEST_PRIORITY (PRIORITIES_TOTAL - 1)
#define PRIORITY_0_CHANCE 5  //must be defined
#define PRIORITY_1_CHANCE 80
#define PRIORITY_2_CHANCE 10
#define PRIORITY_3_CHANCE 5
#define PRIORITY_OTHER_CHANCE 0
#define PRIORITY_UNIQUE_UPTO 3
#define IO_NUMBER 2
#define IO_CALLS 4
#define CALL_NUMBER 6
#define REG_COUNT 5

#define MAX_PC_MIN (MIN_THREAD_CALL + (THREAD_CALL * (CALL_NUMBER+1))) //1000
#define MAX_PC_RANGE (MAX_PC_MIN * 3)
#define MIN_IO_CALL 250
#define THREAD_CALL 100
#define MIN_THREAD_CALL (THREAD_CALL * 3) //must be product of THREAD_CALL
#define TERM_RANGE 10
#define TERM_INFINITE_CHANCE 0
#define MAX_ATTENTION 5

#define CPU_ONLY_MAX 25
#define IO_ONLY_MAX 50
#define PROCON_PAIR_MAX 10   //x2; pair count
#define MUTUAL_PAIR_MAX 10   //x2; pair count
#define MAX_SHARED_RESOURCES (PROCON_PAIR_MAX + MUTUAL_PAIR_MAX) * 2
#define MUTUAL_MAX_RESOURCES 2

#define DEFAULT_STATE created
#define DEFAULT_PC 0Lu
#define DEFAULT_SW 0Lu
#define DEFAULT_TIME_TERMINATE 0lu

#define PCB_NULL_ERROR 547
#define PCB_INIT_ERROR 557
#define PCB_OTHER_ERROR 563
#define PCB_PRIORITY_ERROR 569
#define PCB_UNDEFINED_ERROR 571
#define PCB_RESOURCE_ERROR 577

#define PCB_DEBUG false
#define PCB_TOSTRING_LEN 250

#define LAST_PAIR mutual_A
#define CODE_LOCK 10000
#define CODE_UNLOCK 20000
#define CODE_WAIT_T 30000
#define CODE_WAIT_F 40000
#define CODE_SIGNAL 50000
#define CODE_READ 60000
#define CODE_WRITE 70000
#define CODE_FLAG 80000

static char *STATE[] = {"created    ", "ready      ", "running    ", "waiting    ",
                        "interrupted", "blocked    ", "terminated ", "nostate    "};
static char *TYPE[] = {"regular  ", "producer ", "mutual_A ",
                       "consumer ", "mutual_B ", "undefined"};

static word CODE_TYPE[(LAST_PAIR*2)+1][CALL_NUMBER] = { {0,0,0,0,0,0},
    /* producer */ { CODE_LOCK+0, CODE_WAIT_F+0, CODE_WRITE+0, CODE_FLAG+0, CODE_SIGNAL+0, CODE_UNLOCK+0 },
    /* mutual_a */ { CODE_LOCK+0, CODE_LOCK+1, CODE_WRITE+0, CODE_WRITE+1, CODE_UNLOCK+1, CODE_UNLOCK+0 },
    /* consumer */ { CODE_LOCK+0, CODE_WAIT_T+0, CODE_READ+0, CODE_FLAG+0, CODE_SIGNAL+0, CODE_UNLOCK+0 },
    /* mutual_b */ { CODE_LOCK+1, CODE_LOCK+0, CODE_WRITE+0, CODE_WRITE+1, CODE_UNLOCK+0, CODE_UNLOCK+1 },
};       

static int PRIORITIES[] = {PRIORITY_0_CHANCE, PRIORITY_1_CHANCE,
                           PRIORITY_2_CHANCE, PRIORITY_3_CHANCE,
                           PRIORITY_OTHER_CHANCE};
static int MAX_TYPES[] = {CPU_ONLY_MAX + IO_ONLY_MAX, PROCON_PAIR_MAX,
                          MUTUAL_PAIR_MAX};
static int MAX_IOCPU[] = {CPU_ONLY_MAX, IO_ONLY_MAX};

enum state_type {
    created = 0,
    ready,
    running,
    waiting,
    interrupted,
    blocked,
    terminated,
    nostate
};

enum process_type
{
    regular = 0, producer, mutual_A, consumer, mutual_B, undefined
};


//typedef struct pcb PCB;
typedef unsigned long word;
typedef struct PCB *PCB_p;
typedef struct CPU *CPU_p;
typedef union regfile REG;
typedef REG *REG_p;

struct PCB
{
    REG_p regs;
    //separate from sysStack--don't push/pop
    word pid;        // process ID #, a unique number
    bool io;               // io or cpu intensive
    enum process_type type;   // thread relation to other processes
    unsigned short priority;  // priorities 0=highest, LOWEST_PRIORITY=lowest
    unsigned short orig_priority;
    // og priority
    enum state_type state;    // process state (running, waiting, etc.)
    word timeCreate;
    word timeTerminate;
    word lastClock;           // for starvation check
    word attentionCount;      //times it has been given upgraded attention
    word group;
    word queues;
    bool promoted;
//    THREADQp buddies;
    bool terminate;
};

union regfile
{
    //save to PCB from system
    struct
    {
        word pc, MAX_PC, sw, term_count, TERMINATE, IO_TRAPS[IO_NUMBER][IO_CALLS], CALLS[CALL_NUMBER], CODES[CALL_NUMBER];
    } reg;
    word gpu[REGNUM];
};

PCB_p 	PCB_construct       (int *ptr_error);
PCB_p 	PCB_construct_init  (int *ptr_error);
int 	PCB_destruct        (PCB_p this); 
REG_p   REG_init            (REG_p this, int *ptr_error);
int 	PCB_init            (PCB_p this);      
int     PCBs_available      ();
int 	PCB_setPid          (PCB_p this, word pid); 
word 	PCB_getPid          (PCB_p this, int *ptr_error);
int 	PCB_setState        (PCB_p this, enum state_type state); 
enum state_type PCB_getState(PCB_p this, int *ptr_error);
int 	PCB_setPriority     (PCB_p this, unsigned short priority); 
unsigned short 	PCB_getPriority (PCB_p this, int *ptr_error);
int 	PCB_setPc           (PCB_p this, word pc); 
word 	PCB_getPc           (PCB_p this, int *ptr_error);
int 	PCB_setSw           (PCB_p this, word sw); 
word 	PCB_getSw           (PCB_p this, int *ptr_error);
char* 	PCB_toString        (PCB_p this, char *str, int *ptr_error);
int 	PCB_compareTo       (PCB_p this, PCB_p other, int *ptr_error);
int 	PCB_test_main       (int argc, char** argv);
char*   Reg_File_toString   (REG_p this, char *str, int *ptr_error);

#endif