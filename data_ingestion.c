#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>  // For sleep()

#define SHM_KEY 0x5678
#define MAX_BATCH_SIZE 65536000   // Max size for shared memory

// Function to write data to shared memory
void write_to_shared_memory(int value) {
    // Create shared memory segment
    int shmid = shmget(SHM_KEY, sizeof(int) + MAX_BATCH_SIZE, 0666 | IPC_CREAT);

    // Attach to shared memory
    char* shm_addr = shmat(shmid, NULL, 0);
    // Write the value to shared memory as a string
    snprintf(shm_addr + sizeof(int), MAX_BATCH_SIZE, "{%d}", value);

    // Detach from shared memory
    shmdt(shm_addr);
}

int main() {
    int x = 1000;  // Start at 1000

    while (x > 0) {
        // Write the value of x to shared memory
        write_to_shared_memory(x);

        // Print what was written for debugging purposes
        printf("Writing to shared memory: {%d}\n", x);

        // Sleep for 1 second
        sleep(1);

        // Decrement x
        x--;
    }

    return 0;
}
