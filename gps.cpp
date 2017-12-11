#define CURL_STATICLIB
#include <stdio.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include "gps.h"

static size_t total_written;
int gps_verbose;

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written;

    written = fwrite(ptr, size, nmemb, stream);
    total_written += written;
    return written;
}

#define FETCH_FILESIZE_SHORT               -1
#define FETCH_FOPEN_FAILED                 -2
#define FETCH_CURL_PERFORM_FAILED          -3
#define FETCH_FILESIZE_WRONG               -4
#define FETCH_CURL_INIT_FAILED             -5

static int grab_file( const char *url, const char *outfilename,
                                    const bool append)
{
    CURL *curl = curl_easy_init();

    if (curl) {
        FILE *fp = fopen( outfilename, (append ? "ab" : "wb"));
//      const time_t t0 = time( NULL);

        if( !fp)
            return( FETCH_FOPEN_FAILED);
//      fprintf( fp, "%ld (%.24s) %s\n", (long)t0, ctime( &t0), url);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
        if( res) {
           if( gps_verbose)
              printf( "Curl fail %d\n", res);
           unlink( outfilename);
           return( FETCH_CURL_PERFORM_FAILED);
           }
    } else
        return( FETCH_CURL_INIT_FAILED);
    return 0;
}

static void try_to_download( const char *url, const char *filename)
{
   int rval;

   total_written = 0;
   rval = grab_file( url, filename, false);
   if( gps_verbose)
      printf( "Download '%s': %d, %ld bytes\n", url, rval, total_written);
   if( rval || total_written < 500)      /* just got an error message */
      unlink( filename);
   else if( filename[strlen( filename) - 1] == 'Z')
      {
      char command[60];

      snprintf( command, sizeof( command), "gzip -d %s", filename);
      rval = system( command);
      if( gps_verbose)
         printf( "'%s': %d\n", command, rval);
      }
}


/* GPS ephems are provided at fifteen-minute intervals.  We'll call
15 minutes = 1 "glumph",  for convenience.  I hope it's obvious,  but
there are 96 glumphs in a day. */

const int glumphs_per_day = 24 * 4;

/* To compute the positions of GPS satellites at any given time,
we need the ephemeris data for the two preceding and the two following
glumphs.  Then we can do a cubic spline between the four points.  (With
suitable error checking to make sure all four points are valid;
could be the ephems had a hiccup at one of the four points,  and I
assume some satellites got added/removed.)

   Caching the data becomes important here.  We'll have to correct for
light-time lag,  which means computing the object's position at time
t,  then at t-dist/c,  then again with a slightly different dist.
We may get fed a lot of times that are close together.  So the data
for each glumph is cached.

   For convenience,  when we read in a new data file,  all its positions
are cached.  In the ordinary way of things,  we'll usually use just one
file,  but sometimes two if we're at the "switchover" point from one
day's data to the next.  But by caching,  we ensure that even with some
pretty weird data usage,  the data we need will be available,  almost
always without having to thrash the disk.   */

#define N_CACHED 1000

typedef struct
{
   int glumph;
   double posns[MAX_N_GPS_SATS * 3];
} cached_posns_t;

static cached_posns_t *cache[N_CACHED];

static char desigs[MAX_N_GPS_SATS][4];

static int desig_to_index( const char *desig)
{
   int i;

   for( i = 0; desigs[i][0]; i++)
      if( !memcmp( desigs[i], desig, 3))
         return( i);
   assert( i < MAX_N_GPS_SATS);
   memcpy( desigs[i], desig, 3);
   return( i);
}

char *desig_from_index( const int idx)
{
   assert( idx >= 0 && idx < MAX_N_GPS_SATS);
   return( desigs[idx]);
}

static int read_posns_for_one_glumph( FILE *ifile, double *locs)
{
   char buff[200];
   int i;

   if( locs)
      for( i = 0; i < MAX_N_GPS_SATS * 3; i++)
         locs[i] = 0.;

   while( fgets( buff, sizeof( buff), ifile))
      if( *buff == '*')    /* start of a new glumph */
         {
         while( fgets( buff, sizeof( buff), ifile) && buff[0] == 'P')
            if( locs)
               {
               const int i = desig_to_index( buff + 1);
               double *tptr = locs + i * 3;

               assert( i >= 0);
               assert( i < MAX_N_GPS_SATS);
               sscanf( buff + 4, "%lf %lf %lf", tptr, tptr + 1, tptr + 2);
               }
         fseek( ifile, -strlen( buff), SEEK_CUR);
         return( 1);
         }
   return( 0);
}

static double *fetch_posns_from_cache( const int glumph)
{
   int i;

   for( i = 0; i < N_CACHED; i++)
      if( cache[i] && cache[i]->glumph == glumph)
         {
         cached_posns_t *tptr = cache[i];

         memmove( cache + 1, cache, i * sizeof( cached_posns_t *));
         cache[0] = tptr;
         if( gps_verbose)
            printf( "Found glumph %d in cache: %p\n", glumph, (void *)cache[0]->posns);
         return( cache[0]->posns);
         }
   if( gps_verbose)
      printf( "No luck finding glumph %d in cache\n", glumph);
   return( NULL);       /* not found in cache */
}

