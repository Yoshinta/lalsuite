#if 0
<lalVerbatim file="GenerateInspiralCV">
Author: Thomas Cokelaer
$Id$
</lalVerbatim>
#endif

#if 0
<lalLaTeX>
\subsection{Module \texttt{GenerateInspiral.c}}
\label{ss:GenerateInspiral.c}
\noindent Generates a CoherentGW inspiral waveform for injection.

\subsubsection*{Prototypes}
\vspace{0.1in}
\input{LALGenerateInspiralCP}
\input{LALGetApproxFromStringCP}
\input{LALGetOrderFromStringCP}
\input{LALGenerateInspiralPopulatePPNCP}
\input{LALGenerateInspiralPopulateInspiralCP}


\idx{LALGenerateInspiral}
\idx{LALGetApproxFromString}
\idx{LALGetOrderFromString}
\idx{LALGenerateInspiralPopulatePPN}
\idx{LALGenerateInspiralPopulateInspiral}


\begin{description}
\item[\texttt{LALGenerateInspiral()}] create an inspiral binary
waveform generated either by the \texttt{inspiral} package (EOB,
PadeT1, TaylorT1, TaylorT2, TaylorT3, SpinTaylor) or the
\texttt{inject} package	(GeneratePPN).	It is used in the module
\texttt{FindChirpSimulation} in \texttt{findchirp} package.
 
There are three  parsed arguments
\begin{itemize}
\item a \texttt{CoherentGW}  structure which stores amplitude, 
frequency and phase of the  waveform (output)
\item a \texttt{thisEvent}  structure which provides some 
waveform parameters (input)
\item a \texttt{PPNParamStruc} which gives some input 
parameters needed by the GeneratePPN waveform  generation. That 
arguments is also used as an output by all the different
approximant  (output/input).
\end{itemize}

The input must be composed of a valid thisEvent structure as well as 
the  variable deltaT of the PPNparamsStruct. All others variables 
of the PPNParamStruc are populated within that function.

\item[\texttt{LALGetOrderFromString()}] convert a string  
provided by the \texttt{CoherentGW} structure in order to retrieve the 
order of the waveform to generate. 

\item[\texttt{LALGetApproximantFromString()}] convert a string  
provided by the \texttt{CoherentGW} structure in order to retrieve the 
approximant of the waveform to generate. 

\item[\texttt{LALGenerateInspiralPopulatePPN()}] Populate the 
PPNParamsStruc with the input argument \texttt{thisEvent}. That 
structure is used by both inspiral waveforms inject waveforms.

\item[\texttt{LALGenerateInspiralPopulateInspiral()}]  Populate the 
InspiralTemplate structure if the model chosen belongs to the 
inspiral package.

\end{description}

\subsubsection*{Algorithm}
\noindent None.

\subsubsection*{Notes}
Inject only time-domain waveforms for the time being such as GeneratePPN, 
TaylorT1, TaylorT2, TaylorT3, PadeT1 and EOB , Spintaylor..
\subsubsection*{Uses}
\begin{verbatim}
None.
\end{verbatim}
  
\vfill{\footnotesize\input{GenerateInspiralCV}}
</lalLaTeX> 
#endif

#include <lal/LALInspiral.h>
#include <lal/LALStdlib.h>
#include <lal/GenerateInspiral.h>
#include <lal/GeneratePPNInspiral.h>
#include <lal/SeqFactories.h>
#include <lal/Units.h>

NRCSID( GENERATEINSPIRALC,
"$Id$" );

/* <lalVerbatim file="LALGenerateInspiralCP"> */
void 
LALGenerateInspiral(
    LALStatus		*status,
    CoherentGW		*waveform,
    SimInspiralTable	*thisEvent,
    PPNParamStruc	*ppnParams
    )
