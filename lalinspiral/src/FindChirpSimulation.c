/*
*  Copyright (C) 2007 Duncan Brown, Eirini Messaritaki, Gareth Jones, Jolien Creighton, Patrick Brady, Reinhard Prix, Anand Sengupta, Stephen Fairhurst, Craig Robinson , Thomas Cokelaer
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
 * File Name: FindChirpSimulation.c
 *
 * Author: Brown, D. A., and Creighton, T. D.
 *
 *
 *-----------------------------------------------------------------------
 */

/**
 * \author Brown, D. A. and Creighton, T. D
 * \file
 * \ingroup FindChirp_h
 *
 * \brief Provides an interface between code build from \c findchirp and
 * various simulation packages for injecting chirps into data.
 *
 * ### Prototypes ###
 *
 * <dl>
 * <dt><tt>LALFindChirpInjectSignals()</tt></dt><dd> injects the signals described
 * in the linked list of \c SimInspiralTable structures \c events
 * into the data \c chan. The response function \c resp should
 * contain the response function to use when injecting the signals into the data.</dd>
 * </dl>
 *
 * ### Algorithm ###
 *
 * None.
 *
 * ### Notes ###
 *
 *
 * ### Uses ###
 *
 * \code
 * LALCalloc()
 * LALFree()
 * \endcode
 *
 * ### Notes ###
 *
 */

#include <lal/Units.h>
#include <lal/Date.h>
#include <lal/AVFactories.h>
#include <lal/VectorOps.h>
#include <lal/SeqFactories.h>
#include <lal/DetectorSite.h>
#include <lal/GenerateInspiral.h>
#include <lal/GeneratePPNInspiral.h>
#include <lal/SimulateCoherentGW.h>
#include <lal/LIGOMetadataTables.h>
#include <lal/LIGOMetadataUtils.h>
#include <lal/LIGOMetadataInspiralUtils.h>
#include <lal/LIGOMetadataRingdownUtils.h>
#include <lal/LALInspiralBank.h>
#include <lal/FindChirp.h>
#include <lal/LALStdlib.h>
#include <lal/LALInspiralBank.h>
#include <lal/GenerateInspiral.h>
#include <lal/NRWaveInject.h>
#include <lal/GenerateInspRing.h>
#include <math.h>
#include <lal/LALInspiral.h>
#include <lal/LALError.h>
#include <lal/TimeSeries.h>
#include <lal/LALSimulation.h>

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

static int FindTimeSeriesStartAndEnd (
              REAL4Vector *signalvec,
              UINT4 *start,
              UINT4 *end
             );


void
LALFindChirpInjectSignals (
    LALStatus                  *status,
    REAL4TimeSeries            *chan,
    SimInspiralTable           *events,
    COMPLEX8FrequencySeries    *resp
    )

