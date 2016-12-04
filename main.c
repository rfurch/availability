
#include <stdio.h>


#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <string.h>
#include <signal.h>
#include <sys/wait.h> 
#include <errno.h>
#include <stdlib.h>  
#include <unistd.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "trend_f.h"


int  		_verbose=0;

//----------------------------------------------------------------------
//----------------------------------------------------------------------

void PrintUsage(char *prgname)
{
printf ("============================================================\n");
printf ("Client availability calculation (from BW data trends) \n\n");
printf ("USAGE: %s [options]\n", prgname);
printf ("Options:\n");
printf ("  -v 	Verbose mode\n");
printf ("  -i 	initial time in days before 'now' (default: 90) \n");
printf ("  -f 	final   datetime in format: YYYYMMDDhhmmss (default: 'now') \n");
printf ("  -t 	trend file, as many as you want! (e.g -t /tmp/f01  -t /tmp/f02 \n");
printf ("  -s 	X (fields selector. e.g: -s 0 -s 3  (defaul: -s 0) \n");
printf ("============================================================\n\n");
fflush(stdout);
}




//----------------------------------------------------------------------

// basic binary search in file, searching for datetime string

int findPosition(FILE *f, char *str, long int fsize, long int *pos)
{
long int 	current_pos = fsize / 2;
long int 	current_delta = fsize / 2;
char 		l[2000];
char 		strf[500];
int 		i=0,j=0, end=0, ret=0 ;
int 		slen=0;

if (!f || !str || !pos)
  return (0);

slen=strlen(str);

while (!end)
  {
  if (readLineFromPositionX(f, current_pos, l, NULL, NULL))
	{
	for (i=0 ; l[i] && l[i]!=',' ; i++);
	
	i++;
	j=0;
	for ( ; l[i] && l[i]!=',' ; i++)
	  strf[j++] = l[i];
	strf[j]=0;
	  
	printf("\n Compare -%s-%s- pos: %li  delta:% li", strf, str, current_pos, current_delta);

	current_delta /= 2;
	if ( (ret = strncmp(strf, str, slen)) > 0 )
	  current_pos -= current_delta; 
	else if ( ret < 0 )
	  current_pos += current_delta; 
    else
  	  end=1;	   
    }
  }

return(1);
}

//----------------------------------------------------------------------

// basic binary search in file, searching for time_t value

int findPosition_t(FILE *f, time_t t, long int fsize, int linelen, int getlower, long int *pos)
{
long int 	current_pos = fsize / 2;
long int 	current_delta = fsize / 2;
char 		l[2000], strf[500];
time_t		tf=0, tprev1=0;  //  prevoius values tos stop binary search
int 		i=0, end=0;
long int 	pprev=0, pnext=0;

if (!f || !t || !pos)
  return (0);

while (!end)
  {

  if (readLineFromPositionX(f, current_pos, l, &pprev, &pnext))
	{
	for (i=0 ; l[i] && l[i]!=','  && l[i]!='.' ; i++)
	  strf[i] = l[i];
	strf[i]=0;
	tf=atol(strf);
	  
	if (_verbose > 2)  
	  printf("\n Compare -%li-%li- pos: %li  delta:% li", t, tf, current_pos, current_delta);

	//usleep(100000);

	if (current_delta > (linelen/2))
	  current_delta /= 2;
	else if (getlower &&  (tf < t && tprev1 > t))  // forced end, we are jumping on the same line
	  end=1;
	else if ( (tf > t && tprev1 < t))  // forced end, we are jumping on the same line!!
	  end=1;
	  
	if (!end)
	  {
	  if ( t < tf )
		current_pos -= current_delta; 
	  else if ( t > tf )
		current_pos += current_delta; 
  	  else
  	    end=1;	  
  	  }
  	   
    // some validation to stay 'inside' the file
    if (current_pos < 0 && (current_delta < (linelen/2)) )
        {current_pos = 0; end=1;}

    if (current_pos > fsize && (current_delta < (linelen/2)) )
        {current_pos = fsize; end=1;}
    
  	tprev1 = tf;  
    }  // if readline....
  }   // while ! end

if (getlower)
  *pos = pprev;
else
  *pos = pnext;

return(1);
}

//----------------------------------------------------------------------

