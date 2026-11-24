#ifndef THROTTLING_H
#define THROTTLING_H

#include <semaphore.h>
#include <time.h>
#include <errno.h>


//  Estrutura que representa o controlador de throttling
//  Contém o semáforo e a configuração máxima

typedef struct {
    sem_t semaphore;        // Semáforo para controle de admissão
    int max_concurrent;     // Número máximo de threads simultâneas
} Throttler;


//  Inicializa o throttler com um limite máximo de threads simultâneas
//  @param t        Ponteiro para o Throttler
//  @param max      Número máximo de threads que podem entrar simultaneamente

void throttler_init(Throttler *t, int max);


//  Aguarda permissão para entrar na região crítica (bloqueante)
//  Se não houver vaga, a thread fica bloqueada até conseguir
//  @param t        Ponteiro para o Throttler

void throttler_wait(Throttler *t);


// Libera a permissão após sair da região crítica
// @param t        Ponteiro para o Throttler

void throttler_post(Throttler *t);


//  Aguarda permissão com TIMEOUT (não bloqueante por tempo limitado)
//  @param t             Ponteiro para o Throttler
//  @param timeout_ms    Tempo máximo de espera em milissegundos
//  @return              0 se conseguiu a permissão, -1 se ocorreu timeout

int throttler_timedwait(Throttler *t, int timeout_ms);


//  Destroi o throttler e libera recursos do semáforo
//  @param t        Ponteiro para o Throttler

void throttler_destroy(Throttler *t);

// Alenta o limite máximo concorrente
void throttler_increase_limit(Throttler *t);

#endif