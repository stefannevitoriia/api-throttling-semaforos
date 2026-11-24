/*
 * PROJETO 8 — API de Throttling (Controle de Admissão)
 * Uso:     ./api_throttling [num_threads] [max_in] [work_ms]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <time.h>
#include <string.h>

 
#include "throttling.h"  // Biblioteca que o Rafael criou
#include "metricas.h"    // Biblioteca de telemetria que a Manu criou

#define DEFAULT_THREADS  10
#define DEFAULT_MAX_IN   3
#define DEFAULT_WORK_MS  300
#define TIMEOUT_MS       800  // Limite de tempo para o Throttler (Módulo 5 - Ex 1)

#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define CYAN    "\033[36m"
#define RED     "\033[31m"

#define DOT_RED    RED    "●" RESET


//Controladores de fluxo adaptativo e sincronização básica de threads.

// Substituído o sem_t pela estrutura Throttler do Rafael
static Throttler        simulador_throttler; 

static pthread_mutex_t  log_mutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static volatile int     g_max_in;           
static atomic_int       g_in_region;        /* Threads dentro da região crítica */
static atomic_int       g_waiting;          /* Threads bloqueadas na fila */
static atomic_int       g_done;             /* Threads que concluíram o ciclo */
static int              g_work_ms;          

/* Helper de log visual simplificado */
static void log_line(const char *dot, pthread_t tid, const char *msg, int concurrency) {
    unsigned long short_id = (unsigned long)tid % 1000000UL;
    pthread_mutex_lock(&log_mutex);
    if (concurrency > 0)
        printf("[Thread %06lu] %s %s (Concorrência Interna: %d/%d)\n",
               short_id, dot, msg, concurrency, simulador_throttler.max_concurrent);
    else
        printf("[Thread %06lu] %s %s\n", short_id, dot, msg);
    fflush(stdout);
    pthread_mutex_unlock(&log_mutex);
}


 //Fluxo de execução das requisições simuladas.
 
static void *worker(void *arg) {
    (void)arg;
    pthread_t self = pthread_self();

    // Dispara o cronômetro de alta precisão em metricas.c
    double tempo_inicio = obter_tempo_segundos();

    atomic_fetch_add(&g_waiting, 1);
    
    // Chama a função do código da Manu que exibe o log amarelo estruturado
    registrar_inicio_fila(self);

    // Substituído sem_wait pelo timedwait modular do código do Rafael
    int resultado = throttler_timedwait(&simulador_throttler, TIMEOUT_MS);

    atomic_fetch_sub(&g_waiting, 1);

    if (resultado == -1) {
        // Avisa o código da Manu para computar o Timeout- Exercicio 1
        registrar_timeout_fila(self);
        atomic_fetch_add(&g_done, 1);
        return NULL; 
    }

    // Registra o sucesso e calcula o tempo total de fila no código da Manu
    registrar_admissao(self, tempo_inicio);
    
    int now_in = atomic_fetch_add(&g_in_region, 1) + 1;

    /* Região crítica — Operação simulada */
    usleep((useconds_t)g_work_ms * 1000);

    /* Sair da região crítica */
    int after_out = atomic_fetch_sub(&g_in_region, 1) - 1;
    log_line(DOT_RED, self, "SAIU da Região Crítica.", after_out < 0 ? 0 : after_out);

    // Substituído sem_post pela liberação de token do Rafael
    throttler_post(&simulador_throttler);
    
    atomic_fetch_add(&g_done, 1);
    return NULL;
}

