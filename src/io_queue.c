#include "io_queue.h"
#include <stdlib.h>

/* Min-heap por tempo_conclusao, igual em estrutura ao BlockedHeap que ja
 * existia (duplicado) dentro de src/round_robin.c -- extraido aqui para
 * ser reaproveitavel por outros algoritmos sem copiar a implementacao de
 * novo (ver nota de escopo no header). */

FilaIO *io_queue_cria(int capacidade) {
    if (capacidade <= 0) capacidade = 1;
    FilaIO *f = (FilaIO *)malloc(sizeof(FilaIO));
    if (f == NULL) return NULL;

    f->heap = (EntradaBloqueada *)malloc(sizeof(EntradaBloqueada) * (size_t)capacidade);
    if (f->heap == NULL) {
        free(f);
        return NULL;
    }
    f->capacidade = capacidade;
    f->tamanho = 0;
    return f;
}

void io_queue_destroi(FilaIO *f) {
    if (f == NULL) return;
    free(f->heap);
    free(f);
}

int io_queue_vazia(const FilaIO *f) {
    return (f == NULL) || (f->tamanho == 0);
}

static void troca(EntradaBloqueada *a, EntradaBloqueada *b) {
    EntradaBloqueada t = *a;
    *a = *b;
    *b = t;
}

static void sobe(FilaIO *f, int i) {
    while (i > 0) {
        int pai = (i - 1) / 2;
        if (f->heap[pai].tempo_conclusao <= f->heap[i].tempo_conclusao) break;
        troca(&f->heap[pai], &f->heap[i]);
        i = pai;
    }
}

static void desce(FilaIO *f, int i) {
    for (;;) {
        int esquerda = 2 * i + 1;
        int direita = 2 * i + 2;
        int menor = i;

        if (esquerda < f->tamanho &&
            f->heap[esquerda].tempo_conclusao < f->heap[menor].tempo_conclusao) {
            menor = esquerda;
        }
        if (direita < f->tamanho &&
            f->heap[direita].tempo_conclusao < f->heap[menor].tempo_conclusao) {
            menor = direita;
        }
        if (menor == i) break;

        troca(&f->heap[i], &f->heap[menor]);
        i = menor;
    }
}

void io_queue_bloqueia(FilaIO *f, Process *processes, int indice,
                        int tempo_atual, int duracao_burst_io) {
    if (f == NULL) return;

    processes[indice].current_state = PROCESS_STATE_BLOCKED;

    if (f->tamanho == f->capacidade) {
        /* capacidade dimensionada errada por quem chamou (deveria ser
         * >= process_count, ver header) -- nao ha como crescer aqui
         * sem invalidar indices de quem ja segura ponteiros para o
         * array antigo, entao falha de forma visivel em vez de
         * silenciosamente perder o processo bloqueado. */
        abort();
    }

    int i = f->tamanho++;
    f->heap[i].indice_processo = indice;
    f->heap[i].tempo_conclusao = tempo_atual + duracao_burst_io;
    sobe(f, i);
}

int io_queue_proximo_tempo(const FilaIO *f) {
    return f->heap[0].tempo_conclusao;
}

int io_queue_desbloqueia_proximo(FilaIO *f) {
    if (io_queue_vazia(f)) return -1;

    int indice = f->heap[0].indice_processo;
    f->heap[0] = f->heap[--f->tamanho];
    if (f->tamanho > 0) {
        desce(f, 0);
    }
    return indice;
}
