#!/bin/csh

#**********************************************
#CMU ARPA Speech Project
#SPHINX-III Decoder
#
#Copyright (c) 1996 Carnegie Mellon University.
#ALL RIGHTS RESERVED.
#**********************************************

#
# Sample script for forced alignment of Wall Street Journal 1994 evaluation set,
# using continuous HMM acoustic models.
# 
# Arguments for this script:
#     pgm:    Aligner program (i.e., ../{alpha,linux,...}/s3align),
#     output: Name for the output transcript and log file,
#     offset: Number of utterances to be skipped (optional),
#     count:  Number of utterances to be processed (after the offset; optional).
# 
# The pronunciation dictionary doesn't contain all the words in the test set.  So
# alignment cannot be done for some utterances.
#


if ($#argv < 2) then
    echo "Usage: $0 pgm output [offset [count]]"
    exit
endif

set echo
limit coredumpsize 0

# Private copy of executable so that recompiling the binary doesn't clobber running
# instances.
set PGM = /tmp/$1:t.$$
cp $1 $PGM

set logfile = $2.log

set ctloffset = 0
set ctlcount = 100000000
if ($#argv > 2) then
    set ctloffset = $3
    if ($#argv > 3) then
	set ctlcount = $4
    endif
endif

# Locations of various model files/directories (at CMU)

# set datadir = /net/alf20/usr/rkm/SHARED
set datadir = ../testdata

# set hmmdir = $datadir/hmm/si284/c6k16d-g
set hmmdir = $datadir/hmm-sphinx3

set mdeffile  = $datadir/dict/nov94/h1c1-94.mdef
set dictfile  = $datadir/dict/nov94/h1c1-94.dic
set fdictfile = $datadir/dict/nov94/filler.dic
set ctlfile   = $datadir/ctl/h1_et_94-g.ctl
set cepdir    = $datadir/ctl
set insentfile = $datadir/ctl/h1_et_94-g.correct
set outsentfile = $2.out

echo "Executing" $1 > $logfile
$PGM \
	-mdeffn $mdeffile \
	-senmgaufn .cont. \
	-meanfn $hmmdir/mean \
	-varfn  $hmmdir/var  \
	-mixwfn $hmmdir/mixw \
	-tmatfn $hmmdir/tmat \
	-feat s3_1x39 \
	-topn 1000 \
	-beam 1e-80 \
	-dictfn $dictfile \
	-fdictfn $fdictfile \
	-ctlfn $ctlfile \
	-ctloffset $ctloffset \
	-ctlcount $ctlcount \
	-cepdir $cepdir \
	-insentfn $insentfile \
	-outsentfn $outsentfile \
# Bad directory spec forces word and phone segmentations to be written to logfile: \
	-wdsegdir /dev/null \
	-phsegdir /dev/null \
	>>& $logfile

rm $PGM