{
  UINT4                 k;
  DetectorResponse      detector;
  SimInspiralTable     *thisEvent = NULL;
  PPNParamStruc         ppnParams;
  CoherentGW            waveform;
  INT8                  waveformStartTime;
  REAL4TimeSeries       signalvec;
  COMPLEX8Vector       *unity = NULL;
  CHAR                  warnMsg[512];
  CHAR                  ifo[LIGOMETA_IFO_MAX];
  REAL8                 timeDelay;

  INITSTATUS(status);
  ATTATCHSTATUSPTR( status );

  ASSERT( chan, status,
      FINDCHIRPH_ENULL, FINDCHIRPH_MSGENULL );
  ASSERT( chan->data, status,
      FINDCHIRPH_ENULL, FINDCHIRPH_MSGENULL );
  ASSERT( chan->data->data, status,
      FINDCHIRPH_ENULL, FINDCHIRPH_MSGENULL );

  ASSERT( events, status,
      FINDCHIRPH_ENULL, FINDCHIRPH_MSGENULL );

  ASSERT( resp, status,
      FINDCHIRPH_ENULL, FINDCHIRPH_MSGENULL );
  ASSERT( resp->data, status,
      FINDCHIRPH_ENULL, FINDCHIRPH_MSGENULL );
  ASSERT( resp->data->data, status,
      FINDCHIRPH_ENULL, FINDCHIRPH_MSGENULL );


  /*
   *
   * set up structures and parameters needed
   *
   */


  /* fixed waveform injection parameters */
  memset( &ppnParams, 0, sizeof(PPNParamStruc) );
  ppnParams.deltaT   = chan->deltaT;
  ppnParams.lengthIn = 0;
  ppnParams.ppn      = NULL;


  /*
   *
   * compute the transfer function from the given response function
   *
   */


  /* allocate memory and copy the parameters describing the freq series */
  memset( &detector, 0, sizeof( DetectorResponse ) );
  detector.transfer = (COMPLEX8FrequencySeries *)
    LALCalloc( 1, sizeof(COMPLEX8FrequencySeries) );
  if ( ! detector.transfer )
  {
    ABORT( status, FINDCHIRPH_EALOC, FINDCHIRPH_MSGEALOC );
  }
  memcpy( &(detector.transfer->epoch), &(resp->epoch),
      sizeof(LIGOTimeGPS) );
  detector.transfer->f0 = resp->f0;
  detector.transfer->deltaF = resp->deltaF;

  detector.site = (LALDetector *) LALMalloc( sizeof(LALDetector) );
  /* set the detector site */
  switch ( chan->name[0] )
  {
    case 'H':
      *(detector.site) = lalCachedDetectors[LALDetectorIndexLHODIFF];
      LALWarning( status, "computing waveform for Hanford." );
      strcpy(ifo, "H1");
      break;
    case 'L':
      *(detector.site) = lalCachedDetectors[LALDetectorIndexLLODIFF];
      LALWarning( status, "computing waveform for Livingston." );
      strcpy(ifo, "L1");
      break;
    case 'G':
      *(detector.site) = lalCachedDetectors[LALDetectorIndexGEO600DIFF];
      LALWarning( status, "computing waveform for GEO600." );
      strcpy(ifo, "G1");
      break;
    case 'T':
      *(detector.site) = lalCachedDetectors[LALDetectorIndexTAMA300DIFF];
      LALWarning( status, "computing waveform for TAMA300." );
      strcpy(ifo, "T1");
      break;
    case 'V':
      *(detector.site) = lalCachedDetectors[LALDetectorIndexVIRGODIFF];
      LALWarning( status, "computing waveform for Virgo." );
      strcpy(ifo, "V1");
      break;
    default:
      LALFree( detector.site );
      detector.site = NULL;
      LALWarning( status, "Unknown detector site, computing plus mode "
          "waveform with no time delay" );
      break;
  }

  /* set up units for the transfer function */
  if (XLALUnitDivide( &(detector.transfer->sampleUnits),
                      &lalADCCountUnit, &lalStrainUnit ) == NULL) {
    ABORTXLAL(status);
  }

  /* invert the response function to get the transfer function */
  LALCCreateVector( status->statusPtr, &( detector.transfer->data ),
      resp->data->length );
  CHECKSTATUSPTR( status );

  LALCCreateVector( status->statusPtr, &unity, resp->data->length );
  CHECKSTATUSPTR( status );
  for ( k = 0; k < resp->data->length; ++k )
  {
    unity->data[k] = 1.0;
  }

  LALCCVectorDivide( status->statusPtr, detector.transfer->data, unity,
      resp->data );
  CHECKSTATUSPTR( status );

  LALCDestroyVector( status->statusPtr, &unity );
  CHECKSTATUSPTR( status );


  /*
   *
   * loop over the signals and inject them into the time series
   *
   */


  for ( thisEvent = events; thisEvent; thisEvent = thisEvent->next )
  {
    /*
     *
     * generate waveform and inject it into the data
     *
     */


    /* clear the waveform structure */
    memset( &waveform, 0, sizeof(CoherentGW) );

    LALGenerateInspiral(status->statusPtr, &waveform, thisEvent, &ppnParams );
    CHECKSTATUSPTR( status );

    LALInfo( status, ppnParams.termDescription );

    if ( strstr( thisEvent->waveform, "KludgeIMR") ||
         strstr( thisEvent->waveform, "KludgeRingOnly") )
     {
       CoherentGW *wfm;
       SimRingdownTable *ringEvent;
       int injectSignalType = LALRINGDOWN_IMR_INJECT;


       ringEvent = (SimRingdownTable *)
         LALCalloc( 1, sizeof(SimRingdownTable) );
       wfm = XLALGenerateInspRing( &waveform, thisEvent, ringEvent,
           injectSignalType);
       LALFree(ringEvent);

       if ( !wfm )
       {
         LALInfo( status, "Unable to generate merger/ringdown, "
             "injecting inspiral only");
         ABORT( status, FINDCHIRPH_EIMRW, FINDCHIRPH_MSGEIMRW );
       }
       waveform = *wfm;
     }


    if ( thisEvent->geocent_end_time.gpsSeconds )
    {
      /* get the gps start time of the signal to inject */
      waveformStartTime = XLALGPSToINT8NS( &(thisEvent->geocent_end_time) );
      waveformStartTime -= (INT8) ( 1000000000.0 * ppnParams.tc );
    }
    else
    {
      LALInfo( status, "Waveform start time is zero: injecting waveform "
          "into center of data segment" );

      /* center the waveform in the data segment */
      waveformStartTime = XLALGPSToINT8NS( &(chan->epoch) );

      waveformStartTime += (INT8) ( 1000000000.0 *
          ((REAL8) (chan->data->length - ppnParams.length) / 2.0) * chan->deltaT
          );
    }

    snprintf( warnMsg, XLAL_NUM_ELEM(warnMsg),
        "Injected waveform timing:\n"
        "thisEvent->geocent_end_time.gpsSeconds = %d\n"
        "thisEvent->geocent_end_time.gpsNanoSeconds = %d\n"
        "ppnParams.tc = %e\n"
        "waveformStartTime = %" LAL_INT8_FORMAT "\n",
        thisEvent->geocent_end_time.gpsSeconds,
        thisEvent->geocent_end_time.gpsNanoSeconds,
        ppnParams.tc,
        waveformStartTime );
    LALInfo( status, warnMsg );

    /* if we generated waveform.h then use LALInjectStrainGW
       otherwise use SimulateCoherentGW */
    if( waveform.h == NULL)
    {
      /* clear the signal structure */
      memset( &signalvec, 0, sizeof(REAL4TimeSeries) );

      /* set the start time of the signal vector to the appropriate start time of the injection */
      if ( detector.site )
      {
        timeDelay = XLALTimeDelayFromEarthCenter( detector.site->location, thisEvent->longitude,
          thisEvent->latitude, &(thisEvent->geocent_end_time) );
        if ( XLAL_IS_REAL8_FAIL_NAN( timeDelay ) )
        {
          ABORTXLAL( status );
        }
      }
      else
      {
        timeDelay = 0.0;
      }
      /* Give a little more breathing space to aid band-passing */
      XLALGPSSetREAL8( &(signalvec.epoch), (waveformStartTime * 1.0e-9) - 0.25 + timeDelay );
      UINT4 signalvecLength=waveform.phi->data->length + (UINT4)ceil((0.5+timeDelay)/waveform.phi->deltaT);
      

      /* set the parameters for the signal time series */
      signalvec.deltaT = chan->deltaT;
      if ( ( signalvec.f0 = chan->f0 ) != 0 )
      {
        ABORT( status, FINDCHIRPH_EHETR, FINDCHIRPH_MSGEHETR );
      }
      signalvec.sampleUnits = lalADCCountUnit;

      /* set the start times for injection */
      XLALINT8NSToGPS( &(waveform.a->epoch), waveformStartTime );
      /* put a rug on a polished floor? */
      waveform.f->epoch = waveform.a->epoch;
      waveform.phi->epoch = waveform.a->epoch;
      /* you might as well set a man trap */
      if ( waveform.shift )
      {
        waveform.shift->epoch = waveform.a->epoch;
      }
      /* and to think he'd just come from the hospital */

      /* simulate the detectors response to the inspiral */
      LALSCreateVector( status->statusPtr, &(signalvec.data), signalvecLength );
      CHECKSTATUSPTR( status );

      LALSimulateCoherentGW( status->statusPtr,
          &signalvec, &waveform, &detector );
      CHECKSTATUSPTR( status );
      
      /* Taper the signal */
      {

          if ( ! strcmp( "TAPER_START", thisEvent->taper ) )
          {
              XLALSimInspiralREAL4WaveTaper( signalvec.data, LAL_SIM_INSPIRAL_TAPER_START );
          }
          else if (  ! strcmp( "TAPER_END", thisEvent->taper ) )
          {
              XLALSimInspiralREAL4WaveTaper( signalvec.data, LAL_SIM_INSPIRAL_TAPER_END );
          }
          else if (  ! strcmp( "TAPER_STARTEND", thisEvent->taper ) )
          {
              XLALSimInspiralREAL4WaveTaper( signalvec.data, LAL_SIM_INSPIRAL_TAPER_STARTEND );
          }
          else if ( strcmp( "TAPER_NONE", thisEvent->taper ) )
          {
              XLALPrintError( "Invalid injection tapering option specified: %s\n",
                 thisEvent->taper );
              ABORT( status, LAL_BADPARM_ERR, LAL_BADPARM_MSG );
          }
      }

      /* Band pass the signal */
      if ( thisEvent->bandpass )
      {
          UINT4 safeToBandPass = 0;
          UINT4 start=0, end=0;
          REAL4Vector *bandpassVec = NULL;

          safeToBandPass = FindTimeSeriesStartAndEnd (
                  signalvec.data, &start, &end );

          if ( safeToBandPass )
          {
              /* Check if we can grab some padding at the extremeties.
               * This will make the bandpassing better
               */

              if (((INT4)start - (int)(0.25/chan->deltaT)) > 0 )
                    start -= (int)(0.25/chan->deltaT);
              else
                    start = 0;

              if ((end + (int)(0.25/chan->deltaT)) < signalvec.data->length )
                    end += (int)(0.25/chan->deltaT);
              else
                    end = signalvec.data->length - 1;

              bandpassVec = (REAL4Vector *)
                      LALCalloc(1, sizeof(REAL4Vector) );

              bandpassVec->length = (end - start + 1);
              bandpassVec->data = signalvec.data->data + start;

              if ( XLALBandPassInspiralTemplate( bandpassVec,
                          1.1*thisEvent->f_lower,
                          1.05*thisEvent->f_final,
                          1./chan->deltaT) != XLAL_SUCCESS )
              {
                  LALError( status, "Failed to Bandpass signal" );
                  ABORT (status, LALINSPIRALH_EBPERR, LALINSPIRALH_MSGEBPERR);
              };

              LALFree( bandpassVec );
          }
      }

      /* inject the signal into the data channel */
      int retcode=XLALSimAddInjectionREAL4TimeSeries(chan, &signalvec, NULL);

      if(retcode!=XLAL_SUCCESS){
	ABORTXLAL(status);
      }

    }
    else
    {
      const INT4 wfmLength = waveform.h->data->length;
      INT4 i = wfmLength;
      REAL4 *dataPtr = waveform.h->data->data;
      REAL4 *tmpdata;

      /* XXX This code will BREAK if the first element of the frequency XXX *
       * XXX series does not contain dynRange. This is the case for     XXX *
       * XXX calibrated strain data, but will not be the case when      XXX *
       * XXX filtering uncalibrated data.                               XXX */
      REAL8 dynRange;
      LALWarning (status, "Attempting to calculate dynRange: Will break if un-calibrated strain-data is used.");
      dynRange = 1.0/(crealf(resp->data->data[0]));

      /* set the start times for injection */
      XLALINT8NSToGPS( &(waveform.h->epoch), waveformStartTime );

      /*
       * We are using functions from NRWaveInject which do not take a vector
       * with alternating h+ & hx but rather one that stores h+ in the first
       * half and hx in the second half. That is, we must transpose h.
       *
       * We also must multiply the strain by the distance to be compatible
       * with NRWaveInject.
       */
      if (waveform.h->data->vectorLength != 2)
          LALAbort("expected alternating h+ and hx");
      tmpdata = XLALCalloc(2 * wfmLength, sizeof(*tmpdata));
      for (i=0;i<wfmLength;i++) {
            tmpdata[i] = dataPtr[2*i] * thisEvent->distance;
            tmpdata[wfmLength+i] = dataPtr[2*i+1] * thisEvent->distance;
      }
      memcpy(dataPtr, tmpdata, 2 * wfmLength * sizeof(*tmpdata));
      XLALFree(tmpdata);
      waveform.h->data->vectorLength = wfmLength;
      waveform.h->data->length = 2;

      LALInjectStrainGW( status->statusPtr ,
                                      chan ,
                                waveform.h ,
                                 thisEvent ,
                                       ifo ,
                                  dynRange  );
      CHECKSTATUSPTR( status );

    }




    if ( waveform.shift )
    {
      LALSDestroyVector( status->statusPtr, &(waveform.shift->data) );
      CHECKSTATUSPTR( status );
      LALFree( waveform.shift );
    }

    if( waveform.h )
    {
      LALSDestroyVectorSequence( status->statusPtr, &(waveform.h->data) );
      CHECKSTATUSPTR( status );
      LALFree( waveform.h );
    }
    if( waveform.a )
    {
      LALSDestroyVectorSequence( status->statusPtr, &(waveform.a->data) );
      CHECKSTATUSPTR( status );
      LALFree( waveform.a );
      /*
       * destroy the signal only if waveform.h is NULL as otherwise it won't
       * be created
       * */
      if ( waveform.h == NULL )
      {
	LALSDestroyVector( status->statusPtr, &(signalvec.data) );
        CHECKSTATUSPTR( status );
      }
    }
    if( waveform.f )
    {
      LALSDestroyVector( status->statusPtr, &(waveform.f->data) );
      CHECKSTATUSPTR( status );
      LALFree( waveform.f );
    }
    if( waveform.phi )
    {
      LALDDestroyVector( status->statusPtr, &(waveform.phi->data) );
      CHECKSTATUSPTR( status );
      LALFree( waveform.phi );
    }
  }

  LALCDestroyVector( status->statusPtr, &( detector.transfer->data ) );
  CHECKSTATUSPTR( status );

  if ( detector.site ) LALFree( detector.site );
  LALFree( detector.transfer );

  DETATCHSTATUSPTR( status );
  RETURN( status );
}



