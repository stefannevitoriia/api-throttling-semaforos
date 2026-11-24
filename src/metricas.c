#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "metricas.h"


 //Variáveis de contabilidade estatística exigidas no Exercício 3 do Módulo 5.

static pthread_mutex_t  metrics_mutex = PTHREAD_MUTEX_INITIALIZER;
static int              total_bloqueios = 0;       // Quantas threads de fato esperaram na fila
static int              total_sucessos = 0;        // Quantas requisições processaram com êxito
static int              total_timeouts = 0;        // Quantas requisições desistiram por estourar o tempo
static double           tempo_total_espera = 0;    // Somatório de tempo em fila para cálculo da média

/* FUNÇÕES DE MEDIÇÃO DE ALTA PRECISÃO
 * Captura o tempo do sistema com granularidade de nanossegundos (CLOCK_MONOTONIC). */
double obter_tempo_segundos(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Evento disparado quando a thread trabalhadora anuncia que vai entrar na fila
void registrar_inicio_fila(pthread_t tid) {
    unsigned long short_id = (unsigned long)tid % 1000000UL;
    
    // Integração de fluxo visual e estrutural de logs coloridos do código da Lívia
    printf("[Thread %06lu] \033[33m" "●" "\033[0m Aguardando vaga no Throttler...\n", short_id);
    fflush(stdout);
}

// Evento disparado quando a thread consegue passar pelo semáforo com sucesso
void registrar_admissao(pthread_t tid, double tempo_inicio) {
    unsigned long short_id = (unsigned long)tid % 1000000UL;
    double tempo_fim = obter_tempo_segundos();
    double tempo_gasto = tempo_fim - tempo_inicio;

    pthread_mutex_lock(&metrics_mutex);
    
    // Acumula o tempo para o cálculo posterior
    tempo_total_espera += tempo_gasto;
    total_sucessos++;

    // Se a thread esperou mais de 1 milisegundo, conta como bloqueio efetivo
    if (tempo_gasto > 0.001) {
        total_bloqueios++;
    }


    // Throttler é Adaptativo, a concorrência limite varia dinamicamente.
    printf("[Thread %06lu] \033[32m" "●" "\033[0m ENTROU na Região Crítica.\n", short_id);
    fflush(stdout);
    
    pthread_mutex_unlock(&metrics_mutex);
}

// Evento disparado se a função do Rafael estourar o limite de tempo estipulado
void registrar_timeout_fila(pthread_t tid) {
    unsigned long short_id = (unsigned long)tid % 1000000UL;

    pthread_mutex_lock(&metrics_mutex);
    
    //Captura o retorno de erro (-1) gerado pela lógica de timeout do Rafael
    total_timeouts++;
    
    // Log de alerta indicando a desistência da requisição por Timeout (Exigência do Exercício 1)
    printf("[Thread %06lu] TIMEOUT! Thread desistiu após estourar limite de espera.\n", short_id);
    fflush(stdout);
    
    pthread_mutex_unlock(&metrics_mutex);
}


 // EXIBIÇÃO DE PERFORMANCE 
 //Gera as métricas de Throughput e Latência solicitadas na revisão do Módulo 5.
 
void exibir_relatorio_p4(int total_threads, double tempo_execucao) {
    int local_sucessos;
    int local_timeouts;
    int local_bloqueios;
    double local_tempo_espera;

    // Bloqueia o mutex rapidamente para copiar os valores globais com segurança
    pthread_mutex_lock(&metrics_mutex);
    local_sucessos     = total_sucessos;
    local_timeouts     = total_timeouts;
    local_bloqueios    = total_bloqueios;
    local_tempo_espera = tempo_total_espera;
    pthread_mutex_unlock(&metrics_mutex);


    printf("\n==================================================\n");
    printf("             RELATÓRIO DE PERFORMANCE               \n");
    printf("==================================================\n");
    printf("Total de Requisições Avaliadas  : %d\n", total_threads);
    printf("Requisições com Sucesso          : %d\n", total_sucessos);
    printf("Requisições com Timeout (Falhas) : %d\n", total_timeouts);
    printf("Número Total de Bloqueios na Fila: %d\n", total_bloqueios);
    
    int total_tentativas = total_sucessos + total_timeouts;
    if (total_tentativas > 0) {
        // CÁLCULO DE LATÊNCIA: Média aritmética do tempo que as requisições aguardaram
        double espera_media = tempo_total_espera / total_tentativas;
        printf("Tempo Médio de Espera na Fila    : %.4f segundos\n", espera_media);
    }
    
    if (tempo_execucao > 0) {
        // CÁLCULO DE VAZÃO: Throughput real medindo requisições processadas por segundo absoluto
        double throughput = total_sucessos / tempo_execucao;
        printf("Throughput Real do Sistema       : %.2f req/seg\n", throughput);
    }
    printf("==================================================\n\n");
    fflush(stdout);
}