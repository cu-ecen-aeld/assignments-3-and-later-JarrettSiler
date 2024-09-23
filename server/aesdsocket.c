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
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

volatile sig_atomic_t run = 1; // Flag for main loop
int server_fd = -1, client_fd = -1; // File descriptors

//Signal Handler
void signal_handler(int signo) {
    syslog(LOG_INFO, "Caught signal %d, exiting", signo);
    run = 0;
    if (server_fd != -1) close(server_fd);
    if (client_fd != -1) close(client_fd);
}

//Main Function Structure
int main(int argc, char *argv[]) {
    int opt;
    int daemon_mode = 0;
    // Initialize syslog
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Parse command-line arguments
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    //daemon mode
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            // Parent exits
            exit(EXIT_SUCCESS);
        }

        // Child continues
        if (setsid() < 0) {
            syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Redirect standard file descriptors to /dev/null
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
    }

    // Register signal handlers
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Error registering signal handlers: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Error creating socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Bind socket to port 9000
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Error binding socket: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Error listening on socket: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Main loop to accept and handle connections
    while (run) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno == EINTR) continue; // Interrupted by signal
            syslog(LOG_ERR, "Error accepting connection: %s", strerror(errno));
            break;
        }

        // Log accepted connection
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // Handle client
        char buffer[BUFFER_SIZE];
        char *data = NULL;
        size_t data_size = 0;

        // Open or create the data file for appending
        FILE *fp = fopen(DATA_FILE, "a+");
        if (fp == NULL) {
            syslog(LOG_ERR, "Error opening data file: %s", strerror(errno));
            close(client_fd);
            client_fd = -1;
            continue;
        }

        // Receive data until a newline is found
        int newline_found = 0;
        while (!newline_found && run) {
            ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
            if (bytes_received == -1) {
                if (errno == EINTR) continue;
                syslog(LOG_ERR, "Error receiving data: %s", strerror(errno));
                break;
            } else if (bytes_received == 0) {
                // Connection closed by client
                break;
            }

            // Search for newline
            for (ssize_t i = 0; i < bytes_received; i++) {
                if (buffer[i] == '\n') {
                    newline_found = 1;
                    bytes_received = i + 1; // Include newline
                    break;
                }
            }

            // Allocate or reallocate memory to store received data
            char *temp = realloc(data, data_size + bytes_received + 1);
            if (temp == NULL) {
                syslog(LOG_ERR, "Memory allocation failed");
                free(data);
                data = NULL;
                break;
            }
            data = temp;
            memcpy(data + data_size, buffer, bytes_received);
            data_size += bytes_received;
            data[data_size] = '\0'; // Null-terminate

            // If newline found, stop receiving
            if (newline_found) break;
        }

        // Write received data to file
        if (data != NULL) {
            fprintf(fp, "%s", data);
            fclose(fp);
            fp = NULL;

            // Send back the entire file content
            fp = fopen(DATA_FILE, "r");
            if (fp == NULL) {
                syslog(LOG_ERR, "Error opening data file for reading: %s", strerror(errno));
                free(data);
                data = NULL;
                close(client_fd);
                client_fd = -1;
                continue;
            }

            // Determine file size
            fseek(fp, 0, SEEK_END);
            long file_size = ftell(fp);
            rewind(fp);

            // Allocate buffer to read file content
            char *file_buffer = malloc(file_size);
            if (file_buffer == NULL) {
                syslog(LOG_ERR, "Memory allocation failed for file content");
                fclose(fp);
                free(data);
                data = NULL;
                close(client_fd);
                client_fd = -1;
                continue;
            }

            // Read file content
            fread(file_buffer, 1, file_size, fp);
            fclose(fp);
            fp = NULL;

            // Send file content to client
            ssize_t bytes_sent = send(client_fd, file_buffer, file_size, 0);
            if (bytes_sent == -1) {
                syslog(LOG_ERR, "Error sending data to client: %s", strerror(errno));
            }

            free(file_buffer);
            free(data);
            data = NULL;
        }

        // Log closed connection
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        close(client_fd);
        client_fd = -1;
    }

    // Cleanup
    close(server_fd);
    closelog();
    // Remove data file
    unlink(DATA_FILE);

    return 0;
}