/* ====================================================================
 * Copyright (c) 1999-2001 Carnegie Mellon University.  All rights
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
/*
 * cepstrum file read routines.
 *
 * HISTORY
 * 12-Mar-92  Eric Thayer (eht) at Carnegie-Mellon University
 *	Fixed byte reversal check.
 *
 *  6-Jan-92  Fil Alleva (faa) at Carnegie-Mellon University
 *	Changed format check to use file size information from fstat().
 *
 * 12-Aug-91  Eric Thayer (eht) at Carnegie-Mellon University
 *	Changed openp() call to open().  If anyone uses this, I'll put it
 *	back.
 *
 * 29-Sep-90  Fil Alleva (faa) at Carnegie-Mellon University
 *	Added the byteReversing code that should have been here in the
 *	first place and was in some of the original versions of the code.
 *
 * 01-Sep-90  Eric Thayer (eht) at Carnegie-Mellon University
 *	Created.  Should be made more robust across machine architectures.
 *
 */
/* #include <c.h>  -- this breaks something on DEC alpha's */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>

#ifdef WIN32
#include <posixwin32.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/param.h>
#endif
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "s2types.h"
#include "strfuncs.h"
#include "cepio.h"
#include "err.h"

#ifndef ESUCCESS
#define ESUCCESS 0
#endif

#define SWABL(x) (((x << 24) & 0xFF000000) | ((x <<  8) & 0x00FF0000) | \
	          ((x >>  8) & 0x0000FF00) | ((x >> 24) & 0x000000FF))

int32 cep_read_bin (float32 **buf, int32 *len, char const *file)
{
  int32 fd, floatCount, floatBytes, readBytes;
  int32 byteReverse = FALSE;
  struct stat st_buf;

#ifdef WIN32
  fd = open(file, O_RDONLY|O_BINARY, 0644);
#else
  fd = open(file, O_RDONLY, 0644);
#endif

  if (fd < 0) {
    E_ERROR("%s(%d): Couldn't open %s\n",
	    __FILE__, __LINE__, file);
    return errno;
  }

  /* assume float count file */
  if (read(fd, (char *)&floatCount, sizeof(int32)) != sizeof(int32))
    return errno;

  if (fstat (fd, &st_buf) < 0) {
    perror("cep_read_bin: fstat failed");
    return errno;
  }

  /*  printf("Float count: %d st_buf.st_size: %d \n", floatCount, st_buf.st_size); */

  /*
   * Check if this is a byte reversed file !
   */
  if ((floatCount+4 != st_buf.st_size) &&
      ((floatCount * sizeof(float32) + 4) != st_buf.st_size)) {
	E_INFO("%s(%d): Byte reversing %s\n", __FILE__, __LINE__, file);
	byteReverse = TRUE;
	floatCount = SWABL (floatCount);
  }

  if (floatCount == (st_buf.st_size - 4)) {
	floatBytes = floatCount;
	floatCount /= sizeof (float32);
  }
  else 
	floatBytes = floatCount * sizeof(float32);

  /* malloc size to account for possibility of being a float count file */
  if ((*buf = (float32 *)malloc(floatBytes)) == NULL)
    return errno;
  readBytes = read(fd, (char *)*buf, floatBytes);

  if (readBytes != floatBytes) {
    /* float count is actually a byte count of the file */
    return errno;
  }
  *len = readBytes;
  /*
   * Reorder the bytes if needed
   */
  if (byteReverse) {
	uint32 *ptr = (uint32 *) *buf;
	int32 i, cnt = readBytes >> 2;
	for (i = 0; i < cnt; i++)
	    ptr[i] = SWABL(ptr[i]);
  }
  if (close(fd) != ESUCCESS) return errno;
  return ESUCCESS;
}  

int32 cep_write_bin(char const *file, float32 *buf, int32 len)
{
  int32 fd;

#ifdef WIN32
 fd = open(file, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0644);
#else
  fd = open(file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
#endif

  if (fd < 0) {
    E_ERROR("%s(%d): Couldn't open %s for writing\n",
	    __FILE__, __LINE__, file);
    return errno;
  }
  len *= sizeof(float32);
  if (write(fd, (char *)&len, sizeof(int32)) != sizeof(int32)) return errno;
  if (write(fd, (char *)buf, len) != len) return errno;
  if (close(fd) != ESUCCESS) return errno;

  return ESUCCESS;
}  