// fill internal data array from the raw buffer read from file
// it fills time (as 'time_t', i.e. 'seconds' resolution ) and all fields (columns as 'double')

int     fillArrays(trenddata *t)
{
char 		dtime_str[100];
char 		s[1000];
long int 	k=0, n=0, nline=0;
int 		vn=0;       // it indicates which value



// read each line, parsing time_t, datetime and double values
while ( k < t->bufferlen )
  {
  n=0;
  while (t->raw_data_str[k] && t->raw_data_str[k]!=',')
	s[n++] = t->raw_data_str[k++];
  s[n]=0;
  t->orig_time[nline] = atol(s);	

  k++;
  n=0;
  while (t->raw_data_str[k] && t->raw_data_str[k]!=',')
	dtime_str[n++] = t->raw_data_str[k++];
  dtime_str[n] = 0;

  k++;
  n=0;
  vn=0;
  while (t->raw_data_str[k] && t->raw_data_str[k]!='\n' && t->raw_data_str[k]!='\r')
	{
	if (t->raw_data_str[k] == ',')  // with every ',' copy value to internal array
	  {
	  s[n] = 0;
      t->orig_data[vn][nline] = atof(s);
      n = 0;
      vn++;
	  }
	else
	  s[n++] = t->raw_data_str[k];

	k++;
	}  // end of line reached, we need to copy last value to internal array
  s[n] = 0;
  t->orig_data[vn][nline] = atof(s);
  n = 0;
  vn++;


  while (t->raw_data_str[k] && (t->raw_data_str[k]=='\n' || t->raw_data_str[k]=='\r'))
	k++;

  if (_verbose > 5 )
	{
	int 	m=0;
	printf("\n line %li t: %li tstr: %s", nline, t->orig_time[nline], dtime_str);
	for (m=0 ; m<vn ; m++)
	  printf(" %lf", t->orig_data[m][nline]);    
	
	}
  nline++;
  }
// update real number of samples (== lines) based on what we just processed!

t->orig_samples = nline; 

return(1);
}  

//----------------------------------------------------------------------

// process the internal data arrays 
// do some sampling / average to extract final array
// keep in mind that final samples are 'constant' (e.g. 800)
// and original buffer can have have much more data (e.g. 2000) or just 
// a few points (e.g. 10)
// In the first case we can perform sampling or average
// in the second case we need to calculate points in between                                                      

int     processArrays(trenddata *t)
{
double 		next_sample_time=0;
int 		subsample=1, average=1;

if (t->orig_samples > t->final_samples)  // more samples in file, then just pick some or make some kind of average
  {
  if (subsample)
  	  {
  	  int i=0, n=0, j=0;
  	  
  	  next_sample_time = t->orig_time[0];
  	  for (n=0, i=0 ; i<t->orig_samples ; i++)
  		if ( t->orig_time[i] >= next_sample_time) 
		  {
		  t->final_time[n] = t->orig_time[i];
  		  for (j=0 ; j<t->nfields ; j++)
  			t->final_data[j][n] = t->orig_data[j][i];
  		  next_sample_time += t->step_t;
  		  n++;
          }
  	  }
  	else			// do some kind of average for each sample
  	  {
  	  }  
  }
else			// # samples in file is lower than  request, we need some interpolation 
  {
	if (average)
	  {
	  // in this case we use to 'reference' points: one after and one before 
  	  int 		i=0, j=0;
  	  int 		prev_s_f=0;   	// previous sample in file
      double    t_ratio=0;		// proportional ratio for sample average
      double	sample_time=0;
      
	  // first sample is always the first point found in file, sorry!	
	  t->final_time[0] = t->orig_time[0];
  	  for (j=0 ; j<t->nfields ; j++)
  		t->final_data[j][0] = t->orig_data[j][0];
  	  
  	  sample_time = t->final_time[0];
	  prev_s_f = 0;

  	  for (i=1 ; i<t->final_samples ; i++)
  	    {
		sample_time += t->step_t;	// sample time is not related with original data (it's calculated)
		t->final_time[i] += sample_time;
  	    
  	    do    // be shure our current sample time is between two 'real' (original) points
  	  	  {	
  	  	  if (t->final_time[i] >= t->orig_time[prev_s_f] && t->final_time[i] < t->orig_time[prev_s_f+1] )
  	  		break;
  	  	  prev_s_f++;
  	  	  }while(1);	 
  	    
		// now calculate ratio based on how far or near the first sample we are  	    
		t_ratio = (double)(sample_time - t->orig_time[prev_s_f])  / (t->orig_time[prev_s_f+1] - t->orig_time[prev_s_f]);

  		for (j=0 ; j<t->nfields ; j++)
  		  t->final_data[j][i] = t->orig_data[j][prev_s_f] + t_ratio * (double)( t->orig_data[j][prev_s_f + 1] - t->orig_data[j][prev_s_f] );
		
		if (_verbose > 4)
		  printf("\n curr: %i t: %lf prev_t: %li  next_t:%li  ratio:%lf", i, sample_time, t->orig_time[prev_s_f],  t->orig_time[prev_s_f+1], t_ratio);     
		
  		}
	  }
	else  			// just repeat last sample
	  {
	  }  
  }	

if (_verbose > 6 )
	{
	int 	m=0,n=0;
	
	for (m=0 ; m<t->final_samples ; m++)
	  {
	  printf("\n %i -> t: %li ", m, t->final_time[m]);
	  for (n=0 ; n<t->nfields ; n++)
		printf(" %.2lf", t->final_data[n][m]);    
	  }
	}
	
return(1); 
}  

