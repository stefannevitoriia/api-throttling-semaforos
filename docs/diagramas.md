# 📄 **PROJETO 8 — API de Throttling (Controle de Admissão com Semáforos)**
### Diagramas em ASCII / Texto

---

## **1. Arquitetura do Throttler**

```
+---------------------------+
|        Throttler          |
+---------------------------+
| semaphore(max_requests)   |
| timeout                   |
+---------------------------+
```

---

## **2. Fluxo de Requisição**

```
request():
    sem_wait()
    processa
    sem_post()
```

---
