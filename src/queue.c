#include "queue.h"
#include "process.h" /* precisa do tipo completo Process para a Fachada A */
#include <stdlib.h>

/* ---------------------------------------------------------------------
 * Nucleo generico
 * --------------------------------------------------------------------- */

FilaGenerica *fila_generica_cria(int capacidade) {
    if (capacidade <= 0) capacidade = 1;
    FilaGenerica *f = (FilaGenerica *)malloc(sizeof(FilaGenerica));
    if (f == NULL) return NULL;

    f->itens = (void **)malloc(sizeof(void *) * (size_t)capacidade);
    if (f->itens == NULL) {
        free(f);
        return NULL;
    }
    f->capacidade = capacidade;
    f->inicio = 0;
    f->quantidade = 0;
    return f;
}

/* Dobra a capacidade interna quando a fila enche, em vez de forcar o
 * chamador a saber o tamanho maximo de antemao (a Fachada A, usada por
 * fcfs.c, e inicializada sem capacidade explicita -- queue_init(&q) --
 * entao precisa poder crescer). */
static int fila_generica_cresce(FilaGenerica *f) {
    int nova_capacidade = f->capacidade * 2;
    void **novo = (void **)malloc(sizeof(void *) * (size_t)nova_capacidade);
    if (novo == NULL) return -1;

    /* copia respeitando a ordem logica (FIFO), nao a posicao fisica no
     * array circular antigo */
    for (int i = 0; i < f->quantidade; i++) {
        novo[i] = f->itens[(f->inicio + i) % f->capacidade];
    }
    free(f->itens);
    f->itens = novo;
    f->capacidade = nova_capacidade;
    f->inicio = 0;
    return 0;
}

int fila_generica_insere(FilaGenerica *f, void *item) {
    if (f == NULL) return -1;
    if (f->quantidade == f->capacidade) {
        if (fila_generica_cresce(f) != 0) return -1;
    }
    int pos = (f->inicio + f->quantidade) % f->capacidade;
    f->itens[pos] = item;
    f->quantidade++;
    return 0;
}

void *fila_generica_remove(FilaGenerica *f) {
    if (f == NULL || f->quantidade == 0) return NULL;
    void *item = f->itens[f->inicio];
    f->inicio = (f->inicio + 1) % f->capacidade;
    f->quantidade--;
    return item;
}

int fila_generica_vazia(const FilaGenerica *f) {
    return (f == NULL) || (f->quantidade == 0);
}

void fila_generica_destroi(FilaGenerica *f) {
    if (f == NULL) return;
    free(f->itens);
    free(f);
}

/* ---------------------------------------------------------------------
 * Fachada A -- consumida por src/fcfs.c (nao alterado)
 * Queue por valor, contendo Process*.
 * --------------------------------------------------------------------- */

void queue_init(Queue *q) {
    if (q == NULL) return;
    /* capacidade inicial pequena e arbitraria; cresce sozinha
     * (fila_generica_cresce) se o cenario tiver mais processos
     * simultaneamente prontos do que isso -- ver nota no .h. */
    q->interna = fila_generica_cria(16);
}

void queue_enqueue(Queue *q, Process *p) {
    if (q == NULL || q->interna == NULL) return;
    fila_generica_insere(q->interna, (void *)p);
}

Process *queue_dequeue(Queue *q) {
    if (q == NULL || q->interna == NULL) return NULL;
    return (Process *)fila_generica_remove(q->interna);
}

int queue_is_empty(const Queue *q) {
    if (q == NULL) return 1;
    return fila_generica_vazia(q->interna);
}

void queue_destroy(Queue *q) {
    if (q == NULL) return;
    fila_generica_destroi(q->interna);
    q->interna = NULL;
}

/* ---------------------------------------------------------------------
 * Fachada B -- pensada para src/round_robin.c
 * Queue alocada no heap, contendo indices (int).
 *
 * void* nao guarda um int diretamente com seguranca portavel em todas
 * as plataformas (sizeof(void*) pode ser > sizeof(int), mas o inverso
 * teoricamente nao e garantido pelo padrao C, embora seja verdade em
 * todas as plataformas relevantes para este projeto). Para evitar
 * qualquer aliasing/UB, guardamos o indice num pequeno bloco alocado
 * em vez de fazer cast direto de int para void*.
 * --------------------------------------------------------------------- */

QueueIdx *queue_idx_create(int capacidade) {
    QueueIdx *q = (QueueIdx *)malloc(sizeof(QueueIdx));
    if (q == NULL) return NULL;
    q->interna = fila_generica_cria(capacidade);
    if (q->interna == NULL) {
        free(q);
        return NULL;
    }
    return q;
}

void queue_idx_enqueue(QueueIdx *q, int indice) {
    if (q == NULL || q->interna == NULL) return;
    int *caixa = (int *)malloc(sizeof(int));
    if (caixa == NULL) return; /* falha de alocacao: indice perdido --
                                   aceitavel apenas em condicao de
                                   memoria esgotada, fora do escopo de
                                   tratamento desta fila */
    *caixa = indice;
    fila_generica_insere(q->interna, caixa);
}

int queue_idx_dequeue(QueueIdx *q) {
    if (q == NULL || q->interna == NULL) return -1;
    int *caixa = (int *)fila_generica_remove(q->interna);
    if (caixa == NULL) return -1;
    int indice = *caixa;
    free(caixa);
    return indice;
}

int queue_idx_is_empty(const QueueIdx *q) {
    if (q == NULL) return 1;
    return fila_generica_vazia(q->interna);
}

void queue_idx_destroy(QueueIdx *q) {
    if (q == NULL) return;
    /* libera qualquer "caixa" de indice ainda pendente na fila, para
     * nao vazar memoria se queue_idx_destroy for chamado com itens
     * ainda dentro (nao deveria acontecer em uso normal, mas e
     * defensivo contra saida antecipada/erro). */
    while (!fila_generica_vazia(q->interna)) {
        free(fila_generica_remove(q->interna));
    }
    fila_generica_destroi(q->interna);
    free(q);
}
