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


extern int  		_verbose;



//----------------------------------------------------------------------

// read a line containing:
// time_t, readable_datetime, Icounter, Ocounter, DownloadBW, UploadBW

int getValuesFromLine(char *str, time_t *t, unsigned long long int *dateTimeInMinutes, double *DownBW, double *UpBW)
{
char 	*textElement = NULL;
int 	count=0;
if (!str || !t || !dateTimeInMinutes || !DownBW || !UpBW)
  return(0);

textElement = strtok(str, ",");
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
	textElement = strtok(NULL, ",");
	}
return(1);

}

//----------------------------------------------------------------------

int trenddata_init(trenddata *t)
{
memset(t, 0, sizeof(trenddata));
t->final_samples=1000;
strcpy(t->fpath, "/usr/local/data/bw/");
t->raw_data_str = NULL;
t->orig_data = NULL;
return(1);
}

//----------------------------------------------------------------------


int getFileSize(char *fname, long int *fsize)
{
struct stat f_stat;

if (!fname)
  return(0);
  
if (stat(fname, &f_stat))
  return(0);
  
*fsize = f_stat.st_size;
return(1);
}

//----------------------------------------------------------------------

// convert a given string (201203031145)  to a timestamp  (minute resolution)

int getMinuteFromString(char *str, unsigned long long int *m)
{
char *aux=NULL;

if (!str || !m)
  return(0);

if (strlen(str) < 12)	
  return(0);

aux = strdup(str);
aux[12] = 0;
*m = atoll(aux);  
free(aux);
 
if (_verbose > 2)
  printf("\n convert |%s| to %lli", str, *m);

return(1);
}

//----------------------------------------------------------------------

// convert a given string (201203031145)  to a timestamp  (minute resolution)

int getTimetFromString(char *str, time_t *t)
{
struct tm 	stm;
int 	    xyear=0, xmon=0, xday=0, xhour=0, xmin=0;
	
memset(&stm, 0, sizeof(stm));
if (!str || !t)
  return(0);

if (strlen(str) < 12)	
  return(0);
  
sscanf(str, "%4d%02d%02d%02d%02d", &xyear, &xmon, &xday, &xhour, &xmin);
stm.tm_year=xyear-1900;
stm.tm_mon=xmon-1;
stm.tm_mday=xday;
stm.tm_hour=xhour;
stm.tm_min=xmin;

*t = timelocal(&stm); 
 
if (_verbose > 2)
  printf("\n convert |%s|%4d-%02d-%02d %02d:%02d| into %li", str, xyear, xmon, xday, xhour, xmin, *t);

return(1);
}

//----------------------------------------------------------------------

int composeFname( char *result, char *fpath, char *fname)
{
if (fpath && fname && result)
    {
    strcpy(result, fpath);
    strcat(result, "/");
    strcat(result, fname);

    return(1);
    }

return (0);
}

//----------------------------------------------------------------------

// read first COMPLETE line after position 'l'
// returns the accurate position where the line begins
// and also the begining of next line, if the corresponding pointers are not null

int readLineFromPositionX(FILE *f, long int pos, char *l, long int *inipos, long int *finpos)
{
long int 		ret_len=0;
char 			p[1000];
int 			i=0, j=0;

if (!f || !l)
  return(0);

//  void setbuffer(FILE *stream, char *buf, size_t size);
fseek(f, pos , SEEK_SET);
if ( (ret_len=fread(p, 1, 500, f)) > 0)
  {
  while ( p[i]!=0 && p[i]!='\n' && p[i]!='\r' && i<ret_len )
	i++;  

  while ( p[i]!=0 && (p[i]=='\n' || p[i]=='\r' ) && i<ret_len )
	i++;
	
  if (inipos)  *inipos = pos + i;

  while ( p[i]!=0 && p[i]!='\n' && p[i]!='\r' && i<ret_len )
	l[j++] = p[i++];  

  l[j] = 0;

  if (finpos)
	{	
	while ( p[i]!=0 && (p[i]=='\n' || p[i]=='\r' ) && i<ret_len )
	  i++;
	*finpos = pos + i;
	}
  }
return(1);
}

//----------------------------------------------------------------------

// read first line and last line and calculate time-limits 
// count also the number of fields on those lines

int getFileLimits(trenddata *t)
{
char 			p[1000], x[1000];
int 			i=0, j=0;
int 			l1=0, l2=0;
int 			nfields1=0, nfields2=0;	


//  go to the begining
fseek(t->f, 0 , SEEK_SET);
if ( fgets(p, 500, t->f) )
  {
  l1= strlen(p);
  if (_verbose > 6) printf("\n first line: |%s|", p);
  while ( p[i]!=0 && p[i]!='.' && p[i]!=',' )
	x[j++] = p[i++];  

  x[j] = 0;
  if (_verbose > 4) printf("\n first: |%s|", x);
  t->firstline_t = atol(x);

  // read number of fields in first line
  i=j=0;
  nfields1 = 1;
  while ( p[i]!=0 && p[i]!='\n' && p[i]!='\r' )
	if ( p[i++] == ',' )
	  nfields1++;

  //  go near the end
  i=j=0;
  fseek(t->f, -2000 , SEEK_END);
  while (fgets(p, 500, t->f));

  if (_verbose > 6) printf("\n last line: |%s|", p);

  l2= strlen(p);
  while ( p[i]!=0 && p[i]!='.' && p[i]!=',' )
	x[j++] = p[i++];  

  x[j] = 0;
  if (_verbose > 4) printf("\n last: |%s|", x);
  t->lastline_t = atol(x);

  // read number of fields in last line
  i=j=0;
  nfields2 = 1;
  while ( p[i]!=0 && p[i]!='\n' && p[i]!='\r' )
	if ( p[i++] == ',' )
	  nfields2++;

  // update number of fields (should be nfields1 = nfields2, but just in 
  // case we take the larger (minus 2, due to tiem_t and date-time fields  
  t->nfields = (nfields1 > nfields2) ? nfields1-2 : nfields2-2;
  t->linelen = (l1 + l2)/2;
  }
if (_verbose > 2)
  printf("\n first_t: %li last_t: %li line_len: %i nfields: %i\n", t->firstline_t, t->lastline_t, t->linelen, t->nfields);

return(0);
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
time_t		tf=0, tprev1=0;  //  prevoius values to stop binary search
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
//----------------------------------------------------------------------
