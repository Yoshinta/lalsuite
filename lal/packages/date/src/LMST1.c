/*----------------------------------------------------------------------- 
 * 
 * File Name: LMST1.c 
 * 
 * Author: David Chin <dwchin@umich.edu>
 * 
 * Revision: $Id$
 * 
 *----------------------------------------------------------------------- 
 *
 * SYNOPSIS
 *
 * LALGMST1(): Returns LALGMST1 for given date-time
 * LALLMST1(): Returns LALLMST1 given date-time, and longitude
 * 
 * DESCRIPTION
 *
 * LALGMST1():
 *      Inputs: LALDate *date      -- date-time to compute GMST1
 *              LALMSTUnits outunits -- units requested:
 *                                      MST_SEC - seconds
 *                                      MST_HRS - hours
 *                                      MST_DEG - degrees
 *                                      MST_RAD - radians
 *
 *      Outputs: REAL8  *gmst      -- LALGMST1 in units requested
 *
 * LALLMST1():
 *      Inputs: LALPlaceAndDate *place_and_date -- location and date-time
 *                                                 to compute GMST1
 *              LALMSTUnits outunits -- units requested:
 *                                      MST_SEC - seconds
 *                                      MST_HRS - hours
 *                                      MST_DEG - degrees
 *                                      MST_RAD - radians
 *
 *      Outputs: REAL8  *lmst      -- LALGMST1 in units requested
 *
 * 
 * DIAGNOSTICS 
 * (Abnormal termination conditions, error and warning codes summarized 
 * here. More complete descriptions are found in documentation.)
 *
 * CALLS
 *    LALGMST1():  LALJulianDate()
 *    LALLMST1():  LALGMST1()
 * 
 * NOTES
 * LALGMST1 is from NOVAS - C Version 2.0.1 (10 Dec 99): Naval Observatory
 * Vector Astrometry Subroutines. See http://aa.usno.navy.mil/AA/
 *
 * The basic formula is from the Explanatory Supplement to the Astronomical
 * Almanac, 1992, Ch. 2, Sec. 24, and also Section B of the Almanac. The
 * formula computes LALGMST1 for 0h UT1.  To compute LALGMST1 for other times, a
 * simple linear interpolation is done.
 * 
 *----------------------------------------------------------------------- */

#include <lal/LALRCSID.h>

NRCSID (LMST1C, "$Id$");

#include <math.h>
#include <lal/Date.h>
#include "date_value.h"


/*
 * Compute LALGMST1 in requested units given date UTC.
 * Use algorithm as coded in NOVAS-C Version 2.0.1 (10 Dec 99)
 *   Naval Observatory Vector Astrometry Subroutines
 *  C Version
 */
void
LALGMST1 (LALStatus     *status,
          REAL8         *gmst,
          const LALDate *date,
          LALMSTUnits    outunits)
{
    REAL8 jdate;
    REAL8 jd_hi, jd_lo;
    INT4  tmp;
    REAL8 st;
    REAL8 tu;
    REAL8 tu2;
    const REAL8      a =         -6.2e-6;
    const REAL8      b =          0.093104;
    const REAL8      c =      67310.54841;
    const REAL8      d =    8640184.812866;
    const REAL8      e = 3155760000.0;

    INITSTATUS (status, "LALGMST1", LMST1C);

    /*
     * Check pointer to input variables
     */
    ASSERT (date != (LALDate *)NULL, status,
            GMST1_ENULLINPUT, GMST1_MSGENULLINPUT);

    /*
     * Check pointer to output variable
     */
    ASSERT (gmst != (REAL8 *)NULL, status,
            GMST1_ENULLOUTPUT, GMST1_MSGENULLOUTPUT);

    /*
     * Compute LALGMST1 for 0h UT1 on given date in seconds since J2000.0 
     */
    LALJulianDate(status, &jdate, date);
    jdate -= J2000_0;
    tmp    = (INT4)jdate;  /* get the integral part of jdate */
    jd_hi  = (REAL8)tmp;
    jd_lo  = jdate - jd_hi;

    tu     = jdate / JDAYS_PER_CENT;
    tu2    = tu * tu;

    jd_hi /= JDAYS_PER_CENT;
    jd_lo /= JDAYS_PER_CENT;
    
    st = a*tu2*tu + b*tu2 + c + d*jd_lo + e*jd_lo
        + d*jd_hi + e*jd_hi;

    *gmst = fmod(st, (REAL8)SECS_PER_DAY);

    if (*gmst < 0.)
        *gmst += (REAL8)SECS_PER_DAY;

    /*
     * Convert GMST to requested units
     */
    switch (outunits)
    {
    case MST_SEC:
        break;
    case MST_HRS:
        *gmst /= (REAL8)SECS_PER_HOUR;
        break;
    case MST_DEG:
        *gmst /= (REAL8)SECS_PER_HOUR / (REAL8)DEGS_PER_HOUR;
        break;
    case MST_RAD:
        *gmst /= (REAL8)SECS_PER_HOUR / (REAL8)DEGS_PER_HOUR * 180. /
            (REAL8)LAL_PI;
        break;
    default:
        break;
    }

    RETURN (status);
} /* END LALGMST1() */



