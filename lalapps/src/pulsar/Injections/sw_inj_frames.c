/*
 * Copyright (C) 2010 Erin Macdonald
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

/* Code to create frames and add them to existing data files
Input format as $ ./sw_inj_frames framefile duration epoch
example$ ./sw_inj_frames H1_LDAS_C02_L2_CWINJ H1:LDAS-STRAIN 128 1927836 for the following frame: H-H1_LDAS_C02_L2-945625472-128.gwf
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <dirent.h>

/*LAL Functions */
#include <lal/Units.h>
#include <lal/FrameStream.h>
#include <lal/LALFrameIO.h>
#include <lal/FrameCache.h>
#include <lal/TimeSeries.h>
#include <lal/LALStdlib.h>
#include <lal/AVFactories.h>
#include <lal/UserInput.h>

typedef struct{
    BOOLEAN help; /* Trigger output of help string*/
    CHAR *out_chan; /* Channel output ie. H1_LDAS_C02_L2_CWINJ*/
    CHAR *in_chan;  /*Channel input from .gwf ie. H1:LDAS-STRAIN*/
    REAL8 srate; /* sample rate (16384)*/
    REAL8 duration; /*duration (sec)*/
    REAL8 start; /*epoch (GPSSeconds)*/
    CHAR *inputdir; /* directory for .par files*/
    } UserInput_t;
  
UserInput_t uvar_struct;    