/* </lalVerbatim> */
{
  Order	            order;              /* Order of the model             */
  Approximant       approximant;        /* And its approximant value      */
  InspiralTemplate  inspiralParams;     /* structure for inspiral package */
  CHAR              warnMsg[1024];

  INITSTATUS(status, "LALGenerateInspiral",GENERATEINSPIRALC);
  ATTATCHSTATUSPTR(status);

  ASSERT(thisEvent, status,
      GENERATEINSPIRALH_ENULL, GENERATEINSPIRALH_MSGENULL);

  /* read the event waveform approximant and order */
  LALGetApproximantFromString(status->statusPtr, thisEvent->waveform,
      &approximant);
  CHECKSTATUSPTR(status);

  LALGetOrderFromString(status->statusPtr, thisEvent->waveform, &order);
  CHECKSTATUSPTR(status);

  /* when entering here, approximant is in principle well defined.  */
  /* We dont need any else if or ABORT in the if statement.         */
  /* Depending on the apporixmant we use inject or inspiral package */
  if ( approximant == GeneratePPN )
  {
    /* fill structure with input parameters */
    LALGenerateInspiralPopulatePPN(status->statusPtr, ppnParams, thisEvent);
    CHECKSTATUSPTR(status);

    /* generate PPN waveform */
    LALGeneratePPNInspiral(status->statusPtr, waveform, ppnParams);
    CHECKSTATUSPTR(status);
  }
  else
  {
    inspiralParams.approximant = approximant;
    inspiralParams.order       = order;

    /* We fill ppnParams */
    LALGenerateInspiralPopulatePPN(status->statusPtr, ppnParams, thisEvent);
    CHECKSTATUSPTR(status);

    /* we fill inspiralParams structure as well.*/
    LALGenerateInspiralPopulateInspiral(status->statusPtr, &inspiralParams,
        thisEvent, ppnParams);
    CHECKSTATUSPTR(status);

    /* the waveform generation itself */
    LALInspiralWaveForInjection(status->statusPtr, waveform, &inspiralParams,
        ppnParams);
    /* we populate the simInspiral table with the fFinal needed for 
       template normalisation. */
    thisEvent->f_final = inspiralParams.fFinal;
    CHECKSTATUSPTR(status);
  }

  /* If no waveform has been generated */
  if ( waveform->a == NULL )
  {	
    LALSnprintf( warnMsg, sizeof(warnMsg)/sizeof(*warnMsg),
        "No waveform generated (check lower frequency)\n");
    LALInfo( status, warnMsg );
    ABORT( status, LALINSPIRALH_ENOWAVEFORM, LALINSPIRALH_MSGENOWAVEFORM );
  }


  /* If sampling problem */
  if ( ppnParams->dfdt > 2.0 )
  {
    LALSnprintf( warnMsg, sizeof(warnMsg)/sizeof(*warnMsg),
        "Waveform sampling interval is too large:\n"
        "\tmaximum df*dt = %f", ppnParams->dfdt );
    LALInfo( status, warnMsg );
    ABORT( status, GENERATEINSPIRALH_EDFDT, GENERATEINSPIRALH_MSGEDFDT );
  }

  /* Some info should add everything (spin and so on) */
  LALSnprintf( warnMsg, sizeof(warnMsg)/sizeof(*warnMsg),
      "Injected waveform parameters:\n"
      "ppnParams->mTot\t= %e\n"
      "ppnParams->eta\t= %e\n"
      "ppnParams->d\t= %e\n"
      "ppnParams->inc\t= %e\n"
      "ppnParams->phi\t= %e\n"
      "ppnParams->psi\t= %e\n"
      "ppnParams->fStartIn\t= %e\n"
      "ppnParams->fStopIn\t= %e\n"
      "ppnParams->position.longitude\t= %e\n"
      "ppnParams->position.latitude\t= %e\n"
      "ppnParams->position.system\t= %e\n"
      "ppnParams->epoch.gpsSeconds\t= %e\n" 
      "ppnParams->epoch.gpsNanoSeconds\t= %e\n"
      "ppnParams->tC\t= %e\n"
      "ppnParams->dfdt\t =%e\n", 
      ppnParams->mTot, 
      ppnParams->eta, 
      ppnParams->d,
      ppnParams->inc,
      ppnParams->phi,
      ppnParams->psi, 
      ppnParams->fStartIn,
      ppnParams->fStopIn,	
      ppnParams->position.longitude,
      ppnParams->position.latitude,
      ppnParams->position.system,
      ppnParams->epoch.gpsSeconds,	
      ppnParams->epoch.gpsNanoSeconds,
      ppnParams->tc,
      ppnParams->dfdt );
  LALInfo( status, warnMsg );

  DETATCHSTATUSPTR( status );
  RETURN( status );
}


/* <lalVerbatim file="LALGetOrderFromStringCP"> */
void 
LALGetOrderFromString(
    LALStatus *status,
    CHAR      *thisEvent,
    Order     *order
    )
