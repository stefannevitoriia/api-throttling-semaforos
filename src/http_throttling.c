/*
 * PROJETO 8 — Exercício 4: API HTTP Simples com Throttling
 * Pessoa 5 — Testes e Documentação
 *
 * Servidor HTTP mínimo em C puro (POSIX sockets) que integra o
 * throttler do Projeto 8 para limitar requisições simultâneas.
 *
 * Uso: ./http_throttling [porta] [max_in] [work_ms]
 *      Padrão: porta=8080, max_in=3, work_ms=300
 *
 * Testar com: curl http://localhost:8080/api/processar
 *             ou: ab -n 20 -c 10 http://localhost:8080/api/processar
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "throttling.h"
#include "metricas.h"

/* ── Configurações padrão ─────────────────────────────── */
#define DEFAULT_PORT     8080
#define DEFAULT_MAX_IN   3
#define DEFAULT_WORK_MS  300
#define TIMEOUT_MS       800
#define BACKLOG          64

/* ── Estado global ────────────────────────────────────── */
static Throttler  g_throttler;
static int        g_work_ms;
static long       g_req_total   = 0;
static long       g_req_ok      = 0;
static long       g_req_timeout = 0;
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Argumento passado para cada thread de conexão ─────── */
typedef struct {
    int  client_fd;
    int  req_id;
} ConnArg;

/* ─────────────────────────────────────────────────────────
 * Envia resposta HTTP simples ao cliente
 * ───────────────────────────────────────────────────────── */
static void send_response(int fd, int status_code,
                          const char *status_text,
                          const char *body)
{
    char header[512];
    int  body_len = (int)strlen(body);

    snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, body_len);

    write(fd, header, strlen(header));
    write(fd, body,   (size_t)body_len);
}

/* ─────────────────────────────────────────────────────────
 * Thread que lida com cada conexão HTTP
 * ───────────────────────────────────────────────────────── */
static void *handle_connection(void *arg)
{
    ConnArg *ca = (ConnArg *)arg;
    int      fd = ca->client_fd;
    int      id = ca->req_id;
    free(ca);

    /* Lê (e ignora) os headers HTTP */
    char buf[2048];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) { close(fd); return NULL; }
    buf[n] = '\0';

    double t_inicio = obter_tempo_segundos();
    registrar_inicio_fila((pthread_t)(long)id);

    /* ── Throttling: tenta entrar na região crítica ──── */
    int resultado = throttler_timedwait(&g_throttler, TIMEOUT_MS);

    if (resultado == -1) {
        /* Rejeitado por timeout */
        registrar_timeout_fila((pthread_t)(long)id);

        char body[256];
        snprintf(body, sizeof(body),
            "503 Service Unavailable\n"
            "Requisição #%d rejeitada: servidor sobrecarregado (timeout=%d ms).\n",
            id, TIMEOUT_MS);
        send_response(fd, 503, "Service Unavailable", body);

        pthread_mutex_lock(&g_stats_mutex);
        g_req_timeout++;
        pthread_mutex_unlock(&g_stats_mutex);
    } else {
        /* Admitido — simula trabalho */
        registrar_admissao((pthread_t)(long)id, t_inicio);
        usleep((useconds_t)g_work_ms * 1000);

        double t_total = obter_tempo_segundos() - t_inicio;

        char body[512];
        snprintf(body, sizeof(body),
            "200 OK\n"
            "Requisição #%d processada com sucesso.\n"
            "Tempo de espera na fila : %.4f s\n"
            "Tempo de processamento  : %d ms\n",
            id, t_total, g_work_ms);
        send_response(fd, 200, "OK", body);

        throttler_post(&g_throttler);

        pthread_mutex_lock(&g_stats_mutex);
        g_req_ok++;
        pthread_mutex_unlock(&g_stats_mutex);
    }

    close(fd);

    pthread_mutex_lock(&g_stats_mutex);
    long done = ++g_req_total;
    pthread_mutex_unlock(&g_stats_mutex);

    /* Exibe estatísticas a cada 5 requisições */
    if (done % 5 == 0) {
        printf("\n[Stats] Total=%ld  OK=%ld  Timeout=%ld  MAX_IN=%d\n\n",
               done, g_req_ok, g_req_timeout,
               g_throttler.max_concurrent);
        fflush(stdout);
    }

    return NULL;
}

/* ─────────────────────────────────────────────────────────
 * MAIN
 * ───────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    int port    = DEFAULT_PORT;
    int max_in  = DEFAULT_MAX_IN;
    g_work_ms   = DEFAULT_WORK_MS;

    if (argc > 1) port    = atoi(argv[1]);
    if (argc > 2) max_in  = atoi(argv[2]);
    if (argc > 3) g_work_ms = atoi(argv[3]);

    throttler_init(&g_throttler, max_in);

    /* ── Cria socket TCP ──────────────────────────────── */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return EXIT_FAILURE; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons((uint16_t)port)
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return EXIT_FAILURE;
    }
    listen(server_fd, BACKLOG);

    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  PROJETO 8 — Servidor HTTP com Throttling    ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║  Porta   : %-4d                              ║\n", port);
    printf("║  MAX_IN  : %-4d  (concorrência máxima)       ║\n", max_in);
    printf("║  work_ms : %-4d ms por requisição            ║\n", g_work_ms);
    printf("║  Timeout : %-4d ms (sem_timedwait)           ║\n", TIMEOUT_MS);
    printf("╚══════════════════════════════════════════════╝\n");
    printf("Aguardando conexões em http://0.0.0.0:%d ...\n\n", port);
    fflush(stdout);

    int req_id = 0;

    /* ── Loop de aceitação de conexões ──────────────────── */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t          client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("[HTTP] Conexão #%d de %s\n",
               ++req_id, inet_ntoa(client_addr.sin_addr));
        fflush(stdout);

        ConnArg *ca = malloc(sizeof(ConnArg));
        ca->client_fd = client_fd;
        ca->req_id    = req_id;

        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&tid, &attr, handle_connection, ca);
        pthread_attr_destroy(&attr);
    }

    close(server_fd);
    throttler_destroy(&g_throttler);
    return EXIT_SUCCESS;
}