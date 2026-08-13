#include "simulation_core.h"
#include "fcfs.h"
#include "round_robin.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ---------------------------------------------------------------------
 * Metricas (mesma formula usada internamente por round_robin.c
 * compute_metrics() -- reproduzida aqui para poder calcular as
 * mesmas metricas para FCFS, que nao as devolve sozinho -- ver nota
 * de divergencia conhecida no header). Mantida como copia
 * deliberada, nao como dependencia de round_robin.c, para nao
 * acoplar este modulo a um simbolo interno (static) de outro .c.
 * --------------------------------------------------------------------- */

static double tempo_ideal_processo(const Process *p) {
    double total = 0.0;
    for (int i = 0; i < p->num_bursts; i++) {
        total += p->bursts[i].duration;
    }
    return total;
}

static void calcula_metricas(const Process *processes, int process_count,
                              long total_trocas_contexto, long tempo_total_simulado,
                              ResultadoExecucao *out) {
    double soma_turnaround = 0.0;
    double *slowdown = (double *)malloc(sizeof(double) * (size_t)process_count);

    for (int i = 0; i < process_count; i++) {
        double turnaround = (double)(processes[i].completion_time - processes[i].arrival_time);
        soma_turnaround += turnaround;

        double ideal = tempo_ideal_processo(&processes[i]);
        slowdown[i] = (ideal > 0.0) ? (turnaround / ideal) : 1.0;
    }

    double soma_sd = 0.0, soma_sd_sq = 0.0;
    for (int i = 0; i < process_count; i++) {
        soma_sd += slowdown[i];
        soma_sd_sq += slowdown[i] * slowdown[i];
    }
    double jain = (soma_sd_sq > 0.0)
        ? (soma_sd * soma_sd) / ((double)process_count * soma_sd_sq) * 100.0
        : 100.0;

    out->total_trocas_contexto = total_trocas_contexto;
    out->tempo_total_simulado = tempo_total_simulado;
    out->turnaround_medio = soma_turnaround / (double)process_count;
    out->indice_jain_slowdown = jain;

    free(slowdown);
}

/*
 * Confere o criterio de aceitacao original desta task ("o motor
 * avanca o tempo simulado corretamente"): todo processo terminou
 * (nenhum ficou preso em NEW/READY/RUNNING/BLOCKED) e nenhum
 * completion_time e anterior ao respectivo arrival_time (o que
 * indicaria relogio andando para tras ou processo "terminando antes
 * de comecar" -- ambos sinais de um bug no motor/algoritmo).
 *
 * Aborta com assert em build de debug se a checagem falhar -- isto e
 * deliberado: um motor que "avanca o tempo errado" nao deve devolver
 * metricas silenciosamente incorretas para o resto do pipeline
 * (scripts de plot, artigo). Em build de release (NDEBUG), a funcao
 * ainda retorna 0/1 para o chamador decidir o que fazer.
 */
static int valida_avanco_de_tempo(const Process *processes, int process_count) {
    int ok = 1;
    for (int i = 0; i < process_count; i++) {
        if (processes[i].current_state != PROCESS_STATE_TERMINATED) {
            ok = 0;
        }
        if (processes[i].completion_time < processes[i].arrival_time) {
            ok = 0;
        }
    }
    assert(ok && "simulacao_executa_todos: relogio nao avancou corretamente "
                 "(processo nao terminado, ou completion_time < arrival_time)");
    return ok;
}

/* ---------------------------------------------------------------------
 * FCFS
 * --------------------------------------------------------------------- */

static void executa_fcfs(const Process *carga_original, int process_count,
                          ResultadoExecucao *out) {
    Process *copia = (Process *)calloc((size_t)process_count, sizeof(Process));
    if (copia == NULL) {
        out->valido = 0;
        return;
    }

    for (int i = 0; i < process_count; i++) {
        process_init(&copia[i], carga_original[i].pid, carga_original[i].arrival_time,
                     carga_original[i].priority, carga_original[i].num_bursts);
        copia[i].io_request_count = carga_original[i].io_request_count;
        for (int b = 0; b < carga_original[i].num_bursts; b++) {
            copia[i].bursts[b] = carga_original[i].bursts[b];
        }
    }

    fcfs_run(copia, process_count);

    int tempo_final = 0;
    long trocas_totais = 0;
    for (int i = 0; i < process_count; i++) {
        if (copia[i].completion_time > tempo_final) tempo_final = copia[i].completion_time;
        trocas_totais += copia[i].context_switches;
    }

    int avanco_ok = valida_avanco_de_tempo(copia, process_count);

    out->algoritmo = ALGORITMO_FCFS;
    out->nome = "fcfs (sem custo de troca de contexto aplicado -- ver docs)";
    out->valido = avanco_ok;
    calcula_metricas(copia, process_count, trocas_totais, tempo_final, out);

    generator_free_workload(copia, process_count);
}

/* ---------------------------------------------------------------------
 * Round Robin
 * --------------------------------------------------------------------- */

static void executa_round_robin(const Process *carga_original, int process_count,
                                 int quantum, int custo_troca_contexto,
                                 ResultadoExecucao *out) {
    Process *copia = (Process *)calloc((size_t)process_count, sizeof(Process));
    if (copia == NULL) {
        out->valido = 0;
        return;
    }

    for (int i = 0; i < process_count; i++) {
        process_init(&copia[i], carga_original[i].pid, carga_original[i].arrival_time,
                     carga_original[i].priority, carga_original[i].num_bursts);
        copia[i].io_request_count = carga_original[i].io_request_count;
        for (int b = 0; b < carga_original[i].num_bursts; b++) {
            copia[i].bursts[b] = carga_original[i].bursts[b];
        }
    }

    RRConfig config;
    config.quantum = quantum;
    config.context_switch_cost = custo_troca_contexto;

    RRMetrics metricas_rr;
    int status = round_robin_run(copia, process_count, config, NULL, 0, NULL, &metricas_rr);

    out->algoritmo = ALGORITMO_ROUND_ROBIN;
    out->nome = "round_robin";

    if (status != 0) {
        out->valido = 0;
        generator_free_workload(copia, process_count);
        return;
    }

    int avanco_ok = valida_avanco_de_tempo(copia, process_count);

    out->valido = avanco_ok;
    out->total_trocas_contexto = metricas_rr.total_context_switches;
    out->tempo_total_simulado = metricas_rr.total_ticks;
    out->turnaround_medio = metricas_rr.mean_turnaround;
    out->indice_jain_slowdown = metricas_rr.jain_slowdown_percent;

    generator_free_workload(copia, process_count);
}

/* ---------------------------------------------------------------------
 * Orquestrador
 * --------------------------------------------------------------------- */

int simulacao_executa_todos(ScenarioType cenario, uint64_t seed, int process_count,
                             int quantum_round_robin, int custo_troca_contexto,
                             ResultadoExecucao *out_resultados) {
    if (out_resultados == NULL || process_count <= 0) return -1;

    Process *carga = generate_workload(cenario, seed, process_count);
    if (carga == NULL) return -1;

    executa_fcfs(carga, process_count, &out_resultados[ALGORITMO_FCFS]);
    executa_round_robin(carga, process_count, quantum_round_robin,
                        custo_troca_contexto, &out_resultados[ALGORITMO_ROUND_ROBIN]);

    generator_free_workload(carga, process_count);
    return 0;
}
