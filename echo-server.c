#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX_CLIENTS 1024
#define BUFSZ 4096

static int clients[MAX_CLIENTS];
static int client_count = 0;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static int opt_echo = 0;
static int opt_bcast = 0;

static void add_client(int sock) {
    pthread_mutex_lock(&mtx);
    if (client_count < MAX_CLIENTS) {
        clients[client_count++] = sock;
    }
    pthread_mutex_unlock(&mtx);
}

static void remove_client(int sock) {
    pthread_mutex_lock(&mtx);
    for (int i = 0; i < client_count; ++i) {
        if (clients[i] == sock) {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&mtx);
}

static void broadcast_msg(const char* buf, ssize_t len) {
    pthread_mutex_lock(&mtx);
    for (int i = 0; i < client_count; ++i) {
        ssize_t n = send(clients[i], buf, len, 0);
        (void)n; // ignore partial for simplicity; can loop if needed
    }
    pthread_mutex_unlock(&mtx);
}

typedef struct { int sock; struct sockaddr_in addr; } client_arg_t;

static void* client_thread(void* arg) {
    client_arg_t* carg = (client_arg_t*)arg;
    int sock = carg->sock;

    char addrbuf[64];
    inet_ntop(AF_INET, &carg->addr.sin_addr, addrbuf, sizeof(addrbuf));
    uint16_t port = ntohs(carg->addr.sin_port);
    free(carg);

    char buf[BUFSZ];
    for (;;) {
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break; // client closed or error
        // 로그 출력
        fwrite(buf, 1, n, stdout);
        if (buf[n-1] != '\n') fputc('\n', stdout);
        fflush(stdout);

        // 옵션 처리
        if (opt_echo) {
            send(sock, buf, n, 0);
        }
        if (opt_bcast) {
            broadcast_msg(buf, n);
        }
    }

    close(sock);
    remove_client(sock);
    fprintf(stderr, "[INFO] client %s:%u disconnected\n", addrbuf, port);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "syntax : echo-server <port> [-e[-b]]\n");
        fprintf(stderr, "sample : echo-server 1234 -e -b\n");
        return 1;
    }

    int port = atoi(argv[1]);
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-e") == 0) opt_echo = 1;
        else if (strcmp(argv[i], "-b") == 0) opt_bcast = 1;
        else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    int serv = socket(AF_INET, SOCK_STREAM, 0);
    if (serv < 0) { perror("socket"); return 1; }

    int yes = 1; setsockopt(serv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in saddr = {0};
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(port);

    if (bind(serv, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) { perror("bind"); return 1; }
    if (listen(serv, 128) < 0) { perror("listen"); return 1; }

    fprintf(stderr, "[INFO] echo-server listening on port %d (echo=%d, bcast=%d)\n", port, opt_echo, opt_bcast);

    for (;;) {
        struct sockaddr_in caddr; socklen_t clen = sizeof(caddr);
        int csock = accept(serv, (struct sockaddr*)&caddr, &clen);
        if (csock < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        add_client(csock);

        client_arg_t* carg = (client_arg_t*)malloc(sizeof(client_arg_t));
        carg->sock = csock; carg->addr = caddr;
        pthread_t tid; pthread_create(&tid, NULL, client_thread, carg);
        pthread_detach(tid);
    }

    close(serv);
    return 0;
}