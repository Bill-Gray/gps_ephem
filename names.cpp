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

/* I14.ATX has international (YYYY-NNNA) designations,  but not the NORAD
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

/* https://en.wikipedia.org/wiki/List_of_BeiDou_satellites can aid in adding
to this list as new BeiDou satellites are added.  You still have to get the
international ID elsewhere.         */

static const char *beidou_and_qzs =
   "00000 99999 C01      2010-001A   36287 Beidou 3\n"       /* Compass-G1; geo 140 E */
   "00000 99999 C02      2012-059A   38953 Beidou 16\n"      /* Compass-G6: geo 80 E  */
   "00000 99999 C03      2010-024A   36590 Beidou 4\n"       /* Compass-G7: geo 110.5 E */
   "00000 99999 C04      2010-057A   37210 Beidou 6\n"       /* Compass-G4: geo 160 E */
   "00000 99999 C05      2012-008A   38091 Beidou 11\n"      /* Compass-G5: geo 58.75 E */
   "00000 99999 C06      2010-036A   36828 Beidou 5\n"       /* Compass-IGSO1 : 55 incl, 118 E */
   "00000 99999 C07      2010-068A   37256 Beidou 7\n"       /* Compass-IGSO2 : 55 incl, 118 E */
   "00000 99999 C08      2011-013A   37384 Beidou 8\n"       /* Compass-IGSO3 : 55 incl, 118 E */
   "00000 99999 C09      2011-038A   37763 Beidou 9\n"       /* Compass-IGSO4 : 55 incl, 95 E */
   "00000 99999 C10      2011-073A   37948 Beidou 10\n"      /* Compass-IGSO5 : 55 incl, 95 E */
   "00000 99999 C11      2012-018A   38250 Beidou 12\n"      /* Compass-M3: slot A07         */
   "00000 99999 C12      2012-018B   38251 Beidou 13\n"      /* Compass-M4: slot A08         */
   "00000 99999 C13      2016-021A   41434 Beidou IGSO-6\n"  /* Compass-IGSO6 : 55 incl, 95 E */
   "00000 99999 C14      2012-050B   38775 Beidou 15\n"      /* Compass-M6: slot B04         */
   "00000 99999 C16      2018-057A   43539 Beidou IGSO-7\n"  /* Compass-M6: slot B04         */
   "00000 99999 C19      2017-069A   43001 Beidou-3 M1\n"
   "00000 99999 C20      2017-069B   43002 Beidou-3 M2\n"
   "00000 99999 C21      2018-018B   43208 Beidou-3 M6\n"
   "00000 99999 C22      2018-018A   43207 Beidou-3 M5\n"
   "00000 99999 C23      2018-062A   43581 Beidou-3 M9\n"
   "00000 99999 C24      2018-062B   43582 Beidou-3 M10\n"
   "00000 99999 C25      2018-067B   43603 Beidou-3 M12\n"
   "00000 99999 C26      2018-067A   43602 Beidou-3 M11\n"
   "00000 99999 C27      2018-003A   43107 Beidou-3 M3\n"
   "00000 99999 C28      2018-003B   43108 Beidou-3 M4\n"
   "00000 99999 C29      2018-029A   43245 Beidou-3 M7\n"
   "00000 99999 C30      2018-029B   43246 Beidou-3 M8\n"
   "00000 99999 C32      2018-072A   43622 Beidou-3 M13\n"
   "00000 99999 C33      2018-027B   43623 Beidou-3 M14\n"
   "00000 99999 C34      2018-078B   43648 Beidou-3 M16\n"
   "00000 99999 C35      2018-078A   43647 Beidou-3 M15\n"
   "00000 99999 C36      2018-093A   43706 Beidou-3 M17\n"
   "00000 99999 C37      2018-093B   43707 Beidou-3 M18\n"
   "00000 99999 C38      2019-023A   44204 Beidou-3 IGSO-1\n"
   "00000 99999 C39      2019-035A   44337 Beidou-3 IGSO-2\n"
   "00000 99999 C40      2019-073A   44709 Beidou-3 IGSO-3\n"
   "00000 99999 C41      2019-090A   44864 Beidou-3 M19\n"
   "00000 99999 C42      2019-090B   44865 Beidou-3 M20\n"
   "00000 99999 C43      2019-078B   44794 Beidou-3 M22\n"
   "00000 99999 C44      2019-078A   44793 Beidou-3 M21\n"
   "00000 99999 C45      2019-061B   44543 Beidou-3 M24\n"
   "00000 99999 C46      2019-061A   44542 Beidou-3 M23\n"
   "00000 99999 C59      2018-085A   43683 Beidou-3 G1\n"
   "00000 99999 C60      2020-017A   45344 Beidou-3 G2\n"
   "00000 99999 J01      2010-045A   37158 QZS-1 (MICHIBIKI)\n"
   "00000 99999 J02      2017-028A   42738 QZS-2\n"
   "00000 99999 J03      2017-062A   42965 QZS-4\n"
   "00000 99999 J07      2017-048A   42917 QZS-3\n";

int main( void)
{
   FILE *ifile = fopen( "I14.ATX", "rb");
   char name[BUFF_SIZE], from[BUFF_SIZE], until[BUFF_SIZE], buff[BUFF_SIZE];
   bool done = false;
   time_t t0 = time( NULL);

   if( !ifile)
      {
      printf( "This program needs the file I14.ATX.  Download it from\n"
              "ftp://ftp.aiub.unibe.ch/BSWUSER52/GEN/I14.ATX\n");
      return( -1);
      }
   *name = '\0';
   memset( from, 0, sizeof( from));
   memset( until, 0, sizeof( until));
   printf( "#  See 'names.cpp'. Run at %s", ctime( &t0));
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