INT4
XLALFindChirpSetAnalyzeSegment (
    DataSegmentVector          *dataSegVec,
    SimInspiralTable           *injections
    )

{
  DataSegment      *currentSegment;
  SimInspiralTable *thisInjection;
  INT8              chanStartTime;
  INT8              chanEndTime;
  UINT4             i;
  UINT4             k;

  /* set all segments not to be analyzed by default */
  for ( i = 0; i < dataSegVec->length; ++i )
  {
    /* point to current segment */
    currentSegment = dataSegVec->data + i;
    currentSegment->analyzeSegment = 0;
  }

  /* make sure the sim inspirals are time ordered */
  XLALSortSimInspiral(&injections, XLALCompareSimInspiralByGeocentEndTime);

  /* loop over segments checking for injections into each */
  for ( i = 0; i < dataSegVec->length; ++i )
  {
    /* point to current segment */
    currentSegment = dataSegVec->data + i;

    /* compute the start and end of segment */
    chanStartTime = XLALGPSToINT8NS( &currentSegment->chan->epoch );
    chanEndTime = chanStartTime +
      (INT8) (1e9 * currentSegment->chan->data->length *
              currentSegment->chan->deltaT);

    /* look for injection into segment */
    k = 0;
    thisInjection=injections;
    while (thisInjection)
    {
      INT8 ta = XLALGPSToINT8NS( &thisInjection->geocent_end_time );

      if ( ta > chanStartTime && ta <= chanEndTime )
      {
        currentSegment->analyzeSegment += (UINT4)(pow(2.0,(double)(k)));
      }

      if (ta > chanEndTime)
        break;

      thisInjection=thisInjection->next;
      k = k + 1;
    }
  }

  return 0;
}


