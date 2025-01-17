#define CURL_STATICLIB
#include <stdio.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#include "gps.h"
#include "watdefs.h"
#include "afuncs.h"

const double pi =
      3.1415926535897932384626433832795028841971693993751058209749445923;

static size_t total_written;
time_t download_start_time;
int gps_verbose;

/* Somewhat arbitrarily,  if the overall download rate is less than the
following,  we assume we're stuck and abort the download.  Added because
the IGS server has been rather annoying on this point,  returning just
barely enough data to make you think you might someday get a file.  We
start checking the rate after five seconds.  */

const size_t min_download_rate = 100000;

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written;
    const time_t t_elapsed = time( NULL) - download_start_time;

    written = fwrite(ptr, size, nmemb, stream);
    total_written += written;
    if( t_elapsed > 5)
        if( total_written / (size_t)t_elapsed < min_download_rate)
            return( 0);
    return written;
}

static const char *fail_file = "url_fail.txt";
static int have_netrc = -1;
static const char *netrc_filename = "netrc.txt";
      /* NASA's CDDIS site requires a userid,  password,  and cookies,  for
         no good reason I can see.  Hence the code in this to look for a
         'netrc.txt' file and the temporary creation and removal of the
         'cookies.txt' file.   */

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
        char errbuff[CURL_ERROR_SIZE];
        const char *cookie_file_name = "cookies.txt";

        if( !fp)
            return( FETCH_FOPEN_FAILED);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuff);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
        if( have_netrc)
            {
            curl_easy_setopt(curl, CURLOPT_NETRC, CURL_NETRC_OPTIONAL);
            curl_easy_setopt(curl, CURLOPT_NETRC_FILE, netrc_filename);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file_name);
            curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file_name);
            }
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
        if( have_netrc)
            unlink( cookie_file_name);
        if( res) {
           FILE *ofile = fopen( fail_file, "a");
           time_t t0 = time( NULL);

           if( gps_verbose)
              printf( "Curl fail %d (%s)\n", res, errbuff);
           fprintf( ofile, "# Curl fail %d (%s) %.24s UTC",
                                       res, errbuff, asctime( gmtime( &t0)));
           fclose( ofile);
           unlink( outfilename);
           return( FETCH_CURL_PERFORM_FAILED);
           }
    } else
        return( FETCH_CURL_INIT_FAILED);
    return 0;
}

/* When downloads fail,  we add that info -- including the time of
failure -- to the above file.  When we're about to grab a URL,  we
check to see if it failed within 'retry_wait' seconds.  If it did,
we don't bang on the server trying to get it... should avoid what
amounts to an unintended denial of service attack.   */

static void add_download_failure( const char *url, const int failure_code)
{
   FILE *ofile = fopen( fail_file, "a");

   assert( ofile);
   fprintf( ofile, "%11ld%8d %s\n", (long)time( NULL), failure_code, url);
   if( gps_verbose)
      printf( "Added failure for '%s'\n", url);
   fclose( ofile);
}

static int recent_download_failure( const char *url)
{
   const long t0 = (long)time( NULL);
   long retry_wait = 360;
   FILE *ifile = fopen( fail_file, "r");
   char buff[400], *name = buff + 20;
   const size_t url_len = strlen( url);
   int rval = 0;

   assert( ifile);
   while( !rval && fgets( buff, sizeof( buff), ifile))
      if( *buff != '#' && atol( buff) > t0 - retry_wait
                   && !memcmp( name, url, url_len) && name[url_len] < ' ')
         rval = atoi( buff + 11);
      else if( !memcmp( buff, "Wait ", 5))
         retry_wait = atol( buff + 5);
   if( rval && gps_verbose)
      printf( "Failed (%d) %ld seconds ago, at %.24s UTC\n", rval,
                  t0 - atol( buff), asctime( gmtime( &t0)));
   fclose( ifile);
   return( rval);
}

