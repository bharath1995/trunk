/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2010 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced
 * Research Projects Agency and the National Science Foundation of the
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */

/**
 * @file ngram_trie.c
 * @brief Mutable trie implementation of N-Gram language models
 * @author David Huggins-Daines <dhuggins@cs.cmu.edu>
 */

#include <string.h>
#include <ctype.h>

#include <sphinxbase/pio.h>
#include <sphinxbase/garray.h>
#include <sphinxbase/strfuncs.h>
#include <sphinxbase/listelem_alloc.h>

#include "ngram_trie.h"

#define MIN_LOGPROB 1e-20

/**
 * N-Gram trie.
 */
struct ngram_trie_s {
    int refcount;
    dict_t *dict;      /**< Dictionary which maps words to IDs. */
    int gendict;       /**< Is the dictionary generated from the unigram? */
    logmath_t *lmath;  /**< Log-math used for input/output. */
    int shift;         /**< Shift applied internally to log values. */
    int zero;          /**< Minimum allowable log value. */
    int n;             /**< Maximum N-Gram order. */
    garray_t *counts;  /**< N-Gram counts. */

    ngram_trie_node_t *root;
    listelem_alloc_t *node_alloc;

#if 0 /* Future memory-optimized version... */
    garray_t *nodes;       /**< Flat array of nodes. */
    garray_t *successors;  /**< Flat array of successor pointers. */
#endif
};

/**
 * N-Gram structure
 */
struct ngram_trie_node_s {
    int32 word;
    int16 log_prob;
    int16 log_bowt;
#if 0 /* Future memory-optimized version... */
    int32 history;    /**< Index of parent node. */
    int32 successors; /**< Index of child nodes. */
#endif
    ngram_trie_node_t *history;
    garray_t *successors;
};

/**
 * Iterator over N-Grams in a trie.
 */
struct ngram_trie_iter_s {
    ngram_trie_t *t;        /**< Parent trie. */
    ngram_trie_node_t *cur; /**< Current node (in this successor array). */
    int32 pos;              /**< Position in cur->successors. */
    int nostop;             /**< Continue to next node at same level. */
};

static int
ngram_trie_nodeptr_cmp(garray_t *gar, void const *a, void const *b, void *udata)
{
    ngram_trie_node_t *na, *nb;
    ngram_trie_t *t;

    na = *(ngram_trie_node_t **)a;
    nb = *(ngram_trie_node_t **)b;
    t = (ngram_trie_t *)udata;

    return strcmp(dict_wordstr(t->dict, na->word),
                  dict_wordstr(t->dict, nb->word));
}

ngram_trie_node_t *
ngram_trie_node_alloc(ngram_trie_t *t)
{
    ngram_trie_node_t *node;

    node = listelem_malloc(t->node_alloc);
    node->word = -1;
    node->log_prob = 0;
    node->log_bowt = 0;
    node->history = NULL;
    node->successors = garray_init(0, sizeof(ngram_trie_node_t *));
    garray_set_cmp(node->successors, &ngram_trie_nodeptr_cmp, t);

    return node;
}

ngram_trie_t *
ngram_trie_init(dict_t *d, logmath_t *lmath)
{
    ngram_trie_t *t;

    t = ckd_calloc(1, sizeof(*t));
    t->refcount = 1;
    if (d) {
        t->dict = dict_retain(d);
        t->gendict = FALSE;
    }
    else {
        t->dict = dict_init(NULL, NULL);
        t->gendict = TRUE;
    }
    t->lmath = logmath_retain(lmath);

    /* Determine proper shift to fit min_logprob in 16 bits. */
    t->zero = logmath_log(lmath, MIN_LOGPROB);
    while (t->zero < -32768) {
        t->zero >>= 1;
        ++t->shift;
    }

    t->counts = garray_init(0, sizeof(int));
    t->node_alloc = listelem_alloc_init(sizeof(ngram_trie_node_t));

#if 0
    /* Create arrays and add the root node. */
    t->nodes = garray_init(1, sizeof(ngram_trie_node_t));
    t->successors = garray_init(0, sizeof(int32));
    t->root = garray_ptr(t->nodes, ngram_trie_node_t, 0);
    t->root->history = -1;
    t->root->successors = 0; /* No successors yet. */
#else
    t->root = ngram_trie_node_alloc(t);
#endif

    return t;
}

