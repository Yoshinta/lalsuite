/* Extras for building an Einstein@Home BOINC App from HierarchicalSearch
   Bernd Machenschalk for Einstein@Home
   $Id$
*/

/* TODO:
   - error handling
   - signal handling
   - checkpointing
   - boinc_init(), boinc finish
*/

#include "hs_boinc_extras.h"

#include <stdlib.h>
#include <string.h>


#if !DEBUG
#include "boinc_api.h"
#endif
#include "diagnostics.h"
#include "boinc_zip.h"


#define MAX_PATH_LEN 512

#define BOINC_TRY(test,code,mess) \
  if (test) { \
    LogPrintf (LOG_CRITICAL, "ERROR %d: %s\n", code, mess); \
    boinc_finish(code); \
  }
    

#if DEBUG
/* trivial simulation of LogPrintf for testing */
#define LogPrintf fprintf
#define LOG_CRITICAL stderr
#define LOG_NORMAL stderr
#define LOG_ERROR stderr

/* dummy for boinc_resolve_filename */
int boinc_resolve_filename(char*name,char*resolved,int len) {
  strncpy(resolved,"res:",strlen("res:"));
  strncat(resolved,name,len);
  return(0);
}
#endif /* DEBUG */



/* compare strings s1 and s2 up to the length of s1 (w/o the '\0'!!)
   and set l to the length */
#define MATCH_START(s1,s2,l) (0 == strncmp(s1,s2,(l=strlen(s1))-1))



/* information about the output files */
static char **outfiles = NULL;        /* the names  of the output files of the program */
static int  noutfiles  = 0;           /* the number of the output files of the program */
static char resultfile[MAX_PATH_LEN]; /* the name of the file / zip archive to return */
static double estimated_flops = -1;

/* hooks for communication with the graphics thread */
void (*set_search_pos_hook)(float,float) = NULL;
int (*boinc_init_graphics_hook)(void (*worker)(void)) = NULL;
double *fraction_done_hook = NULL;

/* declare graphics stuff here if we don't get it from a dynamic library */
#if (BOINC_GRAPHICS == 1)
extern double fraction_done;
extern void set_search_pos(float RAdeg, float DEdeg);
extern int boinc_init_graphics(void (*worker)(void));
#endif


/* show progress */
void show_progress(double rac, double dec, long tpl_count, long tpl_total) {
  double fraction_done = (double)tpl_count / (double)tpl_total;
  if (fraction_done_hook)
    *fraction_done_hook = fraction_done;
  if (set_search_pos_hook)
    set_search_pos_hook(rac * 180.0/LAL_PI, dec * 180.0/LAL_PI);
  boinc_fraction_done(fraction_done);
  if (estimated_flops >= 0)
    boinc_ops_cumulative(estimated_flops * fraction_done, 0); /* ignore IOPS */
}


/* this registers a new output file to be zipped into the archive that is returned
   to the server as a result file */
void register_output_file(char*filename) {
  outfiles = (char**)realloc(outfiles,(noutfiles+1)*sizeof(char*));
  if (outfiles == NULL)
    LogPrintf (LOG_CRITICAL, "ERROR: Can't allocate output filename '%s'\n", filename);
  outfiles[noutfiles] = malloc(strlen(filename));
  if (outfiles[noutfiles] == NULL)
    LogPrintf (LOG_CRITICAL, "ERROR: Can't allocate output filename '%s'\n", filename);
  strcpy(outfiles[noutfiles],filename);
  noutfiles++;
}



/* to be inmplemented... */
void set_checkpoint(char*filename,double rac,double dec, long tpl_count, long tpl_total);
void get_checkpoint(char*filename);



/* check if given file is a zip archive by looking for the zip-magic header 'PK\003\044'
 * RETURN: 1 = true, 0 = false, -1 = error
 */
static int is_zipped ( const char *fname ) {
  FILE *fp;
  char zip_magic[] = {'P', 'K', 3, 4 };
  char file_header[4];

  if ( (fp = fopen( fname, "rb")) == NULL ) {
    LogPrintf (LOG_CRITICAL, "Failed to open '%s' for reading.\n", fname);
    return -1;
  }
  if ( 4 != fread ( file_header, sizeof(char), 4, fp ) ) {
    LogPrintf (LOG_CRITICAL, "Failed to read first 4 bytes from '%s'.\n", fname);
    return -1;
  }
  fclose(fp);

  if ( memcmp ( file_header, zip_magic, 4 ) )
    return 0;	/* false: no zip-file */
  else
    return 1;	/* yep, found magic zip-header */
} /* is_zipped() */



