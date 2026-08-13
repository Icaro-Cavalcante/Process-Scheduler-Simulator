#ifndef IO_QUEUE_H
#define IO_QUEUE_H

#include "process.h"

/*
 * ELI-04 -- Fila de bloqueio de E/S.
 *
 * Implementa exatamente a modelagem decidida em docs/io_modeling.md:
 *   - um unico dispositivo logico de E/S (Secao 4: "NO" paralelismo);
 *   - uma unica fila dedicada, FIFO, neutra a prioridade (Secao 5);
 *   - o tempo de bloqueio e exatamente a duracao da rajada de E/S
 *     (Secao 3), sem variabilidade adicional introduzida pelo motor;
 *   - o processo volta para a fila de prontos assim que essa duracao
 *     se esgota (Secao 6), sem tratamento especial ao retornar.
 *
 * Esta e uma fila de PRIORIDADE MINIMA por tempo de conclusao de E/S
 * (min-heap), nao uma fila FIFO de entrada -- a ordem de SAIDA e por
 * tempo de conclusao crescente, que e o que importa para o motor de
 * simulacao saber "quem desbloqueia primeiro". A ordem de ENTRADA no
 * dispositivo (Secao 5, "quem pede primeiro e servido primeiro") e
 * preservada porque, com um unico dispositivo sem paralelismo, o
 * tempo de conclusao de cada processo ja e calculado author em
 * ordem de chegada (ver io_queue_bloqueia()).
 *
 * Esta estrutura e uma extracao do min-heap que ja existia, duplicado
 * e "solto" dentro de src/round_robin.c (BlockedHeap/bh_*). Ela nao
 * substitui esse codigo interno (nao alterei round_robin.c alem da
 * troca de nomes de fila de prontos documentada em queue.h) -- fica
 * disponivel para fcfs.c (que hoje usa um array linear O(n) proprio,
 * ver next_io_completion() em fcfs.c) e para os algoritmos futuros
 * (Priority, algoritmo proprio) adotarem sem duplicar a logica de
 * novo. Unificar round_robin.c e fcfs.c para usar este modulo fica
 * como sugestao de revisao futura (ASH-05 / ICR), fora do escopo
 * desta task.
 */

typedef struct {
    int indice_processo;   /* indice no array de processos do cenario,
                               igual ao usado pela fila de prontos
                               (QueueIdx) -- ver queue.h */
    int tempo_conclusao;    /* instante (tempo absoluto de simulacao)
                               em que a rajada de E/S termina e o
                               processo volta para READY */
} EntradaBloqueada;

typedef struct {
    EntradaBloqueada *heap;
    int capacidade;
    int tamanho;
} FilaIO;

/* Aloca a fila de E/S. `capacidade` deve ser >= numero maximo de
 * processos que podem estar bloqueados simultaneamente -- process_count
 * e sempre um limite seguro, pelo mesmo raciocinio do BlockedHeap em
 * round_robin.c. Retorna NULL em falha de alocacao. */
FilaIO *io_queue_cria(int capacidade);

void io_queue_destroi(FilaIO *f);

int io_queue_vazia(const FilaIO *f);

/*
 * Move um processo para o estado BLOCKED e agenda seu retorno.
 *
 * `tempo_atual` e o instante em que a rajada de CPU anterior terminou
 * (Secao 2 de io_modeling.md: E/S so e solicitada exatamente quando a
 * rajada de CPU corrente acaba). `duracao_burst_io` vem de
 * processes[indice].bursts[...].duration da rajada de E/S atual
 * (Secao 3: tempo de bloqueio = duracao da rajada, sem variacao).
 *
 * Como ha um unico dispositivo sem paralelismo (Secao 4), esta funcao
 * NAO enfileira o processo atras de quem ja esta sendo servido -- o
 * motor de simulacao/algoritmo chamador e responsavel por so invocar
 * io_queue_bloqueia() no instante em que o processo de fato comeca a
 * ser servido pelo dispositivo. Ver simulation_core.c para o caso em
 * que multiplos processos pedem E/S no mesmo tick (fila de espera do
 * dispositivo, nao desta struct).
 *
 * Atualiza processes[indice].current_state para PROCESS_STATE_BLOCKED
 * como efeito colateral (para nao exigir que cada algoritmo repita
 * essa atribuicao).
 */
void io_queue_bloqueia(FilaIO *f, Process *processes, int indice,
                        int tempo_atual, int duracao_burst_io);

/* Retorna o menor tempo_conclusao presente na fila, sem remover.
 * Chamador deve checar io_queue_vazia() antes -- comportamento
 * indefinido (le heap[0] de uma fila vazia) se chamado sem checar. */
int io_queue_proximo_tempo(const FilaIO *f);

/*
 * Remove e retorna o indice do processo cujo tempo_conclusao e o
 * menor da fila (a rajada de E/S dele terminou). NAO decide sozinho
 * SE esse processo deve de fato desbloquear agora -- o chamador
 * compara io_queue_proximo_tempo(f) <= tempo_atual antes de chamar
 * esta funcao, exatamente como round_robin.c ja faz com bh_peek_tick/
 * bh_pop.
 *
 * Retorna -1 se a fila estiver vazia.
 */
int io_queue_desbloqueia_proximo(FilaIO *f);

#endif /* IO_QUEUE_H */