/*
 * Compute LALLMST1 in requested units given GPS time
 */
void LALGPStoGMST1( LALStatus         *status,
                    REAL8             *p_gmst,
                    const LIGOTimeGPS *p_gps,
                    LALMSTUnits        outunits)
{
    LIGOTimeUnix    unixTime;
    LALDate         date;
    
    INITSTATUS (status, "LALGPStoGMST1", LMST1C);

    
    /*
     * Check pointer to input variables
     */
    ASSERT (p_gps != (LIGOTimeGPS *)NULL, status,
            GPSTOGMST1_ENULLINPUT, GPSTOGMST1_MSGENULLINPUT);

    /*
     * Check pointer to output variable
     */
    ASSERT (p_gmst != (REAL8 *)NULL, status,
            GPSTOGMST1_ENULLOUTPUT, GPSTOGMST1_MSGENULLOUTPUT);

    /*
     * Convert GPS to date-time structure
     */

    /* first, GPS to Unix */
    LALGPStoU(status, &unixTime, p_gps);

    /* then, Unix to date-time */
    LALUtime(status, &date, &unixTime);

    LALGMST1(status, p_gmst, &date, outunits);

    return;
} /* END: LALGPStoGMST1() */





/*
 * Compute LALLMST1 in requested units given date-time
 */
void
LALLMST1 (LALStatus             *status,
          REAL8                 *lmst,
          const LALPlaceAndDate *place_and_date,
          LALMSTUnits            outunits)
{
    REAL8 gmst;
	REAL8 day = 0;
    REAL8 longitude =
        place_and_date->detector->frDetector.vertexLongitudeDegrees;


    INITSTATUS (status, "LALLMST1", LMST1C);

    /*
     * Check pointer to input variables
     */
    ASSERT (place_and_date != (LALPlaceAndDate *)NULL, status,
            LMST1_ENULLINPUT, LMST1_MSGENULLINPUT);

    /*
     * Check pointer to output variable
     */
    ASSERT (lmst != (REAL8 *)NULL, status,
            LMST1_ENULLOUTPUT, LMST1_MSGENULLOUTPUT);

    /*
     * Compute LMST1 in seconds since J2000.0
     */

    /* get GMST1 in seconds */
    LALGMST1(status, &gmst, place_and_date->date, outunits);

    /* convert longitude to appropriate units of sidereal time */
    switch (outunits)
	{
    case MST_SEC:
        longitude /= (REAL8)DEGS_PER_HOUR / (REAL8)SECS_PER_HOUR;
		day        = SECS_PER_DAY;
        break;
    case MST_HRS:
        longitude /= (REAL8)DEGS_PER_HOUR;
		day        = 24.;
        break;
    case MST_DEG:
		day        = 360.;
        break;
    case MST_RAD:
        longitude /= 180. / (REAL8)LAL_PI;
		day        = (REAL8)LAL_TWOPI;
        break;
    default:
        break;
    }

    *lmst = gmst + longitude;

	while (*lmst < 0)
        *lmst += day;

    RETURN (status);
} /* END LALLMST1() */



/*
 * Compute LALLMST1 in requested units given GPS time
 */
void LALGPStoLMST1( LALStatus             *status,
                    REAL8                 *p_lmst,
                    const LALPlaceAndGPS  *p_place_and_gps,
                    LALMSTUnits            outunits)
{
    LIGOTimeUnix    unixTime;
    LALDate         date;
    LALPlaceAndDate place_and_date;


    INITSTATUS (status, "LALGPStoLMST1", LMST1C);

    
    /*
     * Check pointer to input variables
     */
    ASSERT (p_place_and_gps != (LALPlaceAndGPS *)NULL, status,
            GPSTOLMST1_ENULLINPUT, GPSTOLMST1_MSGENULLINPUT);

    /*
     * Check pointer to output variable
     */
    ASSERT (p_lmst != (REAL8 *)NULL, status,
            GPSTOLMST1_ENULLOUTPUT, GPSTOLMST1_MSGENULLOUTPUT);

    /*
     * Convert GPS to date-time structure
     */

    /* first, GPS to Unix */
    LALGPStoU(status, &unixTime, p_place_and_gps->gps);

    /* then, Unix to date-time */
    LALUtime(status, &date, &unixTime);

    /* stuff it all into a LALPlaceAndDate */
    place_and_date.detector = p_place_and_gps->detector;
    place_and_date.date     = &date;

    LALLMST1(status, p_lmst, &place_and_date, outunits);

    return;
} /* END: LALGPStoLMST1() */


              
