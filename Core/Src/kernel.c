#include "kernel.h"
#include <stdio.h>
#include "stm32f4xx.h"

#define STACK_SIZE 0x400
#define MAX_STACK_SIZE 0x4000
#define STACK_ALIGNMENT

extern void runFirstThread(void);

uint32_t* stackPtr;
static uint32_t* stack_pool_base;
static uint32_t* next_stack_top;
static uint32_t stack_pool_used = 0;

k_thread threadArray[MAX_THREADS];
int currentThread = -1;
int numThreadsRunning = 0;

void SVC_Handler_Main(unsigned int *svc_args)
{
	unsigned int svc_number;
	svc_number = ((char *)svc_args[6])[-2];

	switch (svc_number)
	{
		case YIELD:
			_ICSR |= 1 << 28;
			__asm("isb");
			break;
		case 17:
			printf("Called system call 17\r\n");
			break;
		case 18:
			break;
		case 3:
			printf("[DEBUG] Setting PSP to 0x%08X\r\n", (uint32_t)stackPtr);
			__set_PSP((uint32_t)stackPtr);
            printf("[DEBUG] About to run first thread\r\n");
			runFirstThread();
			break;
		default:
			printf("Unknown system call: %u\r\n", svc_number);
			break;
	}
}

void syscall_17(void){
	__asm("SVC #17");
}

void syscall_18(void){
	__asm("SVC #18");
}

uint32_t* allocate_stack(void)
{
	if(stack_pool_base == NULL){
		stack_pool_base = *(uint32_t**)0x0; // MSP init val
		next_stack_top = stack_pool_base;
	}


	// return NULL if not enough space for new thread stack
	if (stack_pool_used + STACK_SIZE > MAX_STACK_SIZE){
	return NULL;
	}

	next_stack_top = (uint32_t*)((uint32_t)next_stack_top - STACK_SIZE);
	stack_pool_used += STACK_SIZE;

	return next_stack_top;
}

bool osCreateThread(void (*thread_function)(void*))
{
	// allocate stack
	uint32_t* new_stack = allocate_stack();
	if (new_stack == NULL){
		printf("Failed to allocate new stack for thread. \r\n");
		return false;
	}

	// set up initial context frame
	uint32_t* sp = new_stack;

	*(--sp) = 1 << 24; // xPSR
	*(--sp) = (uint32_t)thread_function; // PC
	*(--sp) = 0xA; // LR
	*(--sp) = 0xA; // R12
	*(--sp) = 0xA; // R3
	*(--sp) = 0xA; // R2
	*(--sp) = 0xA; // R1
	*(--sp) = 0xA; // R0

	for (int i = 0; i < 8; i++){
		*(--sp) = 0xA;
	}

	// update kernel data structures
	threadArray[numThreadsRunning].sp = sp;
	threadArray[numThreadsRunning].thread_function = thread_function;
	numThreadsRunning++;

	printf("Thread created successfully\r\n");
	return true;
}

void osKernelInitialize(void)
{

	stack_pool_base = *(uint32_t**)0x0;
	next_stack_top = stack_pool_base;
	stack_pool_used = 0;
	currentThread = -1;
	numThreadsRunning = 0;

	// set priority of PendSV to almost the weakest
	SHPR3 |= 0xFE << 16;
	SHPR2 |= 0xFD << 24;

	printf("Kernel Initialized\r\n");
}

void osKernelStart(void){
	currentThread = 0;
	stackPtr = threadArray[currentThread].sp;
	__asm("SVC #3");
}

void osSched() {
	threadArray[currentThread].sp = (uint32_t*)(__get_PSP() - 8*4); // save SP of current thread
	currentThread = (currentThread+1) % numThreadsRunning; // change current thread to next thread
	__set_PSP(threadArray[currentThread].sp); // set PSP to new current thread's stack ptr
}

void osYield(){
	__asm("SVC #100");
}