/* </lalVerbatim> */
{
  CHAR  warnMsg[1024];

  INITSTATUS( status, "LALGetOrderFromString", GENERATEINSPIRALC );
  ATTATCHSTATUSPTR( status );
  
  ASSERT( thisEvent, status, 
      GENERATEINSPIRALH_ENULL, GENERATEINSPIRALH_MSGENULL );
 
  if ( strstr(thisEvent, "newtonian") )
  {
    *order = newtonian;
  }
  else if ( strstr(thisEvent, "oneHalfPN") )
  {
    *order = oneHalfPN;
  }
  else if ( strstr(thisEvent, "onePN") )
  {
    *order = onePN;
  }
  else if ( strstr(thisEvent, "onePointFivePN") )
  {
    *order = onePointFivePN;
  }
  else if ( strstr(thisEvent, "twoPN") )
  {
    *order = twoPN;
  }
  else if ( strstr(thisEvent, "twoPointFivePN") )
  {
    *order = twoPointFivePN;
  }
  else if (strstr(thisEvent, "threePN") )
  {
    *order = threePN;
  }
  else if ( strstr(thisEvent, 	"threePointFivePN") )
  {
    *order = threePointFivePN;
  }
  else
  {
    LALSnprintf( warnMsg, sizeof(warnMsg)/sizeof(*warnMsg),
        "Cannot parse order from string: %s\n", thisEvent );
    LALInfo( status, warnMsg );
    ABORT(status, LALINSPIRALH_EORDER, LALINSPIRALH_MSGEORDER);
  }
  
  DETATCHSTATUSPTR( status );
  RETURN( status );
}


/* <lalVerbatim file="LALGetApproxFromStringCP"> */
void 
LALGetApproximantFromString(
    LALStatus   *status,
    CHAR        *thisEvent,
    Approximant *approximant
    )
/* </lalVerbatim> */
{
  /* Function to search for the approximant into a string */
  CHAR warnMsg[1024];

  INITSTATUS( status, "LALGenerateInspiralGetApproxFromString",
      GENERATEINSPIRALC );
  ATTATCHSTATUSPTR( status );

  ASSERT( thisEvent, status,
      GENERATEINSPIRALH_ENULL, GENERATEINSPIRALH_MSGENULL );

  if ( strstr(thisEvent, "TaylorT1" ) )
  {
    *approximant = TaylorT1;
  }
  else if ( strstr(thisEvent, "TaylorT2" ) )
  {
    *approximant = TaylorT2;
  }
  else if ( strstr(thisEvent, "TaylorT3" ) )
  {
    *approximant = TaylorT3;
  }
  else if ( strstr(thisEvent, "EOB" ) )
  {
    *approximant = EOB;
  }
  else if ( strstr(thisEvent, "SpinTaylor" ) )
  {
    *approximant = SpinTaylor;
  }
  else if ( strstr(thisEvent, "PadeT1" ) )
  {
    *approximant = PadeT1;
  }
  else if ( strstr(thisEvent, "GeneratePPN" ) )
  {
    *approximant = GeneratePPN;
  }
  else
  {
    LALSnprintf( warnMsg, sizeof(warnMsg)/sizeof(*warnMsg),
        "Cannot parse approximant from string: %s \n", thisEvent );
    LALInfo( status, warnMsg );
    ABORT( status, LALINSPIRALH_EAPPROXIMANT, LALINSPIRALH_MSGEAPPROXIMANT );
  }

  DETATCHSTATUSPTR( status );
  RETURN( status );
}


/* <lalVerbatim file="LALGenerateInspiralPopulatePPNCP"> */
void
LALGenerateInspiralPopulatePPN(
    LALStatus             *status,
    PPNParamStruc         *ppnParams,
    SimInspiralTable      *thisEvent
    )
/* </lalVerbatim> */
{
  CHAR warnMsg[1024];

  INITSTATUS( status, "LALGenerateInspiralPopulatePPN", GENERATEINSPIRALC );
  ATTATCHSTATUSPTR( status );

  /* input fields */
  ppnParams->mTot = thisEvent->mass1 + thisEvent->mass2;
  ppnParams->eta  = thisEvent->eta;
  ppnParams->d    = thisEvent->distance* 1.0e6 * LAL_PC_SI; /*in Mpc*/
  ppnParams->inc  = thisEvent->inclination;
  ppnParams->phi  = thisEvent->coa_phase;

  /* frequency cutoffs */
  if ( thisEvent->f_lower > 0 )
  {
    ppnParams->fStartIn = thisEvent->f_lower;
  }
  else
  {
    ppnParams->fStartIn = GENERATEINSPIRAL_DEFAULT_FLOWER;
  }
  ppnParams->fStopIn  = -1.0 /
    (6.0 * sqrt(6.0) * LAL_PI * ppnParams->mTot * LAL_MTSUN_SI);

  /* passed fields */
  ppnParams->position.longitude   = thisEvent->longitude;
  ppnParams->position.latitude    = thisEvent->latitude;
  ppnParams->position.system      = COORDINATESYSTEM_EQUATORIAL;
  ppnParams->psi                  = thisEvent->polarization;
  ppnParams->epoch.gpsSeconds     = 0;
  ppnParams->epoch.gpsNanoSeconds = 0;

  DETATCHSTATUSPTR( status );
  RETURN( status );
}