ngram_trie_t *
ngram_trie_retain(ngram_trie_t *t)
{
    ++t->refcount;
    return t;
}

int
ngram_trie_free(ngram_trie_t *t)
{
    if (t == NULL)
        return 0;
    if (--t->refcount > 0)
        return t->refcount;
    dict_free(t->dict);
    logmath_free(t->lmath);
#if 0
    garray_free(t->nodes);
    garray_free(t->successors);
#endif
    ckd_free(t);
    return 0;
}

dict_t *
ngram_trie_dict(ngram_trie_t *t)
{
    return t->dict;
}

logmath_t *
ngram_trie_lmath(ngram_trie_t *t)
{
    return t->lmath;
}

ngram_trie_node_t *
ngram_trie_root(ngram_trie_t *t)
{
    return t->root;
}

ngram_trie_node_t *
ngram_trie_ngram(ngram_trie_t *t, char const *w, ...)
{
    ngram_trie_node_t *node;
    char const *h;
    va_list args;
    int n_hist;
    int32 *hist;
    int32 wid;

    wid = dict_wordid(t->dict, w);
    va_start(args, w);
    n_hist = 0;
    while ((h = va_arg(args, char const *)) != NULL)
        ++n_hist;
    va_end(args);
    hist = ckd_calloc(n_hist, sizeof(*hist));
    va_start(args, w);
    n_hist = 0;
    while ((h = va_arg(args, char const *)) != NULL) {
        hist[n_hist] = dict_wordid(t->dict, h);
        ++n_hist;
    }
    va_end(args);

    node = ngram_trie_ngram_v(t, wid, hist, n_hist);
    ckd_free(hist);
    return node;
}

ngram_trie_node_t *
ngram_trie_ngram_v(ngram_trie_t *t, int32 w,
                   int32 const *hist, int32 n_hist)
{
    ngram_trie_node_t *node;
    int i;

    E_INFO("Looking for N-Gram %s |",
           dict_wordstr(t->dict, w));
    for (i = 0; i < n_hist; ++i) {
        E_INFOCONT(" %s", dict_wordstr(t->dict, hist[n_hist - 1 - i]));
    }
    E_INFOCONT("\n");

    node = ngram_trie_root(t);
    if (n_hist > t->n - 1)
        n_hist = t->n - 1;
    while (n_hist > 0) {
        int32 nextwid = hist[n_hist - 1];
        if ((node = ngram_trie_successor(t, node, nextwid)) == NULL)
            return NULL;
    }

    return ngram_trie_successor(t, node, w);
}

int32
ngram_trie_prob(ngram_trie_t *t, int *n_used, char const *w, ...)
{
    char const *h;
    va_list args;
    int n_hist;
    int32 *hist;
    int32 wid;
    int32 prob;

    wid = dict_wordid(t->dict, w);
    va_start(args, w);
    n_hist = 0;
    while ((h = va_arg(args, char const *)) != NULL)
        ++n_hist;
    va_end(args);
    hist = ckd_calloc(n_hist, sizeof(*hist));
    va_start(args, w);
    n_hist = 0;
    while ((h = va_arg(args, char const *)) != NULL) {
        hist[n_hist] = dict_wordid(t->dict, h);
        ++n_hist;
    }
    va_end(args);

    prob = ngram_trie_prob_v(t, n_used, wid, hist, n_hist);
    ckd_free(hist);
    return prob;
}

