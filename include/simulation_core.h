#ifndef SIMULATION_CORE_H
#define SIMULATION_CORE_H

#include <stdint.h>
#include "process.h"
#include "generator.h"

/*
 * ELI-03 -- Motor de simulacao.
 *
 * CONTEXTO: quando esta task foi originalmente planejada, a ideia era
 * um motor central por eventos que despachasse para os algoritmos.
 * Na pratica, FCFS (fcfs.c) e Round Robin (round_robin.c) ja foram
 * implementados cada um com seu PROPRIO loop de tempo discreto
 * completo e autocontido (cada um gerencia sua fila de prontos, sua
 * fila de E/S e seu proprio relogio local `current_time`). Reescrever
 * os dois para depender de um motor central e fora do escopo desta
 * task (seria reescrever ICR-06/ICR-07) e arriscaria introduzir bugs
 * nos dois algoritmos ja funcionando.
 *
 * Por isso, "motor de simulacao" aqui e implementado como o
 * ORQUESTRADOR DE EXPERIMENTOS: a peca que
 *   (a) gera a carga de trabalho (via generator.h/ICR-05) para um
 *       dado cenario e seed,
 *   (b) roda cada algoritmo disponivel sobre uma COPIA independente
 *       dessa mesma carga (Secao 2.3 de scenarios.md exige que a
 *       carga seja identica entre algoritmos -- copiar em vez de
 *       reusar o mesmo array evita que rodar FCFS "suje" o estado que
 *       o Round Robin leria em seguida),
 *   (c) confere explicitamente que o relogio de simulacao avancou de
 *       forma monotonica e que todo processo terminou (o criterio de
 *       aceitacao original desta task: "o motor avanca o tempo
 *       simulado corretamente"),
 *   (d) devolve metricas comparaveis (turnaround medio, trocas de
 *       contexto, indice de Jain do slowdown -- Secao 9 do enunciado)
 *       para cada algoritmo rodado.
 *
 * O executor de experimentos completo (100+ seeds x 4 cenarios x N
 * algoritmos, ICR-10) e uma camada acima disto -- este modulo roda UMA
 * combinacao (cenario, seed, process_count) por chamada; ICR-10 itera
 * sobre a grade de combinacoes chamando isto em loop.
 */

typedef enum {
    ALGORITMO_FCFS,
    ALGORITMO_ROUND_ROBIN
    /* ALGORITMO_PRIORITY e ALGORITMO_PROPRIO entram aqui quando
     * ELI-06 (Priority) e ICR-09 (algoritmo proprio) existirem —
     * ambos ja tem o formato de metricas abaixo compativel */
} TipoAlgoritmo;

typedef struct {
    TipoAlgoritmo algoritmo;
    const char *nome;                  /* para logs/CSV, ex. "fcfs" */

    int valido;                        /* 0 se a execucao falhou
                                           (parametro invalido, etc.) —
                                           checar antes de usar os
                                           campos abaixo */

    long total_trocas_contexto;
    long tempo_total_simulado;         /* makespan: valor final do
                                           relogio de simulacao */
    double turnaround_medio;
    double indice_jain_slowdown;
} ResultadoExecucao;

/*
 * Executa TODOS os algoritmos disponiveis (ver TipoAlgoritmo) sobre a
 * MESMA carga de trabalho (mesmo cenario + seed + process_count),
 * cada um sobre sua propria copia independente dos processos.
 *
 * `quantum_round_robin` e `custo_troca_contexto` sao os parametros de
 * configuracao exigidos por context_switch.md (custo > 0 nos
 * experimentos principais; ver Secao 6 do doc para custo = 0 como
 * analise complementar) e pelo Round Robin (RRConfig.quantum).
 *
 * FCFS: fcfs_run() nao aceita custo_troca_contexto como parametro nem
 * devolve metricas (ver fcfs.h) -- isso diverge do que
 * context_switch.md exige ("mesmo custo para todos os algoritmos",
 * "metricas obrigatorias"). Ate fcfs.c ser ajustado (fora do escopo
 * desta task -- e ICR-06), este motor roda fcfs_run() como esta (sem
 * custo de troca de contexto aplicado) e calcula as metricas
 * comparaveis a partir do estado final dos processos (completion_time,
 * context_switches), deixando isso registrado como divergencia
 * conhecida no campo `nome` do resultado ("fcfs (sem custo de troca
 * de contexto aplicado -- ver docs)").
 *
 * Preenche `out_resultados[i]` para i em [0, ALGORITMO_ROUND_ROBIN]
 * (ou seja, out_resultados deve ter capacidade para pelo menos 2
 * elementos hoje; TipoAlgoritmo cresce conforme mais algoritmos forem
 * integrados).
 *
 * Retorna 0 em sucesso, -1 se a geracao da carga falhar (parametros
 * invalidos ou falha de alocacao).
 */
int simulacao_executa_todos(ScenarioType cenario, uint64_t seed, int process_count,
                             int quantum_round_robin, int custo_troca_contexto,
                             ResultadoExecucao *out_resultados);

#endif /* SIMULATION_CORE_H */