/* Lógica para ajustar as regras do semáforo sob estresse.*/
static void *adaptive_controller(void *arg) {
    int total = *(int *)arg;
    while (atomic_load(&g_done) < total) {
        usleep(400000); /* Verifica a carga a cada 400 ms */

        int w   = atomic_load(&g_waiting);
        int old = simulador_throttler.max_concurrent;

        pthread_mutex_lock(&stats_mutex);

        // Se houver muita fila, expande o limite de concorrência
        // Invoca a função modular do Rafael para atualizar o limite lógico e liberar o semáforo físico
        if (w > simulador_throttler.max_concurrent * 2 && simulador_throttler.max_concurrent < 8) {
            throttler_increase_limit(&simulador_throttler); 

            pthread_mutex_lock(&log_mutex);
            printf(CYAN "[Adaptive] MAX_IN ajustado: %d → %d (fila=%d)\n" RESET, 
                old, simulador_throttler.max_concurrent, w);
            fflush(stdout);
            pthread_mutex_unlock(&log_mutex);  

        // Se a fila esvaziar, ela reduz as vagas concorrentes para preservar hardware
        } else if (w == 0 && simulador_throttler.max_concurrent > 1) {
            simulador_throttler.max_concurrent--;
            
            // Consome uma vaga do semáforo para sincronizar o hardware com o novo limite menor
            sem_trywait(&simulador_throttler.semaphore);

            pthread_mutex_lock(&log_mutex);
            printf(CYAN "[Adaptive] MAX_IN ajustado: %d → %d (fila vazia)\n" RESET, 
                   old, simulador_throttler.max_concurrent);
            fflush(stdout);
            pthread_mutex_unlock(&log_mutex);
        }

        pthread_mutex_unlock(&stats_mutex);
    }
    return NULL;
}


static void print_header(int n_threads, int max_in, int work_ms) {
    printf(BOLD "╔══════════════════════════════════════════════╗\n");
    printf(     "║   PROJETO 8 — API Throttling Integrada       ║\n");
    printf(     "╠══════════════════════════════════════════════╣\n");
    printf(     "║  threads : %-4d                              ║\n", n_threads);
    printf(     "║  MAX_IN  : %-4d  (Limite Inicial)            ║\n", max_in);
    printf(     "║  work    : %-4d ms por operação              ║\n", work_ms);
    printf(     "║  Controle: ADAPTATIVO ATIVADO                ║\n");
    printf(     "╚══════════════════════════════════════════════╝\n\n" RESET);
    fflush(stdout);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * MAIN 
 * ───────────────────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    int n_threads = DEFAULT_THREADS;
    int max_in    = DEFAULT_MAX_IN;
    int work_ms   = DEFAULT_WORK_MS;

    if (argc > 1) n_threads = atoi(argv[1]);
    if (argc > 2) max_in    = atoi(argv[2]);
    if (argc > 3) work_ms   = atoi(argv[3]);

    if (n_threads < 1 || n_threads > 64) n_threads = DEFAULT_THREADS;
    if (max_in    < 1 || max_in    > 16) max_in    = DEFAULT_MAX_IN;
    if (work_ms   < 50)                  work_ms   = DEFAULT_WORK_MS;

    g_max_in = max_in;
    g_work_ms = work_ms;
    atomic_store(&g_in_region, 0);
    atomic_store(&g_waiting,   0);
    atomic_store(&g_done,      0);

    //Inicializa o componente do Rafael com o limite de partida
    throttler_init(&simulador_throttler, max_in);

    print_header(n_threads, max_in, work_ms);

    // Dispara o cronômetro geral que medirá o Throughput global no código da Manu
    double tempo_inicio_geral = obter_tempo_segundos();

    /* Criar thread do controlador adaptativo da Lívia */
    pthread_t ctrl_tid;
    pthread_create(&ctrl_tid, NULL, adaptive_controller, &n_threads);

    /* Criar as threads trabalhadoras baseadas no laço estrutural da Lívia */
    pthread_t *threads = malloc((size_t)n_threads * sizeof(pthread_t));
    if (!threads) { perror("malloc"); return EXIT_FAILURE; }

    for (int i = 0; i < n_threads; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
        usleep(40000); // Ritmo de chegada ajustado pela Lívia
    }

    /* Sincronização e encerramento */
    for (int i = 0; i < n_threads; i++)
        pthread_join(threads[i], NULL);

    pthread_join(ctrl_tid, NULL);

    // Fim da execução. Calcula o tempo absoluto do  programa no código da Manu
    double tempo_total = obter_tempo_segundos() - tempo_inicio_geral;
    
    // Organiza os dados e imprime o relatório estatístico- final do código de metricas
    exibir_relatorio_p4(n_threads, tempo_total);

    // Destrói o semáforo interno e encerra os recursos do código do Rafael
    throttler_destroy(&simulador_throttler);
    free(threads);

    return EXIT_SUCCESS;
}