INT4
XLALFindChirpTagTemplateAndSegment (
        DataSegmentVector       *dataSegVec,
        InspiralTemplate        *tmpltHead,
        SnglInspiralTable       **events,
        CHAR                    *ifo,
        REAL4                   tdFast,
        UINT4                   *analyseThisTmplt
        )

{
    UINT4                s, t;  /* s over segments and t over templates */
    SnglInspiralTable    *thisEvent = NULL;
    InspiralTemplate     *thisTmplt = NULL;
    DataSegment          *currentSegment = NULL;
    UINT4                flag = 0;
    INT8                 tc, chanStartTime, chanEndTime=0;

#ifndef LAL_NDEBUG
    /* Sanity checks on input arguments for debugging */
    if (!dataSegVec || !tmpltHead || !events || !(*events) ||
          !ifo || !analyseThisTmplt )
       XLAL_ERROR( XLAL_EFAULT );

    if ( tdFast < 0.0 || tdFast > 1.0 )
       XLAL_ERROR( XLAL_EINVAL );
#endif

    /* Do a IFO cut on the coinc list */
    (*events) =  XLALIfoCutSingleInspiral( events, ifo);

    if ( XLALClearErrno() ) {
       XLAL_ERROR( XLAL_EFUNC );
    }

    /* make sure the sngl inspirals are time ordered */
    *events = XLALSortSnglInspiral(*events, LALCompareSnglInspiralByTime);

    /* TODO: Make sure triggers are unique in masses or time */

    /* Loop over the coinc events */
    for (thisEvent=*events; thisEvent; thisEvent=thisEvent->next)
    {
        REAL8    g11, g12, g22;

        tc = XLALGPSToINT8NS( &thisEvent->end );
        flag = 0;

        /* Loop over segments */
        for (s = 0; s < dataSegVec->length; ++s )
        {

           /* point to current segment */
           currentSegment = dataSegVec->data + s;

           /* compute the start and end of segment */
           chanStartTime = XLALGPSToINT8NS( &currentSegment->chan->epoch );
           chanEndTime = chanStartTime +
                (INT8) (1e9 * currentSegment->chan->data->length *
                currentSegment->chan->deltaT);

           if ( tc > chanStartTime && tc <= chanEndTime )
           {
              flag += (1 << s);
           }


        } /* loop over segs */

        /* Check we haven't gone beyond the time of interest */
        if ( !flag )
        {
           if ( tc > chanEndTime )
              break;
           else
              continue;
        }


        /* Projected metric g_ij for this event */
        g11 = thisEvent->Gamma[3] -
           thisEvent->Gamma[1] * thisEvent->Gamma[1]/thisEvent->Gamma[0];

        g12 = thisEvent->Gamma[4] -
           thisEvent->Gamma[1]*thisEvent->Gamma[2]/thisEvent->Gamma[0];

        g22 = thisEvent->Gamma[5] -
           thisEvent->Gamma[2]*thisEvent->Gamma[2]/thisEvent->Gamma[0];

        /* Loop over templates */
        for ( thisTmplt = tmpltHead, t = 0; thisTmplt;
                  thisTmplt = thisTmplt->next, t++ )
        {

            REAL8    dt0, dt3;
            REAL8    match;

            dt0   = thisTmplt->t0 - thisEvent->tau0;
            dt3   = thisTmplt->t3 - thisEvent->tau3;
            match = g11*dt0*dt0 + 2.0*g12*dt0*dt3 + g22*dt3*dt3;
            match = 1.0 - match;


            /* If the match between the template and the event is high enough,
             * we mark the template to analyse the segments. This is achieved
             * using bitwise or. */
            if ( match >= tdFast )
            {
                analyseThisTmplt[t] = analyseThisTmplt[t] | flag;
            }

        } /* loop over templts */


    } /* loop over events */

    return XLAL_SUCCESS;

}




