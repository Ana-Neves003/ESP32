#include "filtro.h"
#include <stdlib.h>
#include <string.h>

/*
 *  - Entrada: palavras de 32 bits com bits PDM (MSB-first), mapeados para {-1,+1}
 *  - CIC: N estágios, decimação R, atraso M (tipicamente 1)
 *  - FIR: coeficientes em float, processamento em ponto flutuante
 *  - Saída: amostras float (CIC->FIR) na taxa decimada (fs/R)
 *
 * Observações:
 *  - A implementação é stateful: integradores/comb e histórico do FIR persistem entre chamadas.
 *  - O ganho do CIC não é normalizado (igual ao Python). Se desejar, adicione escala externa.
 */

// ======= Estado interno =======

typedef struct {
    // --- CIC ---
    int R, N, M;          // decimação, nº de estágios, atraso do comb
    int64_t *integ;       // N integradores (acumuladores)
    int64_t **comb_z;     // N filas de atrasos (tamanho M) para os combs
    int *comb_idx;        // índices circulares dos combs
    int decim_phase;      // contador de fase para decimação (0..R-1)

    // --- FIR ---
    float  *fir;          // coeficientes FIR [0..L-1]
    size_t fir_len;       // L
    float  *fir_hist;     // histórico (ring buffer) com L-1 amostras anteriores
    size_t hist_len;      // L-1
    size_t hist_wr;       // índice de escrita no ring buffer

    // --- Buffer temporário para saída do CIC ---
    int64_t *tmp_cic;     // armazena saídas do CIC deste bloco
    size_t   tmp_cic_cap; // capacidade alocada em elementos
} filtro_state_t;

struct filtro_s {
    filtro_state_t st;
};

// ======= Helpers =======

// Desempacota 32 bits (MSB-first) para 32 amostras PDM em {-1, +1}
static inline void unpack_u32_to_pm1(int32_t *dst, uint32_t w) {
    // bit 31 -> primeiro sample
    
    for (int i = 31; i >= 0; --i) {
        int b = (int)((w >> i) & 1U);
        *dst++ = b ? 1 : -1;
    }
    /*   
    for (int i = 0; i < 32; ++i) {
        int b = (w >> i) & 1U;   // LSB-first
        *dst++ = b ? 1 : -1;
    }
    */
}

// Garante capacidade do buffer temporário (saída do CIC)
static int ensure_tmp_cic(filtro_state_t *s, size_t need) {
    if (need <= s->tmp_cic_cap) return 0;
    size_t newcap = (need + 63U) & ~((size_t)63U); // arredonda pra múltiplo de 64
    int64_t *p = (int64_t*)realloc(s->tmp_cic, newcap * sizeof(int64_t));
    if (!p) return -1;
    s->tmp_cic = p;
    s->tmp_cic_cap = newcap;
    return 0;
}

// ======= API =======
 filtro_t* filtro_create(int R, int N, int M, const float *fir, size_t fir_len)
{
    if (R <= 0 || N <= 0 || M <= 0 || !fir || fir_len == 0) return NULL;

    filtro_t *f = (filtro_t*)calloc(1, sizeof(*f));
    if (!f) return NULL;

    filtro_state_t *s = &f->st;
    s->R = R; s->N = N; s->M = M;

    // Integradores
    s->integ = (int64_t*)calloc((size_t)N, sizeof(int64_t));
    if (!s->integ) { filtro_destroy(f); return NULL; }

    // Combs
    s->comb_z  = (int64_t**)calloc((size_t)N, sizeof(int64_t*));
    s->comb_idx = (int*)calloc((size_t)N, sizeof(int));
    if (!s->comb_z || !s->comb_idx) { filtro_destroy(f); return NULL; }
    for (int stg = 0; stg < N; ++stg) {
        s->comb_z[stg] = (int64_t*)calloc((size_t)M, sizeof(int64_t));
        if (!s->comb_z[stg]) { filtro_destroy(f); return NULL; }
    }

    // Alinha a primeira saída com o Python (primeira amostra útil)
    s->decim_phase = R - 1;

    // FIR
    s->fir = (float*)malloc(fir_len * sizeof(float));
    if (!s->fir) { filtro_destroy(f); return NULL; }
    memcpy(s->fir, fir, fir_len * sizeof(float));
    s->fir_len = fir_len;

    if (fir_len > 1) {
        s->hist_len = fir_len - 1;
        s->fir_hist = (float*)calloc(s->hist_len, sizeof(float));
        if (!s->fir_hist) { filtro_destroy(f); return NULL; }
        s->hist_wr = 0;
    } else {
        s->hist_len = 0;
        s->fir_hist = NULL;
        s->hist_wr = 0;
    }

    // Buffer temporário para saídas do CIC
    s->tmp_cic = NULL;
    s->tmp_cic_cap = 0;

    return f;
}

