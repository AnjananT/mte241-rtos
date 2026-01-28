#include "kernel.h"
#include <stdio.h>
#include "stm32f4xx.h"

#define THREAD_STACK_SIZE 0x400

k_thread threadArray[MAX_THREADS];
int currentThread = -1;
int numThreadsRunning = 0;
uint32_t* stackPtr;

// Static allocation of stacks to avoid MSP collision
static uint32_t thread_stacks[MAX_THREADS][THREAD_STACK_SIZE / 4];

extern void runFirstThread(void);

void osIdleThread(void* args) {
	while(1) {
		// Low power mode could go here
		__asm("wfi");
	}
}

void SVC_Handler_Main(unsigned int *svc_args)
{
	unsigned int svc_number;
	svc_number = ((char *)svc_args[6])[-2];

	switch (svc_number)
	{
		case YIELD:
			_ICSR |= 1 << 28; // Trigger PendSV
			__asm("isb");
			break;
		case 3:
			__set_PSP((uint32_t)stackPtr);
			runFirstThread();
			break;
		default:
			break;
	}
}

bool osCreateThread(void (*thread_function)(void*), void* args, uint32_t priority)
{
	if (numThreadsRunning >= MAX_THREADS) return false;

	int id = numThreadsRunning;
	uint32_t* sp = &thread_stacks[id][(THREAD_STACK_SIZE / 4) - 1];

	// Hardware stack frame (auto-restored by CPU)
	*(--sp) = 1 << 24; // xPSR (Thumb bit must be set)
	*(--sp) = (uint32_t)thread_function; // PC
	*(--sp) = 0xFFFFFFFD; // LR (Return to Thread mode, use PSP)
	*(--sp) = 0x12; // R12
	*(--sp) = 0x3; // R3
	*(--sp) = 0x2; // R2
	*(--sp) = 0x1; // R1
	*(--sp) = (uint32_t)args; // R0

	// Software stack frame (manually restored by PendSV)
	for (int i = 0; i < 8; i++){
		*(--sp) = 0; // R4-R11
	}

	threadArray[id].sp = sp;
	threadArray[id].thread_function = thread_function;
	threadArray[id].priority = priority;
	threadArray[id].state = READY;
	threadArray[id].timeslice = 10; // 10ms default
	threadArray[id].runtime = 10;
	threadArray[id].sleep_ticks = 0;
	
	numThreadsRunning++;
	return true;
}

void osKernelInitialize(void)
{
	for(int i = 0; i < MAX_THREADS; i++) {
		threadArray[i].state = BLOCKED; // Initialize as empty
	}
	
	currentThread = -1;
	numThreadsRunning = 0;

	// Set priority of PendSV and SVC
	SHPR3 |= 0xFF << 16; // PendSV = lowest priority
	SHPR2 |= 0x00 << 24; // SVC = highest priority

	// Create idle thread at lowest priority
	osCreateThread(osIdleThread, NULL, 255);
}

void osKernelStart(void) {
	osSched(); // Find first thread to run
	stackPtr = threadArray[currentThread].sp;
	__asm("SVC #3");
}

void osSched() {
	// Save SP of current thread if one is running
	if (currentThread != -1) {
		threadArray[currentThread].sp = (uint32_t*)(__get_PSP() - 8*4);
		if (threadArray[currentThread].state == RUNNING) {
			threadArray[currentThread].state = READY;
		}
	}

	// Priority-based scheduling logic
	int highestPriority = 256;
	int nextThread = -1;

	for (int i = 0; i < numThreadsRunning; i++) {
		if (threadArray[i].state == READY && threadArray[i].priority < highestPriority) {
			highestPriority = threadArray[i].priority;
			nextThread = i;
		}
	}

	// Fallback to idle thread (should always be there)
	if (nextThread == -1) nextThread = 0; 

	currentThread = nextThread;
	threadArray[currentThread].state = RUNNING;
	threadArray[currentThread].runtime = threadArray[currentThread].timeslice;
	
	__set_PSP((uint32_t)threadArray[currentThread].sp);
}

void osYield(){
	__asm("SVC #100");
}

void osDelay(uint32_t ms) {
	threadArray[currentThread].state = SLEEPING;
	threadArray[currentThread].sleep_ticks = ms;
	osYield();
}

void osTaskTick(void) {
	for(int i = 0; i < numThreadsRunning; i++) {
		if (threadArray[i].state == SLEEPING) {
			if (threadArray[i].sleep_ticks > 0) {
				threadArray[i].sleep_ticks--;
			}
			if (threadArray[i].sleep_ticks == 0) {
				threadArray[i].state = READY;
			}
		}
	}
	
	// Preemption logic
	if (currentThread != -1) {
		threadArray[currentThread].runtime--;
		if (threadArray[currentThread].runtime == 0) {
			_ICSR |= 1 << 28; // Trigger PendSV
		}
	}
}


