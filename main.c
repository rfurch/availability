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

int processFile(trenddata *t)
{
int 					firstLineFound=0, finalLineFound=0;
struct tm   			*pstm=NULL;
long int				toread=0, nread=0, totalread=0;
unsigned long long int 	ll1=0, ll2=0;

getFileSize(t->fname , &(t->fsize));
if (_verbose > 2)
  printf("\n fsize: %li", t->fsize);

// convert received strings to time_t
getTimetFromString(t->initime_s, &(t->initime_t));
getTimetFromString(t->fintime_s, &(t->fintime_t));

// convert received strings to minutes
getMinuteFromString(t->initime_s, &ll1);
getMinuteFromString(t->fintime_s, &ll2);

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
		t->bufferlen=totalread;  

		if (_verbose > 2)
			{
			printf("\n File read succesfully ! ");
			fflush(stdout);
			}
			
			
			




		{
		char 				*textLine=NULL;
		char 				*endLine=NULL;
				
			
		// get every line, separated by newline
		textLine = strtok_r(t->raw_data_str, "\n", &endLine);
		while( textLine != NULL ) 
			{
			char 	*textElement=NULL;
			int 	count = 0;	
			char 	*endElement=NULL;

			printf( " %s\n", textLine );     

			// get every field in each line

			count=0;
			textElement = strtok_r(textLine, ",", &endElement);
			while( textElement != NULL ) 
				{

				// get human readable date
				if (count == 1)
					{
					textElement[12]	= 0;
					printf("\n --- %s\n", textElement);
					}

				// get raw (upload and download) traffic (without filter!)
				else if (count == 4 || count == 5)
					{
//					textElement[12]	= 0;
//					printf("\n --- %s", textElement);
					printf( " %s\n", textElement );      
					}

				else
					printf( " %s\n", textElement );      


				count++;
				textElement = strtok_r(NULL, ",", &endElement);




				
				}

			textLine = strtok_r(NULL, "\n", &endLine);
			}
		}
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