/* unzip a file in-place if it is zipped */
/* TODO: error handling */
static int unzip_if_necessary(char*filename) {
  char zipname[MAX_PATH_LEN];
  int zipped;
  zipped = is_zipped (filename);
  if (zipped<0) {
    return(-1);
  } else if (zipped) { 
    strncpy(zipname,filename,sizeof(zipname));
    strncat(zipname,".zip",sizeof(zipname));
    boinc_delete_file(zipname);
    boinc_rename(filename,zipname);
    boinc_zip(UNZIP_IT,zipname,filename);
    boinc_delete_file(zipname);
  }
  return(0);
}



worker (int argc, char*argv[]) {
  int i,j;                     /* loop counter */
  int l;                       /* length of matched string */
  int res;                     /* return value of function call */
  int rargc = argc;            /* argc and ... */
  char **rargv = NULL;         /* ... argv values for calling the MAIN() function of the worker */
  char tempstr[MAX_PATH_LEN];  /* temporary holds a filename / -path */
  char *startc,*endc,*appc;

  /* try to load the graphics library and set the hooks if successful */
#if (BOINC_GRAPHICS == 2) 
  if (graphics_lib_handle) {
    if (!(set_search_pos_hook = dlsym(graphics_lib_handle,"set_search_pos"))) {
      LogPrintf (LOG_CRITICAL,   "unable to resolve set_search_pos(): %s\n", dlerror());
      boinc_finish(COMPUTEFSTAT_EXIT_DLOPEN);
    }
    if (!(fraction_done_hook = dlsym(graphics_lib_handle,"fraction_done"))) {
      LogPrintf (LOG_CRITICAL,   "unable to resolve fraction_done(): %s\n", dlerror());
      boinc_finish(COMPUTEFSTAT_EXIT_DLOPEN);
    }
  }
  else
    LogPrintf (LOG_CRITICAL,  "graphics_lib_handle NULL: running without graphics\n");
#endif

  /* PATCH THE COMMAND LINE

     The actual parsing of the command line will be left to the
     MAIN() of HierarchicalSearch.c. However, command line arguments
     that can be identified as filenames must be boinc_resolved
     before passing them to the main function.
     We will also look if the files are possibly zipped and unzip
     them as needed.
  */
  rargv = (char**)malloc(argc*sizeof(char*));
  rargv[0] = argv[0];

  /* for all args in the command line (except argv[0]) */
  for (i=1; i<argc; i++) {
    
    /* config file */
    if (argv[i][0] == '@') {
      rargv[i] = (char*)malloc(MAX_PATH_LEN);
      rargv[i][0] = '@';
      if (boinc_resolve_filename(argv[i]+1,rargv[i]+1,MAX_PATH_LEN-1)) {
        LogPrintf (LOG_NORMAL, "WARNING: Can't boinc-resolve config file '%s'\n", argv[i]+1);
      }
    }

    /* skygrid file */
    else if (MATCH_START("--skyGridFile=",argv[i],l)) {
      rargv[i] = (char*)malloc(MAX_PATH_LEN);
      strncpy(rargv[i],argv[i],l);
      if (boinc_resolve_filename(argv[i]+l,rargv[i]+l,MAX_PATH_LEN-l)) {
        LogPrintf (LOG_NORMAL, "WARNING: Can't boinc-resolve skygrid file '%s'\n", argv[i]+1);
      }
      unzip_if_necessary(rargv[i]);
    }

    /* ephermeris files */
    else if (MATCH_START("--ephemE=",argv[i],l)) {
      rargv[i] = (char*)malloc(MAX_PATH_LEN);
      strncpy(rargv[i],argv[i],l);
      if (boinc_resolve_filename(argv[i]+l,rargv[i]+l,MAX_PATH_LEN-l)) {
        LogPrintf (LOG_NORMAL, "WARNING: Can't boinc-resolve skygrid file '%s'\n", argv[i]+1);
      }
      unzip_if_necessary(rargv[i]);
    }
    else if (MATCH_START("--ephemS=",argv[i],l)) {
      rargv[i] = (char*)malloc(MAX_PATH_LEN);
      strncpy(rargv[i],argv[i],l);
      if (boinc_resolve_filename(argv[i]+l,rargv[i]+l,MAX_PATH_LEN-l)) {
        LogPrintf (LOG_NORMAL, "WARNING: Can't boinc-resolve skygrid file '%s'\n", argv[i]+1);
      }
      unzip_if_necessary(rargv[i]);
    }

    /* file to return (zip archive) */
    else if (MATCH_START("--BOINCresfile=",argv[i],l)) {
      if (boinc_resolve_filename(argv[i]+l,resultfile,sizeof(resultfile))) {
        LogPrintf (LOG_NORMAL, "WARNING: Can't boinc-resolve skygrid file '%s'\n", argv[i]+1);
      }
      rargc--; /* this argument is not passed to the main worker function */
    }

    else if (MATCH_START("--WUfpops=",argv[i],l)) {
      estimated_flops=atof(argv[i]+l);
      rargc--; /* this argument is not passed to the main worker function */
    }

    /* SFT files (no unzipping, but dealing with multiple files separated by ';' */
    else if (0 == strncmp("--DataFiles",argv[i],11)) {
      rargv[i] = (char*)malloc(1024);
      /* copy & skip the "[1|2]=" characters, too */
      strncpy(rargv[i],argv[i],13);
      appc = rargv[i]+13;
      startc = argv[i]+13;
      /* look for multiple paths separated by ';' */
      while(endc = strchr(startc,';')) {
	*endc = '\0';
	if (boinc_resolve_filename(startc,appc,255)) {
	  LogPrintf (LOG_NORMAL, "WARNING: Can't boinc-resolve input file '%s'\n", tempstr);
	}
	/* append a ';' */
	appc = appc + strlen(appc) + 1;
	*(appc-1) = ';';
	*appc = '\0';
	/* skip the ';' */
	startc = endc+1;
      }
      /* handle last or only filename */
      if (boinc_resolve_filename(startc,appc,255)) {
	LogPrintf (LOG_NORMAL, "WARNING: Can't boinc-resolve input file '%s'\n", tempstr);
      }
    }

    /* any other argument */
    else 
      rargv[i] = argv[i];
  } /* for all command line arguments */

  if (!resultfile) {
      LogPrintf (LOG_ERROR, "ERROR: no result file has been specified");
  }

  /* CALL WORKER's MAIN()
   */
  
  res = MAIN(rargc,rargv);
  if (res) {
    LogPrintf (LOG_ERROR, "ERROR: main worker returned with error '%d'\n",res);
  }


  /* HANDLE OUTPUT FILES
   */
  if(noutfiles == 0)
    LogPrintf (LOG_ERROR, "ERROR: no output file has been specified");
  for(i=0;i<noutfiles;i++)
    if ( boinc_zip(ZIP_IT, resultfile, outfiles[i]) ) {
      LogPrintf (LOG_NORMAL, "WARNING: Can't zip output file '%s'\n", outfiles[i]);
    }

  /* finally set (fl)ops count if given */
  if (estimated_flops >= 0)
    boinc_ops_cumulative(estimated_flops * fraction_done, 0); /* ignore IOPS */
}