/* <lalVerbatim file="LALGenerateInspiralPopulateInspiralCP"> */
void 
LALGenerateInspiralPopulateInspiral(
    LALStatus           *status,
    InspiralTemplate    *inspiralParams,
    SimInspiralTable    *thisEvent,
    PPNParamStruc       *ppnParams
    )
     
/* </lalVerbatim> */
{
  INITSTATUS( status, "LALGenerateInspiralPopulateInspiral",
      GENERATEINSPIRALC );
  ATTATCHSTATUSPTR( status );

  /* --- Let's fill the inspiral structure now --- */
  inspiralParams->mass1	  =  thisEvent->mass1;  	/* masses 1 */
  inspiralParams->mass2	  =  thisEvent->mass2;  	/* masses 2 */
  inspiralParams->fLower  =  ppnParams->fStartIn; /* lower cutoff frequency */
  inspiralParams->fCutoff = 1./ (ppnParams->deltaT)/2.-1; 

  /* -1 to be  in agreement with the inspiral assert. */
  inspiralParams->tSampling	  = 1./ (ppnParams->deltaT); /* sampling*/
  inspiralParams->signalAmplitude = 1.;
  inspiralParams->distance	  =  thisEvent->distance * LAL_PC_SI * 1e6;
 
  /* distance in Mpc */
  inspiralParams->startTime	  =  0.0;
  inspiralParams->startPhase	  =  thisEvent->coa_phase;
  inspiralParams->startPhase      = 0.0;

  inspiralParams->OmegaS = GENERATEINSPIRAL_OMEGAS;/* EOB 3PN contribution */
  inspiralParams->Theta	 = GENERATEINSPIRAL_THETA; /* EOB 3PN contribution */
  inspiralParams->Zeta2	 = GENERATEINSPIRAL_ZETA2; /* EOB 3PN contribution */

  inspiralParams->alpha	 = -1.;      /* bcv useless for the time being */
  inspiralParams->psi0	 = -1.;      /* bcv useless for the time being */
  inspiralParams->psi3	 = -1.;      /* bcv useless for the time being */
  inspiralParams->alpha1 = -1.;      /* bcv useless for the time being */
  inspiralParams->alpha2 = -1.;      /* bcv useless for the time being */
  inspiralParams->beta	 = -1.;      /* bcv useless for the time being */

  /* inclination of the binary */
  inspiralParams->inclination =  thisEvent->inclination;
  inspiralParams->ieta	    =  1;
  inspiralParams->nStartPad =  0;
  /* increased end padding from zero so that longer waveforms do not
  have errors due to underestimation of number of bins requred */
  inspiralParams->nEndPad   =  16384;

  inspiralParams->massChoice  = m1Andm2;

  /* spin parameters */
  inspiralParams->sourceTheta = GENERATEINSPIRAL_SOURCETHETA;
  inspiralParams->sourcePhi   = GENERATEINSPIRAL_SOURCEPHI;
  inspiralParams->spin1[0]    = thisEvent->spin1x;
  inspiralParams->spin2[0]    = thisEvent->spin2x;
  inspiralParams->spin1[1]    = thisEvent->spin1y;
  inspiralParams->spin2[1]    = thisEvent->spin2y;
  inspiralParams->spin1[2]    = thisEvent->spin1z;
  inspiralParams->spin2[2]    = thisEvent->spin2z;

  /* theta0 can not be equal to zero for SpinTaylor injections */
  if ( inspiralParams->approximant == SpinTaylor && thisEvent->theta0 == 0 )
  {
    ABORT( status, GENERATEINSPIRALH_EZERO, GENERATEINSPIRALH_MSGEZERO );
  }
  inspiralParams->orbitTheta0 = thisEvent->theta0;
  inspiralParams->orbitPhi0   = thisEvent->phi0;

  DETATCHSTATUSPTR( status );
  RETURN( status );
}
