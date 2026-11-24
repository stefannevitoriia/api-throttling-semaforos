#include "throttling.h"
#include <stdio.h>


//  Inicializa o semáforo com o valor máximo de permissões
//  @param t    Ponteiro para o Throttler
//  @param max  Número máximo de threads simultâneas
 
void throttler_init(Throttler *t, int max) {
    t->max_concurrent = max;
    
    // Inicializa semáforo com valor = max
    // Parâmetros: semáforo, compartilhado entre processos? (0 = apenas threads), valor inicial
    sem_init(&t->semaphore, 0, max);
    
    printf("[Throttler] Inicializado com MAX_IN = %d\n", max);
}


//  Entrada na região crítica - bloqueia até conseguir permissão 
//  @param t    Ponteiro para o Throttler
 
void throttler_wait(Throttler *t) {
    // sem_wait() diminui o contador do semáforo
    // Se o contador for 0, a thread BLOQUEIA até que outro thread faça sem_post()
    sem_wait(&t->semaphore);
}


//  Saída da região crítica - libera uma permissão
//  @param t    Ponteiro para o Throttler

void throttler_post(Throttler *t) {
    // sem_post() aumenta o contador do semáforo
    // Se houver threads bloqueadas, uma delas será desbloqueada
    sem_post(&t->semaphore);
}


//  Entrada na região crítica com TIMEOUT
  
//  @param t             Ponteiro para o Throttler
//  @param timeout_ms    Tempo máximo de espera em milissegundos
//  @return              0 se conseguiu, -1 se timeout ocorreu

int throttler_timedwait(Throttler *t, int timeout_ms) {
    struct timespec ts;
    
    // Obtém o tempo atual
    clock_gettime(CLOCK_REALTIME, &ts);
    
    // Converte o timeout de milissegundos para segundos + nanosegundos
    ts.tv_sec += timeout_ms / 1000;                    // segundos inteiros
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;      // resto em nanosegundos
    
    // Ajusta se nanosegundos ultrapassaram 1 bilhão
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    // Tenta obter o semáforo com timeout
    int result = sem_timedwait(&t->semaphore, &ts);
    
    // Verifica se ocorreu timeout
    if (result == -1 && errno == ETIMEDOUT) {
        printf("[Throttler] ⏰ TIMEOUT! Thread desistiu após %d ms\n", timeout_ms);
        return -1;  // Timeout: não conseguiu permissão
    }
    
    // Conseguiu permissão dentro do tempo
    return 0;
}


//  Destroi o semáforo e libera recursos  
//  @param t    Ponteiro para o Throttler

void throttler_destroy(Throttler *t) {
    sem_destroy(&t->semaphore);
    printf("[Throttler] Destruído e recursos liberados\n");
}

void throttler_increase_limit(Throttler *t) {
    // Incrementa o contador lógico do Throttler
    t->max_concurrent++;
    // Abre a vaga física no semáforo
    sem_post(&t->semaphore);
}
