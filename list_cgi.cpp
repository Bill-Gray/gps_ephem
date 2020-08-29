/* list_cgi.cpp: CGI version of GPS satellite tools

Copyright (C) 2017, Project Pluto

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
#include <time.h>
#ifdef __has_include
   #if __has_include(<cgi_func.h>)
       #include "cgi_func.h"
   #else
       #error   \
         'cgi_func.h' not found.  This project depends on the 'lunar'\
         library.  See www.github.com/Bill-Gray/lunar .\
         Clone that repository,  'make'  and 'make install' it.
       #ifdef __GNUC__
         #include <stop_compiling_here>
            /* Above line suppresses cascading errors. */
       #endif
   #endif
#else
   #include "cgi_func.h"
#endif

int dummy_main( const int argc, const char **argv);      /* list_cgi.cpp */

int main( void)
{
   const size_t max_buff_size = 1000000;   /* should be enough for anybody */
   char *buff = (char *)malloc( max_buff_size);
   char time_text[100], observatory_code[20];
   char field[30];
   FILE *lock_file = fopen( "lock.txt", "a");
   int rval, i;
   const time_t t0 = time( NULL);
   int n_args = 3;
   char *args[20];
   bool use_tles = false;

   printf( "Content-type: text/html\n\n");
   printf( "<pre>");
   if( !lock_file)
      {
      printf( "<h1> Server is busy.  Try again in a minute or two. </h1>");
      printf( "<p> Your GPS position request is very important to us! </p>");
      printf( "<p> (I don't really expect this service to get a lot of "
              "use.  If you see this error several times in a row, "
              "something else must be wrong; please contact me.)</p>");
      return( 0);
      }
   setvbuf( lock_file, NULL, _IONBF, 0);
   fprintf( lock_file, "Current time %s", ctime( &t0));
   fprintf( lock_file, "list_cgi ver: %s %s\n", __DATE__, __TIME__);
   avoid_runaway_process( 300);
   fprintf( lock_file, "300-second limit set\n");
   args[0] = NULL;
   args[1] = time_text;
   args[2] = observatory_code;
   rval = initialize_cgi_reading( );
   if( rval <= 0)
      {
      printf( "<p> <b> CGI data reading failed : error %d </b>", rval);
      printf( "This isn't supposed to happen.</p>\n");
      return( 0);
      }
   while( !get_cgi_data( field, buff, NULL, max_buff_size))
      {
      char option = 0;

      fprintf( lock_file, "Field '%s': '%s'\n", field, buff);
      if( !strcmp( field, "time") && strlen( buff) < 80)
         strcpy( time_text, buff);
      if( !strcmp( field, "min_alt") && strlen( buff) < 10)
         option = 'a';
      if( !strcmp( field, "sort") && strlen( buff) < 10)
         option = 's';
      if( !strcmp( field, "relocate") && strlen( buff) < 60)
         option = 'r';
      if( !strcmp( field, "n_steps") && strlen( buff) < 10)
         option = 'n';
      if( !strcmp( field, "ang_fmt") && *buff == '1')
         {
         option = 'd';
         *buff = '\0';
         }
      if( !strcmp( field, "ang_fmt") && *buff == '2')
         {
         option = 'f';
         *buff = '\0';
         }
      if( !strcmp( field, "step") && strlen( buff) < 10)
         option = 'i';
      if( !strcmp( field, "obj") && strlen( buff) < 10)
         option = 'o';
      if( !strcmp( field, "use_tles"))
         use_tles = true;
      if( !strcmp( field, "ast"))
         {
         const char *filename = "temp.ast";
         FILE *ofile = fopen( filename, "wb");

         assert( ofile);
         fwrite( buff, strlen( buff), 1, ofile);
         fclose( ofile);
         strcpy( buff, filename);
         n_args = 1;
         option = 'f';
         }
      if( !strcmp( field, "obscode") && strlen( buff) < 20)
         {
         strcpy( observatory_code, buff);
         if( buff[3] == 'v')
            {
            option = 'v';
            observatory_code[3] = *buff = '\0';
            }
         }
      if( option)
         {
         args[n_args] = (char *)malloc( strlen( buff) + 3);
         args[n_args][0] = '-';
         args[n_args][1] = option;
         strcpy( args[n_args] + 2, buff);
         n_args++;
         }
      }
   if( !use_tles)
      {
      args[n_args] = (char *)malloc( 4);
      strcpy( args[n_args], "-t1");
      n_args++;
      }
   fprintf( lock_file, "Options read and parsed\n");
   for( i = 0; i < n_args; i++)
      fprintf( lock_file, "%d: '%s'\n", i, args[i]);
   rval = dummy_main( n_args, (const char **)args);
   fprintf( lock_file, "Done: rval %d\n", rval);
   for( i = 3; i < n_args; i++)
      free( args[i]);
   fclose( lock_file);
}