ngram_trie_iter_t *
ngram_trie_ngrams(ngram_trie_t *t, int n)
{
    ngram_trie_iter_t *itor;
    ngram_trie_node_t *h;
    int i;

    /* Find first N-1-Gram */
    h = t->root;
    for (i = 1; i < n; ++i) {
        h = garray_ent(h->successors, ngram_trie_node_t *, 0);
        if (h == NULL)
            return NULL;
    }

    /* Create an iterator with nostop=TRUE */
    itor = ckd_calloc(1, sizeof(*itor));
    itor->cur = h;
    itor->pos = 0;
    itor->nostop = FALSE;

    return itor;
}

ngram_trie_iter_t *
ngram_trie_successors(ngram_trie_t *t, ngram_trie_node_t *h)
{
    ngram_trie_iter_t *itor;

    /* Create an iterator with nostop=FALSE */
    itor = ckd_calloc(1, sizeof(*itor));
    itor->cur = h;
    itor->pos = 0;
    itor->nostop = FALSE;

    return itor;
}

void
ngram_trie_iter_free(ngram_trie_iter_t *itor)
{
    ckd_free(itor);
}

static ngram_trie_node_t *
ngram_trie_next_node(ngram_trie_t *t, ngram_trie_node_t *ng)
{
    ngram_trie_node_t *h = ng->history;
    size_t pos;

    if (h == NULL)
        return NULL;
    /* Locate ng in h->successors. */
    pos = garray_bisect_left(h->successors, &ng);
    assert(pos < garray_next_idx(h->successors));
    assert(ng == garray_ent(h->successors, ngram_trie_node_t *, pos));
    ++pos;
    if (pos == garray_next_idx(h->successors)) {
        h = ngram_trie_next_node(t, h);
        if (h == NULL)
            return NULL;
        return garray_ent(h->successors, ngram_trie_node_t *, 0);
    }
    else
        return garray_ent(h->successors, ngram_trie_node_t *, pos);
}

ngram_trie_iter_t *
ngram_trie_iter_next(ngram_trie_iter_t *itor)
{
    ++itor->pos;
    if (itor->pos >= garray_next_idx(itor->cur->successors)) {
        if (itor->nostop) {
            itor->cur = ngram_trie_next_node(itor->t, itor->cur);
            if (itor->cur == NULL) {
                ngram_trie_iter_free(itor);
                return NULL;
            }
            itor->pos = 0;
        }
        else  {
            ngram_trie_iter_free(itor);
            return NULL;
        }
    }
    return itor;
}

ngram_trie_iter_t *
ngram_trie_iter_up(ngram_trie_iter_t *itor)
{
    itor->cur = itor->cur->history;
    if (itor->cur == NULL) {
        ngram_trie_iter_free(itor);
        return NULL;
    }
    return itor;
}

ngram_trie_iter_t *
ngram_trie_iter_down(ngram_trie_iter_t *itor)
{
    itor->cur = garray_ent(itor->cur->successors,
                           ngram_trie_node_t *, itor->pos);
    assert(itor->cur != NULL);
    if (garray_next_idx(itor->cur->successors) == 0) {
        ngram_trie_iter_free(itor);
        return NULL;
    }
    itor->pos = 0;
    return itor;
}

ngram_trie_node_t *
ngram_trie_iter_get(ngram_trie_iter_t *itor)
{
    if (itor->pos >= garray_next_idx(itor->cur->successors))
        return NULL;
    else
        return garray_ent(itor->cur->successors,
                          ngram_trie_node_t *, itor->pos);
}

ngram_trie_node_t *
ngram_trie_iter_get_parent(ngram_trie_iter_t *itor)
{
    return itor->cur;
}

static size_t
ngram_trie_successor_pos(ngram_trie_t *t, ngram_trie_node_t *h, int32 w)
{
    ngram_trie_node_t *png, ng;
    ng.word = w;
    png = &ng;
    return garray_bisect_left(h->successors, &png);
}

ngram_trie_node_t *
ngram_trie_successor(ngram_trie_t *t, ngram_trie_node_t *h, int32 w)
{
    size_t pos;

    E_INFO("Looking for successor %s in node with head word %s\n",
           dict_wordstr(t->dict, w), h->word == -1 
           ? "<root>" : dict_wordstr(t->dict, h->word));
    pos = ngram_trie_successor_pos(t, h, w);
    E_INFO("pos = %lu wtf\n", pos);
    if (pos >= garray_next_idx(h->successors))
        return NULL;
    return garray_ent(h->successors, ngram_trie_node_t *, pos);
}

