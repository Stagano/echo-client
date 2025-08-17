#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFSZ 4096

typedef struct { int sock; } rx_arg_t;

static void* rx_thread(void* arg) {
    rx_arg_t* a = (rx_arg_t*)arg;
    char buf[BUFSZ];
    for (;;) {
        ssize_t n = recv(a->sock, buf, sizeof(buf), 0);
        if (n <= 0) break; // server closed
        fwrite(buf, 1, n, stdout);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "syntax : echo-client <ip> <port>\n");
        fprintf(stderr, "sample : echo-client 192.168.10.2 1234\n");
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET; addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) { perror("inet_pton"); return 1; }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("connect"); return 1; }

    rx_arg_t a = { .sock = sock };
    pthread_t tid; pthread_create(&tid, NULL, rx_thread, &a);

    char line[BUFSZ];
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (send(sock, line, len, 0) < 0) { perror("send"); break; }
    }

    shutdown(sock, SHUT_WR); // notify EOF to server
    pthread_join(tid, NULL);
    close(sock);
    return 0;
}