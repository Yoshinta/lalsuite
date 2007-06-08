/*
*  Copyright (C) 2007 Duncan Brown
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with with program; see the file COPYING. If not, write to the
*  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
*  MA  02111-1307  USA
*/

/*----------------------------------------------------------------------- 
 * 
 * File Name: FindChirpPTF.h
 *
 * Author: Brown, D. A., and Fazi, D.
 * 
 * Revision: $Id$
 * 
 *-----------------------------------------------------------------------
 */

#if 0
<lalVerbatim file="FindChirpPTFHV">
Author: Brown, D. A., and Fazi, D.
$Id$
</lalVerbatim> 

<lalLaTeX>
\section{Header \texttt{FindChirpPTF.h}}
\label{s:FindChirpPTF.h}

Provides structures and functions to filter interferometer data using the
physical template family.

\subsection*{Synopsis}

\begin{verbatim}
#include <lal/FindChirpPTF.h>
\end{verbatim}

\input{FindChirpPTFHDoc}
</lalLaTeX>
#endif


#ifndef _FINDCHIRPPTFH_H
#define _FINDCHIRPPTFH_H

#include <lal/LALDatatypes.h>
#include <lal/RealFFT.h>
#include <lal/DataBuffer.h>
#include <lal/LALInspiral.h>
#include <lal/FindChirp.h>

#ifdef  __cplusplus
extern "C" {
#pragma }
#endif


NRCSID (FINDCHIRPPTFH, "$Id$");

#if 0
<lalLaTeX> 
\subsection*{Error codes} 
</lalLaTeX>
#endif

#if 0
<lalLaTeX>
\subsection*{Types}

None.
</lalLaTeX>
#endif


#if 0
<lalLaTeX>
\vfill{\footnotesize\input{FindChirpPTFHV}}
</lalLaTeX> 
#endif

#if 0
<lalLaTeX>
\newpage\input{FindChirpPTFTemplateC}
</lalLaTeX>
#endif

void
LALFindChirpPTFTemplate (
    LALStatus                  *status,
    FindChirpTemplate          *fcTmplt,
    InspiralTemplate           *tmplt,
    FindChirpTmpltParams       *params
    );

void
LALFindChirpPTFNormalize(
    LALStatus                  *status,
    FindChirpTemplate          *fcTmplt,
    FindChirpSegment           *fcSeg,
    FindChirpDataParams        *params
    );

#if 0
<lalLaTeX>
\newpage\input{FindChirpPTFFilterC}
</lalLaTeX>
#endif

void
LALFindChirpPTFFilterSegment (
    LALStatus                  *status,
    SnglInspiralTable         **eventList,
    FindChirpFilterInput       *input,
    FindChirpFilterParams      *params
    );

#ifdef  __cplusplus
#pragma {
}
#endif

#endif /* _FINDCHIRPPTFH_H */
