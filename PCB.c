/*
 * Final Project - Operating System
 * TCSS 422 A Spring 2016
 * Mark Peters, Luis Solis-Bruno, Chris Ottersen
 */

#include "PCB.h"

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

static int type_count[] = {0, 0, 0, 0, 0};
static int io_count[] = {0, 0};

static bool procon = false;
static bool mutual = false;

/**
 * Constructs and initializes a pcb. Use unless you need to delay or avoid initialization.
 * @param ptr_error
 * @return 
 */
PCB_p PCB_construct_init(int *ptr_error)
{
    PCB_p pcb = PCB_construct(ptr_error);
    *ptr_error += PCB_init(pcb);
    return pcb;
}

/**
 * Returns a pcb pointer to heap allocation.
 * 
 * @return a pointer to a new PCB object
 */
PCB_p PCB_construct(int *ptr_error)
{
    PCB_p this = (PCB_p) malloc(sizeof(struct PCB));
    this->regs = (REG_p) malloc(sizeof(union regfile));
    int error = ((!this) * PCB_INIT_ERROR);
    int r;
    this->pid = -1;
    this->io = false;
    this->type = undefined;
    this->priority = -1;
    this->state = nostate;
    this->timeCreate = -1;
    this->timeTerminate = DEFAULT_TIME_TERMINATE;
    this->terminate = false;
    this->lastClock = -1;
    this->group = 0;
    this->queues = 0;
    for (r = 0; r < REGNUM; r++)
        this->regs->gpu[r] = -1;
    if (ptr_error != NULL) {
        *ptr_error += error;
    }
    return (this);
}

/**
 * Deallocates pcb from the heap.
 * @param this
 */
int PCB_destruct(PCB_p this)
{
    int error = (this == NULL) *
                PCB_NULL_ERROR; // sets error code to 1 if `this` is NULL
    this->terminate = true;
//    while (!THREADQ_is_empty(this->buddies, NULL)) {//TODO:FIFOQ_size
//        thread_type buddy = THREADQ_dequeue(this->buddies, false,
//                                            NULL);//TODO:FIFOQ_dequeue
//    }
//    THREADQ_destruct(this->buddies, NULL);//TODO:FIFOQ_destruct

    if (!error) {
        if (this->regs != NULL)
            free(this->regs);
        free(this);
        this = NULL;
    }
    return error;
}

/**
 * Sets default values for member data.
 * @param this
 * @return 
 */