int
ngram_trie_delete_successor(ngram_trie_t *t, ngram_trie_node_t *h, int32 w)
{
    ngram_trie_node_t *ng;
    size_t pos;

    /* Bisect the successor array. */
    pos = ngram_trie_successor_pos(t, h, w);
    if (pos >= garray_next_idx(h->successors))
        return -1;
    ng = garray_ent(h->successors, ngram_trie_node_t *, pos);
    /* Delete it. */
    listelem_free(t->node_alloc, ng);
    garray_delete(h->successors, pos, pos+1);
    return 0;
}

ngram_trie_node_t *
ngram_trie_add_successor(ngram_trie_t *t, ngram_trie_node_t *h, int32 w)
{
    ngram_trie_node_t *ng;
    size_t pos;

    ng = ngram_trie_node_alloc(t);
    ng->word = w;
    ng->history = h;
    pos = garray_bisect_right(h->successors, &ng);
    if (pos == garray_next_idx(h->successors))
        garray_append(h->successors, &ng);
    else
        garray_insert(h->successors, pos, &ng);

    return ng;
}

int
ngram_trie_add_successor_ngram(ngram_trie_t *t,
                               ngram_trie_node_t *h,
                               ngram_trie_node_t *w)
{
    size_t pos;

    pos = garray_bisect_right(h->successors, &w);
    if (pos == garray_next_idx(h->successors))
        garray_append(h->successors, &w);
    else
        garray_insert(h->successors, pos, &w);

    return 0;
}

int32
ngram_trie_node_get_word_hist(ngram_trie_t *t,
                              ngram_trie_node_t *ng,
                              int32 *out_hist)
{
    ngram_trie_node_t *h;
    int32 n_hist;

    n_hist = 0;
    for (h = ng->history; h->word != -1; h = h->history) {
        out_hist[n_hist] = h->word;
        ++n_hist;
    }

    return n_hist;
}

ngram_trie_node_t *
ngram_trie_backoff(ngram_trie_t *t,
                   ngram_trie_node_t *ng)
{
    ngram_trie_node_t *bong;
    int32 *hist;
    int32 n_hist;

    /* Extract word IDs from ng's history. */
    n_hist = ngram_trie_node_get_word_hist(t, ng, NULL);
    hist = ckd_calloc(n_hist, sizeof(*hist));
    ngram_trie_node_get_word_hist(t, ng, hist);

    /* Look up the backoff N-Gram. */
    bong = ngram_trie_ngram_v(t, ng->word, hist, n_hist - 1);
    ckd_free(hist);

    return bong;
}

int32
ngram_trie_bowt_v(ngram_trie_t *t, int32 w,
                  int32 const *hist, int32 n_hist)
{
    ngram_trie_node_t *ng;

    if ((ng = ngram_trie_ngram_v(t, w, hist, n_hist)) != NULL)
        return ng->log_bowt << t->shift;
    else if (n_hist > 0)
        return ngram_trie_bowt_v(t, hist[0], hist + 1, n_hist - 1);
    else
        return 0;
}

int32
ngram_trie_prob_v(ngram_trie_t *t, int *n_used, int32 w,
                  int32 const *hist, int32 n_hist)
{
    ngram_trie_node_t *ng;

    if (n_used) *n_used = n_hist + 1;
    if ((ng = ngram_trie_ngram_v(t, w, hist, n_hist)) != NULL)
        return ng->log_prob << t->shift;
    else if (n_hist > 0) {
        int32 bong, pong;

        if (n_used) --*n_used;
        bong = ngram_trie_prob_v(t, n_used, w, hist, n_hist - 1);
        pong = ngram_trie_bowt_v(t, hist[0], hist + 1, n_hist - 1);
        return bong + pong;
    }
    else {
        if (n_used) *n_used = 0;
        return t->zero << t->shift;
    }
}

