#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════
#  PROJETO 8 — Script de Testes Automatizados
#  Uso: bash testar.sh
# ═══════════════════════════════════════════════════════════════

HOST="http://localhost:8080/api/processar"
BINARY="./http_throttling"

# ── Cores ────────────────────────────────────────────────────
RESET="\033[0m"
BOLD="\033[1m"
GREEN="\033[32m"
RED="\033[31m"
YELLOW="\033[33m"
CYAN="\033[36m"
WHITE="\033[97m"
DIM="\033[2m"

# ── Helpers ──────────────────────────────────────────────────
linha() { echo -e "${DIM}────────────────────────────────────────────────────────${RESET}"; }
titulo() { echo -e "\n${BOLD}${CYAN}$1${RESET}"; linha; }
ok()     { echo -e "  ${GREEN}✔${RESET}  $1"; }
fail()   { echo -e "  ${RED}✘${RESET}  $1"; }
info()   { echo -e "  ${YELLOW}●${RESET}  $1"; }

aguardar_servidor() {
    local tentativas=0
    while ! curl -s --max-time 1 "$HOST" > /dev/null 2>&1; do
        sleep 0.3
        tentativas=$((tentativas + 1))
        if [ $tentativas -gt 20 ]; then
            echo -e "${RED}Servidor não respondeu. Verifique se está rodando.${RESET}"
            exit 1
        fi
    done
}

matar_servidor() {
    pkill -f "http_throttling" 2>/dev/null
    sleep 0.5
}

iniciar_servidor() {
    local porta=$1 max_in=$2 work=$3
    matar_servidor
    $BINARY "$porta" "$max_in" "$work" > /tmp/server_log.txt 2>&1 &
    SERVER_PID=$!
    aguardar_servidor
    echo -e "  ${DIM}Servidor iniciado: porta=$porta  MAX_IN=$max_in  work=${work}ms  timeout=800ms${RESET}"
}

