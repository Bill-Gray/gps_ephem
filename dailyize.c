/* dailyize.c:  code to read and merge Earth Orientation Parameter (EOP)
files downloaded from sites listed below.

Copyright (C) 2018, Project Pluto

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.    */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

/* Earth Orientation Parameter (EOP) files are provided as
'finals.all' (covers the earth's orientation from early 1973
to nearly the present) and 'finals.daily' (covers from about
three months ago and gives predictions roughly a year ahead).
This program merges them into 'finals.mix',  giving you one
file running from 1973 to about a year from now.

   The files were available from

ftp://ftp.iers.org/products/eop/rapid/standard/finals.all
ftp://ftp.iers.org/products/eop/rapid/daily/finals.daily
http://maia.usno.navy.mil/ser7/finals.all
http://maia.usno.navy.mil/ser7/finals.daily

   and can now (as of June 2022) be found at

https://datacenter.iers.org/data/latestVersion/12_FINALS.DAILY_IAU1980_V2013_0112.txt
https://datacenter.iers.org/data/latestVersion/7_FINALS.ALL_IAU1980_V2013_017.txt

   The URLs have changed in random,  not predictable ways.  Be warned.
But the format of the files,  at least,  hasn't changed since I started
using them.

   Both files contain lines of 188 bytes each,  with each line giving
parameters for one day. The code reads the first line of each file;  the
difference between their MJDs tells us how many lines we can just read
straight from 'finals.all' and write directly to the output.  Then we do
the same thing for 'finals.daily', reading but ignoring the corresponding
lines from 'finals.all' just so that we stay aligned.  Finally,  we read
the remaining lines from 'finals.all' (if any) and output those.

   I have a cron job on the projectpluto.com server that downloads
'finals.all' once a week,  and another that downloads 'finals.daily'
daily and then runs this program to create the 'finals.mix' file.  The
various GPS tools then ingest the .mix version.

   The various files should,  without question,  be openable.  It
also should be possible to read 188-byte lines from them.  If it's
not, we just fail.    */

static FILE *err_fopen( const char *filename, const char *permits)
{
   FILE *rval = fopen( filename, permits);

   if( !rval)
      {
      fprintf( stderr, "Couldn't open %s\n", filename);
      perror( NULL);
      exit( -1);
      }
   return( rval);
}

const size_t eop_line_len = 188;

static char *err_fgets( char *buff, const size_t len, FILE *ifile)
{
   char *rval = fgets( buff, len, ifile);

   if( !rval)
      {
      perror( "Couldn't fgets a line");
      exit( -2);
      }
   if( strlen( buff) != eop_line_len)
      {
      fprintf( stderr, "Line is %d bytes long\n", (int)strlen( buff));
      exit( -3);
      }
   return( rval);
}

/* EOP lines start with a date in YYMMDD form,  without leading
zeroes on the MM or DD.  This transforms the date to YYYY MM DD
form,  with leading zeroes.  I just find that easier to read. */

static char *format_eop_date( const char *iline, char *buff)
{
   sprintf( buff, "%s%.2s %.2s %.2s",
            (*iline > '6' ? "19" : "20"), iline, iline + 2, iline + 4);

   if( buff[5] == ' ')     /* add leading zero for months Jan-Sep */
      buff[5] = '0';
   if( buff[8] == ' ')     /* add leading zero for day of month 1-9 */
      buff[8] = '0';
   return( buff);
}

int main( void)
{
   FILE *all = err_fopen( "finals.all", "rb");
   FILE *daily = err_fopen( "finals.daily", "rb");
   FILE *ofile;
   int mjd1 = 41684, mjd2;
   char buff[200], date_buff[80];
   const char *start_of_finals_dot_all = "73 1 2 41684.00 I  0";
   bool daily_predicts_shown = false, all_predicts_shown = false;

   err_fgets( buff, sizeof( buff), all);
   printf( "%s : start date for finals.all\n", format_eop_date( buff, date_buff));
   assert( !memcmp( buff, start_of_finals_dot_all, 20));
   fseek( all, 0L, SEEK_SET);

   err_fgets( buff, sizeof( buff), daily);
   printf( "%s : start date for finals.daily\n", format_eop_date( buff, date_buff));
   mjd2 = atoi( buff + 7);
   assert( mjd2 > 58150);
   assert( mjd2 > mjd1);
   fseek( daily, 0L, SEEK_SET);
   ofile = err_fopen( "finals.mix", "wb");
   while( mjd1++ < mjd2)
      {
      err_fgets( buff, sizeof( buff), all);
      fwrite( buff, eop_line_len, 1, ofile);
      }
   while( fgets( buff, sizeof( buff), daily))
      {
      fwrite( buff, eop_line_len, 1, ofile);
      if( !daily_predicts_shown && buff[16] == 'P')
         {
         printf( "%s : predicts begin for finals.daily\n", format_eop_date( buff, date_buff));
         daily_predicts_shown = true;
         }
      err_fgets( buff, sizeof( buff), all);
      if( !all_predicts_shown && buff[16] == 'P')
         {
         printf( "%s : predicts begin for finals.all\n", format_eop_date( buff, date_buff));
         all_predicts_shown = true;
         }
      }
   printf( "%s : end date for finals.daily\n", format_eop_date( buff, date_buff));
   while( fgets( buff, sizeof( buff), all))
      fwrite( buff, eop_line_len, 1, ofile);

   printf( "%s : end date for finals.all\n", format_eop_date( buff, date_buff));
   fclose( all);
   fclose( daily);
   fclose( ofile);
   return( 0);
}
