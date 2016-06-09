/*
 * Final Project - Operating System
 * TCSS 422 A Spring 2016
 * Mark Peters, Luis Solis-Bruno, Chris Ottersen
 */

#include "OS.h"

#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#define WRITE_TO_FILE false

/*global declarations for system stack*/
static word SysStack[SYSSIZE];
static int SysPointer;
static int closeable;
static int junk;

/*timer fields*/
static thread THREAD_timer;
static mutex MUTEX_timer;
static cond COND_timer;
static bool INTERRUPT_timer;
static bool SHUTOFF_timer;
static word clock_;

/*IO*/
static io_thread IO[IO_NUMBER];

/*Shared Resources for PCBs*/
static PCB_r empty;
static PCB_r group[MAX_SHARED_RESOURCES + 1];

/*OS only declarations for current and idle processes*/
static PCB_p current;
static PCB_p idl;
static FIFOq_p createQ;
static FIFOq_p readyQ[PRIORITIES_TOTAL];
static FIFOq_p terminateQ;

/* Launches the OS. Sets default values, initializes idle process and calls the
 * mainLoopOS to simulate running the cpu. Afterwards it cleans up reports
 * any errors encountered.
 */
int main(void)
{
    uint64_t s = clock();
    int run;
    word errors[SYSTEM_RUNS] = {0};

    if (SYSTEM_RUNS < 1)
        printf("NO SYSTEM RUN SET\n");

    for (run = 1; run <= SYSTEM_RUNS; run++) {

        if (EXIT_STATUS_MESSAGE)
            printf("\nSYSTEM START RUN %d of %d\n\n", run, SYSTEM_RUNS);
        if (DEBUG)
            printf("Main begin\n");

        if (WRITE_TO_FILE) {
            freopen("scheduleTrace.txt", "w", stdout);
        }

        int base_error = bootOS();
        if (DEBUG)
            printf("OS booted\n");

        int exit = mainLoopOS(&base_error);
        if (DEBUG)
            printf("OS shutdown\n");

        stackCleanup();

        if (base_error) {
            if (EXIT_STATUS_MESSAGE || OUTPUT)
                printf("\n>System exited with error %d\n", base_error);
        } else {
            if (EXIT_STATUS_MESSAGE)
                printf("\n>System exited without incident\n");
            if (OUTPUT) if (EXIT_ON_MAX_PROCESSES && exit == -2)
                printf(
                    "\n>%d processes have been created so system has exited\n",
                    MAX_PROCESSES);
            else if (SHUTDOWN && exit == -SHUTDOWN)
                printf("\n>%d cycles have run so system has exited\n",
                       SHUTDOWN);
            else
                printf(
                    "\n>Of %d processes created, all terminable ones have terminated so system has exited\n",
                    MAX_PROCESSES);
        }

        if (OUTPUT)
            printf(">Execution ended in %.3lf seconds.\n\n",
               (clock() - s) * 1.0 / CLOCKS_PER_SEC);
        errors[run - 1] = base_error;
        if (EXIT_STATUS_MESSAGE)
            printf("\nSYSTEM END RUN %d of %d\n\n", run, SYSTEM_RUNS);
        int d;
        if (EXIT_STATUS_MESSAGE)
            for (d = 0; d < 4; d++)
                printf("---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    }

    word mass_error = 0;
    for (run = 0; run < SYSTEM_RUNS; run++)
        mass_error += errors[run];
    return mass_error;

}

//initializes values and returns error
int bootOS()
{

    int boot_error = OS_NO_ERROR;
    int t;

    //system wide
    srand(time(NULL)); // seed random with current time
    SysPointer = 0; //points at next unassigned stack item; 0 is empty
    closeable = 0;

    //Timer
    clock_ = 0;
    INTERRUPT_timer = false;
    SHUTOFF_timer = false;
    pthread_mutex_init(&MUTEX_timer, NULL);
    pthread_cond_init(&COND_timer, NULL);
    pthread_create(&THREAD_timer, NULL, timer, NULL);

    //shared resources
    empty = (PCB_r) malloc(sizeof(struct shared_resource));
    empty->members = 0;
    empty = mutexPair(&boot_error);
    for (t = 0; t <= MAX_SHARED_RESOURCES; t++)
        group[t] = empty;

    //IO
    for (t = 0; t < IO_NUMBER; t++) {
        IO[t] = (io_thread) malloc(sizeof(struct io_thread_type));
        IO[t]->waitingQ = FIFOq_construct(&boot_error);
        IO[t]->INTERRUPT_iocomplete = false;
        IO[t]->SHUTOFF_io = false;
        pthread_mutex_init(&(IO[t]->MUTEX_io), NULL);
        pthread_cond_init(&(IO[t]->COND_io), NULL);
        pthread_create(&(IO[t]->THREAD_io), NULL, io, (void *) t);
    }

    //idl pcb has special parameters
    idl = PCB_construct(&boot_error);
    idl->pid = ULONG_MAX;
    idl->io = false;
    idl->type = undefined;
    idl->priority = LOWEST_PRIORITY;
    idl->state = waiting;
    idl->timeCreate = 0;
    idl->timeTerminate = ULONG_MAX;

    idl->regs->reg.pc = 0;
    idl->regs->reg.MAX_PC = 1000;
    idl->regs->reg.sw = ULONG_MAX;
    idl->regs->reg.term_count = 0;
    idl->regs->reg.TERMINATE = 0;
    for (t = 0; t < IO_NUMBER * IO_CALLS; t++)
        idl->regs->reg.IO_TRAPS[t / IO_CALLS][t % IO_CALLS] = -1;


    //queues
    createQ = FIFOq_construct(&boot_error);
    for (t = 0; t < PRIORITIES_TOTAL; t++)
        readyQ[t] = FIFOq_construct(&boot_error);
    terminateQ = FIFOq_construct(&boot_error);

    return boot_error;
}

/* Main loop for the operating system. Initializes queues and PC/SW values.
 * Runs until exit is triggered (in assignment 2, this is when 30 PCBs have been
 * created). Repeatedly creates new PCBs, simulates running, gets interrupted
 * by the timer, and restarting the loop.
 *
 * returns error
 */
int mainLoopOS(int *error)
{

    if (DEBUG)
        printf("Main loop initialization\n");

    int t;
    bool exit_okay = false;
    current = idl; //current's default state if no ready PCBs to run
    current->state = running;

    sysStackPush(current->regs, error);
    if (STACK_DEBUG)
        printf("current max_pc is %lu\n", current->regs->reg.MAX_PC);
    const CPU_p const CPU = (CPU_p) malloc(sizeof(struct CPU));
    CPU->regs = (REG_p) malloc(sizeof(union regfile));
    REG_init(CPU->regs, error);
    if (STACK_DEBUG)
        printf("pre max_pc is %lu\n", CPU->regs->reg.MAX_PC);
    sysStackPop(CPU->regs, error);
    if (STACK_DEBUG)
        printf("post max_pc is %lu\n", CPU->regs->reg.MAX_PC);

    //alias CPU's registers
    word *const pc = &(CPU->regs->reg.pc);
    word *const MAX_PC = &(CPU->regs->reg.MAX_PC);
    word *const sw = &(CPU->regs->reg.sw);
    word *const term_count = &(CPU->regs->reg.term_count);
    word *const TERMINATE = &(CPU->regs->reg.TERMINATE);
    word(*const IO_TRAPS)[IO_NUMBER][IO_CALLS] = &(CPU->regs->reg.IO_TRAPS);


    int exit = 0;

    if (*error) {
        printf("ERROR detected before launch! %d", *error);
        return *error;
    }

    if (EXIT_STATUS_MESSAGE)
        printf(">OS System Clock at Start %lu\n\n", clock_);
    if (DEBUG)
        printf("Main loop begin\n");


    /**************************************************************************/
    /*************************** MAIN LOOP OS *********************************/
    /**************************************************************************/
    do {
        clock_++;
        exit = createPCBs(error);
        if (!EXIT_ON_MAX_PROCESSES && exit && !exit_okay) {
            exit_okay = true;
            exit = 0;
        }
        if (current != idl && current->pid > MAX_PROCESSES) {
            if (DEBUG)
                printf(
                    "pre-cycle: pcb with pid %lu exceeds max processes, exiting system\n",
                    current->pid);
            break;
        }
        
        //printf("ouch %d and %d and %d and %lu and %lu\n", exit, current == NULL, error == NULL, *pc, *MAX_PC);

        if (DEBUG)
            printf("PCBs created exit = %d\n", exit);
        
        if (current == NULL || error == NULL) {
            
            if (error != NULL)
                *error += CPU_NULL_ERROR;
            exit = -1;
            printf("ERROR current process unassigned or error lost! %d", *error);

        } else { //no error, full cycle
            
            //printf("0x%05lx\t", *pc);
                     
            /*** TERMINATE CHECK ***/
            if (*pc == *MAX_PC) {
                *pc -= (*pc);
                (*term_count)++;
                if (DEBUG)
                    printf("term_count now %lu / %lu\n", *term_count,
                           *TERMINATE);
                if (*term_count == *TERMINATE) {
                    word pid = current->pid;
                    if (DEBUG)
                        printf("At cycle PC = %lu, terminate interrupts %lu\n",
                               *pc, current->pid);
                    sysStackPush(CPU->regs, error);
                    trap_terminate(error);
                    sysStackPop(CPU->regs, error);
                    if (DEBUG)
                        printf("Process %lu terminated\n", pid);
                    if (DEBUG)
                        printf("At cycle PC = %lu, process %lu begins\n", *pc,
                               current->pid);
                    continue;
                }
            }
            
            
            /*** THREAD CHECK ***/
            if (current->type > regular && current->type <= (LAST_PAIR * 2)) {
                bool waitforbuddy = false;
                bool reentry = false;
                bool showit = false;
                int resource;
                for (t = 0; t < CALL_NUMBER; t++)
                    if (*pc == current->regs->reg.CALLS[t]) {
                        showit = true;
                        word call = current->regs->reg.CODES[t] & -2; //truncates 1's place
                        resource = current->regs->reg.CODES[t] & 1; //1's place
                        char curstr[PCB_TOSTRING_LEN];
                        char pcbstr[PCB_TOSTRING_LEN];
                        printf("call: %lu   resource: %d   pc: %lu\n", call, resource, (*pc));
                        switch (call) {
                            case CODE_LOCK:
                                if (OUTPUT || MUTEX_DEBUG)
                                    printf(">Lock %02lu-%d request:  %s\n", current->group, resource,
                                            PCB_toString(current, curstr, error));
                                bool lock = FIFOq_is_empty(group[current->group]->fmutex[resource], error);
                                if (FIFOq_peek(group[current->group]->fmutex[resource], error) == current)
                                    lock = true;
                                else
                                    FIFOq_enqueuePCB(group[current->group]->fmutex[resource], current, error);
                                if (lock) {
                                    if (OUTPUT || MUTEX_DEBUG)
                                        printf(">Mutex attained:  %s\n",
                                                PCB_toString(current, pcbstr, error));
                                } else {
                                    waitforbuddy = true;
                                    if (OUTPUT || MUTEX_DEBUG) {
                                        PCB_p block = FIFOq_peek(group[current->group]->fmutex[resource], error);
                                        if (block == NULL) printf("fuck\n");
                                        printf(">Lock blocked by: %s\n",
                                            PCB_toString(block, pcbstr, error));
                                    }
                                }
                                break;
                            case CODE_UNLOCK:
                                reentry = true;
                                resource += 1;
                                resource *= -1;
                                break;
                            case CODE_WAIT_T:
                                if (group[current->group]->flag[resource]) { //flag is true
                                    FIFOq_enqueuePCB(group[current->group]->fcond[resource], current, error);
//                                    thread_cond_wait(current,
//                                                     group[current->group]->fcond[resource],
//                                                     group[current->group]->fmutex[resource]);
                                    reentry = true;
                                    waitforbuddy = true;
                                    resource += 1;
                                    resource *= -1;
                                    if (OUTPUT || MUTEX_DEBUG)
                                        printf(">Cond %02lu-%d wait:     %s\n", current->group, resource,
                                                PCB_toString(current, curstr, error));
                                }
                                break;
                            case CODE_WAIT_F:
                                if (!group[current->group]->flag[resource]) {//flag is false
                                    FIFOq_enqueuePCB(group[current->group]->fcond[resource], current, error);
//                                    thread_cond_wait(current,
//                                                     group[current->group]->fcond[resource],
//                                                     group[current->group]->fmutex[resource]);
                                    reentry = true;
                                    waitforbuddy = true;
                                    resource += 1;
                                    resource *= -1;
                                    if (OUTPUT || MUTEX_DEBUG)
                                        printf(">Cond %02lu-%d wait:     %s\n", current->group, resource,
                                                PCB_toString(current, curstr, error));
                                }
                                break;
                            case CODE_SIGNAL:
                                reentry = true;
                                break;
                            case CODE_READ:
                                (*sw) = group[current->group]->resource[resource];
                                if (OUTPUT || MUTEX_DEBUG)
                                    printf(">Shared %02lu-%d read:   %s\n", current->group, resource,
                                        PCB_toString(current, curstr, error));
                                break;
                            case CODE_WRITE:
                                (group[current->group]->resource[resource])++;
                                if (OUTPUT || MUTEX_DEBUG)
                                    printf(">Shared %02lu-%d write:  %s\n", current->group, resource,
                                        PCB_toString(current, curstr, error));
                                break;
                            case CODE_FLAG:
                                group[current->group]->flag[resource] ^= true;
                                if (OUTPUT || MUTEX_DEBUG)
                                    printf(">Flag %02lu-%d switched: %s\n", current->group, resource,
                                        PCB_toString(current, curstr, error));
                                break;
                            default:
                                showit = false;
                                break;
                        }
                        break;
                    }
                if (showit) {
                    int y = current->group;
                    int stz = FIFOQ_TOSTRING_MAX;
                    char str[PCB_TOSTRING_LEN];
                    char mut0[stz]; char mut1[stz]; char con0[stz]; char con1[stz];
                    printf("\n%s\nExists: %d %d %lu %lu \nMUTEX 0 %s \nMUTEX 1 %s \nCOND 0  %s \nCOND 1  %s \n\n",
                                       PCB_toString(current, str, error),
                                       group[y]->flag[0],     group[y]->flag[1],
                                       group[y]->resource[0], group[y]->resource[1],
                                       FIFOq_toString(group[y]->fmutex[0], mut0, &stz, error),
                                       FIFOq_toString(group[y]->fmutex[1], mut1, &stz, error),
                                       FIFOq_toString(group[y]->fcond[0], con0, &stz, error),
                                       FIFOq_toString(group[y]->fcond[1], con1, &stz, error)
                            );
                }
                if (waitforbuddy || reentry) {
                    if (reentry) {
                        sysStackPush(CPU->regs, error);
                        trap_requehandler(resource, error);
                        sysStackPop(CPU->regs, error);
                    }
                    if (waitforbuddy) {
                        (*pc)++;
                        sysStackPush(CPU->regs, error);
                        trap_mutexhandler(resource, error);
                        sysStackPop(CPU->regs, error);
                        continue;
                    }
                    
                }
            }


            /*** IO CHECK ***/
            if (DEBUG)
                printf("Checking IO if complete \n");
            for (t = 0; t < IO_NUMBER; t++)
                if (!pthread_mutex_trylock(&(IO[t]->MUTEX_io))) {
                    if (DEBUG)
                        printf("io %d Mutex locked\n", t + FIRST_IO);
                    if (IO[t]->INTERRUPT_iocomplete) {
                        sysStackPush(CPU->regs, error);
                        interrupt(INTERRUPT_IOCOMPLETE, (void *) t, error);
                        sysStackPop(CPU->regs, error);
                        IO[t]->INTERRUPT_iocomplete = false;
                    }
                    pthread_mutex_unlock(&(IO[t]->MUTEX_io));
                    pthread_cond_signal(&(IO[t]->COND_io));
                    if (DEBUG)
                        printf("io %d Mutex unlocked\n", t + FIRST_IO);
                }
            if (current != idl && current->pid > MAX_PROCESSES) {
                if (DEBUG)
                    printf(
                        "post-iocm: pcb with pid %lu exceeds max processes, exiting system\n",
                        current->pid);
                break;
            }

            
            /*** PCB TRAPS CHECK ***/
            if (current->io) {
                bool switchout = false;
                for (t = 0; t < IO_NUMBER * IO_CALLS; t++)
                    if (*pc == (*IO_TRAPS)[t / IO_CALLS][t % IO_CALLS]) {
                        t = t / IO_CALLS;
                        if (DEBUG)
                            printf(
                                "Process %lu at PC %lu place in wQ of IO %d\n",
                                current->pid, *pc, t + FIRST_IO);
                        pthread_mutex_lock(&(IO[t]->MUTEX_io));
                        (*pc)++;
                        sysStackPush(CPU->regs, error);
                        trap_iohandler(t, error);
                        sysStackPop(CPU->regs, error);
                        switchout = true;
                        pthread_mutex_unlock(&(IO[t]->MUTEX_io));
                        pthread_cond_signal(&(IO[t]->COND_io));
                        if (DEBUG)
                            printf("At cycle PC = %lu, process %lu begins\n",
                                   *pc, current->pid);
                        break;
                    }
                if (switchout)
                    continue;
            }
            
            
            /*** INCREMENT PC ***/
            (*pc)++;
            if (DEBUG)
                printf("PCB %lu PC incremented to %lu of %lu\n", current->pid,
                       *pc, *MAX_PC);
            
            /*** TIMER CHECK ***/
            if (!pthread_mutex_trylock(&MUTEX_timer)) {
                if (DEBUG)
                    printf("Timer check \n");
                bool context_switch = false;
                if (INTERRUPT_timer) {
                    INTERRUPT_timer = false;
                    context_switch = true;
                }
                pthread_mutex_unlock(&MUTEX_timer);
                if (context_switch || PCB_SCHEDULE_EVERY) {
                    if (DEBUG)
                        printf("At cycle PC = %lu, timer interrupts %lu\n", *pc,
                               current->pid);
                    //(*pc)--;
                    sysStackPush(CPU->regs, error);
                    interrupt(INTERRUPT_TIMER, NULL, error);
                    sysStackPop(CPU->regs, error);
                    if (DEBUG)
                        printf("At cycle PC = %lu, process %lu begins\n", *pc,
                               current->pid);
                    continue;
                }
                if (DEBUG)
                    printf("No cycle PC \n");
            }
            if (current != idl && current->pid > MAX_PROCESSES) {
                if (DEBUG)
                    printf(
                        "post-time: pcb with pid %lu exceeds max processes, exiting system\n",
                        current->pid);
                break;
            }
            
            /*** CLOCK CYCLE END CHECK ***/
            if (current != idl && current->pid > MAX_PROCESSES) {
                if (EXIT_STATUS_MESSAGE)
                    printf(
                        "post-trap: pcb with pid %lu exceeds max processes, exiting system\n",
                        current->pid);
                break;
            }

            if (DEBUG)
                printf("PCB %lu PC cycle %lu finished\n", current->pid, *pc);

                

                
        }

        if (!EXIT_ON_MAX_PROCESSES) if (exit_okay && closeable == 0)
            exit = -1;
        else
            exit = 0;

        if (SHUTDOWN && clock_ >= SHUTDOWN)
            exit = -SHUTDOWN;

    } while (!*error && !exit);
    /**************************************************************************/
    /*************************** *********** **********************************/
    /**************************************************************************/

    if (DEBUG)
        printf("Main loop OS stop\n");

    sysStackPush(CPU->regs, error);
    sysStackPop(current->regs, error);

    if (EXIT_STATUS_MESSAGE)
        printf("\n>OS System Clock at Exit: %lu\n\n\n", clock_);

//    for(t = 0; t < TIMER_SLEEP; t++);

    cleanup(error);

    return exit;
}




/******************************************************************************/
/******************************* THREADS **************************************/
/******************************************************************************/

void *timer(void *unused)
{

    if (THREAD_DEBUG)
        printf("\tTIMER: begin THREAD\n");

    int c;
    word next = 0;
    bool shutoff;
    word clock = 0;

    do {

        do {
            for (c = 0; c < TIMER_SLEEP; c++); //sleeping simulation
            pthread_mutex_lock(&MUTEX_timer);
            clock = clock_;
            shutoff = SHUTOFF_timer;
            pthread_mutex_unlock(&MUTEX_timer);
            pthread_cond_signal(&COND_timer);
            if (THREAD_DEBUG)
                printf("\tTIMER: clock is %lu out of %lu, shutoff is %s\n",
                       clock, next, shutoff ? "true" : "false");
        } while (clock < next && !shutoff);
        pthread_mutex_lock(&MUTEX_timer);
        INTERRUPT_timer = true;
        next = clock_ + TIME_QUANTUM;
        if (THREAD_DEBUG)
            printf("\tTIMER: begin clock at %lu\n", clock);
        pthread_mutex_unlock(&MUTEX_timer);
    } while (!shutoff);

    if (THREAD_DEBUG)
        printf("\tTIMER: end clock at %lu\n", clock);
    pthread_mutex_unlock(&MUTEX_timer);
    pthread_cond_signal(&COND_timer);
    pthread_exit(NULL);
    return NULL;
}

void *io(void *tid)
{

    int t = (int) tid;
    //tid is int for thread number

    if (THREAD_DEBUG)
        printf("\t\tIO %d: begin THREAD\n", t + FIRST_IO);

    int io_error = OS_NO_ERROR;
    int c;
    bool empty;
    bool shutoff;


    do {
        pthread_mutex_lock(&(IO[t]->MUTEX_io));
        shutoff = IO[t]->SHUTOFF_io;
        empty = FIFOq_is_empty(IO[t]->waitingQ, &io_error);
        pthread_mutex_unlock(&(IO[t]->MUTEX_io));
        while (!empty && !shutoff) {
            if (THREAD_DEBUG)
                printf("\t\tIO %d: queue has %d PCBs left; beginning IO ops\n",
                       t + FIRST_IO, IO[t]->waitingQ->size);
            word sleep = rand() % (IO_MAX_SLEEP - IO_MIN_SLEEP) + IO_MIN_SLEEP;
            for (c = 0; c < sleep; c++); //sleeping simulation

            pthread_mutex_lock(&(IO[t]->MUTEX_io));
            shutoff = IO[t]->SHUTOFF_io;

            if (!shutoff) {
                IO[t]->INTERRUPT_iocomplete = true;
                pthread_mutex_unlock(&(IO[t]->MUTEX_io));
                pthread_cond_wait(&(IO[t]->COND_io), &(IO[t]->MUTEX_io));
                empty = FIFOq_is_empty(IO[t]->waitingQ, &io_error);
            }

            if (THREAD_DEBUG)
                printf(
                    "\t\tIO %d: IO ops finished; queue has %d PCBs left, shutoff is %s\n",
                    t + FIRST_IO, IO[t]->waitingQ->size,
                    shutoff ? "true" : "false");
            pthread_mutex_unlock(&(IO[t]->MUTEX_io));

        }
        pthread_mutex_lock(&(IO[t]->MUTEX_io));
        empty = FIFOq_is_empty(IO[t]->waitingQ, &io_error);
        if (empty && !shutoff)
            pthread_cond_wait(&(IO[t]->COND_io), &(IO[t]->MUTEX_io));
        pthread_mutex_unlock(&(IO[t]->MUTEX_io));
    } while (!shutoff);

    if (THREAD_DEBUG)
        printf("\t\tIO %d: shutting off\n", t + FIRST_IO);
    pthread_exit(NULL);
    return NULL;
}




/******************************************************************************/
/******************************** TRAPS ***************************************/
/******************************************************************************/

void trap_terminate(int *error)
{
    sysStackPop(current->regs, error);
    PCB_setState(current,
                 terminated); //this is the ONLY PLACE a pcb should ever be terminated
    current->timeTerminate = clock_;
    current->priority = current->orig_priority;
    closeable--;
    if (current->group) {
        group[current->group]->members--;
        if (group[current->group]->members == 0) {
            PCB_r pair = group[current->group];
            group[current->group] = empty;
            junk = current->group;
            mutexEmpty(pair, error);
        }
    }
    char pcbstr[PCB_TOSTRING_LEN];
    if (OUTPUT)
        printf(">Terminated:      %s\n", PCB_toString(current, pcbstr, error));

    FIFOq_enqueuePCB(terminateQ, current, error);
    //current = idl;
    scheduler(error);
}

void trap_iohandler(const int t, int *error)
{
    sysStackPop(current->regs, error);
    current->state = waiting;
    char pcbstr[PCB_TOSTRING_LEN];
    if (OUTPUT)
        printf(">I/O %d added:     %s\n", t + FIRST_IO,
               PCB_toString(current, pcbstr, error));

    FIFOq_enqueuePCB(IO[t]->waitingQ, current, error);
    if (THREAD_DEBUG)
        printf("\t\tIO %d: gained PCB\n", t + FIRST_IO);
    scheduler(error);
}

void trap_mutexhandler(const int T, int *error)
{
    int t = T;
    if (t<0) t = -t - 1;
    sysStackPop(current->regs, error);
    current->state = blocked;
    char pcbstr[PCB_TOSTRING_LEN];
    if (OUTPUT || MUTEX_DEBUG)
        printf(">Group %d enqueue: %s\n", t,
               PCB_toString(current, pcbstr, error));

    //current already enqueued in mutex/cond
    if (MUTEX_DEBUG)
        printf("\t\tGroup %d: gained PCB\n", t);
    scheduler(error);
}

void trap_requehandler(const int T, int *error)
{   
    int t = T;
    bool newlock = false;
    char pcbstr[PCB_TOSTRING_LEN];
    PCB_p pcb = NULL;
    printf("t: %d\n", t);
    if (t < 0) {
        t = -t - 1;
        
        //current = thread_mutex_unlock(current, group[current->group]->fmutex[t]);
        if (FIFOq_peek(group[current->group]->fmutex[t], error) == current)
            FIFOq_dequeue(group[current->group]->fmutex[t], error);
        else *error += OS_MUTEX_ERROR;
        if (!FIFOq_is_empty(group[current->group]->fcond[t], error) &&
            FIFOq_peek(group[current->group]->fcond[t], error) != NULL)
            pcb = FIFOq_peek(group[current->group]->fmutex[t], error); 
        if ((OUTPUT || MUTEX_DEBUG))
            printf(">Unlocked lock %02lu-%d:   %s\n", current->group, t,
               PCB_toString(current, pcbstr, error));
        if (pcb != NULL)
            newlock = true;


    } else {
        //pcb = thread_cond_signal(group[current->group]->fcond[t]);
        if (!FIFOq_is_empty(group[current->group]->fcond[t], error) &&
            FIFOq_peek(group[current->group]->fcond[t], error) != NULL)
            pcb = FIFOq_dequeue(group[current->group]->fcond[t], error);
            
        else
            printf("bad signal\n");
        if (pcb != NULL) {
            FIFOq_enqueuePCB(group[pcb->group]->fmutex[pcb->queues], pcb, error);
            if (FIFOq_peek(group[pcb->group]->fmutex[pcb->queues], error) == pcb)
                newlock = true;
//            pcb->queues = -1;
        }
        if (OUTPUT || MUTEX_DEBUG)
            printf(">Signal to cond %02lu-%d:  %s\n", current->group, t,
               PCB_toString(current, pcbstr, error));
    }
    
    if (newlock == true) {
        pcb->state = ready;
        pcb->lastClock = clock_; //to track starvation
        if (pcb->promoted)
            pcb->attentionCount++;
        FIFOq_enqueuePCB(readyQ[pcb->priority], pcb, error);
        char unlstr[PCB_TOSTRING_LEN];
        if ((OUTPUT || MUTEX_DEBUG))
            printf(">Group %d dequeue: %s\n", t,
               PCB_toString(pcb, unlstr, error));
    }


}




/******************************************************************************/
/*********************** INTERRUPT SERVICE ROUTINES ***************************/
/******************************************************************************/

void interrupt(const int INTERRUPT, void *args, int *error)
{
    switch (INTERRUPT) {

        case NO_INTERRUPT:
            current->state = running;
            break;

        case INTERRUPT_TIMER:
            isr_timer(error);
            break;

        case INTERRUPT_IOCOMPLETE:
            isr_iocomplete(((int) args), error);
            break;
            
        default:
            *error += OS_UNKOWN_INTERRUPT_ERROR;
            break;
    }
}

/* Interrupt service routine for the timer: interrupts the current PCB and saves
 * the CPU state to it before calling the scheduler.
 */
void isr_timer(int *error)
{

    //change the state from running to interrupted
    PCB_setState(current, interrupted);

    //assigns Current PCB PC and SW values to popped values of SystemStack

    if (DEBUG)
        printf("\t\tStack going to pop isrtimer: %d\n", SysPointer);
    sysStackPop(current->regs, error);

    //call Scheduler and pass timer interrupt parameter
    scheduler(error);
}

void isr_iocomplete(const int t, int *error)
{

    if (!FIFOq_is_empty(IO[t]->waitingQ, error)) {
        PCB_p pcb = FIFOq_dequeue(IO[t]->waitingQ, error);
        pcb->state = ready;
        pcb->lastClock = clock_; //to track starvation
        if (pcb->promoted) {
            pcb->attentionCount++;
        }
        FIFOq_enqueuePCB(readyQ[pcb->priority], pcb, error);
        char pcbstr[PCB_TOSTRING_LEN];
        if (OUTPUT)
            printf(">I/O %d complete:  %s\n", t + FIRST_IO,
                   PCB_toString(pcb, pcbstr, error));

    } else if (THREAD_DEBUG)
        printf("ERROR! nothing to dequeue in IO %d\n", t);

}




/******************************************************************************/
/************************** SCHEDULERS/LOADERS ********************************/
/******************************************************************************/

/* Always schedules any newly created PCBs into the ready queue, then checks
 * the interrupt type: if the interrupt is a timer interrupt, requeues the
 * currently running process in the ready queue and sets its state to ready.
 * Then calls the dispatcher.
 */
void scheduler(int *error)
{
    static int schedules = 0;

    PCB_p temp;
    PCB_p pcb = current;
    bool pcb_idl = current == idl;
    bool pcb_term = current->state == terminated;
    bool pcb_io = current->state == waiting;
    bool pcb_mtx = current->state == blocked;

    if (createQ == NULL) {
        *error += FIFO_NULL_ERROR;
        puts("ERROR: createQ is null");
        return;
    }

    if (readyQ == NULL) {
        *error += FIFO_NULL_ERROR;
        puts("ERROR: readyQ is null");
        return;
    }

    //enqueue any created processes
    while (!FIFOq_is_empty(createQ, error)) {
        temp = FIFOq_dequeue(createQ, error);
        temp->state = ready;
        temp->lastClock = clock_;
        FIFOq_enqueuePCB(readyQ[temp->priority], temp, error);
        if (OUTPUT) {
            char pcbstr[PCB_TOSTRING_LEN];
            printf(">Enqueued readyQ: %s\n", PCB_toString(temp, pcbstr, error));
        }
    }

    if (DEBUG)
        printf("createQ transferred to readyQ\n");

    int r;
    for (r = 0; r < PRIORITIES_TOTAL; r++)
        if (!FIFOq_is_empty(readyQ[r], error))
            break;

    if (r == PRIORITIES_TOTAL) {// || current->pid == readyQ[r]->head->data->pid) { //nothing in any ready queues //changed
        if (pcb_term || pcb_io || pcb_mtx)
            current = idl;
        PCB_setState(current, running);
        sysStackPush(current->regs, error);
        return;
    }

    schedules++;
    if (!(schedules % STARVATION_CHECK_FREQUENCY)) {
        awakeStarvationDaemon(error);
    }
    if (!(schedules % (STARVATION_CHECK_FREQUENCY + 10))) {
        int i = 0;
        for(i = 0; i < MAX_SHARED_RESOURCES + 1; i++){
            if (group[i]->fmutex[0]->head != NULL
                && group[i]->fmutex[1]->head !=NULL
                && (group[i]->fmutex[0]->head->data->type == mutual_A || group[i]->fmutex[0]->head->data->type == mutual_B)
                && (group[i]->fmutex[1]->head->data->type == mutual_A || group[i]->fmutex[1]->head->data->type == mutual_B)
                && (group[i]->fmutex[0]->head->data !=  group[i]->fmutex[1]->head->data)
               ){
                puts("deadlock detected");
                printf("%x and %x are deadlocked\n",group[i]->fmutex[0]->head->data->pid, group[i]->fmutex[1]->head->data->pid);
            }
        }
    }


    for (r = 0; r < PRIORITIES_TOTAL; r++)
        if (!FIFOq_is_empty(readyQ[r], error))
            break;

    if (OUTPUT) {
        char pcbstr[PCB_TOSTRING_LEN];
        printf(">PCB:             %s\n", PCB_toString(current, pcbstr, error));
        char rdqstr[PCB_TOSTRING_LEN];
        printf(">Switching to:    %s\n",
               PCB_toString(readyQ[r]->head->data, rdqstr, error));
    }
    //if it's a timer interrupt
    if (!pcb_idl && !pcb_term && !pcb_io && !pcb_mtx) {
        current->state = ready;
        current->lastClock = clock_;
        if (current->promoted) {
            current->attentionCount++;
        }
        FIFOq_enqueuePCB(readyQ[current->priority], current, error);
    } else
        idl->state = waiting;
    dispatcher(error);

    if (OUTPUT) {
        char runstr[PCB_TOSTRING_LEN];
        printf(">Now running:     %s\n", PCB_toString(current, runstr, error));
        char rdqstr[PCB_TOSTRING_LEN];
        if (!pcb_idl && !pcb_term && !pcb_io)
            if (readyQ[r]->size > 1)
                printf(">Requeued readyQ: %s\n",
                       PCB_toString(readyQ[r]->tail->data, rdqstr, error));
            else
                printf(">No process return required.\n");
        else if (pcb_idl)
            printf(">Idle process:    %s\n", PCB_toString(idl, rdqstr, error));
        else if (pcb_term)
            printf(">Exited system:   %s\n",
                   PCB_toString(terminateQ->tail->data, rdqstr, error));
        else if (pcb_io)
            printf(">Requested I/O:   %s\n", PCB_toString(pcb, rdqstr, error));
        else if (pcb_mtx)
            printf(">Mutex lock/wait: %s\n", PCB_toString(pcb, rdqstr, error));
        int stz = FIFOQ_TOSTRING_MAX;
        char str[stz];
        if (OUTPUT)
            printf(">Priority %d %s\n", r,
                   FIFOq_toString(readyQ[r], str, &stz, error));

    }


}

/**
 * Checks if any pcb's in the waiting queue need to be promoted due to starvation or
 * demoted due to being promoted and recieving enough attention.
 * @param error - error collects all the error signals.
 */
void awakeStarvationDaemon(int *error)
{

    if (DEBUG)
        puts(
            "~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?");
    if (DEBUG)
        puts("Starvation Daemon");
    int rank;
    for (rank = 0; rank < PRIORITIES_TOTAL; rank++) {
        Node_p curr = readyQ[rank]->head;
        Node_p prev = curr;
        Node_p next;
        while (curr != NULL) {
            if (DEBUG)
                printf(">PID of inspect: %lu while Rank: %d PCB rank: "
                           "%hu, attentionRecieved: %lu\n",
                       curr->data->pid, rank, curr->data->priority,
                       curr->data->attentionCount);
            if (DEBUG)
                printf("Wait time: %lu\n", (clock_ - curr->data->lastClock));
            //if current received enough attention then remove, demote and set current to next
            if (curr->data->attentionCount >= PROMOTION_ALLOWANCE) {
                if (DEBUG)
                    puts("Demoting a process");
                if (DEBUG)
                    printf(">Demoted PID: %lu while Rank: %d PCB rank:"
                               " %hu attentionRecieved: %lu\n",
                           curr->data->pid, rank, curr->data->priority,
                           curr->data->attentionCount);
                next = FIFOq_remove_and_return_next(curr, prev, readyQ[rank]);
                curr->data->attentionCount = 0;
                curr->data->promoted = false;
                curr->data->priority = curr->data->orig_priority;
                FIFOq_enqueue(readyQ[curr->data->priority], curr, error);
                char pcbstr[PCB_TOSTRING_LEN];
                if (OUTPUT)
                    printf(">Demoted:         %s\n",
                           PCB_toString(curr->data, pcbstr, error));
                curr = next;
                if (curr == readyQ[rank]->head) {
                    prev = curr;
                }
            } else if (rank > 0 && (clock_ - curr->data->lastClock) >
                                   STARVATION_CLOCK_LIMIT) {
                //remove current, promote current process, set current to next
                if (DEBUG)
                    puts("Promoting a process");
                if (DEBUG)
                    printf(">Promoted PID: %lu while Rank: %d PCB rank: "
                               "%hu attentionRecieved: %lu\n",
                           curr->data->pid, rank, curr->data->priority,
                           curr->data->attentionCount);

                next = FIFOq_remove_and_return_next(curr, prev, readyQ[rank]);
                if (!curr->data->promoted) {
                    curr->data->attentionCount = 0;
                } else {
                    if (DEBUG)
                        printf("Node PID: %lu\n promoted once again.\n",
                               curr->data->pid);
                }
                char pcbstr[PCB_TOSTRING_LEN];
                if (OUTPUT)
                    printf(">Promoted:        %s\n",
                           PCB_toString(curr->data, pcbstr, error));
                curr->data->promoted = true;
                curr->data->priority = rank - 1;
                FIFOq_enqueue(readyQ[curr->data->priority], curr, error);
                curr = next;
                if (curr == readyQ[rank]->head) {
                    prev = curr;
                }
            } else {
                prev = curr;
                curr = curr->next_node;
            }
        }
    }
    if (DEBUG)
        puts(
            "~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?~?");


}

/* Dispatches a new current process by dequeuing the head of the ready queue,
 * setting its state to running and copying its CPU state onto the stack.
 */
void dispatcher(int *error)
{

    int r;

    if (readyQ == NULL) {
        *error += FIFO_NULL_ERROR;
        printf("%s", "ERROR: readyQ is null");
    } else
        for (r = 0; r < PRIORITIES_TOTAL; r++)
            if (!FIFOq_is_empty(readyQ[r],
                                error)) { //dequeue the head of readyQueue
                current = FIFOq_dequeue(readyQ[r], error);
                break;
            }

    //change current's state to running point
    current->state = running;

    if (DEBUG) {
        char pcbstr[PCB_TOSTRING_LEN];
        printf("CURRENT: \t%s\n", PCB_toString(current, pcbstr, error));
    }

    //copy currents's PC value to SystemStack
    if (DEBUG)
        printf("\t\tStack going to push dispatch: %d\n", SysPointer);
    sysStackPush(current->regs, error);

}

/* Creates 0 to 5 PCBs and enqueues them into a special queue, create queue.
 * Keeps track of how many PCBs have been created and sends an exit signal
 * when 30 have been created.
 */
int createPCBs(int *error)
{
    static bool first_batch = !PCB_CREATE_FIRST;
    if (first_batch && (!PCB_CREATE_EVERY) && (rand() % PCB_CREATE_CHANCE))
        return OS_NO_ERROR;
    // random number of new processes between 0 and 5
    static int processes_created = 0;
    static int g = 0;

    if (processes_created >= MAX_PROCESSES)
        return -1;

    int i;
    int r = rand() % (MAX_NEW_PCB + 1);
    char buffer[PCB_TOSTRING_LEN];

    if (r + processes_created >= MAX_PROCESSES) {
        r = MAX_PROCESSES - processes_created;
    }

    if (START_IDLE && processes_created == 0) {
        r = 0;
        processes_created += 1;
    }

    if (createQ == NULL) {
        *error += CPU_NULL_ERROR;
        printf("ERROR: FIFOq_p passed to createPCBs is NULL\n");
        return *error;
    }

    if (CREATEPCB_DEBUG)
        printf(
            "createPCBs: creating up to %d PCBs and enqueueing them to createQ\n",
            r);
    for (i = 0; i < r; i++) {


        if (!PCBs_available()) {
            r = i;
            if (CREATEPCB_DEBUG)
                printf("all pcbs used up; %d pcbs created\n", r);
            break;
        }
        // PCB_construct initializes state to 0 (created)
        PCB_p newPcb = PCB_construct_init(error);

        if (newPcb->type == undefined)
            *error += PCB_UNDEFINED_ERROR;

        newPcb->timeCreate = clock_;
        newPcb->lastClock = clock_;
        closeable += newPcb->regs->reg.TERMINATE > 0;
        char *TYPE[] = {"regular", "producer", "mutual_A", "consumer",
                        "mutual_B", "undefined"};

        if (!g && newPcb->type > regular && newPcb->type <= LAST_PAIR) {
            for (g = 1; g <= MAX_SHARED_RESOURCES; g++)
                if (group[g] == empty)
                    break;
            if (MUTEX_DEBUG)
                printf("%d\n", g);
            if (g == MAX_SHARED_RESOURCES+1) {
                printf("ERROR: max_resource exceeded: %s\n",
                       TYPE[newPcb->type]);
                *error += PCB_RESOURCE_ERROR;
            } else {
                group[g] = mutexPair(error);
                group[g]->members = 1;
                newPcb->group = g;
            }
        } else if (g) {
            if (newPcb->type <= LAST_PAIR || newPcb->type == undefined) {
                printf("ERROR: second pair not of pair type: %s + %d\n",
                       TYPE[newPcb->type], g);
                *error += PCB_RESOURCE_ERROR;

            } else {
                newPcb->group = g;
                group[g]->members++;
                g = 0;
                if (group[g]->members == MUTUAL_MAX_RESOURCES)
                    g = 0;
            }

        }

        if (CREATEPCB_DEBUG)
            printf("New PCB created: %s\n",
                   PCB_toString(newPcb, buffer, error));
        FIFOq_enqueuePCB(createQ, newPcb, error);
        processes_created++;
    }
    if (CREATEPCB_DEBUG)
        printf("PCBs all created\n");
    if (!first_batch)
        first_batch = r;
    if (CREATEPCB_DEBUG)
        printf("total created pcbs: %d and exit = %d\n", processes_created,
               (processes_created >= MAX_PROCESSES ? -1 : 0));
    return (processes_created >= MAX_PROCESSES ? -2 : 0);
}

PCB_r mutexPair(int *error)
{
    if (group == NULL)
        *error += PCB_RESOURCE_ERROR;
    PCB_r pair = (PCB_r) malloc(sizeof(struct shared_resource));
    //sprintf(pair->fcond, "created");
    pair->members = 0;
    int r; //instantiate group members such as mutex, conds, in loop
    for (r = 0; r < MUTUAL_MAX_RESOURCES; r++) { 
        //todo:add error
        pair->flag[r] = true;
        pair->resource[r] = 0;
        pair->fmutex[r] = FIFOq_construct(error);//mutex_lock_create(NULL);
        pair->fcond[r] = FIFOq_construct(error);//cond_var_create(NULL);
    }
    return pair;
}

void mutexEmpty(PCB_r pair, int *error)
{

    if (pair != empty && pair != NULL) {
        int r; //deallocate pair members such as mutex, conds, in loop
        for (r = 0; r < MUTUAL_MAX_RESOURCES; r++) {
            char mQ[24] = "group_yy mutexQ_x";
            mQ[6] = (junk/10) + '0';
            mQ[7] = (junk%10) + '0';
            mQ[16] = r + '0';
            if (!FIFOq_is_empty(pair->fmutex[r], NULL)) {
                queueCleanup(pair->fmutex[r], mQ, error);
                              *error += PCB_RESOURCE_ERROR;
            } else
                FIFOq_destruct(pair->fmutex[r], error);
//            mutex_lock_terminate(pair->fmutex[r], NULL);
            char cQ[24] = "group_yy condQ_x";
            cQ[6] = (junk/10) + '0';
            cQ[7] = (junk%10) + '0';
            cQ[15] = r + '0';
            if (!FIFOq_is_empty(pair->fcond[r], NULL)) {
                queueCleanup(pair->fcond[r], cQ, error);
                *error += PCB_RESOURCE_ERROR;
            } else
                FIFOq_destruct(pair->fcond[r], error);
//            cond_var_terminate(pair->fcond[r], NULL);
        }
        free(pair);
        pair = NULL;
    }
}




/******************************************************************************/
/************************** SYSTEM SUBROUTINES ********************************/
/******************************************************************************/

void sysStackPush(REG_p fromRegs, int *error)
{
    int r;

    for (r = 0; r < REGNUM; r++) {
        if (SysPointer >= SYSSIZE - 1) {
            *error += CPU_STACK_ERROR;
            printf("ERROR: Sysstack exceeded\n");
        } else {
            SysStack[SysPointer++] = fromRegs->gpu[r];
        }
    }
    int i;
    if (STACK_DEBUG)
        for (i = 0; i <= SysPointer; i++)
            printf("\tPostPush SysStack[%d] = %lu\n", i, SysStack[i]);
}

void sysStackPop(REG_p toRegs, int *error)
{
    int r;
    if (STACK_DEBUG)
        printf("max_pc is %lu\n", toRegs->reg.MAX_PC);
    for (r = REGNUM - 1; r >= 0; r--)
        if (SysPointer <= 0) {
            *error += CPU_STACK_ERROR;
            printf("ERROR: Sysstack exceeded\n");
            toRegs->gpu[r] = STACK_ERROR_DEFAULT;
        } else {
            if (STACK_DEBUG)
                printf("\tpopping SysStack[%d] = %lu ", SysPointer - 1,
                       SysStack[SysPointer - 1]);
            toRegs->gpu[r] = SysStack[--SysPointer];
            if (STACK_DEBUG)
                printf("at location %d: %lu\n", r, toRegs->gpu[r]);
        }
    if (STACK_DEBUG)
        printf("max_pc is %lu\n", toRegs->reg.MAX_PC);
    int i;
    for (i = 0; i <= SysPointer; i++)
        if (STACK_DEBUG)
            printf("\tPost-Pop SysStack[%d] = %lu\n", i, SysStack[i]);
}




/******************************************************************************/
/**************************** DEALLOCATION ************************************/
/******************************************************************************/

void cleanup(int *error)
{

    if (EXIT_STATUS_MESSAGE)
        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n\n");

    int t;
    int s;
    if (DEBUG)
        printf("cleanup begin\n");

    //while (!pthread_mutex_trylock(&MUTEX_timer)); //wait for timer
    pthread_cond_wait(&COND_timer, &MUTEX_timer);
    SHUTOFF_timer = true;
    pthread_mutex_unlock(&MUTEX_timer);
    pthread_cond_signal(&COND_timer);
    if (THREAD_DEBUG)
        printf("timer shutoff signal sent\n");
    pthread_join(THREAD_timer, NULL);

    if (THREAD_DEBUG)
        printf("timer shutoff successful\n");
    pthread_mutex_destroy(&MUTEX_timer);
    pthread_cond_destroy(&COND_timer);

    queueCleanup(terminateQ, "terminateQ", error);


    for (t = 0; t < IO_NUMBER; t++) {
        char wQ[12] = "waitingQ_x";
        wQ[9] = t + '0';
        pthread_cond_signal(&(IO[t]->COND_io));

        pthread_mutex_lock(&(IO[t]->MUTEX_io));

        IO[t]->SHUTOFF_io = true;
        pthread_mutex_unlock(&(IO[t]->MUTEX_io));
        pthread_cond_signal(&(IO[t]->COND_io));
        if (THREAD_DEBUG)
            printf("io %d shutoff signal sent\n", t + FIRST_IO);
        pthread_join((IO[t]->THREAD_io), NULL);
        //...
        if (THREAD_DEBUG)
            printf("io %d shutoff successful\n", t + FIRST_IO);
        pthread_mutex_destroy(&(IO[t]->MUTEX_io));
        pthread_cond_destroy(&(IO[t]->COND_io));
        queueCleanup(IO[t]->waitingQ, wQ, error);
        free(IO[t]);
        IO[t] = NULL;
    }

    int r;
    char qustr[16];
    for (r = 0; r < PRIORITIES_TOTAL; r++) {
        sprintf(qustr, "readyQ[%d]", r);
        queueCleanup(readyQ[r], qustr, error);
    }
    queueCleanup(createQ, "createQ", error);

        for (t = 1; t <= MAX_SHARED_RESOURCES; t++) {
        if (group[t] != empty) {
            //deallocation
            uint64_t longerror;
            int r;
            for (r = 0; r < MUTUAL_MAX_RESOURCES; r++) {
                char mQ[24] = "group_yy mutexQ_x";
                mQ[6] = (t/10) + '0';
                mQ[7] = (t%10) + '0';
                mQ[16] = r + '0';
//                FIFOq_p mutexQ = FIFOq_construct(error);
//                while (!FIFOq_is_empty(group[t]->fmutex[r]->waiting, NULL))
//                    FIFOq_enqueuePCB(mutexQ,
//                        FIFOq_dequeue(group[t]->fmutex[r]->waiting, NULL), error);
                if (!FIFOq_is_empty(group[t]->fmutex[r], error))
                    queueCleanup(group[t]->fmutex[r], mQ, error);
                else
                    FIFOq_destruct(group[t]->fmutex[r], error);
                //mutex_lock_terminate(group[t]->fmutex[r], &longerror);
                
                char cQ[24] = "group_yy condQ_x";
                cQ[6] = (t/10) + '0';
                cQ[7] = (t%10) + '0';
                cQ[15] = r + '0';
//                FIFOq_p condQ = FIFOq_construct(error);
//                while (!FIFOq_is_empty(group[t]->fcond[r]->waiting, NULL))
//                    FIFOq_enqueuePCB(condQ,
//                        FIFOq_dequeue(group[t]->fcond[r]->waiting, NULL), error);
                if (!FIFOq_is_empty(group[t]->fcond[r], error))
                   queueCleanup(group[t]->fcond[r], cQ, error);
                else
                    FIFOq_destruct(group[t]->fcond[r], error);
                //cond_var_terminate(group[t]->fcond[r], &longerror);
            }
            free(group[t]);
            group[t] = NULL;
        }
    }
    free(empty);
    empty = NULL;

    
    int endidl = current->pid == idl->pid;
    if (idl != NULL) {
        if (EXIT_STATUS_MESSAGE) {
            char pcbstr[PCB_TOSTRING_LEN];
            printf("\n>System Idle: %s\n", PCB_toString(idl, pcbstr, error));
        }
        PCB_destruct(idl);
    }
    if (!endidl && current != NULL) {
        if (EXIT_STATUS_MESSAGE) {
            char pcbstr[PCB_TOSTRING_LEN];
            printf("\n>Running PCB: %s\n",
                   PCB_toString(current, pcbstr, error));
        }
        PCB_destruct(current);
    } else if (EXIT_STATUS_MESSAGE)
        printf("\n>Idle process was final Running PCB\n");

}

/* Deallocates the queue data structures on the C simulator running this program
 * so that we don't have memory leaks and horrible garbage.
 */
void queueCleanup(FIFOq_p queue, char *qstr, int *error)
{
    int stz = FIFOQ_TOSTRING_MAX;
    char str[stz];

    if (EXIT_STATUS_MESSAGE) {
        printf("\n>%s deallocating...\n", qstr);
        printf(">%s", FIFOq_toString(queue, str, &stz, error));
    }

    if (queue->size) {
        printf("size: %d\n", queue->size);
        if (EXIT_STATUS_MESSAGE) {
            printf(">System exited with non-empty %s\n", qstr);
        }
        while (!FIFOq_is_empty(queue, error)) {
            PCB_p pcb = FIFOq_dequeue(queue, error);
            
            if (pcb != idl) {
                char pcbstr[PCB_TOSTRING_LEN];
                if (EXIT_STATUS_MESSAGE)
                    printf("\t%s\n", PCB_toString(pcb, pcbstr, error));
                if (pcb->queues == 0) PCB_destruct(pcb);

            } else
                puts("IDL!!!!!!!!!");

        }
    } else if (EXIT_STATUS_MESSAGE)
        printf(" empty\n");
    FIFOq_destruct(queue, error);
}

/* Deallocates the stack data structure on the C simulator running this program
 * so that we don't have memory leaks and horrible garbage.
 */
void stackCleanup()
{
    int i;
    if (SysPointer > 0 && EXIT_STATUS_MESSAGE) {
        printf("System exited with non-empty stack\n");
        for (i = 0; i <= SysPointer; i++) {
            printf("\tSysStack[%d] = %lu\n", i, SysStack[i]);
        }
    }
}




/******************************************************************************/
/*************************** TESTING/DEBUG ************************************/
/******************************************************************************/

void nanosleeptest()
{
    int i = 0;
    struct timespec m = {.tv_sec = 0, .tv_nsec = 5};
    while (i < 100000) {
        nanosleep(&m, NULL);
        printf("%d nanoseconds have passes", i);
        i++;
    }
}



