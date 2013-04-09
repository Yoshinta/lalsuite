

#include <stdio.h>
#include <stdlib.h>

#define LAL_USE_OLD_COMPLEX_STRUCTS
#include <lal/LALStdio.h>
#include <lal/LALStdlib.h>

#include <lal/LALInspiral.h>
#include <lal/FrameCache.h>
#include <lal/FrameStream.h>
#include <lal/TimeFreqFFT.h>
#include <lal/LALDetectors.h>
#include <lal/AVFactories.h>
#include <lal/ResampleTimeSeries.h>
#include <lal/TimeSeries.h>
#include <lal/FrequencySeries.h>
#include <lal/Units.h>
#include <lal/Date.h>
#include <lal/StringInput.h>
#include <lal/VectorOps.h>
#include <lal/Random.h>
#include <lal/LALNoiseModels.h>
#include <lal/XLALError.h>
#include <lal/GenerateInspiral.h>
#include <lal/LIGOLwXMLRead.h>
#include <lal/LIGOLwXMLInspiralRead.h>

#include <lal/SeqFactories.h>
#include <lal/DetectorSite.h>
#include <lal/GenerateInspiral.h>
#include <lal/GeneratePPNInspiral.h>
#include <lal/SimulateCoherentGW.h>
#include <lal/Inject.h>
#include <lal/LIGOMetadataTables.h>
#include <lal/LIGOMetadataUtils.h>
#include <lal/LIGOMetadataInspiralUtils.h>
#include <lal/LIGOMetadataRingdownUtils.h>
#include <lal/LALInspiralBank.h>
#include <lal/FindChirp.h>
#include <lal/LALInspiralBank.h>
#include <lal/GenerateInspiral.h>
#include <lal/NRWaveInject.h>
#include <lal/GenerateInspRing.h>
#include <lal/LALErrno.h>
#include <math.h>
#include <lal/LALInspiral.h>
#include <lal/LALSimulation.h>

#include <lal/LALInference.h>
#include <lal/LALInferenceReadData.h>
#include <lal/LALInferenceLikelihood.h>
#include <lal/LALInferenceTemplate.h>
#include <LALInferenceRemoveLines.h>

#define max(a,b) (((a)>(b))?(a):(b))

int LALInferenceRemoveLinesChiSquared(
    REAL8FrequencySeries        *spectrum,
    const REAL8TimeSeries       *tseries,
    UINT4                        seglen,
    UINT4                        stride,
    const REAL8Window           *window,
    const REAL8FFTPlan          *plan,
    REAL8                       *pvalues
    )
{
  REAL8FrequencySeries *work; /* array of frequency series */
  REAL8 *bin; /* array of bin values */
  UINT4 reclen; /* length of entire data record */
  UINT4 numseg;
  UINT4 seg;
  UINT4 k,l;

  if ( ! spectrum || ! tseries || ! plan )
      XLAL_ERROR( XLAL_EFAULT );
  if ( ! spectrum->data || ! tseries->data )
      XLAL_ERROR( XLAL_EINVAL );
  if ( tseries->deltaT <= 0.0 )
      XLAL_ERROR( XLAL_EINVAL );

  reclen = tseries->data->length;
  numseg = 1 + (reclen - seglen)/stride;

  /* consistency check for lengths: make sure that the segments cover the
 *    * data record completely */
  if ( (numseg - 1)*stride + seglen != reclen )
    XLAL_ERROR( XLAL_EBADLEN );
  if ( spectrum->data->length != seglen/2 + 1 )
    XLAL_ERROR( XLAL_EBADLEN );

  /* create frequency series data workspaces */
  work = XLALCalloc( numseg, sizeof( *work ) );
  if ( ! work )
    XLAL_ERROR( XLAL_ENOMEM );
  for ( seg = 0; seg < numseg; ++seg )
  {
    work[seg].data = XLALCreateREAL8Vector( spectrum->data->length );
    if ( ! work[seg].data )
    {
      median_cleanup_REAL8( work, numseg ); /* cleanup */
      XLAL_ERROR( XLAL_EFUNC );
    }
  }

  for ( seg = 0; seg < numseg; ++seg )
  {
    REAL8Vector savevec; /* save the time series data vector */
    int code;

    /* save the time series data vector */
    savevec = *tseries->data;

    /* set the data vector to be appropriate for the even segment */
    tseries->data->length  = seglen;
    tseries->data->data   += seg * stride;

    /* compute the modified periodogram for the even segment */
    code = XLALREAL8ModifiedPeriodogram( work + seg, tseries, window, plan );

    /* restore the time series data vector to its original state */
    *tseries->data = savevec;

    /* now check for failure of the XLAL routine */
    if ( code == XLAL_FAILURE )
    {
      median_cleanup_REAL8( work, numseg ); /* cleanup */
      XLAL_ERROR( XLAL_EFUNC );
    }
  }

  /* create array to hold a particular frequency bin data */
  bin = XLALMalloc( numseg * sizeof( *bin ) );
  if ( ! bin )
  {
    median_cleanup_REAL8( work, numseg ); /* cleanup */
    XLAL_ERROR( XLAL_ENOMEM );
  }

  double meanVal, normVal, chisqrVal;

  int numBins = 100;
  int binIndex;
  float Observed[numBins];
  float Expected[numBins];
  int min = 0; int max = 100;
  int count;
  float interval = (float)(max - min ) / numBins;
  double CriticalValue = 0.0;
  double XSqr, XSqrTerm, XSqrVal;
  double bin_val, expected_val;

  for ( k = 0; k < spectrum->data->length; ++k ) {
      pvalues[k] = 0.0;
  }
  

  /* now loop over frequency bins and compute the median-mean */
  for ( k = 0; k < spectrum->data->length; ++k )
  {
    /* assign array of even segment values to bin array for this freq bin */
    for ( seg = 0; seg < numseg; ++seg ) {
      bin[seg] = work[seg].data->data[k];
      meanVal = meanVal + work[seg].data->data[k];
    }

    meanVal = meanVal / (double) numseg;

    for ( l = 0; l < numBins; ++l ) {
        Observed[l] = 0.0;
        Expected[l] = 0.0;
    }

    count = 0.0;
    for ( seg = 0; seg < numseg; ++seg ) {
      normVal = 2 * bin[seg] / meanVal;
      binIndex = (int)((normVal- min)/interval);

      Observed[binIndex] = Observed[binIndex] + 1;
      count = count + 1;
    }  

    CriticalValue = 0.0;

    for ( l = 0; l < numBins; ++l ) {

      bin_val = l*interval + min;
      expected_val = count * chisqr(2,bin_val);

      Expected[l] = expected_val;

      XSqr = Observed[l] - Expected[l];

      XSqrTerm = (float) ((XSqr * XSqr) / Expected[l]);

      CriticalValue = CriticalValue + XSqrTerm;

      /*printf("%f %f %f %f %f %f\n",bin_val,Observed[l],Expected[l],XSqr,XSqrTerm,CriticalValue); */ 
  
    }

    XSqrVal = chisqr(count-1,CriticalValue);
    pvalues[k] = XSqrVal;

  }

  /* free the workspace data */
  XLALFree( bin );
  median_cleanup_REAL8( work, numseg );

  return 0;
}

