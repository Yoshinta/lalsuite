/*********************** <lalVerbatim file="CoarseGrainFrequencySeriesHV">
Author: UTB Relativity Group; contact J. T. Whelan (original by S. Drasco)
$Id$ 
*********************************************************** </lalVerbatim> */

/********************************************************** <lalLaTeX>
\section{Header \texttt{CoarseGrainFrequencySeries.h}}
\label{stochastic:s:CoarseGrainFrequencySeries.h}

\subsection*{Synopsis}
\begin{verbatim}
#include <lal/CoarseGrainFrequencySeries.h>
\end{verbatim}

\noindent 

\subsection*{Error conditions}
\input{CoarseGrainFrequencySeriesHE}

\subsection*{Structures}

*********************************************************** </lalLaTeX> */

#include <lal/LALStdlib.h>

#ifdef  __cplusplus
extern "C" {
#endif

NRCSID( COARSEGRAINFREQUENCYSERIESH,
        "$Id$" );

/****************** <lalErrTable file="CoarseGrainFrequencySeriesHE"> */

#define COARSEGRAINFREQUENCYSERIESH_ENULLP          1
#define COARSEGRAINFREQUENCYSERIESH_EZEROLEN        2
#define COARSEGRAINFREQUENCYSERIESH_ENONPOSDELTAF   3
#define COARSEGRAINFREQUENCYSERIESH_ENEGFMIN        5
#define COARSEGRAINFREQUENCYSERIESH_EMMHETERO       7
#define COARSEGRAINFREQUENCYSERIESH_EMMFMIN         8
#define COARSEGRAINFREQUENCYSERIESH_EMMDELTAF       9
#define COARSEGRAINFREQUENCYSERIESH_EMMLEN         10
#define COARSEGRAINFREQUENCYSERIESH_EOORCOARSE     16

#define COARSEGRAINFREQUENCYSERIESH_MSGENULLP      "Null pointer"
#define COARSEGRAINFREQUENCYSERIESH_MSGEZEROLEN    "Zero length for data member of series" 
#define COARSEGRAINFREQUENCYSERIESH_MSGENONPOSDELTAF "Negative or zero frequency spacing" 
#define COARSEGRAINFREQUENCYSERIESH_MSGENEGFMIN "Negative start frequency" 
#define COARSEGRAINFREQUENCYSERIESH_MSGEMMHETERO   "Mismatch in heterodyning frequencies"
#define COARSEGRAINFREQUENCYSERIESH_MSGEMMFMIN     "Mismatch in start frequencies"
#define COARSEGRAINFREQUENCYSERIESH_MSGEMMDELTAF   "Mismatch in frequency spacings"
#define COARSEGRAINFREQUENCYSERIESH_MSGEMMLEN      "Mismatch in sequence lengths"
#define COARSEGRAINFREQUENCYSERIESH_MSGEOORCOARSE  "Coarse-graining paramaters out of range"

/************************************ </lalErrTable> */

  /*************************************************************
   *                                                           *
   *       Structures and prototypes associated with           *
   *             CoarseGrainFrequencySeries.c                  *
   *                                                           *
   *************************************************************/

/********************************************************** <lalLaTeX>

\subsubsection*{\texttt{struct FrequencySamplingParams}}
\index{\texttt{FrequencySamplingParams}}

\noindent 
Contains the parameters needed to specify the sampling of a
frequency series:
 
\begin{description}
\item[\texttt{UINT4 length}]
The number of points in the frequency series.

\item[\texttt{REAL8 f0}]
The start frequency of the frequency series.

\item[\texttt{REAL8 deltaF}]
The frequency spacing of the frequency series.
\end{description}

*********************************************************** </lalLaTeX> */

typedef struct
tagFrequencySamplingParams
{
  REAL8         f0;
  REAL8         deltaF;
  UINT4         length;
}
FrequencySamplingParams;

void
LALSCoarseGrainFrequencySeries(LALStatus                      *status,
			       REAL4FrequencySeries           *output,
			       const REAL4FrequencySeries     *input,
			       const FrequencySamplingParams  *params);

#ifdef  __cplusplus
}
#endif /* C++ protection */

/********************************************************** <lalLaTeX>

\vfill{\footnotesize\input{CoarseGrainFrequencySeriesHV}}

\newpage\input{CoarseGrainFrequencySeriesC}
\newpage\input{CoarseGrainFrequencySeriesTestC}
\newpage\input{ZeroPadAndFFTC}
\newpage\input{SZeroPadAndFFTTestC}
\newpage\input{StochasticOptimalFilterC}
\newpage\input{StochasticOptimalFilterTestC}
\newpage\input{StochasticInverseNoiseC}
\newpage\input{StochasticInverseNoiseTestC}
\newpage\input{StochasticOmegaGWC}
\newpage\input{StochasticOmegaGWTestC}
\newpage\input{OverlapReductionFunctionC}
\newpage\input{OverlapReductionFunctionTestC}

*********************************************************** </lalLaTeX> */
