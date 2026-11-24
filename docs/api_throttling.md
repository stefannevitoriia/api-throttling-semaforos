---

# ✅ **PROJETO 8 — API de Throttling (Controle de Admissão)**

---

## 🎯 **Objetivo**
Implementar um mecanismo de **throttling** usando **semáforos** para limitar quantas threads podem entrar simultaneamente em uma região crítica ou operação custosa.

Esse padrão é essencial para:

- Evitar esgotamento de recursos (memória, CPU, conexões).
- Controlar concorrência em APIs, servidores e pipelines.
- Garantir que apenas **K threads** possam executar uma operação ao mesmo tempo.

---

# 🧱 **Arquitetura do Sistema**

### **Componentes**
1. **Semáforo global (`sem_t`)**
   - Inicializado com o número máximo de threads permitidas.
   - Cada thread faz `sem_wait()` antes de entrar.
   - Ao sair, faz `sem_post()`.

2. **Threads trabalhadoras**
   - Simulam requisições a uma API.
   - Antes de entrar na região crítica → `sem_wait()`.
   - Após terminar → `sem_post()`.

3. **Região crítica**
   - Operação custosa (ex.: compressão, cálculo pesado, I/O).
   - Limitada pelo semáforo.

---

# 📊 **Diagrama de Arquitetura (ASCII)**

```
                   +-----------------------------+
                   |      Semáforo Global        |
                   |   valor inicial = MAX_IN    |
                   +-----------------------------+
                       ^       ^        ^
                       |       |        |
                thread A   thread B   thread C
                sem_wait   sem_wait   sem_wait
                       \      |      /
                        \     |     /
                         v    v    v
                   +-----------------------------+
                   |     Região Crítica          |
                   | (apenas MAX_IN threads)     |
                   +-----------------------------+
```

---

# 🧩 **Fluxo de Execução**

1. Inicializar semáforo com valor `MAX_IN`.
2. Criar várias threads simulando requisições.
3. Cada thread:
   - Chama `sem_wait()` → se não houver vaga, bloqueia.
   - Executa operação custosa.
   - Chama `sem_post()` liberando a vaga.

---

# 🧪 **Código Inicial em C (Skeleton)**
*(POSIX threads + semáforos)*

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define THREADS 10
#define MAX_IN 3   // máximo de threads simultâneas

sem_t throttle;

void *worker(void *arg) {
    int id = (int)(long)arg;

    printf("[Thread %d] Aguardando permissão...\n", id);
    sem_wait(&throttle);  // controle de admissão

    printf("[Thread %d] Entrou na região crítica.\n", id);
    usleep(300000); // simula operação custosa

    printf("[Thread %d] Saindo da região crítica.\n", id);
    sem_post(&throttle);

    return NULL;
}

int main() {
    pthread_t threads[THREADS];

    sem_init(&throttle, 0, MAX_IN);

    for (int i = 0; i < THREADS; i++)
        pthread_create(&threads[i], NULL, worker, (void *)(long)i);

    for (int i = 0; i < THREADS; i++)
        pthread_join(threads[i], NULL);

    sem_destroy(&throttle);
    return 0;
}
```

---

# 🛠️ **Roteiro Completo de Implementação**

### **1. Definir limite de concorrência**
- `MAX_IN` determina quantas threads podem entrar simultaneamente.
- Ex.: 3 threads por vez.

### **2. Inicializar semáforo**
- `sem_init(&throttle, 0, MAX_IN)`
- O valor inicial define a capacidade.

### **3. Criar threads**
- Cada thread simula uma requisição.
- Antes de entrar na região crítica → `sem_wait()`.

### **4. Implementar região crítica**
- Pode ser qualquer operação custosa:
  - Compressão
  - Acesso a banco
  - Leitura de arquivo
  - Cálculo pesado

### **5. Liberar permissão**
- Após terminar → `sem_post()`.

### **6. Testar**
- Aumentar número de threads
- Diminuir `MAX_IN`
- Verificar que nunca entram mais threads do que permitido

### **7. Melhorias opcionais**
- Timeout com `sem_timedwait()`
- Métricas de uso (quantas threads bloqueadas)
- Throttling adaptativo (ajusta MAX_IN dinamicamente)
- Logs detalhados

---