static void median_cleanup_REAL8( REAL8FrequencySeries *work, UINT4 n )
{
  int saveErrno = xlalErrno;
  UINT4 i;
  for ( i = 0; i < n; ++i )
    if ( work[i].data )
      XLALDestroyREAL8Vector( work[i].data );
  XLALFree( work );
  xlalErrno = saveErrno;
  return;
}

static int compare_REAL8( const void *p1, const void *p2 )
{
  REAL8 x1 = *(const REAL8 *)p1;
  REAL8 x2 = *(const REAL8 *)p2;
  return (x1 > x2) - (x1 < x2);
}

double chisqr(int Dof, double Cv)
{
    if(Cv < 0 || Dof < 1)
    {
        return 0.0;
    }
    double K = ((double)Dof) * 0.5;
    double X = Cv * 0.5;
    if(Dof == 2)
    {
	return exp(-1.0 * X);
    }
 
    double PValue = igf(K, X);
    //if(isnan(PValue) || isinf(PValue) || PValue <= 1e-8)
    if(isnan(PValue) || isinf(PValue))
    {
        return 1e-14;
    } 

    //PValue /= gamma(K);
    PValue /= tgamma(K); 

    return (1.0 - PValue);
}


static double igf(double S, double Z)
{
    if(Z < 0.0)
    {
	return 0.0;
    }
    double Sc = (1.0 / S);
    Sc *= pow(Z, S);
    Sc *= exp(-Z);
 
    double Sum = 1.0;
    double Nom = 1.0;
    double Denom = 1.0;
 
    for(int k = 0; k < 200; k++)
    {
	Nom *= Z;
	S++;
	Denom *= S;
	Sum += (Nom / Denom);
    }
 
    return Sum * Sc;
}