int PCB_init(PCB_p this)
{
    static word pidCounter = 0;
    static int firstCall = 1;
    if (!firstCall) {
        srand(time(NULL) << 1);
        firstCall = 0;
    }
    int error = (this == NULL) * PCB_NULL_ERROR;
    int t;
    if (!error) {
        int chance, percent, priority, type;
        
        this->pid = ++pidCounter;
        this->attentionCount = 0;
        this->promoted = false;
        //IO and type determination
        int type_chance[LAST_PAIR + 1];
        percent = 0;
        for (type = LAST_PAIR; type >= regular; type--) {
            type_chance[type] = (MAX_TYPES[type] != type_count[type] && MAX_TYPES[type+LAST_PAIR] != type_count[type+LAST_PAIR]);
            if (type_chance[type])
                percent++;
        }
        
        if (PCB_DEBUG)
            printf("\t\tPER R: %d  P: %d   M: %d\n", type_chance[0],
                   type_chance[1], type_chance[2]);

        
        if (PCB_DEBUG)
            printf("\t\tpercent: %d\n", percent);
        if (percent || procon || mutual) {
            if (procon || mutual) {
                this->type = (procon ? consumer : mutual_B);
                if (PCB_DEBUG)
                    printf("\t\tkilled it %s\n", TYPE[this->type]);
            }
            else {
                for (type = LAST_PAIR; type >= regular; type--)
                    type_chance[type] *= 100 / percent;
                type_chance[regular] *= 1.5;
                
                if (PCB_DEBUG)
                    printf("\t\tCHA R: %d  P: %d   M: %d\n", type_chance[0],
                           type_chance[1], type_chance[2]);
                
                chance = rand() % 100;
                percent = 0;
                for (type = regular; type <= LAST_PAIR; type++) {
                    percent += type_chance[type];
                    if (chance < percent) {
                        this->type = type;
                        break;
                    }
                }
            }
            
            if (this->type == regular && ((MAX_IOCPU[0] == io_count[0]) ||
                                          (MAX_IOCPU[1] == io_count[1]))) {
                this->io = (MAX_IOCPU[0] == io_count[0]);
                if (PCB_DEBUG)
                    printf("\t\treg max detected io now %d\n", this->io);
            }
            else
                this->io = rand() % 2;

            procon = (type == producer);
            mutual = (type == mutual_A);
            if (PCB_DEBUG && (procon || mutual))
                printf("\t\twe got a live one %s\n", TYPE[this->type]);

            type_count[this->type]++;
            io_count[this->io] += (type == regular);

        }
        
        if (PCB_DEBUG)
            printf(
                "\t\tCUR R: %d {C: %d   I:%d}   P: %d   A: %d   C: %d   B: %d\n",
                type_count[0], io_count[0], io_count[1], type_count[1],
                type_count[2], type_count[3], type_count[4]);
        if (PCB_DEBUG)
            printf(
                "\t\tMAX R: %d {C: %d   I:%d}   P: %d   A: %d   C: %d   B: %d\n",
                MAX_TYPES[0], MAX_IOCPU[0], MAX_IOCPU[1], MAX_TYPES[1],
                MAX_TYPES[2], MAX_TYPES[1], MAX_TYPES[2]);

        
        //Priority determination
        chance = rand() % 100;
        percent = 0;
        for (priority = 0; priority < PRIORITIES_TOTAL; priority++) {
            percent += PRIORITIES[min(priority, PRIORITY_UNIQUE_UPTO)];
            if (chance < percent) {
                this->priority = priority;
                this->orig_priority = priority;
                break;
            }
            //printf("\npid: %lu chance: %d percent %d priority: %d\n", this->pid, chance, percent, priority);
        }
        if (this->priority < 0)
            error += PCB_PRIORITY_ERROR;
        this->state = DEFAULT_STATE;
        //this->timeCreate = 0;
        this->timeTerminate = DEFAULT_TIME_TERMINATE;
        REG_init(this->regs, &error);
        int c;
        if (this->type != regular && this->type <= LAST_PAIR * 2)
            for (c = 0; c < CALL_NUMBER; c++)
                this->regs->reg.CODES[c] = CODE_TYPE[(this->type == mutual_B && !DEADLOCK)? mutual_A : this->type][c];
        
        if (PCB_DEBUG) {
            int t;
            printf("\nPID:%lu\tType:%s\n", this->pid, TYPE[this->type]);
            for (t = 0; t < CALL_NUMBER; t++)
                printf("%lu\t%lu\n", this->regs->reg.CALLS[t], this->regs->reg.CODES[t]);
            printf("%lu\n", this->regs->reg.MAX_PC);
            printf("\n");
        }        
    }
    return error;
}

int PCBs_available()
{
    int type_chance[LAST_PAIR + 1];
    int type;
    int available = 0;
    for (type = LAST_PAIR; type >= regular; type--) {
        type_chance[type] = (MAX_TYPES[type] != type_count[type]);
        if (type_chance[type])
            available++;
    }
    return available + procon + mutual;
}

/**
 * Initializes registers. Must have PCB metadata correct for optimal values.
 * @param this
 * @param ptr_error
 * @return 
 */
