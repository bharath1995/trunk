/*
 * vit_hist.c -- Viterbi search history.
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1997 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 *
 * HISTORY
 * 
 * 03-Mar-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Added vithist_sort().
 * 
 * 25-Feb-1999	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Modified vithist_append() interface to include lmhist.
 * 
 * 23-Jul-1997	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University.
 * 		Started.
 */


#include <libutil/libutil.h>
#include "vithist.h"
#include "senone.h"
#include "hyp.h"


glist_t vithist_append (glist_t hlist, int32 id, int32 frm, int32 score,
			vithist_t *hist, vithist_t *lmhist)
{
    vithist_t *h;
    
    h = (vithist_t *) mymalloc (sizeof(vithist_t));
    h->id = id;
    h->frm = frm;
    h->scr = score;
    h->hist = hist;
    h->lmhist = lmhist ? lmhist : h;	/* Self if none provided */
    
    hlist = glist_add_ptr (hlist, (void *)h);
    return hlist;
}


glist_t vithist_backtrace (vithist_t *hist, int32 *senscale)
{
    glist_t hyp;
    hyp_t *hypnode;
    
    hyp = NULL;
    
    for (; hist; hist = hist->hist) {
	hypnode = (hyp_t *) mymalloc (sizeof(hyp_t));
	hypnode->id = hist->id;
	hypnode->ef = hist->frm;

	if (hist->hist) {
	    hypnode->sf = hist->hist->frm+1;
	    hypnode->ascr = hist->scr - hist->hist->scr;	/* Still scaled */
	} else {
	    hypnode->sf = 0;
	    hypnode->ascr = hist->scr;
	}
	
	/* Undo senone score scaling */
	hypnode->ascr += senone_get_senscale (senscale, hypnode->sf, hypnode->ef);
	
	hypnode->lscr = 0;
	hypnode->scr = hypnode->ascr;
	
	hyp = glist_add_ptr (hyp, (void *)hypnode);
    }

    return hyp;
}


glist_t vithist_sort (glist_t vithist_list)
{
    heap_t heap;
    gnode_t *gn;
    vithist_t *h;
    glist_t vithist_new;
    int32 ret, score;
    
    vithist_new = NULL;
    
    heap = heap_new();
    for (gn = vithist_list; gn; gn = gnode_next(gn)) {
	h = (vithist_t *) gnode_ptr(gn);
	if (heap_insert (heap, (void *) h, h->scr) < 0) {
	    E_ERROR("Panic: heap_insert() failed\n");
	    return NULL;
	}
    }
    
    /*
     * Note: The heap returns nodes with ASCENDING values; and glist_add adds new nodes to the
     * HEAD of the list.  So we get a glist in the desired descending score order.
     */
    while ((ret = heap_pop (heap, (void **)(&h), &score)) > 0)
	vithist_new = glist_add_ptr (vithist_new, (void *)h);
    if (ret < 0) {
	E_ERROR("Panic: heap_pop() failed\n");
	return NULL;
    }
    
    heap_destroy (heap);
    
    return vithist_new;
}


int32 vithist_log (FILE *fp, glist_t *vithist, int32 nfr, char *(*func)(void *kb, int32 id),
		   void *kb)
{
    int32 f, n;
    gnode_t *gn;
    vithist_t *h;
    
    n = 0;
    for (f = 0; f < nfr; f++) {
	for (gn = vithist[f]; gn; gn = gnode_next(gn), n++) {
	    h = (vithist_t *) gnode_ptr(gn);
	    fprintf (fp, " %5d %5d %11d %s\n",
		     h->hist ? h->hist->frm + 1 : h->frm, h->frm,
		     h->scr - (h->hist ? h->hist->scr : 0),
		     func ? (*func)(kb, h->id) : "");
	}
    }
    fflush (fp);

    return n;
}
