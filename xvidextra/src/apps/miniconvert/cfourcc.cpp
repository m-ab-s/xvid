/*
    cfourcc.c - this is the main (and only) module.
    
    cfourcc - change the FOURCC code in the Microsoft RIFF AVI file.
    Copyright (C) 2004-2005 mypapit <papit58@yahoo.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Project's web : http://sarovar.org/projects/gfourcc


	Reworked for use with Xvid MiniConvert application on 26/03/2011
	by Michael Militzer <michael@xvid.org>

*/

#include "stdafx.h"

#define AVILEN 224
typedef char AVIHDR[AVILEN];

int
setDesc (AVIHDR header, const char *str)
{
  memcpy (&header[0x70], str, 4);
  return 0;
}

int
setUsed (AVIHDR header, const char *str)
{
  memcpy (&header[0xbc], str, 4);
  return 0;
}

int
fourcc_helper(TCHAR filename[MAX_PATH], char *ptrused, char *ptrdesc, int check_only)
{
  AVIHDR avihdr;
  FILE *fin;
  char MAGIC[] = { 'R', 'I', 'F', 'F' };

  memset (&avihdr, 0, AVILEN);	/* init avihdr just in case! */

  fin = _tfopen (filename, TEXT("rb"));
  if (fin == NULL) {
	OutputDebugString(TEXT("Error: Cannot open avi file!"));
	return -1;
  }

  if (fread (avihdr, sizeof (char), AVILEN, fin) < AVILEN) {
	OutputDebugString(TEXT("Error: Read end unexpectedly, incomplete file?"));
	return -1;
  }
  fclose (fin);

  if (check_only) {
    /*lazy verifier! */
    if (memcmp (avihdr, MAGIC, 4)) {
	  OutputDebugString(TEXT("Error: Probably not a supported avi file"));
	  return -1;
    }
  }
  else {
    fin = _tfopen (filename, TEXT("r+b"));
    if (fin == NULL) {
      OutputDebugString(TEXT("Error: Cannot open avi file for writing\n"));
	  return -1;
    }

    setUsed (avihdr, ptrused);
    setDesc (avihdr, ptrdesc);

    fseek (fin, 0, SEEK_SET);
    fwrite (avihdr, sizeof (char), AVILEN, fin);
    fflush (fin);
    fclose (fin);
  }

  return 0;
}
