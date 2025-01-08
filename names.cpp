#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#ifdef __has_include
   #if __has_include(<watdefs.h>)
       #include "watdefs.h"
   #else
       #error   \
         'watdefs.h' not found.  This project depends on the 'lunar'\
         library.  See www.github.com/Bill-Gray/lunar .\
         Clone that repository,  'make'  and 'make install' it.
       #ifdef __GNUC__
         #include <stop_compiling_here>
            /* Above line suppresses cascading errors. */
       #endif
   #endif
#else
   #include "watdefs.h"
#endif
#include "date.h"

/* The .sp3 ephemerides for GNSS satellites use three-character
designations for the satellites.  The designations are a letter
followed by two digits,  such as 'G03' or 'R15'.

   Those designations aren't used much of anyplace else.  They
also get re-used,  so the .sp3 designation G03 could mean one
satellite today and a replacement satellite next year.  So we
need some sort of cross-designation table to give you the 'real'
designations.

   The file ftp://ftp.aiub.unibe.ch/BSWUSER54/REF/I20.ATX
you those cross-designations,  along with a lot of GNSS antenna
data not relevant to this purpose.

   For GPS satellites,  the .sp3 designation is a 'G' followed by
the PRN number.  For GLONASS,  it's an 'R' followed by a slot
number, which I think means an orbital slot.  Galileo uses 'E',
BeiDou 'C', and the new Japanese satellites 'J'.  Presumably, future
constellations will use other letters.  This all ought to work, as
long as there are 99 or fewer satellites to a constellation.

Anyway.  This code looks for lines such as the following in I20.ATX,

BLOCK IIA           G01                 G032      1992-079A TYPE / SERIAL NO
  1992    11    22     0     0    0.0000000                 VALID FROM
  2008    10    16    23    59   59.9999999                 VALID UNTIL

skipping over a lot of things that are very relevant if you're
interested in GNSS antenna characteristics,  but aren't relevant
to cross-referencing .sp3 designations (G01,  in the above case)
to international designations (1992-079A,  in the above case,
for dates from 1992 Nov 22 = MJD 48948 to 2008 Oct 17 = MJD 54756.)

The international designation is then cross-referenced to a five-digit NORAD
one.   The results are written to 'names.txt' and used in 'list_gps.cpp'.
The above example for G01 = 1992-079A = NORAD 22231 gets written as :

48948 54756 G01 G032 1992-079A   22231 BLOCK IIA
*/

#define BUFF_SIZE 100

/* Many designations have no 'UNTIL' line.  Those get marked 'until
forever'... with MJD 99999 = 2132 Aug 31 being 'forever enough'. */

const char *until_forever =
   "  2132    08    31    00    00   00.0000000                 VALID UNTIL";

static void output_mjd( const char *date)
{
   long jd = dmy_to_day( atoi( date + 16), atoi( date + 10),
                  atol( date + 2), CALENDAR_GREGORIAN);

   if( date[22] == '2')    /* i.e.,  23 59 59.99999 */
      jd++;
   printf( "%ld ", jd - 1 - 2400000);
}

#define MAX_REMAPS 100000

/* I20.ATX has international (YYYY-NNNA) designations,  but not the NORAD
five-digit designations.  Those are gathered from Jonathan McDowell's
"master satellite list",  updated "every few months",  at

https://planet4589.org/space/gcat/data/cat/satcat.html

   and is currently hard-coded to be in a particular place on my hard drive.
*/

static int get_norad_number( const char *intl_id)
{
   static char *remap = NULL;
   char *tptr;
   int rval = 0;

   if( !remap)
      {
      FILE *ifile = fopen( "../.find_orb/satcat.html", "rb");
      char buff[750];

      assert( ifile);
      remap = tptr = (char *)malloc( MAX_REMAPS * 21);
      while( fgets( buff, sizeof( buff), ifile))
         {
         memcpy( tptr, buff + 12, 7);     /* NORAD number */
         memcpy( tptr + 7, buff + 20, 14);      /* intl desig */
         tptr += 21;
         }
      *tptr = '\0';
      fclose( ifile);
      }
   tptr = remap;
   while( !rval && *tptr)
      {
      if( !memcmp( intl_id, tptr + 7, 9))
         rval = atoi( tptr);
      tptr += 21;
      }
   if( !rval)
      {
      fprintf( stderr, "Couldn't find '%s'\n", intl_id);
      fprintf( stderr, "Update 'satcat.html' and try again\n");
      exit( -1);
      }
   return( rval);
}

static void output_line( const char *name, const char *from, const char *until)
{
   int len = 13;

   output_mjd( from);
   output_mjd( until);
   while( len && name[len - 1] == ' ')
      len--;
   printf( "%.3s %.4s %.9s   %05d %.*s\n",
               name + 20, name + 40, name + 50,
               get_norad_number( name + 50),
               len, name);
}

int main( void)
{
   FILE *ifile = fopen( "I20.ATX", "rb");
   char name[BUFF_SIZE], from[BUFF_SIZE], until[BUFF_SIZE], buff[BUFF_SIZE];
   bool done = false;
   time_t t0 = time( NULL);

   if( !ifile)
      {
      printf( "This program needs the file I20.ATX.  Download it from\n"
              "ftp://ftp.aiub.unibe.ch/BSWUSER54/REF/I20.ATX\n");
      return( -1);
      }
   *name = '\0';
   memset( from, 0, sizeof( from));
   memset( until, 0, sizeof( until));
   printf( "#  See 'names.cpp'. Run at %.24s UTC\n", asctime( gmtime( &t0)));
   while( !done && fgets( buff, sizeof( buff), ifile))
      {
      if( !memcmp( buff + 60, "TYPE / SERIAL NO", 16))
         {
         if( *name)
            output_line( name, from, until);
         buff[59] = '\0';
         strcpy( name, buff);
         strcpy( until, until_forever);
         if( buff[50] == ' ')
            done = true;
         }
      if( !memcmp( buff + 60, "VALID FROM", 10))
         strcpy( from, buff);
      if( !memcmp( buff + 60, "VALID UNTIL", 11))
         strcpy( until, buff);
      }
   fclose( ifile);
   return( 0);
}