/* the main function of the BOINC App
   deals with boinc_init(_graphics) and calls the worker
*/

int main(int argc, char**argv) {

#if (BOINC_GRAPHICS == 1)
  set_search_pos_hook = set_search_pos;
  fraction_done_hook = &fraction_done;
#endif

  /* boinc_init() or boinc_init_graphics() needs to be run before any
   * boinc_api functions are used */
  
#if BOINC_GRAPHICS>0
  {
    int retval;
#if BOINC_GRAPHICS==2
    /* Try loading screensaver-graphics as a dynamic library.  If this
       succeeds then extern void* graphics_lib_handle is set, and can
       be used with dlsym() to resolve symbols from that library as
       needed.
    */
    retval=boinc_init_graphics_lib(worker, argv[0]);
#endif /* BOINC_GRAPHICS==2 */
#if BOINC_GRAPHICS==1
    /* no dynamic library, just call boinc_init_graphics() */
    retval = boinc_init_graphics(worker);
#endif /* BOINC_GRAPHICS==1 */
    LogPrintf (LOG_CRITICAL,  "boinc_init_graphics[_lib]() returned %d. This indicates an error...\n", retval);
    boinc_finish(COMPUTEFSTAT_EXIT_WORKER );
  }
#endif /*  BOINC_GRAPHICS>0 */
    
  /* we end up hereo only if BOINC_GRAPHICS == 0
     or a call to boinc_init_graphics failed */
  boinc_init();
  worker(argc,argv);
  boinc_finish(COMPUTEFSTAT_EXIT_WORKER );
  /* boinc_init(graphics) ends the program, we never get here!! */
  return 0;
}