//----------------------------------------------------------------------

int printData(trenddata *t)
{
int             m=0, n=0 ;
//int             printday=0;
struct tm       *pstm=NULL;

//printf("\nTime,d1,d2"); 

//if ((t->fintime_t - t->initime_t) > 160000)  // more than two days -> print also day
//    printday=1;

for (m=0 ; m<t->final_samples ; m++)
    {
    if (t->final_time[m] > 0)
        {
        pstm = localtime(&(t->final_time[m]));

        printf("\n%4d-%02d-%02d %02d:%02d:%02d", pstm->tm_year+1900, pstm->tm_mon+1, pstm->tm_mday, pstm->tm_hour, pstm->tm_min, pstm->tm_sec);

        if (t->fieldsel_n <= 0)     // no fields explicitly required, the show only the first
            printf(",%.2lf", t->final_data[0][m]);    
        else
            {
            int k=0;

            for (n=0 ; n<t->nfields ; n++)  // match required fields
                {
                for (k=0 ; k < t->fieldsel_n ; k++ )
                    if (t->fieldsel_d[k] == n )
                        printf(",%.2lf", t->final_data[n][m]);    
                }  
            }
        }
    }

return(1);
}

//----------------------------------------------------------------------

int getBWDataFromFile(trenddata *t)
{
char 			fullpathname[500];
int 			p1f=0, p2f=0;
long int		toread=0, nread=0, totalread=0;
int 			n=0;

// make complete path
composeFname(fullpathname, t->fpath, t->fname);

getFileSize(fullpathname, &(t->fsize));
if (_verbose > 2)
  printf("\n fsize: %li", t->fsize);

// convert received strings to time_t
getTimetFromString(t->initime_s, &(t->initime_t));
getTimetFromString(t->fintime_s, &(t->fintime_t));

if (t->initime_t > t->fintime_t)		// swap them if necessary
  {
  time_t aux = t->initime_t;
  t->initime_t = t->fintime_t;
  t->fintime_t = aux;
  }

if ( ((t->f)=fopen(fullpathname, "r")) != NULL )
  {
  //readLineFromPositionX(f, 40010, l, NULL, NULL);

  // check file min initial and final time_t
  // and set ini fin marks accordingly
  getFileLimits(t);
		  
  if ( t->initime_t <= t->firstline_t )
	{ t->inip = 0; p1f=1; t->initime_t = t->firstline_t; } 
  else	
	p1f = findPosition_t(t->f, t->initime_t, t->fsize, t->linelen, 1, &(t->inip));

  if ( t->fintime_t >= t->lastline_t )
	{ t->finp = t->fsize; p2f=1; t->fintime_t = t->lastline_t; }
  else	
	p2f = findPosition_t(t->f, t->fintime_t, t->fsize, t->linelen, 0, &(t->finp));

  if (p1f && p2f)
	{
	t->step_t =  ((double)(t->fintime_t - t->initime_t)) / (t->final_samples - 1);

	if (_verbose > 2)
	  printf("\n delta t: %lf t1/p1: %li/%li  t1/p2: %li/%li p2-p1: %li", t->step_t, t->initime_t, t->inip, t->fintime_t, t->finp, t->finp-t->inip);

	// space allocation for raw data array (we read file chunk in it)
	if ( ( (t->raw_data_str)=malloc( 10 + t->finp - t->inip ) ) == NULL )
	  {
	  perror(" allocation error in 'l' [getBWDataFromFile]");
	  exit(1);
	  }

	// read the file chunk (required interval)
	fseek(t->f, t->inip , SEEK_SET);
	toread = t->finp - t->inip;
	totalread=0;
	do 
	  {
	  nread = fread(&t->raw_data_str[totalread], 1, ((toread > 50000) ? 50000 : toread) , t->f);
	  toread -= nread;
	  totalread += nread;
	  }while (toread>0 && nread!=0);

	// if VERY verbose mode is set, then dump initial an final pieces of raw buffer, just for checking...	   
	if (_verbose > 8)
	  {
	  int n=0;
	  printf("\n Block start: |");
	  do 
		{
		printf("%c", t->raw_data_str[n++]);
        } while (n<300);
	  printf("|\n");

	  n=300;
	  printf("\n Block end: |");
	  do 
		{
		printf("%c", t->raw_data_str[totalread - (n--)]);
        } while (n>=0);
	  printf("|\n");
      }   // do (highly verbose)
      
      
    t->bufferlen=totalread;  
    t->orig_samples = t->bufferlen / t->linelen;

	// now it's high time for memory allocation, based on nsamples. nfields, etc
	if ( ( t->orig_data = (double **) malloc(t->nfields * sizeof(double *)) ) == NULL )
	  {
	  perror(" allocation error in 't->dd' [getBWDataFromFile]");
	  exit(1);
	  }
	for (n=0 ; n < t->nfields ; n++)
	  if ( ( t->orig_data[n] = (double *) malloc(t->orig_samples * sizeof(double) * 2) ) == NULL )
		{
		perror(" allocation error in 't->dd[n]' [getBWDataFromFile]");
		exit(1);
		}

	// the '2' in following allocation is awful!
	// the right way would be to allocate a small number and then reallocate 
	// as needed -> to much code! 
	if ( ( t->orig_time = (time_t *) malloc(t->orig_samples * sizeof(time_t) * 2) ) == NULL )
		{
		perror(" allocation error in 't->t' [getBWDataFromFile]");
		exit(1);
		}

	// now we allocate space for resutls (final samples)
	if ( ( t->final_time = (time_t *) malloc(t->final_samples * sizeof(time_t) + 10) ) == NULL )
		{
		perror(" allocation error in 't->tfs' [getBWDataFromFile]");
		exit(1);
		}
	
	if ( ( t->final_data = (double **) malloc(t->nfields * sizeof(double *)) ) == NULL )
	  {
	  perror(" allocation error in 't->dfs' [getBWDataFromFile]");
	  exit(1);
	  }
	for (n=0 ; n < t->nfields ; n++)
	  if ( ( t->final_data[n] = (double *) malloc(t->final_samples * sizeof(double) + 10) ) == NULL )
		{
		perror(" allocation error in 't->dfs[n]' [getBWDataFromFile]");
		exit(1);
		}

    // now fill internal arrays of double from raw buffer and then get final samples
    // the the (anoying) 'client'
    if (fillArrays(t))  
        if (processArrays(t))
            printData(t);

	} // pf1 and pf2 found
  fclose(t->f);
  }  // fopen
  
return(1);

}

                                                                                 
//----------------------------------------------------------------------