int main(int argc, char **argv){
    const char *fn = __func__;
    
    UserInput_t *uvar = &uvar_struct; /*structure for user-defined input variables*/
    
    uvar->srate=16384;
    /* Register User Variables*/
    XLALregBOOLUserStruct( help,            'h', UVAR_HELP, "Print this meessage");
    XLALregSTRINGUserStruct(out_chan,   'o', UVAR_OPTIONAL, "Output channel i.e. (IFO)_LDAS_C02_L2_CWINJ");
    XLALregSTRINGUserStruct(in_chan,        'i', UVAR_OPTIONAL, "Input channel from .gwf file, i.e. (IFO):LDAS-STRAIN");
    XLALregREALUserStruct(srate,            'r', UVAR_OPTIONAL, "user defined sample rate, default = 16384");
    XLALregREALUserStruct(duration,       'd', UVAR_OPTIONAL, "duration of frame (sec)");
    XLALregREALUserStruct(start,            's', UVAR_OPTIONAL, "epoch in GPS Seconds");
    XLALregSTRINGUserStruct(inputdir,       'p', UVAR_OPTIONAL, "directory for .par files");
    
    if (XLALUserVarReadAllInput (argc, argv ) != XLAL_SUCCESS) {
        XLALPrintError ("%s: XLALUserVarReadAllInput() failed with errno=%d\n", fn, xlalErrno);
        return 1;
        }
  /*char lalframefile[256];
  char gwfframefile[256];

  double srate;
  double ndata; *//* data taken every 1 sec */

  REAL8Vector *injsig=NULL;
  FILE *inject;
  REAL8Vector *Tstamp=NULL;
  FILE *outtest;


  int j=0;
  int k=0;

  REAL8TimeSeries *series=NULL;
  REAL8TimeSeries *gwfseries=NULL;
  LIGOTimeGPS epoch;

  FrStream *frfile=NULL;
  FrStream *gwffile=NULL;

  /*struct FrFile *frfile;
  struct FrameH *frame;
  struct FrVect *vect;*/

  char fname[256];
  char gwfname[256];

  char detname[2];
  char channame[256];
  char *pos;
  int ipos = 0;
  char injpos[256];

  char inputdir[256];
  int n;
  struct dirent **namelist;
  char fin[256];

  lalDebugLevel = 1;	/* debug level for this code */

  if ( argc < 2 ) {
    XLALPrintError ("Need some input arguments!\n");
    return 1;
  }

/*  sprintf(lalframefile, "%s", argv[1]);  User defined frame file -- need to simplify */
/*  sprintf(gwfframefile, "%s", argv[2]);  Frame file to be read in*/

/*  srate = atoi(argv[3]);  User defined sample rate (16384)*/

/*  ndata = atoi(argv[4]);  length of data set */

/*  epoch.gpsSeconds = atoi(argv[5]);  User defined gps epoch */
/*  epoch.gpsNanoSeconds = 0;*/

/*   define the input directory for the mfd_files*/
/*  sprintf(inputdir, "/home/emacdonald/par_files/generated/%s/mfd_files", argv[6]);*/
/*  fprintf(stderr, "%s\n", inputdir);*/
  epoch.gpsSeconds = uvar->start;
  epoch.gpsNanoSeconds = 0;

/*Error Checks*/
    if (uvar->in_chan == 0){
        XLALPrintError ("\nNeed an input channel!\n");
        return 1;
    }

    REAL8 ndata;
    REAL8 srate;
    
    ndata = uvar->duration;
    srate = uvar->srate;

  /* Get .gwf Frame File */
  gwfseries = XLALCreateREAL8TimeSeries( uvar->in_chan, &epoch, 0., 1./srate,
					 &lalSecondUnit, (int)(ndata*srate) );

  /* extract .gwf file name from inputs*/
  pos = strchr(uvar->out_chan, '_');
  ipos = pos-(uvar->out_chan);
  strncpy(detname, uvar->out_chan, ipos);
  /*fprintf(stderr, "%s\n", detname);*/

  strcpy(channame, uvar->out_chan[ipos+1]);

  strncpy(injpos, uvar->out_chan, 14);

  sprintf( gwfname, "%c-%s-%d-%s.gwf", detname[0], injpos, epoch.gpsSeconds, argv[4] );
  /*fprintf( stderr, "%s\n", gwfname);*/

  if (( gwffile = XLALFrOpen( "frames/.", gwfname )) == NULL)
    fprintf(stderr, "Cannot open file!\n");
  else fprintf(stderr, "File opened successfully - Hooray!\n");

  XLALFrGetREAL8TimeSeries( gwfseries, gwffile );

  /*for (i=0; i < gwfseries->data->length; i++)
    fprintf(stderr, "%le\n", gwfseries->data->data[i]);*/


  /* create CWINJ time series */
  series = XLALCreateREAL8TimeSeries( uvar->out_chan, &epoch, 0., 1./srate,
				      &lalSecondUnit, (int)(ndata*srate) );

  fprintf(stderr, "length = %d\n", series->data->length);

  /* define output .gwf file */
  sprintf(fname, "%c-%s-%d-%s.gwf", detname[0], uvar->out_chan, epoch.gpsSeconds, argv[4]);
  /*fprintf(stderr, "%s\n", fname);*/

  /* read in and test generated frame with XLAL function*/

  if (( frfile = XLALFrOpen( "CWINJframes/.", fname )) == NULL)
    fprintf(stderr, "Cannot open file!\n");
  else fprintf(stderr, "File opened successfully - Hooray!\n");

  series=NULL;
  series = XLALCreateREAL8TimeSeries( uvar->out_chan, &epoch, 0., 1./srate,
				      &lalSecondUnit, (int)(ndata*srate) );

  XLALFrGetREAL8TimeSeries( series, frfile );

  /*fprintf(stderr, "%d\n", series->data->length);

    fprintf(stderr, "%s\n", series->name);*/

  /*for(i=0; i< series->data->length;i++)
    fprintf(stderr, "%lf\n", series->data->data[i]);*/

  /*open sine.txt here and read in and add to series*/

  injsig = XLALCreateREAL8Vector((UINT4)(ndata*srate));
  Tstamp = XLALCreateREAL8Vector((UINT4)(ndata*srate));
  /*fprintf(stderr, "length = %d\n", injsig->length);*/

  /*Read in all mfd_files from inputdir*/
  n = scandir(inputdir, &namelist, 0, alphasort);

  UINT4 i;
  for (k=2; k < n; k++){
    /*fprintf( stderr, "%s\n",namelist[k]->d_name );*/
    sprintf(fin, "%s/%s",inputdir, namelist[k]->d_name);
    fprintf(stderr, "%s\n", fin);
    if (( inject = fopen( fin, "r" )) ==NULL)
      fprintf(stderr, "Error opening file\n" );
    else{
      double temp2=0.;

      j=0;

      while(!feof(inject)){
        fscanf(inject,"%lf %lf", &Tstamp->data[j], &temp2);
	injsig->data[j] += temp2;
	j++;
      }

      for(i=0; i<3; i++)
	fprintf(stderr, "%g\n", injsig->data[i]);
      fclose(inject);
    }
  }
  /* junk */

  /*add to series*/
    for (i=0; i < series->data->length; i++){
      series->data->data[i] += injsig->data[i]; /*read in makefakedata file*/
      /*fprintf(stderr, "%le\n", series->data->data[i]);*/
    }
    fprintf(stderr,"%e\n", series->data->data[1]);

  for (i=0; i < series->data->length; i++){
    series->data->data[i] += gwfseries->data->data[i]; /*read in series from .gwf file*/
  }

  /****Test for Matlab****/
  if (( outtest = fopen( "/home/erinmacdonald/lsc/analyses/sw_injections/test.txt", "w+" )) == NULL)
    fprintf(stderr, "Error opening file\n");
  else{
    for (i = 0; i < series->data->length; i++){
      fprintf( outtest,"%le\n", series->data->data[i] );
    }
    fclose(outtest);
  }
  /****End test****/

  XLALFrWriteREAL8TimeSeries( series, 1 );

  /* Free up the memory */

  XLALDestroyREAL8Vector(injsig);

  XLALDestroyREAL8TimeSeries( gwfseries );

  XLALDestroyREAL8TimeSeries( series );

  return 0;
}