#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stdbool.h>

#define SHPR2 *(uint32_t*)0xE000ED1C // SVC priority
#define SHPR3 *(uint32_t*)0xE000ED20 // PendSV priority
#define _ICSR *(uint32_t*)0xE000ED04 // PendSV trigger

#define YIELD 100
#define MAX_THREADS 8

//thread control block
typedef struct {
	uint32_t* sp;						 // stack pointer
	void (*thread_function)(void* args); // function to run
} k_thread;

// function declarations
void SVC_Handler_Main(unsigned int *svc_args);
void syscall_17(void);
void syscall_18(void);

uint32_t* allocate_stack(void);
bool osCreateThread(void (*thread_function)(void*));
void osKernelInitialize(void);
void osKernelStart(void);
void osYield(void);
void osSched(void);

#endif