void free_cached_gps_positions( void)
{
   int i;

   for( i = 0; i < N_CACHED; i++)
      if( cache[i])
         {
         free( cache[i]);
         cache[i] = NULL;
         }
}

static void add_posns_to_cache( const int glumph, double *loc)
{
   cached_posns_t *tptr = cache[N_CACHED - 1];

   if( !tptr)
      tptr = (cached_posns_t *)calloc( 1, sizeof( cached_posns_t));
   assert( tptr);
   memmove( cache + 1, cache, (N_CACHED - 1) * sizeof( cached_posns_t *));
   cache[0] = tptr;
   tptr->glumph = glumph;
   memcpy( tptr->posns, loc, sizeof( tptr->posns));
}

/* The GPS timing system starts on 1980 Jan 7 = MJD 44245.
Seems to be a one-day offset within the .sp3 files... */

#define GPS_SYSTEM_START 44244.

static double *get_cached_posns( const char *filename, const int glumph)
{
   double *rval = fetch_posns_from_cache( glumph);

   if( !rval)
      {
      FILE *ifile = fopen( filename, "r");

      if( ifile)
         {
         double locs[MAX_N_GPS_SATS * 3];
         char buff[200];
         int i, freq, glumph0;

         for( i = 0; i < 2; i++)
            if( !fgets( buff, sizeof( buff), ifile))
               printf( "Error reading line %d\n", i + 1);
         freq = atoi( buff + 25);
         glumph0 = (int)( (atof( buff + 39) + atof( buff + 45)
                        - GPS_SYSTEM_START) * glumphs_per_day + .0001);
         assert( glumph0 > 0);
         assert( glumph >= glumph0);
         assert( freq == 300 || freq == 900);   /* five or 15 minutes */
         while( read_posns_for_one_glumph( ifile, locs))
            {
            bool already_got_it = false;
            int i;

            for( i = 0; i < N_CACHED && !already_got_it; i++)
               if( cache[i] && cache[i]->glumph == glumph0)
                  already_got_it = true;
            if( !already_got_it)
               add_posns_to_cache( glumph0, locs);
            glumph0++;
            if( freq == 300)        /* skip two glumphs */
               {
               read_posns_for_one_glumph( ifile, NULL);
               read_posns_for_one_glumph( ifile, NULL);
               }
            }
         fclose( ifile);
         }
      rval = fetch_posns_from_cache( glumph);  /* should be in cache now */
      }
   else if( gps_verbose)
      printf( "Glumph %d found in cache\n", glumph);
   return( rval);
}

/* Days between (year) Jan 1 and 1980 Jan 7 = MJD 44245,  start of the
GPS ddddy system.  Should return -6 for year=1980,  360 for year=1981. */

static int start_of_year( const int year)
{
/* return( 365 * (year - 1980) + (year - 1977) / 4 - 6);    */
   return( 365 * year  + (year - 1) / 4 - 723200);
}

/* If possible,  we get positions from the IGR ("rapid") file.  If not,
we look for an IGU ("ultra-rapid") file.  These are slightly less
accurate (though still within centimeters,  i.e.,  massive overkill
for my needs),  and cover the preceding and following day.  IGUs are
provided at six-hour intervals;  combine that with their two-day
coverage,  and you see that we might get data in any of eight files. */

/* Base URL for the University of Bern files: */

#define UNIBE_BASE_URL "ftp://ftp.aiub.unibe.ch/CODE/"