static void try_to_download( const char *url, const char *filename)
{
   int rval;

   total_written = 0;
   download_start_time = time( NULL);
   if( recent_download_failure( url))
      return;
   rval = grab_file( url, filename, false);
   if( gps_verbose)
      printf( "Download '%s': %d, %ld bytes, %.24s UTC\n", url, rval, total_written,
                     asctime( gmtime( &download_start_time)));
   if( rval || total_written < 22000)      /* just got an error message */
      {
      unlink( filename);
      add_download_failure( url, rval);
      }
   else if( toupper( filename[strlen( filename) - 1]) == 'Z')
      {
      char command[160];

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
const double seconds_per_glumph = 15. * 60.;

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
char is_from_tle[MAX_N_GPS_SATS];

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

/* By default,  the ephemeris data files (.sp3,  .EPH, .EPH_R, .EPH_5D) will
be stored in the current directory.  But it can be desirable to have them
in a different path.  I keep them,  for example,  in ~/gps_eph.  So I run

./list_gps (date/time) (other options) -p ~/gps_eph               */

const char *ephem_data_path = "";

static void insert_data_path( char *filename)
{
   size_t len = strlen( ephem_data_path);
   const bool add_trailing_slash = (len && ephem_data_path[len - 1] != '/');

   if( add_trailing_slash)
      len++;
   memmove( filename + len, filename, strlen( filename) + 1);
   memcpy( filename, ephem_data_path, len);
   if( add_trailing_slash)
      filename[len - 1] = '/';
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

/* The GPS timing system starts on Monday, 1980 Jan 7 = MJD 44245 = GPS 00001
(day 1 of week 0).  The 'real' start is Sunday, 1980 Jan 6 = GPS 00000. */

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

static void remove_dot_z( char *filename)
{
   char *tptr = strstr( filename, ".Z");

   if( !tptr)
      tptr = strstr( filename, ".gz");
   assert( tptr);
   *tptr = '\0';
}

/* Days between (year) Jan 1 and 1980 Jan 7 = MJD 44245,  start of the
GPS ddddy system.  Should return -6 for year=1980,  360 for year=1981. */

static int start_of_year( const int year)
{
/* return( 365 * (year - 1980) + (year - 1977) / 4 - 6);    */
   return( 365 * year  + (year - 1) / 4 - 723200);
}

/* If possible,  we get positions from the MGEX (Multi-GNSS Experiment)
files:  see http://mgex.igs.org/ for information about this.  MGEX
conveniently provides data for (as of early 2021) GPS,  GLONASS,  Galileo,
BeiDou,  and QZSS satellites.  The file naming conventions changed after
week 1797,  and we try both GBM (from the University of Potsdam) files
and WUM (Wuhan University) and SHA (Shanghai?) files.  The last two
actually extend a bit into the future,  meaning you can sometimes know
where to look for all five constellations at least slightly ahead of
time.

   (Note that,  as of December 2024,  the Potsdam files were unavailable.
They are shut off for the nonce.)

   However,  MGEX provides very limited predictions,  and only goes back
to early 2012.  Also,  some MGEX files are only available (as best I can
tell) from the NASA CDDIS site,  which turned off anonymous ftp in early
2021.  If we can't get an MGEX file,  we use CODE ephems (Center for
Orbit Determination in Europe),  downloaded from the University of Berne
in Switzerland.  Unfortunately,  they'll only get you GPS,  GLONASS,  and
Galileo satellites.  (Though that's usually plenty.)

   And then,  if all else fails,  we go for the IGU ("ultra-rapid") file.
These give only GPS satellites.  I don't think those will ever actually
be used.  I think I added them at a time when access to the CODE data
was sometimes an iffy prospect.

   Note that we can also use TLEs to get approximate positions for
Galileo,  BeiDou,  and QZSS satellites;  you can then observe those
objects,  wait for MGEX data to come out,  and then check your results.
See the get_gps_positions_from_tle( ) function below.

Base URL for the University of Bern files: */

#define UNIBE_BASE_URL "ftp://ftp.aiub.unibe.ch/CODE/"

bool use_mgex_data = true;

/* As of 2018 Jun 9,  the MGEX data actually started in GPS week 1680
(2012 March 17) and the IGU ("ultra-rapid") predictive CDDIS data in week
1080 (2000 Sep 16).  I've backed both up by about a year in case other
data turns up.  This check could be skipped completely,  but I figure this
avoids a certain amount of requests for files that almost surely do not
exist.

In a similar vein,  the CODwwwwd.EPH files only come up within a week or
so of the present,  and the .EPH_5D files start after that and proceed
a day or two into the future.  So we compute 'curr_day' and compare
against that,  with some extra margin because the limits appear to vary.
We'll sometimes check for files that don't actually exist yet/existed
and have been removed.        */

#define MGEX_START_WEEK 1689
#define IGU_START_WEEK 1030
#define UNIBE_COD_START_WEEK 649

static double *get_tabulated_gps_posns( const int glumph, int *err_code,
            const bool fetch_files)
{
   int day = glumph / glumphs_per_day, i;
   int day_of_year, week = day / 7;
   char filename[125], command[200];
   double *rval;
   const int curr_day = (int)( time( NULL) / 86400) - 3658;

   *err_code = 0;
   rval = get_cached_posns( filename, glumph);
   if( rval)
      {
      if( gps_verbose)
         printf( "Already got glumph %d in cache\n", glumph);
      return( rval);
      }
   i = 1980;
   while( i < 2200 && start_of_year( i + 1) < day)
      i++;
   day_of_year = day - start_of_year( i);
   if( use_mgex_data && day > MGEX_START_WEEK * 7 && day < curr_day + 4)
      {
      int pass;
      const char *suffix = (day < curr_day - 28 ? "" : "_IGS20");

      if( week >= 1797)                /* roughly after June 2014 */
         {
         if( week < 2081)
            snprintf( filename, sizeof( filename), "gbm%04d%d.sp3.Z",
                                         week, day % 7);
         else
            snprintf( filename, sizeof( filename), "GBM0MGXRAP_%d%03d0000_01D_05M_ORB.SP3.gz",
                           i, day_of_year);
         }
      snprintf( command, sizeof( command),
               "ftp://ftp.gfz-potsdam.de/GNSS/products/mgex/%4d%s/%s",
               week, suffix, filename);
      insert_data_path( filename);
#ifdef GFZ_POTSDAM
      if( gps_verbose)
         printf( "MGEX (multi-GNSS) file: '%s', %d %d: '%s'\n", filename, glumph,
                glumph - day * glumphs_per_day, command);
      if( fetch_files)
         try_to_download( command, filename);
#endif
      remove_dot_z( filename);
      rval = get_cached_posns( filename, glumph);
      if( have_netrc == -1)         /* haven't checked yet for a .netrc */
         {
         FILE *netrc_file = fopen( netrc_filename, "rb");

         if( netrc_file)
            fclose( netrc_file);
         have_netrc = (netrc_file ? 1 : 0);
         }
      for( pass = (have_netrc ? 0 : 1); !rval && pass < 4; pass++)
         {           /* try CDDIS,  WUM (Wuhan) & SHA files */
         if( !pass)
            {
            snprintf( filename, sizeof( filename),
                  "GFZ0OPSULT_%d%d0000_02D_05M_ORB.SP3.gz", i, day_of_year);
            snprintf( command, sizeof( command),
                   "https://cddis.nasa.gov/archive/gnss/products/%4d/%s",
                   week, filename);
            }
         else
            {                 /* WUM (Wuhan) & SHA (Shanghai?) from IGS */
            const char *paths[3] = { "SHA0MGXULT", "WUM0MGXFIN", "WUM0MGXULT" };

            snprintf( filename, sizeof( filename), "%s_%d%03d0000_01D_05M_ORB.SP3.gz",
                     paths[pass - 1], i, day_of_year);
            snprintf( command, sizeof( command),
                  "ftp://igs.ign.fr/pub/igs/products/mgex/%4d/%s",
                  week, filename);
            }
         insert_data_path( filename);
         if( gps_verbose)
            printf( "MGEX (multi-GNSS) file: '%s', %d %d: '%s'\n", filename, glumph,
                glumph - day * glumphs_per_day, command);
         if( fetch_files)
            try_to_download( command, filename);
         remove_dot_z( filename);
         rval = get_cached_posns( filename, glumph);
         }
      if( rval)
         return( rval);
      }

#ifdef UNIBE_BASE_URL
   if( week >= UNIBE_COD_START_WEEK && day <= curr_day + 1)
      {
      if( i < 2023)
         snprintf( filename, sizeof( filename), "COD%04d%d.EPH.Z", week, day % 7);
      else
         snprintf( filename, sizeof( filename), "COD0OPSFIN_%d%03d0000_01D_05M_ORB.SP3.gz",
                                    i, day_of_year);
      snprintf( command, sizeof( command), UNIBE_BASE_URL "%4d/%s", i, filename);
      insert_data_path( filename);
      if( gps_verbose)
         printf( "Final file: '%s', %d %d: '%s'\n", filename, glumph,
               glumph - day * glumphs_per_day, command);
      if( fetch_files)
         try_to_download( command, filename);
      remove_dot_z( filename);
      rval = get_cached_posns( filename, glumph);
      if( rval)
         return( rval);

      snprintf( filename, sizeof( filename), "COD%04d%d.EPH_R", week, day % 7);
      snprintf( command, sizeof( command), UNIBE_BASE_URL "%s", filename);
      insert_data_path( filename);
      if( gps_verbose)
         printf( "Rapid file: '%s', %d %d: '%s'\n", filename, glumph,
               glumph - day * glumphs_per_day, command);
      if( fetch_files)
         try_to_download( command, filename);
      rval = get_cached_posns( filename, glumph);
      }

   for( i = 0; !rval && i < 5; i++, day--)
      if( day > curr_day - 20 && day < curr_day + 3)
         {
         snprintf( filename, sizeof( filename), "COD%04d%d.EPH_5D",
                        day / 7, day % 7);
         snprintf( command, sizeof( command), UNIBE_BASE_URL "%s", filename);
         insert_data_path( filename);
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
   return( rval);
}

/* If,  as described below,  you observed an object with a light-time lag of
(say) 0.07 seconds,  then it should be precessed from Earth-fixed inertial
coordinates to J2000 using the earth's orientation as it was 0.07 seconds
before you made your observation.  Instead,  it's processed using the
orientation at the time of observation.

To fix this small difference,  we just spin around the Z axis by the
amount the earth rotates during those 0.07 seconds. */

static inline void rotate_vect( double *vect, const double angle)
{
   const double cos_angle = cos( angle), sin_angle = sin( angle);
   const double new_y = vect[1] * cos_angle - vect[0] * sin_angle;

   vect[0] = vect[0] * cos_angle + vect[1] * sin_angle;
   vect[1] = new_y;
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

/* If observer_loc == NULL,  we just compute positions without any light-time
lag considered.  Otherwise,  we start out assuming a lag of 0.07 seconds,
about right for most navsats.  That gets us a highly accurate distance,
and a second iteration gets us as good as we're gonna get.  A more
thorough implementation would examine how much the light-time lag changed;
then,  if the observer_loc were near Saturn,  it would do another iteration
or two until the light-time lag converged near an hour and a half.  But
this should suffice for ordinary purposes. */

int get_gps_positions( double *output_coords, const double *observer_loc,
                            const double mjd_gps)
{
   const double jan_6_1980 = 44244.0;
   const double glumphs = (mjd_gps - jan_6_1980) * (double)glumphs_per_day;
   const int iglumph = (int)glumphs + 1 - INTERPOLATION_ORDER / 2;
   double *posns[INTERPOLATION_ORDER];
   const double interpolation_loc = glumphs - (double)iglumph;
   int i, j, err_code;

   memset( is_from_tle, 0, MAX_N_GPS_SATS);
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
      int pass;
      double light_time_lag = 0.07;   /* initial guess */

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
         for( pass = 0; pass < 2; pass++)
            {
            double *tptr = output_coords + i * 3;
            double dist_squared = 0., delta;
            const double delay = light_time_lag / seconds_per_glumph;

            for( j = 0; j < 3; j++)
               {
               tptr[j] = interpolate( tarray[j],
                                     interpolation_loc - delay,
                                     INTERPOLATION_ORDER);
               if( observer_loc)
                  delta = tptr[j] - observer_loc[j];
               else
                  delta = 0.;
               dist_squared += delta * delta;
               }
            light_time_lag = sqrt( dist_squared) / SPEED_OF_LIGHT;
            rotate_vect( tptr, 2. * pi * light_time_lag / seconds_per_day);
            }
      }
   return( err_code);
}

char *fgets_trimmed( char *buff, size_t max_bytes, FILE *ifile)
{
   char *rval = fgets( buff, (int)max_bytes, ifile);

   if( rval)
      {
      int i;

      for( i = 0; buff[i] && buff[i] != 10 && buff[i] != 13; i++)
         ;
      buff[i] = '\0';
      }
   return( rval);
}

char **load_file_into_memory( const char *filename, size_t *n_lines)
{
   FILE *ifile = fopen( filename, "rb");
   char **rval = NULL;

   if( ifile)
      {
      size_t filesize = 0, lines_read = 0;
      char buff[400];

      while( fgets_trimmed( buff, sizeof( buff), ifile))
         {
         lines_read++;
         filesize += strlen( buff) + 1;
         }
      rval = (char **)malloc( filesize + (lines_read + 1) *  sizeof( char *));
      if( rval &&
             (rval[0] = (char *)( rval + lines_read + 1)) != NULL)
         {
         fseek( ifile, 0L, SEEK_SET);
         lines_read = 0;
         while( fgets_trimmed( buff, sizeof( buff), ifile))
            {
            strcpy( rval[lines_read], buff);
            rval[lines_read + 1] = rval[lines_read] + strlen( buff) + 1;
            lines_read++;
            }
         rval[lines_read] = NULL;
         }
      fclose( ifile);
      if( n_lines)
         *n_lines = lines_read;
      }
   return( rval);
}

/* Used for cross-designations/identifications.  This looks through the file
'names.txt' (see 'names.cpp' for a discussion of how this was made).  If the
search string is three characters,  it's assumed to be a letter/number/number
GNSS designator and we look for that text.  If it's four characters,  we look
for the alternative GNSS designation of a letter and three-digit number.  If
it's nine characters,  we look for a YYYY-NNNA international designation
match.  If it's five digits,  we look for a NORAD number match. Otherwise,
it's assumed to be a six-character international YYNNNletter designation,
and we look for that instead.

   For the YYNNNletter form,  the length is deliberately not checked;  we
may be testing an un-truncated line from a TLE.

   A pointer to the relevant line from 'names.txt' is returned. */

const char *names_filename = "names.txt";

const char *get_name_data( const char *search_str, const int mjd)
{
   static char **lines;
   const char *rval = NULL;

   if( !search_str)
      {
      if( lines)
         free( lines);
      lines = NULL;
      }
   else
      {
      size_t i = 0;
      const size_t search_len = strlen( search_str);

      if( !lines)
         lines = load_file_into_memory( names_filename, NULL);
      assert( lines);
      while( lines[i] && !rval)
         {
         int ids_match;
         char *line = lines[i];
         const int start_mjd = atoi( line), end_mjd = atoi( line + 6);

         switch( search_len)
            {
            case 3:              /* GNSS letter-digit-digit identifier */
               ids_match = !memcmp( search_str, line + 12, 3);
               break;
            case 4:              /* alternative letter-three-digits */
               ids_match = !memcmp( search_str, line + 16, 4);
               break;
            case 5:              /* NORAD five-digit desig */
               ids_match = !memcmp( search_str, line + 33, 5);
               break;
            case 9:              /* YYYY-NNNA international designation */
               ids_match = !memcmp( search_str, line + 21, 9);
               break;
            default:             /* Assume YYNNNA 'short form' int'l */
               ids_match = (!memcmp( search_str, line + 23, 2)
                           && !memcmp( search_str + 2, line + 26, 4));
               break;
            }
         if( ids_match && mjd >= start_mjd && mjd <= end_mjd)
            rval = line;
         i++;
         }
      }
   return( rval);
}

#include "norad.h"

static const char *gnss_filename = "gnss.tle";

static int extract_gnss_tles( const char *tle_filename, const int mjd_gps)
{
   FILE *ifile = fopen( tle_filename, "rb");
   FILE *ofile = fopen( gnss_filename, "wb");
   char line0[100], line1[100], line2[100];
   int rval = 0;

   assert( ifile);
   assert( ofile);
   *line0 = *line1 = '\0';
   while( fgets( line2, sizeof( line2), ifile))
      {
      tle_t tle;

      if( *line2 == '2' && *line1 == '1'
               && get_name_data( line1 + 9, (int)mjd_gps) != NULL
               && parse_elements( line1, line2, &tle) >= 0)
         fprintf( ofile, "%s%s%s", line0, line1, line2);
      strcpy( line0, line1);
      strcpy( line1, line2);
      }
   fclose( ifile);
   fclose( ofile);
   return( rval);
}


/* We read in TLEs,  and check to see that the object's international ID
(the YYNNNletter one) matches a GNSS satellite for that date,  as listed
in 'names.txt'.  We also check to see that the position for the satellite
is all zeroes;  if it isn't,  it means the position was already computed,
much more accurately,  using .sp3 data.

   Tabulated ephems in .sp3 files are in earth-centered,  earth-fixed coords
(i.e.,  if the satellite is above lat=lon=0,  it'll have y=z=0.)  The results
from SDP4 are in earth-centered coordinates of date.  So we have to "remove"
the earth's rotation. */

int get_gps_positions_from_tle( const char *tle_filename,
                        double *output_coords, const double mjd_gps)
{
   FILE *ifile;
   char line0[100], line1[100], line2[100];
   int rval = 0;
   const double tdt_minus_tai = 32.184;       /* seconds */
   const double tai_minus_gps = 19.;      /* seconds */
   const double tdt_minus_gps = tdt_minus_tai + tai_minus_gps;
   const double utc_minus_gps =
               tdt_minus_gps - td_minus_utc( mjd_gps + 2400000.5);
   const double mjd_utc = mjd_gps + utc_minus_gps / seconds_per_day;
   const double rotation = green_sidereal_time( mjd_utc + 2400000.5);
   static int gnss_tle_created = 0;

   if( !gnss_tle_created)
      {
      extract_gnss_tles( tle_filename, (int)mjd_gps);
      gnss_tle_created = 1;
      }
   ifile = fopen( gnss_filename, "rb");
   assert( ifile);
   if( !ifile)
      return( -1);
   *line0 = *line1 = '\0';
   while( fgets( line2, sizeof( line2), ifile))
      {
      tle_t tle;
      const char *name_line;

      if( *line2 == '2' && *line1 == '1'
               && (name_line = get_name_data( line1 + 9, (int)mjd_gps)) != NULL
               && parse_elements( line1, line2, &tle) >= 0)
         {
         char desig[5];
         double *posn, t_since, tval;
         double sat_params[N_SAT_PARAMS];
         int idx;

         memcpy( desig, name_line + 12, 3);
         desig[3] = '\0';
         idx = desig_to_index( desig);
         posn = output_coords + 3 * idx;

         if( !posn[0] && !posn[1] && !posn[2])
            {
            SDP4_init( sat_params, &tle);
            t_since = mjd_utc - (tle.epoch - 2400000.5);
            SDP4( t_since * minutes_per_day, &tle, sat_params, posn, NULL);
            tval = posn[0] * cos( rotation) + posn[1] * sin( rotation);
            posn[1] = posn[1] * cos( rotation) - posn[0] * sin( rotation);
            posn[0] = tval;
            is_from_tle[idx] = 1;
            rval++;
            }
         }
      strcpy( line0, line1);
      strcpy( line1, line2);
      }
   fclose( ifile);
   return( rval);
}