int LALInferenceRemoveLinesKS(
    REAL8FrequencySeries        *spectrum,
    const REAL8TimeSeries       *tseries,
    UINT4                        seglen,
    UINT4                        stride,
    const REAL8Window           *window,
    const REAL8FFTPlan          *plan,
    REAL8                       *pvalues
    )
{
  REAL8FrequencySeries *work; /* array of frequency series */
  REAL8 *bin; /* array of bin values */
  UINT4 reclen; /* length of entire data record */
  UINT4 numseg;
  UINT4 seg;
  UINT4 k,l;

  if ( ! spectrum || ! tseries || ! plan )
      XLAL_ERROR( XLAL_EFAULT );
  if ( ! spectrum->data || ! tseries->data )
      XLAL_ERROR( XLAL_EINVAL );
  if ( tseries->deltaT <= 0.0 )
      XLAL_ERROR( XLAL_EINVAL );

  reclen = tseries->data->length;
  numseg = 1 + (reclen - seglen)/stride;

  /* consistency check for lengths: make sure that the segments cover the
 *  *  *    * data record completely */
  if ( (numseg - 1)*stride + seglen != reclen )
    XLAL_ERROR( XLAL_EBADLEN );
  if ( spectrum->data->length != seglen/2 + 1 )
    XLAL_ERROR( XLAL_EBADLEN );
  /* create frequency series data workspaces */
  work = XLALCalloc( numseg, sizeof( *work ) );
  if ( ! work )
    XLAL_ERROR( XLAL_ENOMEM );
  for ( seg = 0; seg < numseg; ++seg )
  {
    work[seg].data = XLALCreateREAL8Vector( spectrum->data->length );
    if ( ! work[seg].data )
    {
      median_cleanup_REAL8( work, numseg ); /* cleanup */
      XLAL_ERROR( XLAL_EFUNC );
    }
  }

  for ( seg = 0; seg < numseg; ++seg )
  {
    REAL8Vector savevec; /* save the time series data vector */
    int code;

    /* save the time series data vector */
    savevec = *tseries->data;

    /* set the data vector to be appropriate for the even segment */
    tseries->data->length  = seglen;
    tseries->data->data   += seg * stride;

    /* compute the modified periodogram for the even segment */
    code = XLALREAL8ModifiedPeriodogram( work + seg, tseries, window, plan );

    /* restore the time series data vector to its original state */
    *tseries->data = savevec;

    /* now check for failure of the XLAL routine */
    if ( code == XLAL_FAILURE )
    {
      median_cleanup_REAL8( work, numseg ); /* cleanup */
      XLAL_ERROR( XLAL_EFUNC );
    }
  }

  /* create array to hold a particular frequency bin data */
  bin = XLALMalloc( numseg * sizeof( *bin ) );
  if ( ! bin )
  {
    median_cleanup_REAL8( work, numseg ); /* cleanup */
    XLAL_ERROR( XLAL_ENOMEM );
  }

  double meanVal, normVal, chisqrVal;

  int numBins = 100;
  int binIndex;
  float Observed[numBins];
  float Expected[numBins];
  float ObservedCDF[numBins];
  float ExpectedCDF[numBins];
  int min = 0; int max = 100;
  int count;
  float interval = (float)(max - min ) / numBins;
  float ObservedSum, ExpectedSum;
  double KS, KSVal, nKSsquared;
  double bin_val, expected_val;

  for ( k = 0; k < spectrum->data->length; ++k ) {
      pvalues[k] = 0.0;
  }

  /* now loop over frequency bins and compute the median-mean */
  for ( k = 0; k < spectrum->data->length; ++k )
  {
    /* assign array of even segment values to bin array for this freq bin */
    for ( seg = 0; seg < numseg; ++seg ) {
      bin[seg] = work[seg].data->data[k];
      meanVal = meanVal + work[seg].data->data[k];
    }

    meanVal = meanVal / (double) numseg;

    for ( l = 0; l < numBins; ++l ) {
        Observed[l] = 0.0;
        Expected[l] = 0.0;
        ObservedCDF[l] = 0.0;
        ExpectedCDF[l] = 0.0;
    }

    count = 0.0;
    for ( seg = 0; seg < numseg; ++seg ) {
      normVal = 2 * bin[seg] / meanVal;
      binIndex = (int)((normVal- min)/interval);

      Observed[binIndex] = Observed[binIndex] + 1;
      count = count + 1;
    }

    for ( l = 0; l < numBins; ++l ) {

      bin_val = l*interval + min;
      expected_val = count * chisqr(2,bin_val);

      Expected[l] = expected_val;

    }

    ObservedSum = 0.0;
    ExpectedSum = 0.0;

    for ( l = 0; l < numBins; ++l ) {

      if (l > 0) {
          ObservedCDF[l] = ObservedCDF[l-1] + Observed[l];
          ExpectedCDF[l] = ExpectedCDF[l-1] + Expected[l];

      }
      else {
          ObservedCDF[l] = Observed[l];
          ExpectedCDF[l] = Expected[l];
      }

      ObservedSum = ObservedSum + Observed[l];
      ExpectedSum = ExpectedSum + Expected[l];

    }

    KS = 0.0;

    for ( l = 0; l < numBins; ++l ) {

        ObservedCDF[l] = ObservedCDF[l]/ObservedSum;
        ExpectedCDF[l] = ExpectedCDF[l]/ExpectedSum;

        KSVal = ObservedCDF[l] - ExpectedCDF[l];

        if (KSVal < 0) {
            KSVal = -KSVal;
        }

        if (KSVal > KS) {
            KS = KSVal;
        }

    }

    nKSsquared = count * KS * KS;
    pvalues[k] = 2*exp(-(2.000071+.331/sqrt(count)+1.409/count)*nKSsquared);

  }

  /* free the workspace data */
  XLALFree( bin );
  median_cleanup_REAL8( work, numseg );

  return 0;
}