INT4
XLALFindChirpSetFollowUpSegment (
    DataSegmentVector          *dataSegVec,
    SnglInspiralTable          **events
    )

{
  DataSegment       *currentSegment;
  SnglInspiralTable *thisEvent;
  INT8               chanStartTime;
  INT8               chanEndTime;
  UINT4              i;
  UINT4              k;

  /* set all segments not to be analyzed by default */
  for ( i = 0; i < dataSegVec->length; ++i )
  {
    /* point to current segment */
    currentSegment = dataSegVec->data + i;
    currentSegment->analyzeSegment = 0;
  }

  /* make sure the sngl inspirals are time ordered */
  *events = XLALSortSnglInspiral(*events, LALCompareSnglInspiralByTime);

  /* loop over segments checking for injections into each */
  for ( i = 0; i < dataSegVec->length; ++i )
  {
    /* point to current segment */
    currentSegment = dataSegVec->data + i;

    /* compute the start and end of segment */
    chanStartTime = XLALGPSToINT8NS( &currentSegment->chan->epoch );
    chanEndTime = chanStartTime +
      (INT8) (1e9 * currentSegment->chan->data->length *
              currentSegment->chan->deltaT);

    /* look for event into segment */
    k = 0;
    thisEvent = *events;
    while (thisEvent)
    {
      INT8 ta = XLALGPSToINT8NS( &thisEvent->end );

      if ( ta > chanStartTime && ta <= chanEndTime )
      {
        currentSegment->analyzeSegment += 1;
      }

      if (ta > chanEndTime)
        break;

      thisEvent=thisEvent->next;
      k = k + 1;
    }
  }

  return 0;
}


