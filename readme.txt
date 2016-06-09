Experiments

Set up your simulation to go through a fixed number of loops, say 100,000 for each run. Be sure to seed the random number generator with a different number each time (a convenient way to do this is to use the system clock at runtime) you do a run.

Run the simulator at least ten times with the no-deadlock and ten times with the deadlock possible setups. Do a search through the output files for key events (deadlock in particular) and summarize these in a report.

Report

For each run of the simulator collect data on the number of processes that were run (total) and the numbers of processes still in all of the queues (ready and waiting) at the termination of the run. This will require an additional kind of "instrumentation". Collect all of these data for all runs and create a text report that summarizes the data. I'm not mandating any particular format - use your best judgment - but try to think through what kinds of questions might be satisfied by this summarized data.

In addition to the report on performance, include a summary of your starvation prevention algorithm and implementation techniques so I can see how you solved the problem.

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



     


