int filtro_process_words(filtro_t *f,
                         const uint32_t *in_words, size_t n_words,
                         float *out, size_t *n_out)
{
    if (!f || !in_words || !out || !n_out) return -1;

    filtro_state_t *s = &f->st;

    // Previsão de saídas do CIC neste bloco
    size_t max_out_cic = ((n_words * 32U) + (size_t)s->decim_phase) / (size_t)s->R + 8U;
    if (ensure_tmp_cic(s, max_out_cic) != 0) return -2;

    size_t w_cic = 0;          // quantas saídas CIC geradas
    int32_t pdm32[32];

    // ====== CIC: Integradores + Decimação + Combs ======
    for (size_t w = 0; w < n_words; ++w) {
        unpack_u32_to_pm1(pdm32, in_words[w]);

        for (int i = 0; i < 32; ++i) {
            // Integradores
            int64_t x = (int64_t)pdm32[i];
            for (int stg = 0; stg < s->N; ++stg) {
                s->integ[stg] += x;
                x = s->integ[stg];
            }

            // Decimação por R
            s->decim_phase++;
            if (s->decim_phase >= s->R) {
                s->decim_phase = 0;

                int64_t y = x;
                // Combs (cada estágio com atraso M e estado próprio)
                for (int stg = 0; stg < s->N; ++stg) {
                    int idx = s->comb_idx[stg];
                    int64_t old = s->comb_z[stg][idx];
                    s->comb_z[stg][idx] = y;
                    idx = (idx + 1) % s->M;
                    s->comb_idx[stg] = idx;

                    y = y - old;
                }
                s->tmp_cic[w_cic++] = y;
            }
        }
    }

    // ====== FIR ======
    size_t L = s->fir_len;
    float *h = s->fir;
    size_t produced = 0;

    if (L == 1) {
        float h0 = h[0];
        for (size_t n = 0; n < w_cic; ++n)
            out[produced++] = h0 * (float)s->tmp_cic[n];
    } else {
        size_t hist_len = s->hist_len;
        float *hist = s->fir_hist;
        size_t wr = s->hist_wr;

        for (size_t n = 0; n < w_cic; ++n) {
            float x = (float)s->tmp_cic[n];
            float acc = h[0] * x;
            for (size_t k = 1; k < L; ++k) {
                size_t back = (wr + hist_len - k) % hist_len;
                acc += h[k] * hist[back];
            }
            hist[wr] = x;
            wr = (wr + 1) % hist_len;
            out[produced++] = acc;
        }
        s->hist_wr = wr;
    }

    *n_out = produced;
    return 0;
}

void filtro_destroy(filtro_t *f)
{
    if (!f) return;
    filtro_state_t *s = &f->st;

    free(s->integ);
    if (s->comb_z) {
        for (int stg = 0; stg < s->N; ++stg)
            free(s->comb_z[stg]);
        free(s->comb_z);
    }
    free(s->comb_idx);
    free(s->fir);
    free(s->fir_hist);
    free(s->tmp_cic);

    memset(f, 0, sizeof(*f));
    free(f);
}