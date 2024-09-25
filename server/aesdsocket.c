#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>
#include <errno.h>

//Defining Constants and Global Variables
#define PORT 9000
#define BACKLOG 10
#define FILE_OI "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

int run = 1; // Flag for main loop
int server_fd = -1, client_fd = -1; // File descriptors

//cleanup steps
void cleanup() {
    if (server_fd != -1) close(server_fd);
    if (client_fd != -1) close(client_fd);
    if (remove(FILE_OI) != 0) syslog(LOG_ERR, "Failed to remove file: %s", strerror(errno));
    syslog(LOG_INFO, "Cleanup was reached!");
    closelog();
}

//Signal Handler
void signal_handler(int sign) {
    if (sign == SIGINT || sign == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        run = 0;
        cleanup();
        return;
    }
}

int modify_file(int client_fd) {
    char buffer[1024];
    int bytes_received;
    FILE *fs = fopen(FILE_OI, "a+");
    if (!fs) {
        syslog(LOG_ERR, "Server failed to open file: %s", strerror(errno));
        return -1;
    }
    while ((bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        fputs(buffer, fs);
        fflush(fs); 
        if (strchr(buffer, '\n')) break;
    }
    fseek(fs, 0, SEEK_SET);
    while ((bytes_received = fread(buffer, 1, sizeof(buffer), fs)) > 0) {
        send(client_fd, buffer, bytes_received, 0);
    }
    fclose(fs);
    return 0;
}


//Server structure
int main(int argc, char *argv[]) {
    
    // Initialize syslog and signal handling
    openlog("aesdsocket", LOG_PID, LOG_USER);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    ////////////////////////////////////////////////////////////
    //check if dameon mode is selected, then run it if so
    int opt;
    int daemon_mode = 0;
    opt = getopt(argc, argv, "d");
    if (opt == 'd') daemon_mode = 1;
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
            cleanup();
            return -1;
        }
        if (pid > 0) {
            syslog(LOG_INFO, "running daemonized");
            return 0;
        }

        //stop output to the terminal
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
    ////////////////////////////////////////////////////////////

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == -1) { //socket error handling
        syslog(LOG_ERR, "Error creating socket: %s", strerror(errno));
        cleanup();
        return -1;
    }

    // Allow socket to be reused
    int opt_val = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val)) < 0) {
        syslog(LOG_ERR, "Error setting socket options: %s", strerror(errno));
        cleanup();
        return -1;
    }


    // Bind socket to port 9000
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces
    server_addr.sin_port = htons(PORT);

    //error handling

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Error binding socket: %s", strerror(errno));
        close(server_fd);
        cleanup();
        return -1;
    }

    // Listen for connections
    struct sockaddr_in client_addr;
    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Error listening on socket: %s", strerror(errno));
        close(server_fd);
        cleanup();
        return -1;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // handle connections
    while (run) {

        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            //syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }

        // Log accepted connection
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // Handle client requests
        int result = modify_file(client_fd);
        if (result < 0) {
            syslog(LOG_ERR, "Error handling request from %s", client_ip);
        } else if (result == 0) {
            // Indicate that the client has disconnected properly
            syslog(LOG_INFO, "Client %s disconnected", client_ip);
        }

        close(client_fd);
        client_fd = -1;
    }
    
    syslog(LOG_INFO, "The end of the script was reached gracefully!");
    cleanup();
    return 0;
}