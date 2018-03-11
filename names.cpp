#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "watdefs.h"
#include "date.h"

/* The .sp3 ephemerides for GNSS satellites use three-character
designations for the satellites.  The designations are a letter
followed by two digits,  such as 'G03' or 'R15'.

   Those designations aren't used much of anyplace else.  They
also get re-used,  so the .sp3 designation G03 could mean one
satellite today and a replacement satellite next year.  So we
need some sort of cross-designation table to give you the 'real'
designations.

   The file ftp://ftp.aiub.unibe.ch/BSWUSER52/GEN/I14.ATX
you those cross-designations,  along with a lot of GNSS antenna
data not relevant to this purpose.

   For GPS satellites,  the .sp3 designation is a 'G' followed by
the PRN number.  For GLONASS,  it's an 'R' followed by a slot
number, which I think means an orbital slot.  Galileo uses 'E',
BeiDou 'C', and the new Japanese satellites 'J'.  Presumably, future
constellations will use other letters.  This all ought to work, as
long as there are 99 or fewer satellites to a constellation.

Anyway.  This code looks for lines such as the following in I14.ATX,

BLOCK IIA           G01                 G032      1992-079A TYPE / SERIAL NO
  1992    11    22     0     0    0.0000000                 VALID FROM
  2008    10    16    23    59   59.9999999                 VALID UNTIL

skipping over a lot of things that are very relevant if you're
interested in GNSS antenna characteristics,  but aren't relevant
to cross-referencing .sp3 designations (G01,  in the above case)
to international designations (1992-079A,  in the above case,
for dates from 1992 Nov 22 = MJD 48948 to 2008 Oct 17 = MJD 54756.)

The results are written to 'names.txt' and used in 'list_gps.cpp'.
The above example for G01 = 1992-079A gets written as :

48948 54756 G01 G032 1992-079A BLOCK IIA

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

static void output_line( const char *name, const char *from, const char *until)
{
   int len = 13;

   output_mjd( from);
   output_mjd( until);
   while( len && name[len - 1] == ' ')
      len--;
   printf( "%.3s %.4s %.9s %.*s\n",
               name + 20, name + 40, name + 50, len, name);
}

static const char *beidou_and_qzs =
   "00000 99999 C11      2012-018A Beidou 12\n"
   "00000 99999 C12      2012-018B Beidou 13\n"
   "00000 99999 C05      2012-008A Beidou 11\n"
   "00000 99999 C02      2012-059A Beidou 16\n"
   "00000 99999 C09      2011-038A Beidou 9\n"
   "00000 99999 C10      2011-073A Beidou 10\n"
   "00000 99999 C06      2010-036A Beidou 5\n"
   "00000 99999 C13      2016-021A Beidou IGSO-6\n"
   "00000 99999 C03      2010-024A Beidou 4\n"
   "00000 99999 C08      2011-013A Beidou 8\n"
   "00000 99999 C07      2010-068A Beidou 7\n"
   "00000 99999 C01      2010-001A Beidou 3\n"
   "00000 99999 C04      2010-057A Beidou 6\n"
   "00000 99999 C14      2012-050B Beidou 15\n"
   "00000 99999 J01      2010-045A QZS-1 (MICHIBIKI)\n"
   "00000 99999 J02      2017-028A QZS-2\n"
   "00000 99999 J03      2017-062A QZS-4\n";


int main( const int argc, const char **argv)
{
   FILE *ifile = fopen( "I14.ATX", "rb");
   char name[BUFF_SIZE], from[BUFF_SIZE], until[BUFF_SIZE], buff[BUFF_SIZE];
   bool done = false;

   if( !ifile)
      {
      printf( "This program needs the file I14.ATX.  Download it from\n"
              "ftp://ftp.aiub.unibe.ch/BSWUSER52/GEN/I14.ATX\n");
      return( -1);
      }
   *name = '\0';
   memset( from, 0, sizeof( from));
   memset( until, 0, sizeof( until));
   printf( "#  See 'names.cpp'\n");
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
   printf( "%s", beidou_and_qzs);
   return( 0);
}
