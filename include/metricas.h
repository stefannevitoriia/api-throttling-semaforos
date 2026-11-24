#ifndef METRICAS_H
#define METRICAS_H


 //Funções para auditoria de latência, vazão e controle de bloqueios.
 

double obter_tempo_segundos(void);
void registrar_inicio_fila(pthread_t tid);
void registrar_admissao(pthread_t tid, double tempo_inicio);
void registrar_timeout_fila(pthread_t tid);
void exibir_relatorio_p4(int total_threads, double tempo_execucao);


#endif // METRICAS_H