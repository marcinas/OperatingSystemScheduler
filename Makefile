problem3: OS.o FIFOq.o PCB.o
	gcc -o problem3 OS.o FIFOq.o PCB.o

OS.o: OS.c OS.h
	gcc -c OS.c

FIFOq.o: FIFOq.c FIFOq.h
	gcc -c FIFOq.c

PCB.o: PCB.c PCB.h
	gcc -c PCB.c

clean:
	rm -f problem3.exe OS.o FIFOq.o PCB.o u3.exe

debugging:
	gcc -g -Wall OS.c FIFOq.c PCB.c -o u3


 
