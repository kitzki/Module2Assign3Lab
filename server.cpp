#include <iostream>
#include <fstream>
#include <cstring>
#include <ctime>
#include <csignal>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define PORT 8080
#define BACKLOG 5
#define BUFFER_SIZE 1024

// Error logging function
void log_error(const std::string &msg) {
    std::ofstream logFile("server_errors.log", std::ios::app);
    if (logFile.is_open()) {
        time_t now = time(0);
        char* dt = ctime(&now);
        dt[strlen(dt) - 1] = '\0'; // remove newline
        logFile << "[" << dt << "] ERROR: " << msg << std::endl;
        logFile.close();
    }
}

// Handle client communication
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            log_error("recv() failed in child process");
            break;
        } else if (bytes_received == 0) {
            // client disconnected
            break;
        }

        buffer[bytes_received] = '\0';

        if (strcmp(buffer, "exit") == 0) {
            break;
        }

        std::string response = "Echo: " + std::string(buffer);
        if (send(client_socket, response.c_str(), response.size(), 0) < 0) {
            log_error("send() failed in child process");
            break;
        }
    }
    close(client_socket);
    _exit(0); // ensure child exits
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr{}, client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        log_error("socket() failed");
        return 1;
    }

    // Bind
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("bind() failed");
        close(server_socket);
        return 1;
    }

    // Listen
    if (listen(server_socket, BACKLOG) < 0) {
        log_error("listen() failed");
        close(server_socket);
        return 1;
    }

    std::cout << "Server is listening on port " << PORT << "..." << std::endl;

    // Accept loop
    while (true) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            log_error("accept() failed");
            continue; // keep server running
        }

        pid_t pid = fork();
        if (pid < 0) {
            log_error("fork() failed");
            close(client_socket);
            continue;
        }

        if (pid == 0) {
            // Child process
            close(server_socket); // child doesn't need server socket
            handle_client(client_socket);
        } else {
            // Parent process
            close(client_socket); // parent doesn't need client socket
        }
    }

    close(server_socket);
    return 0;
}
