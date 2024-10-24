#define _GNU_SOURCE  // Define GNU feature test macro

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <arpa/inet.h>
#include <sched.h>   // Required for setting thread affinity
#include <errno.h>  // For error handling


#define PORT 6435             // Port number for the server
#define SHM_KEY 0x5678        // Key for the shared memory
#define MAX_BATCH_SIZE 65536000  // Max data size in shared memory
#define BUFFER_SIZE 1024      // Buffer size for client communication

// Shared memory data function
const char* read_from_shared_memory() {
    int shmid = shmget(SHM_KEY, sizeof(int) + MAX_BATCH_SIZE, 0666);

    // Attach to the shared memory
    char* shm_addr = shmat(shmid, NULL, 0);

    // Skip the first sizeof(int) bytes to access the actual data
    const char* data = shm_addr + sizeof(int);
    return data;
}

// Global variables to manage data between threads
char shared_memory_data[MAX_BATCH_SIZE];  // Buffer to store data read from shared memory
pthread_mutex_t data_lock;  // Mutex to manage access to shared_memory_data

// Function to bind a thread to a specific core
void bind_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        fprintf(stderr, "Error setting thread affinity: %s\n", strerror(result));
    }
}

// Function to handle reading from shared memory in a separate thread
void* shared_memory_reader(void* arg) {
    // Bind this thread to core 0
    bind_thread_to_core(0);
    
    while (1) {
        // Read the data from shared memory
        const char* data = read_from_shared_memory();

        // Lock before writing to shared_memory_data to avoid race conditions
        pthread_mutex_lock(&data_lock);
        strncpy(shared_memory_data, data, MAX_BATCH_SIZE);
        pthread_mutex_unlock(&data_lock);

        // Sleep for a short period before reading again (simulate periodic reads)
        usleep(50000);  // Sleep for 50ms
    }
    return NULL;
}

// Function to handle communication with a client
void* client_handler(void* socket_desc) {
    // Bind this thread to core 1
    bind_thread_to_core(1);

    int client_sock = *(int*)socket_desc;
    free(socket_desc);  // Free the pointer passed to this thread

    char buffer[BUFFER_SIZE];
    while (1) {
        // Lock to ensure we read the latest data from shared memory
        pthread_mutex_lock(&data_lock);
        strncpy(buffer, shared_memory_data, BUFFER_SIZE - 1);
        buffer[BUFFER_SIZE - 1] = '\0';  // Null-terminate the string
        pthread_mutex_unlock(&data_lock);

        // Send the data to the client
        if (send(client_sock, buffer, strlen(buffer), 0) < 0) {
            perror("Send failed");
            close(client_sock);
            pthread_exit(NULL);
        }

        // Sleep to control the frequency of data sent to the client
        usleep(100000);  // Sleep for 100ms
    }

    // Close the client socket
    close(client_sock);
    pthread_exit(NULL);
}

// Main server function
int main() {
    int server_fd, client_sock, *new_sock;
    struct sockaddr_in server, client;
    socklen_t client_len = sizeof(struct sockaddr_in);

    // Initialize the mutex
    pthread_mutex_init(&data_lock, NULL);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Could not create socket");
        exit(1);
    }

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(1);
    }
    puts("Bind done");

    // Listen for connections
    listen(server_fd, 5);
    puts("Waiting for incoming connections...");

    // Create a thread to read from shared memory
    pthread_t reader_thread;
    if (pthread_create(&reader_thread, NULL, shared_memory_reader, NULL) != 0) {
        perror("Could not create shared memory reader thread");
        exit(1);
    }

    // Bind the reader thread to core 0
    bind_thread_to_core(0);

    // Main server loop to accept client connections
    while ((client_sock = accept(server_fd, (struct sockaddr*)&client, &client_len)) >= 0) {
        puts("Connection accepted");

        // Create a new thread for each client
        new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, client_handler, (void*)new_sock) != 0) {
            perror("Could not create client handler thread");
            free(new_sock);
            continue;
        }

        // Bind the client thread to core 1 (or choose dynamically)
        bind_thread_to_core(1);

        pthread_detach(client_thread);  // Detach the thread so it cleans up after itself
    }

    if (client_sock < 0) {
        perror("Accept failed");
        close(server_fd);
        exit(1);
    }

    // Cleanup
    close(server_fd);
    pthread_mutex_destroy(&data_lock);
    return 0;
}
