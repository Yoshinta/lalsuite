#include <stdlib.h>
#include <lal/LALStdlib.h>
#include <lal/AVFactories.h>
#include <lal/Date.h>

INT4 lalDebugLevel = 2;

NRCSID (TESTLMSTC, "$Id$");

int
main(int argc, char *argv[])
{
    LALDate date;
    LALDate mstdate;
    REAL8   gmsthours, lmsthours;
    REAL8   gmstsecs;
    REAL8   longitude;
    /* time_t  timer; */
    CHAR    timestamp[64], tmpstr[64];
    CHARVector *tmpstamp = NULL;
    INT4        testid;
    INT4        dayofmonth, monthofyear;
    static LALStatus  status;

    
    if (argc != 3)
    {
        /*
         * Print help message and exit
         */
        printf("Usage: TestUTCtoGPS testid debug_level\n");
        printf("              testid      = [1,2]\n");
        printf("              debug_level = [0,1,2]\n");
        return 0;
    }

    testid     = atoi(argv[1]);
    lalDebugLevel = atoi(argv[2]);


    LALCHARCreateVector(&status, &tmpstamp, (UINT4)64);

    printf("TEST of LALGMST1 routine\n");
    printf("=====================\n");

    if (testid == 1)
    {
        /*
         * Check against the Astronomical Almanac:
         * For 1994-11-16 0h UT - Julian Date 2449672.5, GMST 03h 39m 21.2738s
         */
        date.unixDate.tm_sec  = 0;
        date.unixDate.tm_min  = 0;
        date.unixDate.tm_hour = 0;
        date.unixDate.tm_mday = 16;
        date.unixDate.tm_mon  = 10;
        date.unixDate.tm_year = 94;

        longitude = 0.; /* Greenwich */
        LALGMST1(&status, &gmsthours, &date, MST_HRS);
        LALLMST1(&status, &lmsthours, &date, longitude, MST_HRS);

        LALGMST1(&status, &gmstsecs, &date, MST_SEC);
        LALSecsToLALDate(&status, &mstdate, gmstsecs);
        strftime(timestamp, 64, "%Hh %Mm %S", &(mstdate.unixDate));
        sprintf(tmpstr, "%fs", mstdate.residualNanoSeconds * 1.e-9);
        strcat(timestamp, tmpstr+1); /* remove leading 0 */
        LALDateString(&status, tmpstamp, &date);

        printf("     Time = %s\n", tmpstamp->data);
        printf("gmsthours = %f = %s\n", gmsthours, timestamp);
        printf("    expect: 3.655728 = 03h 39m 20.6222s \n");
        /* printf("lmsthours = %f\n", lmsthours); */

        /*
         * Another check against the Almanac:
         * For 1994-08-17 2h 19m 03.0736s -> 0h GMST
         */
        printf("\n");
        date.residualNanoSeconds = 73600000;
        date.unixDate.tm_sec = 3;
        date.unixDate.tm_min = 19;
        date.unixDate.tm_hour = 2;
        date.unixDate.tm_mday = 17;
        date.unixDate.tm_mon  = 7;
        date.unixDate.tm_year = 94;

        longitude = 0.; /* Greenwich */
        LALGMST1(&status, &gmsthours, &date, MST_HRS);
        LALLMST1(&status, &lmsthours, &date, longitude, MST_HRS);

        LALGMST1(&status, &gmstsecs, &date, MST_SEC);
        LALSecsToLALDate(&status, &mstdate, gmstsecs);
        strftime(timestamp, 64, "%Hh %Mm %S", &(mstdate.unixDate));
        sprintf(tmpstr, "%fs", mstdate.residualNanoSeconds * 1.e-9);
        strcat(timestamp, tmpstr+1); /* remove leading 0 */
        LALDateString(&status, tmpstamp, &date);

        printf("     Time = %s\n", tmpstamp->data);
        printf("gmsthours = %f = %s\n", gmsthours, timestamp);
        printf("    expect:        0h = 00h 00m 00s \n");


        /*
         * A third check against the Almanac:
         * For 1994-05-17 0h
         */
        printf("\n");
        date.residualNanoSeconds = 0;
        date.unixDate.tm_sec = 0;
        date.unixDate.tm_min = 0;
        date.unixDate.tm_hour = 0;
        date.unixDate.tm_mday = 17;
        date.unixDate.tm_mon  = 4;
        date.unixDate.tm_year = 94;

        longitude = 0.; /* Greenwich */
        LALGMST1(&status, &gmsthours, &date, MST_HRS);
        LALLMST1(&status, &lmsthours, &date, longitude, MST_HRS);

        LALGMST1(&status, &gmstsecs, &date, MST_SEC);
        LALSecsToLALDate(&status, &mstdate, gmstsecs);
        strftime(timestamp, 64, "%Hh %Mm %S", &(mstdate.unixDate));
        sprintf(tmpstr, "%fs", mstdate.residualNanoSeconds * 1.e-9);
        strcat(timestamp, tmpstr+1); /* remove leading 0 */
        LALDateString(&status, tmpstamp, &date);

        printf("     Time = %s\n", tmpstamp->data);
        printf("gmsthours = %f   = %s\n", gmsthours, timestamp);
        printf("    expect: 15.63105328 = 15h 37m 51.7918s\n");

        /* And increment the date by 1 hour to see
         * if it makes GMST change */
        printf("\n");
        date.residualNanoSeconds = 0;
        date.unixDate.tm_sec = 0;
        date.unixDate.tm_min = 0;
        date.unixDate.tm_hour = 1;
        date.unixDate.tm_mday = 17;
        date.unixDate.tm_mon  = 4;
        date.unixDate.tm_year = 94;

        longitude = 0.; /* Greenwich */
        LALGMST1(&status, &gmsthours, &date, MST_HRS);
        LALLMST1(&status, &lmsthours, &date, longitude, MST_HRS);

        LALGMST1(&status, &gmstsecs, &date, MST_SEC);
        LALSecsToLALDate(&status, &mstdate, gmstsecs);
        strftime(timestamp, 64, "%Hh %Mm %S", &(mstdate.unixDate));
        sprintf(tmpstr, "%fs", mstdate.residualNanoSeconds * 1.e-9);
        strcat(timestamp, tmpstr+1); /* remove leading 0 */
        LALDateString(&status, tmpstamp, &date);

        printf("     Time = %s\n", tmpstamp->data);
        printf("gmsthours = %f   = %s\n", gmsthours, timestamp);
        printf("    expect: 16.63105328 = 16h 37m 51.7918s\n");


        /*
         * A fourth check against the Almanac:
         * For 1994-05-17 08:20:46.7448 -> 0h GMST
         */
        printf("\n");
        date.residualNanoSeconds = 744800000;
        date.unixDate.tm_sec = 46;
        date.unixDate.tm_min = 20;
        date.unixDate.tm_hour = 8;
        date.unixDate.tm_mday = 17;
        date.unixDate.tm_mon  = 4;
        date.unixDate.tm_year = 94;

        longitude = 0.; /* Greenwich */
        LALGMST1(&status, &gmsthours, &date, MST_HRS);
        LALLMST1(&status, &lmsthours, &date, longitude, MST_HRS);

        LALGMST1(&status, &gmstsecs, &date, MST_SEC);
        LALSecsToLALDate(&status, &mstdate, gmstsecs);
        strftime(timestamp, 64, "%Hh %Mm %S", &(mstdate.unixDate));
        sprintf(tmpstr, "%fs", mstdate.residualNanoSeconds * 1.e-9);
        strcat(timestamp, tmpstr+1); /* remove leading 0 */
        LALDateString(&status, tmpstamp, &date);

        printf("     Time = %s\n", tmpstamp->data);
        printf("gmsthours = %f = %s\n", gmsthours, timestamp);
        printf("    expect:        0h = 00h 00m 00s \n");
    }

    if (testid == 2)
    {
        /* Generate column G (Mean) of Sec. B of the Almanac for 1994 */
        date.unixDate.tm_sec = 0;
        date.unixDate.tm_min = 0;
        date.unixDate.tm_hour = 0;
        date.unixDate.tm_year = 94;
        printf("\nGMST1 of 0h UT1 for 1994:\n");
        for (monthofyear = 0; monthofyear < 12; ++monthofyear)
        {
            date.unixDate.tm_mon = monthofyear;
            
            for (dayofmonth = 1; dayofmonth < 32; ++dayofmonth)
            {
                date.unixDate.tm_mday = dayofmonth;

                LALGMST1(&status, &gmstsecs, &date, MST_SEC);
                LALSecsToLALDate(&status, &mstdate, gmstsecs);
                strftime(timestamp, 64, "%Hh %Mm %S", &(mstdate.unixDate));
                sprintf(tmpstr, "%fs", mstdate.residualNanoSeconds * 1.e-9);
                strcat(timestamp, tmpstr+1); /* remove leading 0 */
                LALDateString(&status, tmpstamp, &date);
                printf("%s: %s\n", tmpstamp->data, timestamp); 
                
                /* February */
                if (monthofyear == 1)
                    if (dayofmonth == 28)
                        break;
                /* the 30-day months */
                if (monthofyear == 3 || monthofyear == 5 ||
                    monthofyear == 8 || monthofyear == 10)
                    if (dayofmonth == 30)
                        break;
            }
        }
    }

    /*
     * Housekeeping
     */
    LALCHARDestroyVector(&status, &tmpstamp);

    return(0);
}
