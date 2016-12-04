/* 
 * File:   trend_f.h
 * Author: rfurch
 *
 * Created on February 26, 2012, 9:30 PM
 */

#ifndef TREND_F_H
#define	TREND_F_H

#ifdef	__cplusplus
extern "C" {
#endif


    
    

#ifndef EP2MYSQL_H
#define	EP2MYSQL_H

#ifdef	__cplusplus
extern "C" {
#endif

#define 	LOOP_COUNT	100000
#define 	MAX_DATA	100000
#define 	MAX_THREADS	10

#define     DEFDATALEN      56 
#define     MAXIPLEN        60
#define     MAXICMPLEN      76
#define     MAXPACKET       65468

#define     BUFSIZE         1500
#define     MAXLLEN         2000

#ifndef 	CLOCK_TICK_RATE
#define 	CLOCK_TICK_RATE 1193180
#endif




#ifdef	__cplusplus
}
#endif

#endif	/* EP2MYSQL_H */


typedef struct trenddata
  {
  char 		  fpath[1000];
  char 		  fname[1000];
  
  int       fieldsel_d[20];
  int       fieldsel_n;

  FILE		  *f;
  long int 	fsize;
  char 		  initime_s[100];
  time_t	  initime_t;
  char 		  fintime_s[100];
  time_t	  fintime_t;
  long int 	inip, finp;

  int 		  nfields;
  int 		  final_samples;    		// requiered from upper layers
  int 		  orig_samples;    // # of samples in trend file for the given period (> or < than nsamples) 
  double	  step_t;

  time_t	  firstline_t;
  time_t	  lastline_t;
  int 		  linelen;
  
  long int 	bufferlen;
  // raw data from file, as a big string
  char 		  *raw_data_str;
  
  // parsed data
  time_t  	*orig_time;
  double 	  **orig_data;

  // final samples
  time_t  	*final_time;
  double 	  **final_data;

  //--------------------------------------

  unsigned char *minutes;
  unsigned long int intialMinute, finalMinute;

  }trenddata;

    
    
    


#ifdef	__cplusplus
}
#endif

#endif	/* TREND_F_H */


// util.c

int trenddata_init(trenddata *t);
int getFileSize(char *fname, long int *fsize);
int getTimetFromString(char *str, time_t *t);
int composeFname( char *result, char *fpath, char *fname);
int readLineFromPositionX(FILE *f, long int pos, char *l, long int *inipos, long int *finpos);
int getFileLimits(trenddata *t);