UINT4
XLALCmprSgmntTmpltFlags (
    UINT4 numInjections,
    UINT4 TmpltFlag,
    UINT4 SgmntFlag
    )

{
  UINT4 k1, bitTmplt, bitSgmnt, analyseTag;

  /* To begin with assume that the comparison is false. */
  analyseTag = 0;

  /* Loop over all the injections */
  for (k1=0; k1<numInjections; k1++)
  {
    bitTmplt = (TmpltFlag>>k1)&01;
    bitSgmnt = (SgmntFlag>>k1)&01;
    if (bitSgmnt && (bitTmplt == bitSgmnt))
    {
      analyseTag = 1;
      break;
    }
  }

  return analyseTag;
}



UINT4
XLALFindChirpBankSimInitialize (
    REAL4FrequencySeries       *spec,
    COMPLEX8FrequencySeries    *resp,
    REAL8                       fLow
    )

{
  UINT4 k, cut;
  REAL4 psdMin = 0;
  const REAL8 psdScaleFac = 1.0e-40;

  /* set low frequency cutoff index of the psd and    */
  /* the value the psd at cut                         */
  cut = fLow / spec->deltaF > 1 ? fLow / spec->deltaF : 1;

  psdMin = spec->data->data[cut] *
    ( ( crealf(resp->data->data[cut]) * crealf(resp->data->data[cut]) +
        cimagf(resp->data->data[cut]) * cimagf(resp->data->data[cut]) ) / psdScaleFac );

  /* calibrate the input power spectrum, scale to the */
  /* range of REAL4 and store the as S_v(f)           */
  for ( k = 0; k < cut; ++k )
  {
    spec->data->data[k] = psdMin;
  }
  for ( k = cut; k < spec->data->length; ++k )
  {
    REAL4 respRe = crealf(resp->data->data[k]);
    REAL4 respIm = cimagf(resp->data->data[k]);
    spec->data->data[k] = spec->data->data[k] *
      ( ( respRe * respRe + respIm * respIm ) / psdScaleFac );
  }

  /* set the response function to the sqrt of the inverse */
  /* of the psd scale factor since S_h = |R|^2 S_v        */
  for ( k = 0; k < resp->data->length; ++k )
  {
    resp->data->data[k] = crectf( sqrt( psdScaleFac ), 0 );
  }

  return cut;
}