int32
ngram_trie_successor_prob(ngram_trie_t *t,
                          ngram_trie_node_t *h, int32 w)
{
    int32 prob;
    int32 *hist;
    int32 n_hist;

    /* Extract word IDs from ng's history. */
    n_hist = ngram_trie_node_get_word_hist(t, h, NULL) + 1;
    hist = ckd_calloc(n_hist, sizeof(*hist));
    ngram_trie_node_get_word_hist(t, h, hist + 1);
    hist[0] = h->word;
    
    prob = ngram_trie_prob_v(t, NULL, w, hist, n_hist);
    return prob;
}

int32
ngram_trie_calc_bowt(ngram_trie_t *t, ngram_trie_node_t *h)
{
    return 0;
}

static int
skip_arpa_header(lineiter_t *li)
{
    while (li) {
        string_trim(li->buf, STRING_BOTH);
        if (0 == strcmp(li->buf, "\\data\\")) {
            break;
        }
        li = lineiter_next(li);
    }
    if (li == NULL) {
        E_ERROR("Unexpected end of file when reading ARPA format");
        return -1;
    }
    return 0;
}

static int
read_ngram_counts(lineiter_t *li, garray_t *counts)
{
    int one = 1;

    /* Reset and add the number of zerograms (there is one of them) */
    garray_reset(counts);
    garray_append(counts, &one);

    for (;li;li = lineiter_next(li)) {
        string_trim(li->buf, STRING_BOTH);
        if (strlen(li->buf) == 0)
            break;
        if (0 == strncmp(li->buf, "ngram ", 6)) {
            char *n, *c;
            int ni, ci;
            n = li->buf + 6;
            if (n == NULL) {
                E_ERROR("Invalid N-Gram count line when reading ARPA format");
                return -1;
            }
            c = strchr(n, '=');
            if (c == NULL || c[1] == '\0') {
                E_ERROR("Invalid N-Gram count line when reading ARPA format");
                return -1;
            }
            E_INFO("%s\n", li->buf);
            *c++ = '\0';
            ni = atoi(n);
            ci = atoi(c);
            garray_expand_to(counts, ni + 1);
            garray_put(counts, ni, &ci);
        }
    }
    return garray_size(counts) - 1;
}

static ngram_trie_node_t *
add_ngram_line(ngram_trie_t *t, char *buf, int n,
               char **wptr, int32 *wids,
               ngram_trie_node_t **last_history)
{
    int nwords = str2words(buf, NULL, 0);
    double prob, bowt;
    int i;
    char *libuf = ckd_salloc(buf);
    ngram_trie_node_t *node;

    assert(nwords <= n + 2);
    if (nwords < n + 1) {
        E_ERROR("Expected at least %d fields for %d-Gram\n", n+1, n);
        return NULL;
    }
    str2words(buf, wptr, nwords);

    prob = atof_c(wptr[0]);
    if (nwords == n + 2)
        bowt = atof_c(wptr[n + 1]);
    else
        bowt = 0.0;

    wids[0] = dict_wordid(t->dict, wptr[n]);
    /* Add a unigram word to the dictionary if we are generating one. */
    if (wids[0] == BAD_S3WID) {
        if (t->gendict)
            wids[0] = dict_add_word(t->dict, wptr[n], NULL, 0);
        else {
            E_WARN("Unknown unigram %s in ARPA file, skipping\n");
            return NULL;
        }
    }
    for (i = 1; i < n; ++i) {
        wids[i] = dict_wordid(t->dict, wptr[n-i]);
        if (wids[i] == BAD_S3WID) {
            E_WARN("Unknown unigram %s in ARPA file, skipping\n");
            return NULL;
        }
    }
    E_INFO("Line is %s N-Gram is %s |",
           libuf,
           dict_wordstr(t->dict, wids[0]));
    for (i = 1; i < n; ++i) {
        E_INFOCONT(" %s", dict_wordstr(t->dict, wids[n-i]));
    }
    E_INFOCONT("\n");
    ckd_free(libuf);

    /* Determine if this N-Gram has the same history as the previous one. */
    if (n == 1) {
        /* Always the same. */
        assert(*last_history == t->root);
    }
    else {
        /* Unfortunately, we can't (well, we shouldn't) assume that
         * the N-Grams are sorted, so we have to look up the entire
         * history. */
        ngram_trie_node_t *h = *last_history;
        for (i = 1; h != NULL && i < n; ++i) {
            if (h->word != wids[i])
                break;
            h = h->history;
        }
        /* Not an exact match, have to get a new one. */
        if (i < n)
            *last_history = ngram_trie_ngram_v(t, wids[1], wids + 2, n - 2);
        if (*last_history == NULL) {
            E_WARN("Unknown history for N-Gram: %s |",
                   dict_wordstr(t->dict, wids[0]));
            for (i = 1; i < n; ++i) {
                E_INFOCONT(" %s", dict_wordstr(t->dict, wids[n-i]));
            }
            E_INFOCONT("\n");
            return NULL;
        }
    }

    /* Fall through and add a successor to last_history. */
    node = ngram_trie_add_successor(t, *last_history, wids[0]);
    node->log_prob = logmath_log(t->lmath, prob) >> t->shift;
    node->log_bowt = logmath_log(t->lmath, bowt) >> t->shift;
    node->history = *last_history;
    return node;
}

