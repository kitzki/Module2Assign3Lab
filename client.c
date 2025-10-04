#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[1024];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        exit(1);
    }

    printf("Connected to server. Type messages (type 'exit' to quit):\n");

    // Fork into two processes: one for sending, one for receiving
    if (fork() == 0) {
        // Child handles receiving
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            int n = read(sock, buffer, sizeof(buffer));
            if (n <= 0) {
                printf("Disconnected from server.\n");
                close(sock);
                exit(0);
            }
            printf("\nBroadcast: %s\n", buffer);
        }
    } else {
        // Parent handles sending
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = 0; // remove newline

            if (strcmp(buffer, "exit") == 0) {
                close(sock);
                exit(0);
            }

            send(sock, buffer, strlen(buffer), 0);
        }
    }

    return 0;
}
