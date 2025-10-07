#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_FILE_SIZE 255
#define BUFFER_SIZE 512

void handle_client(int client_sock) {
    char filename[256];
    char response[BUFFER_SIZE];
    FILE *file;
    int bytes_read;
    long file_size;
    
    // Receive filename from client
    memset(filename, 0, sizeof(filename));
    bytes_read = recv(client_sock, filename, sizeof(filename) - 1, 0);
    
    if (bytes_read <= 0) {
        printf("Error receiving filename\n");
        close(client_sock);
        return;
    }
    
    // Remove newline if present
    filename[strcspn(filename, "\n")] = 0;
    printf("Client requested file: %s\n", filename);
    
    // Validate filename (basic check for path traversal)
    if (strstr(filename, "..") != NULL || strchr(filename, '/') != NULL) {
        strcpy(response, "ERROR: Invalid filename");
        send(client_sock, response, strlen(response), 0);
        close(client_sock);
        return;
    }
    
    // Try to open the file
    file = fopen(filename, "r");
    if (file == NULL) {
        strcpy(response, "ERROR: File not found");
        send(client_sock, response, strlen(response), 0);
        close(client_sock);
        return;
    }
    
    // Check file size
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size > MAX_FILE_SIZE) {
        sprintf(response, "ERROR: File size exceeds limit (%ld > %d bytes)", 
                file_size, MAX_FILE_SIZE);
        send(client_sock, response, strlen(response), 0);
        fclose(file);
        close(client_sock);
        return;
    }
    
    // Read and send file content
    memset(response, 0, sizeof(response));
    bytes_read = fread(response, 1, MAX_FILE_SIZE, file);
    
    if (bytes_read > 0) {
        response[bytes_read] = '\0';
        send(client_sock, response, bytes_read, 0);
        printf("File sent successfully (%d bytes)\n", bytes_read);
    } else {
        strcpy(response, "ERROR: Unable to read file");
        send(client_sock, response, strlen(response), 0);
    }
    
    fclose(file);
    close(client_sock);
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d...\n", PORT);
    
    // Accept and handle clients
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }
        
        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));
        handle_client(client_sock);
    }
    
    close(server_sock);
    return 0;
}