static int
read_ngrams(ngram_trie_t *t, lineiter_t *li, int n)
{
    /* Pre-allocate these. */
    char **wptr = ckd_calloc(n + 2, sizeof(*wptr));
    int32 *wids = ckd_calloc(n, sizeof(*wids));
    ngram_trie_node_t *last_history;

    if (n == 1)
        last_history = t->root; /* always the same for 1-grams */
    else
        last_history = NULL;
    for (;li;li = lineiter_next(li)) {
        string_trim(li->buf, STRING_BOTH);
        /* Skip blank lines. */
        if (strlen(li->buf) == 0)
            continue;
        /* No more N-Grams to work with. */
        if (0 == strcmp(li->buf, "\\end\\"))
            return 0;
        /* Look for an N-Gram start marker. */
        if (li->buf[0] == '\\') {
            char *c;
            int nn;

            if (!isdigit(li->buf[1])) {
                E_ERROR("Expected an N-Gram start marker, got %s", li->buf);
                ckd_free(wptr);
                return -1;
            }
            for (c = li->buf + 1; *c && isdigit(*c); ++c)
                ;
            if (0 != strcmp(c, "-grams:")) {
                E_ERROR("Expected an N-Gram start marker, got %s", li->buf);
                ckd_free(wptr);
                return -1;
            }
            nn = atoi(li->buf + 1);
            if (nn > n) {
                ckd_free(wptr);
                return nn;
            }
            else {
                E_INFO("%s\n", li->buf);
                continue;
            }
        }
        /* Now interpret the line as an N-Gram. */
        if (add_ngram_line(t, li->buf, n, wptr,
                           wids, &last_history) == NULL) {
            ckd_free(wptr);
            return -1;
        }
    }
    ckd_free(wptr);
    E_ERROR("Expected \\end\\ or an N-Gram marker\n");
    return -1;
}

int
ngram_trie_read_arpa(ngram_trie_t *t, FILE *arpafile)
{
    lineiter_t *li;
    int n;

    li = lineiter_start(arpafile);

    /* Skip header text. */
    if (skip_arpa_header(li) < 0)
        return -1;

    /* Read N-Gram counts. */
    if ((t->n = read_ngram_counts(li, t->counts)) < 0)
        return -1;

    /* Now read each set of N-Grams for 1..n */
    n = 1;
    while ((n = read_ngrams(t, li, n)) > 1)
        ;
    lineiter_free(li);
    if (n < 0)
        return -1;

    return 0;
}

int
ngram_trie_write_arpa(ngram_trie_t *t, FILE *arpafile)
{
    return 0;
}

