# 🚦 API de Throttling com Semáforos POSIX

Projeto da disciplina de **Sistemas Operacionais** (UFV, Campus Rio Paranaíba): controle de admissão (*throttling*) em C, usando **threads e semáforos POSIX** para limitar quantas requisições executam ao mesmo tempo em uma região crítica.

## 🎯 O que o projeto faz

- Implementa um **`Throttler`** (módulo reutilizável) baseado em `sem_t`, com espera bloqueante, espera com **timeout** (`sem_timedwait`) e ajuste do limite em tempo de execução.
- **Simulação com threads** (`api_throttling`): várias threads competem por `MAX_IN` vagas; um **controlador adaptativo** aumenta ou reduz o limite conforme o tamanho da fila.
- **Servidor HTTP mínimo** em C puro, com sockets POSIX (`http_throttling`): cada conexão vira uma thread e só entra no processamento se conseguir uma vaga a tempo. Quem espera mais de 800 ms recebe **HTTP 503**.
- **Telemetria** (`metricas.c`): tempo de fila, admissões, timeouts, vazão e relatório final.
- **Suíte de testes automatizados** (`testar.sh`): requisição simples, concorrência com fila, timeout e 503, alta carga com 50 requisições simultâneas e comparativo de `MAX_IN`.

## 📁 Estrutura

```
├── include/
│   ├── throttling.h      # interface do Throttler
│   └── metricas.h        # interface das métricas
├── src/
│   ├── throttling.c      # Throttler: init, wait, timedwait, post, destroy
│   ├── metricas.c        # telemetria de fila, admissão e timeout
│   ├── main.c            # simulação com threads + controlador adaptativo
│   └── http_throttling.c # servidor HTTP com throttling
├── docs/
│   ├── api_throttling.md # especificação e arquitetura
│   └── diagramas.md      # diagramas em texto
├── testar.sh             # testes automatizados
└── Makefile
```

## ⚙️ Como compilar e executar

Pré-requisitos: Linux (ou WSL no Windows), `gcc` e `make`.

```bash
make
```

**Simulação com threads:**
```bash
./api_throttling [num_threads] [max_in] [work_ms]
# exemplo: ./api_throttling 10 3 300
```

**Servidor HTTP:**
```bash
./http_throttling [porta] [max_in] [work_ms]
# padrão: porta=8080, max_in=3, work_ms=300

curl http://localhost:8080/api/processar
```

## 🧪 Testes

Com o projeto compilado (`make`), rode no Linux/WSL (precisa de `curl` e `bc`):

```bash
bash testar.sh
```

## 🛠️ Tecnologias

C · pthreads · semáforos POSIX (`sem_t`, `sem_timedwait`) · sockets POSIX · Bash

## 👩‍💻 Autoria

Projeto desenvolvido em grupo por:

- **Stefanne** — [@stefannevitoriia](https://github.com/stefannevitoriia)
- **Anne**
- **Lívia** — [@liviabs](https://github.com/liviabs)
- **Rafael**
- **Manuela**
