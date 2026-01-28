#include "sync.h"
#include "kernel.h"
#include "stm32f4xx.h"
#include <stdio.h>

extern k_thread threadArray[];
extern int currentThread;
extern int numThreadsRunning;

void osSemaphoreInit(osSemaphore* sem, uint32_t init_count) {
    sem->count = init_count;
}

void osSemaphorePend(osSemaphore* sem) {
    while (1) {
        __disable_irq();
        if (sem->count > 0) {
            sem->count--;
            __enable_irq();
            return;
        }
        threadArray[currentThread].state = BLOCKED;
        __enable_irq();
        osYield();
    }
}

void osSemaphorePost(osSemaphore* sem) {
    __disable_irq();
    sem->count++;
    // Unblock threads
    for (int i = 0; i < numThreadsRunning; i++) {
        if (threadArray[i].state == BLOCKED) {
            threadArray[i].state = READY;
        }
    }
    __enable_irq();
    osYield();
}

void osMutexInit(osMutex* mutex) {
    mutex->owner_id = -1;
    mutex->owner_original_priority = 255;
}

void osMutexLock(osMutex* mutex) {
    while (1) {
        __disable_irq();
        
        if (mutex->owner_id == -1) {
            // Take the mutex
            mutex->owner_id = currentThread;
            mutex->owner_original_priority = threadArray[currentThread].priority;
            __enable_irq();
            return;
        }

        // Priority Inheritance logic
        int owner = mutex->owner_id;
        if (threadArray[currentThread].priority < threadArray[owner].priority) {
            // Boost owner priority
            threadArray[owner].priority = threadArray[currentThread].priority;
            // Note: In a real kernel, we'd recursively boost if the owner is also blocked
        }

        threadArray[currentThread].state = BLOCKED;
        __enable_irq();
        osYield();
    }
}

void osMutexUnlock(osMutex* mutex) {
    __disable_irq();
    
    if (mutex->owner_id != currentThread) {
        __enable_irq();
        return;
    }

    // Restore original priority
    threadArray[currentThread].priority = mutex->owner_original_priority;
    mutex->owner_id = -1;

    // Wake up blocked threads
    for (int i = 0; i < numThreadsRunning; i++) {
        if (threadArray[i].state == BLOCKED) {
            threadArray[i].state = READY;
        }
    }
    
    __enable_irq();
    osYield();
}