int processFile(trenddata *t)
{
int 				firstLineFound=0, finalLineFound=0;
struct tm   *pstm=NULL;


getFileSize(t->fname , &(t->fsize));
if (_verbose > 2)
  printf("\n fsize: %li", t->fsize);

// convert received strings to time_t
getTimetFromString(t->initime_s, &(t->initime_t));
getTimetFromString(t->fintime_s, &(t->fintime_t));


if ( ((t->f)=fopen(t->fname, "r")) != NULL )
  {
  // check file min initial and final time_t
  // and set ini fin marks accordingly
  getFileLimits(t);


	if ( t->initime_t <= t->firstline_t )
		{ t->inip = 0; firstLineFound=1; t->initime_t = t->firstline_t; } 
  else	
		firstLineFound = findPosition_t(t->f, t->initime_t, t->fsize, t->linelen, 1, &(t->inip));

  	if ( t->fintime_t >= t->lastline_t )
		{ t->finp = t->fsize; finalLineFound=1; t->fintime_t = t->lastline_t; }
  else	
		finalLineFound = findPosition_t(t->f, t->fintime_t, t->fsize, t->linelen, 0, &(t->finp));

  if (firstLineFound && finalLineFound)
		{
		if (_verbose > 2)
			{
	  	printf("\n delta t: %lf t1/p1: %li/%li  t1/p2: %li/%li p2-p1: %li", t->step_t, t->initime_t, t->inip, t->fintime_t, t->finp, t->finp-t->inip);
			pstm = localtime(&(t->initime_t));
			printf("\nStart date: %4d-%02d-%02d %02d:%02d", pstm->tm_year+1900, pstm->tm_mon+1, pstm->tm_mday, pstm->tm_hour, pstm->tm_min);
			pstm = localtime(&(t->fintime_t));
			printf("\nFinal date: %4d-%02d-%02d %02d:%02d", pstm->tm_year+1900, pstm->tm_mon+1, pstm->tm_mday, pstm->tm_hour, pstm->tm_min);
			printf("\n Total minutes to compute: %li", (t->fintime_t - t->initime_t) / 60);
			}
			// now it's high time for memory allocation, based on nsamples. nfields, etc
	
		if ( ( t->minutes = (unsigned char *) malloc( (1000 + (t->fintime_t - t->initime_t) / 60) * sizeof(char)) ) == NULL )
	  	{
	  	perror(" allocation error in 't->minutes' [processFile]");
	  	exit(1);
	  	}
		memset(t->minutes, 0, (1000 + (t->fintime_t - t->initime_t) / 60) * sizeof(char) );	

  	t->intialMinute = t->initime_t / 60;
  	t->finalMinute = t->fintime_t / 60;
		



		}

  fclose(t->f);
  }  // fopen
  
return(1);


return(0);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------

int main(int argc,char *argv[])
{
int 						c=0; 
trenddata				td;
struct tm       *pstm=NULL;
time_t          t=0;

trenddata_init(&td);

t=time(NULL);
pstm = localtime(&t);
sprintf(td.fintime_s, "%4d%02d%02d%02d%02d", pstm->tm_year+1900, pstm->tm_mon+1, pstm->tm_mday, pstm->tm_hour, pstm->tm_min);

t=time(NULL) - 3600 * 24 * 90;  //  last 90 days
pstm = localtime(&t);
sprintf(td.initime_s, "%4d%02d%02d%02d%02d", pstm->tm_year+1900, pstm->tm_mon+1, pstm->tm_mday, pstm->tm_hour, pstm->tm_min);
        

while ( ( c= getopt(argc,argv,"t:i:f:s:vp:")) != -1 )
  {
  switch(c)
	{
	case 'v': 
	  _verbose++; 
	break;
  
	case 't':
	  if (optarg)
		strcpy(td.fname, optarg); 
	break;

	case 's':
    if (optarg)
        {
        int n=atoi(optarg);
        td.fieldsel_d[td.fieldsel_n] = n;
        td.fieldsel_n++;
        }
	break;

  case 'i':
    if (optarg)
        {
        int ndays=atoi(optarg);
        t=time(NULL) - 3600 * 24 * ndays;
        pstm = localtime(&t);
        sprintf(td.initime_s, "%4d%02d%02d%02d%02d%02d", pstm->tm_year+1900, pstm->tm_mon+1, pstm->tm_mday, pstm->tm_hour, pstm->tm_min, pstm->tm_sec);
        }
	break;

	case 'f':
	  if (optarg)
		strcpy(td.fintime_s, optarg); 
	break;

	case '?': 
	  PrintUsage(argv[0]); 
	  exit(0); 
	break;
	}
  }


if (_verbose > 0)
    {
    printf ("* Verbose mode     (-v): %i \n", _verbose);
    printf ("* Compiled: %s / %s \n", __DATE__, __TIME__);
    }


if ( !td.fname[0] )
  {
  PrintUsage(argv[0]); 
  exit(0); 
  }


//getBWDataFromFile(&td);

processFile(&td);

printf("\n\n");

return(0);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
