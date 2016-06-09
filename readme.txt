things to do:

everyone, please get all your coding done by Saturday night.
I need to make sure everything runs smoothly and the project is complete
if you don't have your code in by Saturday night I'll consider your contribution AWOL
I only have a short time to piece everything together and bugfix

Chris
	fthread.c; fthread.h; integrate into OS.c and test to make sure it runs right

Daniel
	resource deadlock monitor with tests; or commented out functional code by Sat night if Chris hasn't uploaded fthread integration yet

Bun
	fthreads, deadlock monitor help; tune-up/comment; help Chris integrate fthreads

Mark
	make non-deadlock version; tune-up/comment

Bruno
	tune-up/comment

C pocket reference cuz there's a cow on the front

4 actual pthreads running:
	CPU
	Timer
	I/O 1
	I/O 2

todo:
    comment everything
    integrate fthreads
    integrate resource deadlock monitor
    tune-up, bugfix
    simulate timer interrupt in isr, traps that aren't timer related
	simulate interrupt hierarchy?

-----------------------------------------------
OS.c
-----------------------------------------------
create new thread for creating PCBs called void* user(void*)
	need: mutexes to be shared with scheduler
	does  not call scheduler--just enqueues to createQ
	createQ should not overload timer, be roughly equal with

create new thread for terminating as well?

new checks for the mutex/cond arrays

bunch of new static counters for PCB types and totals
	scheduler add to readyQ ++
	terminate deletion --

parallel global array containing pcb pro-con structs and integer resource


special deadlock monitor
every time it runs, checkings all of the mutex queues
if a pair pcb has one pcb locking mutex 1 and the other locking mutex 2
	then deadlock was foundederest!



scheduling? how do we simulate?
	if pcb checks lock and its partner has the lock, it goes into the waitingq in mutex
		it has to call some kind of interrupt to scheduler
		and the pcb itself does NOT go back into readyQ but into mutex queue


SUPER SCHEDULER CHANGES
	four priorities
		level 0,  5% only CPU types, always run that first: round-robin
		level 1, 80% all types, always run that first: round-robin and starvation watch
		level 2, 10% all types, always run that first: round-robin and starvation watch
		level 3,  5% all types, always run that first: round-robin and starvation watch

	the scheduler every 10-20 schedulings will go through all processes in all levels and
		check their internal last clock against current clock--any pcbs that have a
		significant amount of difference are moved the the back of the next highest
		readyQ

	when the scheduler re-enqueues a process, it always puts it in its original priority level

	there should be no special interrupts for when priority 0 gets enqueued--scheduler will take care of it	

-----------------------------------------------
PCB.c
-----------------------------------------------

PCB types
IO Processes		50 max
CPU Processes		25 max
Producer Processes	10 max	
Consumer Processes	10 max
Mutual 2R A Processes	10 max
Mutual 2R B Processes	10 max

struct pair {
	pcbA
	pcbB
	mutex
	cond
	special resource to be shared and cared for :)
}

PCB now has 4 priority levels
0 -  5%
1 - 80%
2 - 10%
3 -  5%

PCB new data fields
last	clock at last time pcb was running
type	which type
pair	the pid of the paired PCB
or index in global array of struct pcb pairs

regfile new data fields
thread arrays of pc values


prod
pc 400 lock
pc 450 wait if con hasn't read yet
pc 510 change value
pc 520 set flag to unread
pc 550 signal
pc 600 unlock

con
pc 200 lock
pc 250 wait if pro hasn't written yet
pc 310 read value
pc 320 set flag to read
pc 350 signal
pc 400 unlock

mutual 2r (non-deadlock A and B and deadlock A)

pc 600 lock mutex for resource 1
pc 650 lock mutex for resource 2
pc 700 "modify" resource 1
pc 750 "modify" resource 2
pc 800 unlock mutex for resource 2
pc 850 unlock mutex for resource 1

mutual 2r (deadlock for B only)

pc 600 lock mutex for resource 2
pc 650 lock mutex for resource 1
pc 700 "modify" resource 1
pc 750 "modify" resource 2
pc 800 unlock mutex for resource 1
pc 850 unlock mutex for resource 2




-----------------------------------------------
fthread.c
-----------------------------------------------

Mutex malloc
PCB pointer to current owner of lock
fifoQ of processes waiting for the lock


Condition malloc
fifoQ of processes waiting for signal
parallel fifoQ of the mutex for each process


lock()
unlock()
	mutex then must put head of queue into readyQ
wait()
signal()
	cond puts head of queue into its mutex
trylock()





Starvation Check Report

Our starvation algorithm goes through the whole Ready Queue and checks
the time that each PCB was last given CPU time. If the last time it
was give CPU time was too long ago we promote it. We do this for 
every some number of schedules. To implement this alogorithm we added a
field to the PCB data structure. This new field is given the clocktime
of when this PCB's process last left the CPU. Here is the lifecycle of
a PCB that will be checked for starvation:

1. When a PCB gets created it waits in the creation queue.
2. When its time to add all created pcb's to the ready queue we give
   them each created pcb the current clocktime and then add it to 
   the ready queue.
3. We allow our OS to run normally. Although whenever a process gets 
   interrupted we save the process in its pcb, update the clocktime
   to the current clocktime, and then insert it to the ready queue.
4. We keep a total count of how many times the schedules had been
   called. If hits an interval that we've defined (e.g. every 15
   scheduler calls) then we awaken the starvation daemon.
5. The Starvation daemon goes through the ready queue and compares
   the time each PCB was given CPU time (given by the new field) to
   a threshold we have defined (e.g. 3000 cycles). If the PCB's
   clocktime minus the current computer clocktime is greater than 
   this threshold then we promoted by 1 priority. Promotion is done
   by simply splicing it from it's original priority queue and
   enqueing it to the it's new priority.
6. If the starvations daemon finds a PCB that was previously promoted
   and that has recieved enough CPU attention then it puts that PCB
   into it's original priority queue.
7. Every time a PCB that had promoted priority is scheduled it counts
   how many times it has recieved CPU time. If this number reaches a 
   threshold (e.g. 3) then the next time the daemon is called it will
   be put back into it's original priority queue. 

    
 


