REG_p REG_init(REG_p this, int *ptr_error)
{
    if (this == NULL)
        *ptr_error += PCB_NULL_ERROR;
    else {
        int t;
        int j;
        int thread_step;
        this->reg.pc = DEFAULT_PC;
        this->reg.MAX_PC =
            rand() % (MAX_PC_RANGE + 1 - MAX_PC_MIN) + MAX_PC_MIN;
        this->reg.sw = DEFAULT_SW;
        this->reg.term_count = 0; //set to 0 for infinite process
        this->reg.TERMINATE = ((int) (rand() % 100 >= TERM_INFINITE_CHANCE)) *
                              (rand() % TERM_RANGE + 1);
        for (t = 0; t < IO_NUMBER * IO_CALLS; t++)
            this->reg.IO_TRAPS[(int) (t / IO_CALLS)][t % IO_CALLS] =
                MIN_IO_CALL + (rand() % (this->reg.MAX_PC - MIN_IO_CALL));
        for (t = 0; t < IO_NUMBER * IO_CALLS; t++)
            if (!(this->reg.IO_TRAPS[(int) (t / IO_CALLS)][t % IO_CALLS] %
                  THREAD_CALL))
                this->reg.IO_TRAPS[(int) (t / IO_CALLS)][t %
                                                         IO_CALLS] = -1; //dup
            else
                for (j = 0; j < t; j++)
                    if (this->reg.IO_TRAPS[(int) (j / IO_CALLS)][j %
                                                                 IO_CALLS] ==
                        this->reg.IO_TRAPS[(int) (t / IO_CALLS)][t % IO_CALLS])
                        this->reg.IO_TRAPS[(int) (j / IO_CALLS)][j %
                                                                 IO_CALLS] = -1; //duplicate values erased
        thread_step = THREAD_CALL * (int) (
            ((this->reg.MAX_PC - MIN_THREAD_CALL) / CALL_NUMBER) / THREAD_CALL);
        for (t = 0; t < CALL_NUMBER; t++) {
            this->reg.CALLS[t] = MIN_THREAD_CALL + t * thread_step;
            this->reg.CODES[t] = 0;
        }
    }

    return this;
}

/**
 * Sets the pid of the process.
 * 
 * @param this 
 * @param pid the new pid value
 * @return 
 */
int PCB_setPid(PCB_p this, word pid)
{
    // verify pid does not already exist
    int error = (this == NULL) * PCB_NULL_ERROR;
    if (!error) {
        this->pid = pid;
    }
    return error; // TODO: write
}

/**
 * Returns the pid of the process.
 * 
 * @param this
 * @return the pid of the process
 */
word PCB_getPid(PCB_p this, int *ptr_error)
{
    int error = (this == NULL) * PCB_NULL_ERROR;

    if (ptr_error != NULL) {
        *ptr_error += error;
    }

    return error ? ~0 : this->pid; // TODO: write
}

/**
 * Sets the state of the process.
 * 
 * @param this 
 * @param state the new state value
 * @return error 
 */
int PCB_setState(PCB_p this, enum state_type state)
{
    int error = 0;
    if (this == NULL) {
        error |= PCB_NULL_ERROR;
    }
    if (state < created || state > terminated) {
        error |= PCB_OTHER_ERROR;
    }
    if (!error) {
        this->state = state;
        if (state == terminated) {
            type_count[this->type]--;
            io_count[this->io] -= (this->type == regular);
            if (PCB_DEBUG)
                printf("\t\tprocess terminated, freed up slot:\n");
            if (PCB_DEBUG)
                printf(
                    "\t\tCUR R: %d {C: %d   I:%d}   P: %d   A: %d   C: %d   B: %d\n",
                    type_count[0], io_count[0], io_count[1], type_count[1],
                    type_count[2], type_count[3], type_count[4]);
            if (PCB_DEBUG)
                printf(
                    "\t\tMAX R: %d {C: %d   I:%d}   P: %d   A: %d   C: %d   B: %d\n",
                    MAX_TYPES[0], MAX_IOCPU[0], MAX_IOCPU[1], MAX_TYPES[1],
                    MAX_TYPES[2], MAX_TYPES[1], MAX_TYPES[2]);
        }
    }
    return error;
}

/**
 * Returns the current state of the process.
 * 
 * @param this
 * @return the state of the process
 */
enum state_type PCB_getState(PCB_p this, int *ptr_error)
{
    int error = (this == NULL) * PCB_NULL_ERROR;

    if (ptr_error != NULL) {
        *ptr_error += error;
    }
    return error ? ~0 : this->state;
}


/**
 * Sets the pid of the process.
 * 
 * @param this 
 * @param pid the new pid value
 * @return 
 */
int PCB_setPriority(PCB_p this, unsigned short priority)
{
    int error = 0;
    if (this == NULL) {
        error |= PCB_NULL_ERROR;
    }
    if (priority > LOWEST_PRIORITY) {
        error |= PCB_OTHER_ERROR;
    }
    if (!error) {
        this->priority = priority;
    }
    return error;
}

/**
 * Returns the current state of the process.
 * 
 * @param this
 * @return the pid of the process
 */
unsigned short PCB_getPriority(PCB_p this, int *ptr_error)
{
    int error = (this == NULL) * PCB_NULL_ERROR;

    if (ptr_error != NULL) {
        *ptr_error += error;
    }
    return error ? ~0 : this->priority; // TODO: write
}

