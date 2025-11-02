#ifndef FILTRO_H
#define FILTRO_H

#include <stddef.h>
#include <stdint.h>

/*
 * filtro.h — Interface do filtro (CIC + FIR) para o ESP
 *
 * Este header define uma API mínima para:
 *   - criar/inicializar o filtro com parâmetros (R, N, M) e coeficientes FIR;
 *   - processar blocos de entrada em PALAVRAS de 32 bits com PDM (MSB-first);
 *   - obter amostras já decimadas após CIC->FIR em ponto flutuante (float);
 *   - destruir/liberar recursos do filtro.
 */

/* Opaque type do filtro */
typedef struct filtro_s filtro_t;

/*
 * filtro_create
 *  Cria e inicializa o filtro.
 *
 *  Parâmetros:
 *    R        -> fator de decimação do CIC
 *    N        -> número de estágios do CIC
 *    M        -> atraso do comb (normalmente 1)
 *    fir      -> ponteiro para os coeficientes do FIR (float)
 *    fir_len  -> número de coeficientes FIR
 *
 *  Retorno:
 *    Ponteiro válido para filtro_t em caso de sucesso, ou NULL em caso de erro.
 */
filtro_t* filtro_create(int R, int N, int M,
                        const float *fir, size_t fir_len);

/*
 * filtro_process_words
 *  Processa um bloco de palavras de 32 bits contendo bits PDM (MSB-first),
 *  aplicando CIC (com decimação) seguido de FIR.
 *
 *  Parâmetros:
 *    f         -> ponteiro retornado por filtro_create
 *    in_words  -> vetor de uint32_t; cada word contém 32 amostras PDM (bits)
 *    n_words   -> quantidade de words no bloco
 *    out       -> buffer de saída (float) para receber as amostras filtradas
 *    n_out     -> [out] recebe a quantidade de amostras geradas neste chamado
 *
 *  Observações:
 *    - A função acumula estado interno; pode ser chamada em blocos consecutivos.
 *    - O tamanho máximo necessário de 'out' depende de n_words e do fator R.
 *      Aproximação: out_capacity >= floor((n_words*32)/R) + margem.
 *
 *  Retorno:
 *    0 em sucesso; !=0 em erro.
 */
int filtro_process_words(filtro_t *f,
                         const uint32_t *in_words, size_t n_words,
                         float *out, size_t *n_out);

/*
 * filtro_destroy
 *  Libera recursos associados ao filtro criado com filtro_create.
 */
void filtro_destroy(filtro_t *f);

#endif /* FILTRO_H */
