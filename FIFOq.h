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

#ifndef FIFOQ_H
#define FIFOQ_H

#include "PCB.h"

#define this this_
#define FIFO_NULL_ERROR 431
#define FIFO_CONSTRUCT_ERROR 433
#define FIFO_STRING_ERROR 439

#define NODE_NULL_ERROR 401
#define NODE_CONSTRUCT_ERROR 409
#define NODE_STRING_ERROR 419
#define NODE_DATA_ERROR 421

#define FIFOQ_TOSTRING_MAX (PCB_TOSTRING_LEN * 100)

typedef struct FIFOq *FIFOq_p;
typedef struct Node_type Node;
typedef Node *Node_p;

struct FIFOq
{
    int size;
    Node_p head;
    Node_p tail;
};

struct Node_type
{
    int pos;
    PCB_p data;
    Node_p next_node;
};

FIFOq_p FIFOq_construct(int*);
void    FIFOq_destruct (FIFOq_p, int*);              // deallocates pcb from the heap
int     FIFOq_init     (FIFOq_p, int*);              // sets default values for member data
int     FIFOq_is_empty (FIFOq_p, int*);  
PCB_p   FIFOq_peek(FIFOq_p, int*);
void    FIFOq_enqueue  (FIFOq_p, Node_p, int*);
void    FIFOq_enqueuePCB(FIFOq_p, PCB_p, int*);
PCB_p   FIFOq_dequeue  (FIFOq_p, int*);
char *  FIFOq_toString (FIFOq_p, char*, int*, int*); // returns a string representing the contents of the pcb
Node_p Node_construct(PCB_p data, Node_p next, int * ptr_error);
int    Node_init     (Node_p this, PCB_p, Node_p, int*);
int    Node_destruct (Node_p this);
PCB_p  Node_getData  (Node_p this, int * ptr_error);
int    Node_setData  (Node_p this, PCB_p data);
Node_p Node_getNext  (Node_p this, int * ptr_error);
int    Node_setNext  (Node_p this, Node_p next);
char * Node_toString (Node_p this, char *str, int *ptr_error);
Node_p FIFOq_remove_and_return_next(Node_p curr, Node_p prev, FIFOq_p list);
int is_null(void *this, int *ptr_error);
int FIFOq_test_main(int argc, char** argv);

#endif /* FIFOQ_H */


