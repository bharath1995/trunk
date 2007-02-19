"""Calculate Fisher's linear discriminant for acoustic models.

This module implements Linear Discriminant Analysis for single
stream Sphinx-III acoustic models.
"""

# Copyright (c) 2006 Carnegie Mellon University
#
# You may copy and modify this freely under the same terms as
# Sphinx-III

__author__ = "David Huggins-Daines <dhuggins@cs.cmu.edu>"
__version__ = "$Revision: 6368 $"

import numpy
import s3lda
import itertools

def makelda(gauden_counts=None, covfile=None, countfile=None):
    """Calculate an LDA matrix from a set of mean/full-covariance
    counts as output by the 'bw' program from SphinxTrain"""
    if gauden_counts != None:
        if not gauden_counts.pass2var:
            raise Exception, "Please re-run bw with '-2passvar yes'"
        mean = numpy.array(map(lambda x: x[0][0], gauden_counts.getmeans()))
        var = numpy.array(map(lambda x: x[0][0], gauden_counts.getvars()))
        dnom = numpy.array(map(lambda x: x[0][0], gauden_counts.getdnom()))
    if covfile != None:
        var = s3lda.open(covfile).getall()
    if countfile != None:
        dfh = open(countfile)
        dnom = numpy.array(map(lambda x: int(x.rstrip()), dfh))

    # If CMN was used, this should actually be very close to zero
    globalmean = mean.sum(0) / dnom.sum()
    sw = var.sum(0)
    sb = numpy.zeros(var[0].shape, 'd')
    for d, m in itertools.izip(dnom, mean):
        diff = m / d - globalmean
        sb += d * numpy.outer(diff, diff)

    print "Sw:\n", sw
    print "Sb:\n", sb
    BinvA = numpy.dot(numpy.linalg.inv(sw), sb)
    u, v = numpy.linalg.eig(BinvA)
    
    top = list(u.argsort())
    top.reverse()
    u = u.take(top)
    # Remember, the eigenvalues are in the columns, but Sphinx expects
    # them to be in the rows.
    v = v.T.take(top)

    print "u:\n", u
    print "v:\n", v

    return v