static double *get_tabulated_gps_posns( const int glumph, int *err_code,
            const bool fetch_files)
{
   int day = glumph / glumphs_per_day, i;
   char filename[20], command[200];
   double *rval;

   *err_code = 0;
   i = 1980;
   while( i < 2200 && start_of_year( i + 1) < day)
      i++;

   sprintf( filename, "gbm%04d%d.sp3.Z", day / 7, day % 7);
   sprintf( command, "ftp://cddis.gsfc.nasa.gov/pub/gps/products/mgex/%4d/%s",
               day / 7, filename);
   if( gps_verbose)
      printf( "MGEX (multi-GNSS) file: '%s', %d %d: '%s'\n", filename, glumph,
            glumph - day * glumphs_per_day, command);
   if( fetch_files)
      try_to_download( command, filename);
   filename[12] = '\0';       /* remove .Z extension */
   rval = get_cached_posns( filename, glumph);
   if( rval)
      return( rval);

#ifdef UNIBE_BASE_URL
   sprintf( filename, "COD%04d%d.EPH.Z", day / 7, day % 7);
   sprintf( command, UNIBE_BASE_URL "%4d/%s", i, filename);
   if( gps_verbose)
      printf( "Final file: '%s', %d %d: '%s'\n", filename, glumph,
            glumph - day * glumphs_per_day, command);
   if( fetch_files)
      try_to_download( command, filename);
   filename[12] = '\0';       /* remove .Z extension */
   rval = get_cached_posns( filename, glumph);
   if( rval)
      return( rval);

   strcpy( filename + 12, "_R");
   sprintf( command, UNIBE_BASE_URL "%s", filename);
   if( gps_verbose)
      printf( "Rapid file: '%s', %d %d: '%s'\n", filename, glumph,
            glumph - day * glumphs_per_day, command);
   if( fetch_files)
      try_to_download( command, filename);
   rval = get_cached_posns( filename, glumph);

   for( i = 0; !rval && i < 5; i++, day--)
      {
      sprintf( filename, "COD%04d%d.EPH_5D", day / 7, day % 7);
      sprintf( command, UNIBE_BASE_URL "%s", filename);
      if( gps_verbose)
          printf("Five-day file: '%s', %d %d: '%s'\n", filename, glumph,
            glumph - day * glumphs_per_day, command);
      if( fetch_files)
         try_to_download( command, filename);
      rval = get_cached_posns( filename, glumph);
      if( gps_verbose)
         printf( "get_cached_posns: %p\n", (void *)rval);
      }
#endif         /* #ifdef UNIBE_BASE_URL */
   for( i = 0; !rval && i < 8; i++)
      {
      const int tglumph = glumph + (i - 3) * glumphs_per_day / 4;
      const int day = tglumph / glumphs_per_day;
      const int week = day / 7;
      const int glumphs_per_hour = 4;
      const int hour = (tglumph % glumphs_per_day) / glumphs_per_hour;


      sprintf( filename, "igu%d%d_%02d.sp3.Z",
                  week, day % 7, (hour / 6) * 6);
      sprintf( command, "ftp://cddis.gsfc.nasa.gov/pub/gps/products/%d/%s",
                  week, filename);
      if( gps_verbose)
         printf( "IGU file: '%s'\n", command);
      if( fetch_files)
         try_to_download( command, filename);
      filename[15] = '\0';       /* remove .Z extension */
      rval = get_cached_posns( filename, glumph);
      if( rval)
         return( rval);
      }
   return( rval);
}

/* Lagrange interpolation through n_pts evenly spaced in x,  y[0] = value
at x=0, y[1] = value at x=1,  etc.  Experimentation with the following
function shows negligible errors with n_pts = 10 and x "in the middle",
i.e.,  x > 4 && x < 5,  such that it has five points on either side. */

static double interpolate( const double *y, const double x, const int n_pts)
{
   double t = 1., c = 1., rval;
   int i;

   for( i = 0; i < n_pts; i++)
      {
      c *= x - (double)i;
      if( i)
         t *= -(double)i;
      }
   if( !c)        /* we're on an abscissa */
      rval = y[(int)( x + .5)];
   else
      {
      rval = y[0] / (t * x);
      for( i = 1; i < n_pts; i++)
         {
         t *= (double)i / (double)( i - n_pts);
         rval += y[i] / (t * (x - (double)i));
         }
      rval *= c;
      }
   return( rval);
}

#define INTERPOLATION_ORDER 10

int get_gps_positions( double *output_coords, const double mjd_gps)
{
   const double jan_6_1980 = 44244.0;
   const double glumphs = (mjd_gps - jan_6_1980) * (double)glumphs_per_day;
   const int iglumph = (int)glumphs + 1 - INTERPOLATION_ORDER / 2;
   double *posns[INTERPOLATION_ORDER];
   const double interpolation_loc = glumphs - (double)iglumph;
   int i, j, err_code;

   for( i = 0; i < MAX_N_GPS_SATS * 3; i++)
      output_coords[i] = 0.;
   for( i = 0; i < INTERPOLATION_ORDER; i++)
      {
      posns[i] = get_tabulated_gps_posns( iglumph + i, &err_code, false);
      if( !posns[i])       /* maybe we need to download data */
         posns[i] = get_tabulated_gps_posns( iglumph + i, &err_code, true);
      if( !posns[i])
         return( err_code);
      }
   for( i = 0; i < MAX_N_GPS_SATS; i++)
      {
      double tarray[3][INTERPOLATION_ORDER];
      bool got_data = true;

      for( j = 0; j < INTERPOLATION_ORDER && got_data; j++)
         {
         double *tptr = posns[j] + i * 3;

         if( tptr[0] == 0. && tptr[1] == 0. && tptr[2] == 0.)
            got_data = false;
         tarray[0][j] = tptr[0];
         tarray[1][j] = tptr[1];
         tarray[2][j] = tptr[2];
         }
      if( got_data)
         {
         double *tptr = output_coords + i * 3;

         for( j = 0; j < 3; j++)
            tptr[j] = interpolate( tarray[j], interpolation_loc, INTERPOLATION_ORDER);
         }
      }
   return( err_code);
}
