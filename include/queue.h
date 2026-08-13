#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>

/*
 * ELI-04 -- Fila de prontos (ready queue).
 *
 * PROBLEMA ENCONTRADO NO REPOSITORIO ao implementar esta task: os dois
 * algoritmos classicos ja escritos esperam DUAS interfaces de fila
 * incompativeis entre si, e nenhuma delas tinha sido implementada:
 *
 *   - src/fcfs.c        usa Queue POR VALOR (Queue q; queue_init(&q);)
 *                        guardando ponteiros Process* diretamente,
 *                        via queue_enqueue(&q, &processes[i]) e
 *                        queue_dequeue(&q) retornando Process*.
 *
 *   - src/round_robin.c usa Queue* ALOCADA NO HEAP
 *                        (Queue *rq = queue_create(process_count);)
 *                        guardando INDICES (int) dentro do array de
 *                        processos, via queue_enqueue(rq, indice) e
 *                        queue_dequeue(rq) retornando int.
 *
 * Nenhum dos dois .c existentes foi alterado (nao e escopo desta
 * task mexer em ICR-06/ICR-07). Em vez disso, este modulo implementa
 * UM nucleo generico de fila FIFO de ponteiros (void*), e expoe DUAS
 * fachadas por cima dele -- uma para cada interface ja assumida pelo
 * codigo existente. As duas fachadas sao trivialmente conversiveis
 * porque "indice do processo no array" e "ponteiro para o elemento do
 * array" carregam a mesma informacao (given o array base).
 *
 * Este conflito de interface deve ser levado ao conhecimento de
 * ICR (dono de round_robin.c) e de quem escreveu fcfs.c, para decidir
 * se vale padronizar as duas em uma interface so no futuro -- por ora,
 * as duas fachadas abaixo desbloqueiam a compilacao de ambos sem
 * reescrever nenhum dos dois algoritmos.
 */

/* ---------------------------------------------------------------------
 * Nucleo generico: fila FIFO circular de ponteiros void*.
 * Usado internamente pelas duas fachadas abaixo. Pode tambem ser usado
 * diretamente por quem preferir, ex.: para a fila de bloqueados de E/S
 * (ver io_queue.h), mantendo uma unica implementacao de fila no
 * projeto todo.
 * --------------------------------------------------------------------- */
typedef struct {
    void **itens;
    int capacidade;
    int inicio;   /* indice do proximo a sair */
    int quantidade; /* quantos elementos ha atualmente na fila */
} FilaGenerica;

/* Aloca uma fila com a capacidade dada (deve ser >= numero maximo de
 * elementos que estarao na fila simultaneamente -- process_count e
 * sempre um limite seguro, ja que nenhum processo entra na fila mais
 * de uma vez ao mesmo tempo). Retorna NULL em falha de alocacao. */
FilaGenerica *fila_generica_cria(int capacidade);

/* Insere um ponteiro no fim da fila. Retorna 0 em sucesso, -1 se a
 * fila estiver cheia (nao deveria acontecer se capacidade foi
 * dimensionada corretamente -- indica um bug de outro modulo, nao
 * desta fila). */
int fila_generica_insere(FilaGenerica *f, void *item);

/* Remove e retorna o item do inicio da fila (FIFO). Retorna NULL se a
 * fila estiver vazia -- checar fila_generica_vazia() antes se isso
 * importar para o chamador. */
void *fila_generica_remove(FilaGenerica *f);

int fila_generica_vazia(const FilaGenerica *f);

void fila_generica_destroi(FilaGenerica *f);

/* ---------------------------------------------------------------------
 * Fachada A -- interface esperada por src/fcfs.c
 * Queue por valor, contendo ponteiros Process*.
 * --------------------------------------------------------------------- */

/* Process e uma struct ANONIMA (typedef struct { ... } Process em
 * process.h, sem tag), entao nao existe "struct Process" para
 * forward-declarar -- 'struct Process' seria um tipo distinto e
 * incompativel do Process de verdade aos olhos do compilador C. Por
 * isso este header inclui process.h diretamente, em vez de tentar
 * evitar a dependencia com forward declaration. */
#include "process.h"

typedef struct {
    FilaGenerica *interna;
} Queue;

/* Inicializa uma Queue ja alocada pelo chamador (por valor, na stack
 * ou como campo de outra struct), como fcfs.c espera:
 *   Queue ready_queue; queue_init(&ready_queue);
 * Capacidade interna cresce dinamicamente se necessario (ver .c) para
 * nao exigir que o chamador saiba o tamanho maximo de antemao. */
void queue_init(Queue *q);

void queue_enqueue(Queue *q, Process *p);

Process *queue_dequeue(Queue *q);

int queue_is_empty(const Queue *q);

void queue_destroy(Queue *q);

/* ---------------------------------------------------------------------
 * Fachada B -- interface esperada por src/round_robin.c
 * Queue alocada no heap, contendo indices (int) no array de processos.
 * NOTA DE NOMES: round_robin.c usa exatamente os mesmos nomes de
 * funcao da Fachada A (queue_enqueue, queue_dequeue, queue_is_empty,
 * queue_destroy), mas com assinaturas diferentes (int em vez de
 * Process*, e Queue* em vez de Queue). Isso so compila nos dois
 * arquivos porque cada .c inclui esta mesma queue.h e o C nao faz
 * overload de funcao -- ou seja, ESTE HEADER NAO PODE, HOJE, DECLARAR
 * queue_enqueue COM DUAS ASSINATURAS DIFERENTES AO MESMO TEMPO.
 *
 * Solucao adotada: a Fachada B usa nomes prefixados
 * (queue_idx_create/queue_idx_enqueue/...) e round_robin.c precisara
 * de UM ajuste minimo (renomear as chamadas que ja faz para os nomes
 * abaixo) antes de compilar. Isso e o menor ajuste possivel dado o
 * conflito -- ver nota completa no topo deste arquivo e no relatorio
 * de entrega desta task.
 * --------------------------------------------------------------------- */
typedef struct {
    FilaGenerica *interna;
} QueueIdx;

QueueIdx *queue_idx_create(int capacidade);

void queue_idx_enqueue(QueueIdx *q, int indice);

/* Retorna o indice removido, ou -1 se a fila estiver vazia (chamar
 * apenas apos checar queue_idx_is_empty(), como round_robin.c ja faz). */
int queue_idx_dequeue(QueueIdx *q);

int queue_idx_is_empty(const QueueIdx *q);

void queue_idx_destroy(QueueIdx *q);

#endif /* QUEUE_H */
