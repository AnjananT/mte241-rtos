#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t count;
} osSemaphore;

typedef struct {
    int owner_id; // -1 if free
    uint32_t owner_original_priority;
} osMutex;

void osSemaphoreInit(osSemaphore* sem, uint32_t init_count);
void osSemaphorePend(osSemaphore* sem);
void osSemaphorePost(osSemaphore* sem);

void osMutexInit(osMutex* mutex);
void osMutexLock(osMutex* mutex);
void osMutexUnlock(osMutex* mutex);

#endif