SimInstParamsTable *
XLALFindChirpBankSimMaxMatch (
    SnglInspiralTable         **bestTmplt,
    REAL4                       matchNorm
    )
{
  SnglInspiralTable     *loudestEvent = NULL;

  /* allocate memory for the loudest event over the template bank */
  loudestEvent = (SnglInspiralTable *) LALCalloc( 1, sizeof(SnglInspiralTable) );

  /* find the loudest snr over the template bank */
  while ( *bestTmplt )
  {
    SnglInspiralTable *tmpEvent = *bestTmplt;

    if ( (*bestTmplt)->snr > loudestEvent->snr )
      memcpy( loudestEvent, *bestTmplt, sizeof(SnglInspiralTable) );

    *bestTmplt = (*bestTmplt)->next;
    LALFree( tmpEvent );
  }

  /* make sure we return a null terminated list with only the best tmplt */
  loudestEvent->next = NULL;
  *bestTmplt = loudestEvent;

  /* return the match for the loudest event */
  return XLALFindChirpBankSimComputeMatch( *bestTmplt, matchNorm );
}


SimInstParamsTable *
XLALFindChirpBankSimComputeMatch (
    SnglInspiralTable   *tmplt,
    REAL4                matchNorm
    )
{
  SimInstParamsTable    *maxMatch = NULL;

  /* create the match output table */
  maxMatch = (SimInstParamsTable *) LALCalloc( 1, sizeof(SimInstParamsTable) );
  snprintf( maxMatch->name, LIGOMETA_SIMINSTPARAMS_NAME_MAX, "match" );

  /* store the match in the sim_inst_params structure */
  maxMatch->value = tmplt->snr / matchNorm;

  return maxMatch;

}

static int FindTimeSeriesStartAndEnd (
        REAL4Vector *signalvec,
        UINT4 *start,
        UINT4 *end
        )
{
  UINT4 i; /* mid, n; indices */
  UINT4 flag, safe = 1;
  UINT4 length;

#ifndef LAL_NDEBUG
  if ( !signalvec )
    XLAL_ERROR( XLAL_EFAULT );

  if ( !signalvec->data )
    XLAL_ERROR( XLAL_EFAULT );
#endif

  length = signalvec->length;

  /* Search for start and end of signal */
  flag = 0;
  i = 0;
  while(flag == 0 && i < length )
  {
      if( signalvec->data[i] != 0.)
      {
          *start = i;
          flag = 1;
      }
      i++;
  }
  if ( flag == 0 )
  {
      return flag;
  }

  flag = 0;
  i = length - 1;
  while(flag == 0)
  {
      if( signalvec->data[i] != 0.)
      {
          *end = i;
          flag = 1;
      }
      i--;
  }

  /* Check we have more than 2 data points */
  if(((*end) - (*start)) <= 1)
  {
      XLALPrintWarning( "Data less than 3 points in this signal!\n" );
      safe = 0;
  }

  return safe;

}