/**
 * Sets the pid of the process.
 * 
 * @param this 
 * @param pid the new pid value
 * @return 
 */
int PCB_setPc(PCB_p this, word pc)
{
    int error = (this == NULL) * PCB_NULL_ERROR;
    if (!error) {
        this->regs->reg.pc = pc;
    }
    return error; // TODO: write
}

/**
 * Returns the pc value state of the process.
 * 
 * @param this
 * @return the address where the program will resume
 */
word PCB_getPc(PCB_p this, int *ptr_error)
{
    int error = (this == NULL) * PCB_NULL_ERROR;

    if (ptr_error != NULL) {
        *ptr_error += error;
    }
    return error ? -1 : this->regs->reg.pc; // TODO: write
}

/**
 * Sets the pid of the process.
 * 
 * @param this 
 * @param pid the new pid value
 * @return 
 */
int PCB_setSw(PCB_p this, word sw)
{
    int error = (this == NULL) * PCB_NULL_ERROR;
    if (!error) {
        this->regs->reg.sw = sw;
    }
    return error; // TODO: write
}

/**
 * Returns the pc value state of the process.
 * 
 * @param this
 * @return the address where the program will resume
 */
word PCB_getSw(PCB_p this, int *ptr_error)
{
    int error = (this == NULL) * PCB_NULL_ERROR;
    if (ptr_error != NULL) {
        *ptr_error += error;
    }
    return error ? ~0 : this->regs->reg.sw; // TODO: write
}

/**
 * Returns a string representing the contents of the pcb. 
 * <br /><em><strong>Note:</strong> parameter string must be 80 chars or more.</em>
 * @param this
 * @param str
 * @param int
 * @return string representing the contents of the pc.
 */
char *PCB_toString(PCB_p this, char *str, int *ptr_error)
{
    int error = (this == NULL || str == NULL) * PCB_NULL_ERROR;
    if (!error) {
        str[0] = '\0';
        char regString[PCB_TOSTRING_LEN - 1];

//        if (this->timeTerminate == DEFAULT_TIME_TERMINATE) {
//            const char *format = "PID: 0x%04lx  PC: 0x%05lx  State: %s  Priority: 0x%x  Intensity: %s  Type: %s  Group: %lu2  %s Created: 0x%05lx";
//            snprintf(str, (size_t) PCB_TOSTRING_LEN - 1, format, this->pid,
//                     this->regs->reg.pc,
//                     STATE[this->state], this->orig_priority,
//                     this->io ? "IO" : "CPU", TYPE[this->type], this->group,
//                     Reg_File_toString(this->regs, regString, ptr_error),
//                     this->timeCreate);
//
//
//        } else {
        const char *format = "PID: 0x%04lx  PC: 0x%05lx  State: %s  Priority: 0x%x  Intensity: %s  Type: %s  Group: %2lu  %s Create: 0x%05lx End: 0x%05lx";
        snprintf(str, (size_t) PCB_TOSTRING_LEN - 1, format, this->pid,
                 this->regs->reg.pc,
                 STATE[this->state], this->orig_priority,
                 this->io ? "IO " : "CPU", TYPE[this->type], this->group,
                 Reg_File_toString(this->regs, regString, ptr_error),
                 this->timeCreate, this->timeTerminate);

    }

    

    if (ptr_error != NULL) {
        *ptr_error += error;
    }
    return str;

}

/**
 * Returns a string representing the contents of the regfile of the pcb. 
 * Note: parameter string must be 80 chars or more. 
 * @param this
 * @param str
 * @param int
 * @return string representing the contents of the regfile.
 */
char *Reg_File_toString(REG_p this, char *str, int *ptr_error)
{
    int error = (this == NULL || str == NULL) * PCB_NULL_ERROR;
    if (!error) {
        str[0] = '\0';
        const char *format = "MaxPC: 0x%06lx  Rollover: %3lu  Terminate: %3lu";
        snprintf(str, (size_t) PCB_TOSTRING_LEN - 1, format, this->reg.MAX_PC,
                 this->reg.term_count, this->reg.TERMINATE);
    }

    if (ptr_error != NULL) {
        *ptr_error += error;
    }
    return str;
}