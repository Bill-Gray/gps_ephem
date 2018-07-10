/* dailyize.c:  code to read and merge Earth Orientation Parameter (EOP)
files downloaded from ftp://maia.usno.navy.mil/ser7.  Details below licence.

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

/* 'finals.all' contains essentially all EOPs,  but is updated weekly.
'finals.daily' gives you the last three months and the next three months,
but is (as the name suggests) updated daily. Ideally,  you'd mix the two,
using 'daily' data where you can and 'all' data for everything else.
The following code does just that,  creating a 'finals.mix' file.

   Both files contain lines of 188 bytes each,  with each line giving
parameters for one day. The code reads the first line of each file;  the
difference between their MJDs tells us how many lines we can just read
straight from 'finals.all' and write directly to the output.  Then we do
the same thing for 'finals.daily', reading but ignoring the corresponding
lines from 'finals.all' just so that we stay aligned.  Finally,  we read
the remaining lines from 'finals.all' and output those.

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

int main( const int argc, const char **argv)
{
   FILE *all = err_fopen( "finals.all", "rb");
   FILE *daily = err_fopen( "finals.daily", "rb");
   FILE *ofile = err_fopen( "finals.mix", "wb");
   int mjd1, mjd2;
   char buff[200];

   err_fgets( buff, sizeof( buff), all);
   mjd1 = atoi( buff + 7);
   assert( mjd1 > 41600);
   fseek( all, 0L, SEEK_SET);

   err_fgets( buff, sizeof( buff), daily);
   mjd2 = atoi( buff + 7);
   assert( mjd2 > 58150);
   assert( mjd2 > mjd1);
   fseek( daily, 0L, SEEK_SET);
   while( mjd1++ < mjd2)
      {
      err_fgets( buff, sizeof( buff), all);
      fwrite( buff, eop_line_len, 1, ofile);
      }
   while( fgets( buff, sizeof( buff), daily))
      {
      fwrite( buff, eop_line_len, 1, ofile);
      err_fgets( buff, sizeof( buff), all);
      }
   while( fgets( buff, sizeof( buff), all))
      fwrite( buff, eop_line_len, 1, ofile);

   fclose( all);
   fclose( daily);
   fclose( ofile);
   return( 0);
}
