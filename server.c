#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT 8080
#define MAX_CLIENTS 100

// Shared memory structure to hold client sockets
struct SharedData {
    int client_sockets[MAX_CLIENTS];
};

// Broadcast function
void broadcast_message(int sender_sock, const char *msg, struct SharedData *shared) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        int sock = shared->client_sockets[i];
        if (sock > 0 && sock != sender_sock) {
            write(sock, msg, strlen(msg));
        }
    }
}

void handle_client(int client_sock, struct SharedData *shared) {
    char buffer[1024];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int n = read(client_sock, buffer, sizeof(buffer));
        if (n <= 0) {
            // client disconnected
            close(client_sock);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (shared->client_sockets[i] == client_sock) {
                    shared->client_sockets[i] = 0;
                    break;
                }
            }
            exit(0);
        }

        printf("Received from client %d: %s\n", client_sock, buffer);

        // Broadcast to others
        broadcast_message(client_sock, buffer, shared);
    }
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    // Create socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("listen failed");
        exit(1);
    }

    // Shared memory
    key_t key = ftok("shmfile", 65);
    int shmid = shmget(key, sizeof(struct SharedData), 0666 | IPC_CREAT);
    struct SharedData *shared = (struct SharedData*)shmat(shmid, NULL, 0);

    // Initialize sockets list
    for (int i = 0; i < MAX_CLIENTS; i++) {
        shared->client_sockets[i] = 0;
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("accept failed");
            continue;
        }

        printf("New client connected: %d\n", client_sock);

        // Store client socket in shared memory
        int stored = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (shared->client_sockets[i] == 0) {
                shared->client_sockets[i] = client_sock;
                stored = 1;
                break;
            }
        }
        if (!stored) {
            printf("Max clients reached. Connection rejected.\n");
            close(client_sock);
            continue;
        }

        // Fork child process
        if (fork() == 0) {
            close(server_sock); // child doesn’t need server socket
            handle_client(client_sock, shared);
            exit(0);
        } else {
            close(client_sock); // parent doesn’t need client socket
        }
    }

    // Cleanup (never reached in this infinite loop)
    shmdt(shared);
    shmctl(shmid, IPC_RMID, NULL);
    close(server_sock);
    return 0;
}