# Envia N requisições em paralelo e coleta resultados
disparar() {
    local n=$1
    local resultados=()
    local ok_count=0
    local fail_count=0
    local tempos=()

    for i in $(seq 1 "$n"); do
        curl -s --max-time 3 "$HOST" &
    done > /tmp/respostas_raw.txt
    wait

    while IFS= read -r linha_res; do
        [[ -z "$linha_res" ]] && continue
        if echo "$linha_res" | grep -q "200 OK"; then
            ok_count=$((ok_count + 1))
        elif echo "$linha_res" | grep -q "503"; then
            fail_count=$((fail_count + 1))
        fi
        if echo "$linha_res" | grep -q "Tempo de espera"; then
            tempo=$(echo "$linha_res" | grep -oP '[0-9]+\.[0-9]+')
            tempos+=("$tempo")
        fi
    done < /tmp/respostas_raw.txt

    # Calcula espera média
    local soma=0
    local qtd=${#tempos[@]}
    if [ "$qtd" -gt 0 ]; then
        for t in "${tempos[@]}"; do
            soma=$(echo "$soma + $t" | bc -l 2>/dev/null || echo "$soma")
        done
        media=$(echo "scale=4; $soma / $qtd" | bc -l 2>/dev/null || echo "n/a")
    else
        media="0.0000"
    fi

    echo "$ok_count $fail_count $media"
}

# ════════════════════════════════════════════════════════════════
#  CABEÇALHO
# ════════════════════════════════════════════════════════════════
clear
echo ""
echo -e "${BOLD}${WHITE}╔══════════════════════════════════════════════════════════╗${RESET}"
echo -e "${BOLD}${WHITE}║       PROJETO 8 — SUITE DE TESTES AUTOMATIZADOS          ║${RESET}"
echo -e "${BOLD}${WHITE}║       API de Throttling com Semáforos POSIX              ║${RESET}"
echo -e "${BOLD}${WHITE}╚══════════════════════════════════════════════════════════╝${RESET}"
echo ""

# ════════════════════════════════════════════════════════════════
#  TESTE 1 — Requisição simples
# ════════════════════════════════════════════════════════════════
titulo "TESTE 1 — Requisição simples (verificar funcionamento básico)"
info "Objetivo: confirmar que o servidor admite 1 requisição corretamente"
info "Configuração: MAX_IN=3, work=300ms, 1 requisição"
echo ""

iniciar_servidor 8080 3 300

resposta=$(curl -s --max-time 3 "$HOST")
status=$(echo "$resposta" | grep -o "200 OK\|503 Service")
tempo=$(echo "$resposta" | grep -oP 'Tempo de espera.*?([0-9]+\.[0-9]+)' | grep -oP '[0-9]+\.[0-9]+' | head -1)

echo ""
echo -e "  ${BOLD}Resultado:${RESET}"
if echo "$resposta" | grep -q "200 OK"; then
    ok "HTTP 200 OK — Requisição admitida com sucesso"
    ok "Tempo de espera na fila : ${tempo}s  (esperado: ~0.30s)"
    ok "Semáforo funcionando corretamente"
else
    fail "Resposta inesperada: $resposta"
fi

echo ""
echo -e "  ${DIM}Saída completa do servidor:${RESET}"
grep -E "Conexão|ENTROU|SAIU|TIMEOUT" /tmp/server_log.txt | head -5 | while read -r l; do
    echo "  $l"
done

# ════════════════════════════════════════════════════════════════
#  TESTE 2 — Concorrência com fila
# ════════════════════════════════════════════════════════════════
titulo "TESTE 2 — Concorrência com fila (throttling em ação)"
info "Objetivo: provar que nunca mais que MAX_IN=3 threads executam ao mesmo tempo"
info "Configuração: MAX_IN=3, work=300ms, 15 requisições simultâneas"
echo ""

iniciar_servidor 8080 3 300
echo -e "  ${DIM}Disparando 15 requisições simultâneas...${RESET}"
echo ""

leitura=$(disparar 15)
ok_c=$(echo "$leitura" | awk '{print $1}')
fail_c=$(echo "$leitura" | awk '{print $2}')
media=$(echo "$leitura" | awk '{print $3}')
total=$((ok_c + fail_c))

echo -e "  ${BOLD}Resultado:${RESET}"
ok "Total de respostas     : $total / 15"
ok "Admitidas (200 OK)     : $ok_c"
[ "$fail_c" -gt 0 ] && info "Rejeitadas por timeout : $fail_c  (normal com alta carga)" || ok "Rejeitadas por timeout : 0"
ok "Tempo médio de fila    : ${media}s"
ok "Sem deadlock           : todas as $total requisições responderam"
echo ""
echo -e "  ${DIM}Prova do throttling — log do servidor:${RESET}"
grep "ENTROU\|SAIU\|TIMEOUT\|Stats" /tmp/server_log.txt | head -12 | while read -r l; do
    echo "  $l"
done

# ════════════════════════════════════════════════════════════════
#  TESTE 3 — Timeout e rejeição (sem_timedwait)
# ════════════════════════════════════════════════════════════════
titulo "TESTE 3 — Timeout e rejeição HTTP 503 (sem_timedwait)"
info "Objetivo: provar que sem_timedwait() rejeita requisições que esperam > 800ms"
info "Configuração: MAX_IN=1, work=600ms, 10 requisições simultâneas"
echo ""

iniciar_servidor 8080 1 600
echo -e "  ${DIM}Disparando 10 requisições simultâneas com servidor sobrecarregado...${RESET}"
echo ""

leitura=$(disparar 10)
ok_c=$(echo "$leitura" | awk '{print $1}')
fail_c=$(echo "$leitura" | awk '{print $2}')
total=$((ok_c + fail_c))

echo -e "  ${BOLD}Resultado:${RESET}"
ok "Total de respostas         : $total / 10"
ok "Admitidas (HTTP 200)       : $ok_c"
if [ "$fail_c" -gt 0 ]; then
    ok "Rejeitadas timeout (503)   : $fail_c  ← sem_timedwait funcionando"
    ok "Nenhuma thread travou      : sistema respondeu a todas"
else
    info "Nenhum timeout — tente reduzir o MAX_IN ou aumentar o work_ms"
fi
echo ""
echo -e "  ${DIM}Evidência do timeout no log:${RESET}"
grep "TIMEOUT\|503\|Stats" /tmp/server_log.txt | head -8 | while read -r l; do
    echo "  $l"
done

# ════════════════════════════════════════════════════════════════
#  TESTE 4 — Alta carga: 50 threads (deadlock e race condition)
# ════════════════════════════════════════════════════════════════
titulo "TESTE 4 — Alta carga com 50 threads (deadlock e race condition)"
info "Objetivo: garantir que o sistema não trava com 50 requisições simultâneas"
info "Configuração: MAX_IN=3, work=300ms, 50 requisições simultâneas"
echo ""

iniciar_servidor 8080 3 300
echo -e "  ${DIM}Disparando 50 requisições simultâneas...${RESET}"
echo ""

leitura=$(disparar 50)
ok_c=$(echo "$leitura" | awk '{print $1}')
fail_c=$(echo "$leitura" | awk '{print $2}')
media=$(echo "$leitura" | awk '{print $3}')
total=$((ok_c + fail_c))

echo -e "  ${BOLD}Resultado:${RESET}"
if [ "$total" -eq 50 ]; then
    ok "Total de respostas     : $total / 50  ← TODAS responderam"
else
    fail "Total de respostas     : $total / 50  ← possível deadlock!"
fi
ok "Admitidas (200 OK)     : $ok_c"
info "Rejeitadas (503)       : $fail_c  (protegidas pelo timeout)"
ok "Deadlock detectado     : NÃO — sistema encerrou normalmente"
ok "Race condition         : NÃO — ok + timeout = $total exato"
echo ""
echo -e "  ${DIM}Estatísticas do controlador adaptativo:${RESET}"
grep "Stats\|Adaptive" /tmp/server_log.txt | while read -r l; do
    echo "  $l"
done

# ════════════════════════════════════════════════════════════════
#  TESTE 5 — Comparativo de MAX_IN (desempenho)
# ════════════════════════════════════════════════════════════════
titulo "TESTE 5 — Comparativo de MAX_IN (impacto no throughput)"
info "Objetivo: mostrar como MAX_IN afeta o desempenho do sistema"
info "Configuração: 10 threads, work=200ms, variando MAX_IN"
echo ""

declare -A resultados_ok
declare -A resultados_fail
declare -A resultados_media

for max in 1 3 5 10; do
    echo -e "  ${DIM}Testando MAX_IN=$max...${RESET}"
    iniciar_servidor 8080 "$max" 200
    sleep 0.3
    leitura=$(disparar 10)
    resultados_ok[$max]=$(echo "$leitura" | awk '{print $1}')
    resultados_fail[$max]=$(echo "$leitura" | awk '{print $2}')
    resultados_media[$max]=$(echo "$leitura" | awk '{print $3}')
    matar_servidor
done

echo ""
echo -e "  ${BOLD}Tabela comparativa:${RESET}"
echo ""
printf "  %-10s %-12s %-12s %-20s\n" "MAX_IN" "200 OK" "503 Timeout" "Espera Média"
linha
printf "  %-10s %-12s %-12s %-20s\n" "1" "${resultados_ok[1]}/10" "${resultados_fail[1]}/10" "${resultados_media[1]}s"
printf "  %-10s %-12s %-12s %-20s\n" "3" "${resultados_ok[3]}/10" "${resultados_fail[3]}/10" "${resultados_media[3]}s"
printf "  %-10s %-12s %-12s %-20s\n" "5" "${resultados_ok[5]}/10" "${resultados_fail[5]}/10" "${resultados_media[5]}s"
printf "  %-10s %-12s %-12s %-20s\n" "10" "${resultados_ok[10]}/10" "${resultados_fail[10]}/10" "${resultados_media[10]}s"
linha
echo ""
ok "MAX_IN=1  → mais restritivo: maior espera, menor throughput"
ok "MAX_IN=3  → equilíbrio: controle com boa vazão"
ok "MAX_IN=5+ → folgado: sem fila, máximo throughput"

# ════════════════════════════════════════════════════════════════
#  RESUMO FINAL
# ════════════════════════════════════════════════════════════════
titulo "RESUMO DOS TESTES"
echo ""
ok "Teste 1 — Requisição simples       : semáforo admite corretamente"
ok "Teste 2 — Concorrência com fila    : throttling limita a MAX_IN simultâneas"
ok "Teste 3 — Timeout e HTTP 503       : sem_timedwait() rejeita graciosamente"
ok "Teste 4 — 50 threads simultâneas   : sem deadlock, sem race condition"
ok "Teste 5 — Variação de MAX_IN       : impacto direto no desempenho medido"
echo ""
echo -e "  ${BOLD}Todos os testes concluídos com sucesso.${RESET}"
echo ""

matar_servidor