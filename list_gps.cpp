/* list_gps.cpp: gives RA/decs for GPS satellites (I hope)

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
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <math.h>
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
#include "afuncs.h"
#include "date.h"
#include "lunar.h"         /* for obliquity( ) prototype */
#include "mpc_func.h"
#include "gps.h"

const char *get_name_data( const char *search_str, const int mjd); /* gps.c */
static char *fgets_trimmed( char *buff, const size_t max_bytes, FILE *ifile);

#define EARTH_SEMIMAJOR_AXIS 6378.137

#define PI 3.1415926535897932384626433832795028841971693993751058209749445923

#define MANGLED_EMAIL "pluto (at) \x70roject\x70lu\x74o (d\x6ft) co\x6d"

const char *imprecise_position_message =
  "WARNING: Your observatory's position is given with low precision.  This will\n"
  "cause the computed positions for navigation satellites to be imprecise,  too.\n"
  "I'd recommend getting a corrected,  precise latitude,  longitude,  and altitude\n"
  "using GPS or mapping software,  and sending that to the MPC and to me at\n"
  MANGLED_EMAIL ".\n";

char relocation[80];
static FILE *log_file = NULL;

int snprintf_append( char *string, const size_t max_len,      /* ephem0.cpp */
                                   const char *format, ...)
#ifdef __GNUC__
         __attribute__ (( format( printf, 3, 4)))
#endif
;

int snprintf_append( char *string, const size_t max_len,      /* ephem0.cpp */
                                   const char *format, ...)
{
   va_list argptr;
   int rval;
   const size_t ilen = strlen( string);

   assert( ilen <= max_len);
   va_start( argptr, format);
#if _MSC_VER <= 1100
   rval = vsprintf( string + ilen, format, argptr);
#else
   rval = vsnprintf( string + ilen, max_len - ilen, format, argptr);
#endif
   string[max_len - 1] = '\0';
   va_end( argptr);
   return( rval);
}

static int get_observer_loc( mpc_code_t *cdata, const char *code)
{
   int rval = -1, i;
   static bool imprecision_warning_shown = false;
   static mpc_code_t cached_cdata;

   if( !strcmp( cached_cdata.code, code))
      {
      *cdata = cached_cdata;
      return( 0);
      }
   if( relocation[0])
      {
      static bool relocation_message_shown = false;

      rval = get_lat_lon_info( cdata, relocation);
      if( !relocation_message_shown)
         {
         if( rval)
            printf( "Relocation text '%s' wasn't parsed correctly\n", relocation);
         else
            printf( "Repositioned: latitude %.8f, longitude %.8f%c, alt %f meters above ellipsoid\n",
                           cdata->lat * 180. / PI,
                           fabs( cdata->lon) * 180. / PI,
                           (cdata->lon > 0. ? 'E' : 'W'),
                           cdata->alt);
         }
      relocation_message_shown = true;
      return( rval);
      }

   for( i = 0; i < 3 && rval; i++)
      {
      const char *filenames[3] = { "rovers.txt", "ObsCodes.htm",
                                                 "ObsCodes.html" };
      FILE *ifile = fopen( filenames[i], "rb");

      if( ifile)
         {
         char buff[200], *obs_name;
         const char end_char = (code[3] ? code[3] : ' ');

         while( rval && fgets_trimmed( buff, sizeof( buff), ifile))
            if( !memcmp( buff, code, 3) && buff[3] == end_char
                     && get_mpc_code_info( cdata, buff) == 3)
               {
               rval = 0;         /* we got it */
               obs_name = (char *)malloc( strlen( cdata->name) + 1);
               strcpy( obs_name, cdata->name);
               cdata->name = obs_name;
               if( buff[4] != '!' && !imprecision_warning_shown)
                  if( buff[12] == ' ' || buff[20] == ' ' || buff[29] == ' ')
                     {
                     imprecision_warning_shown = true;
                     printf( "%s", imprecise_position_message);
                     }
               if( !memcmp( code, "568", 3))
                  printf( "\nSpecial fix for Mauna Kea observers:  use 'codes' for specific\n"
                          "telescopes such as CFH (sic),  2.2,  etc.  Full list is at\n\n"
#ifdef CGI_VERSION
   "<a href='https://github.com/Bill-Gray/find_orb/blob/master/rovers.txt#L161'>"
#endif
                          "https://github.com/Bill-Gray/find_orb/blob/master/rovers.txt#L161"
#ifdef CGI_VERSION
   "</a>"
#endif
                          "\n\n(scroll up for an explanation of these codes)\n\n");
               if( code[3] == '+' || code[3] == '-')
                  {                                   /* altitude offset */
                  double offset = atof( code + 3);
                  const double meters_per_kilometer = 1000.;

                  cdata->alt += offset;
                  offset /= EARTH_SEMIMAJOR_AXIS * meters_per_kilometer;
                  cdata->rho_cos_phi += offset * cos( cdata->lat);
                  cdata->rho_sin_phi += offset * sin( cdata->lat);
                  }
               if( !i)
                  printf( "Location for (%s) %s found in 'rovers' file\n",
                          cdata->code, cdata->name);
               cached_cdata = *cdata;
               }
         fclose( ifile);
         }
      }
   return( rval);
}

/* Aberration from the Ron-Vondrak method,  from Meeus'
_Astronomical Algorithms_, p 153,  just the leading terms */

static void compute_aberration( const double t_cen, double *ra, double *dec)
{
   const double l3 = 1.7534703 + 628.3075849 * t_cen;
   const double sin_l3 = sin( l3), cos_l3 = cos( l3);
   const double sin_2l3 = sin( l3 + l3), cos_2l3 = cos( l3 + l3);
   const double x = -1719914. * sin_l3 - 25. * cos_l3
                       +6434. * sin_2l3 + 28007 * cos_2l3;
   const double y = 25. * sin_l3 + 1578089 * cos_l3
                +25697. * sin_2l3 - 5904. * cos_2l3;
   const double z = 10. * sin_l3 + 684185. * cos_l3
                +11141. * sin_2l3 - 2559. * cos_2l3;
   const double c = 17314463350.;    /* speed of light is 173.1446335 AU/day */
   const double sin_ra = sin( *ra), cos_ra = cos( *ra);

   *ra -= (y * cos_ra - x * sin_ra) / (c * cos( *dec));
   *dec += ((x * cos_ra + y * sin_ra) * sin( *dec) - z * cos( *dec)) / c;
}

static bool show_decimal_degrees = false;
static bool is_topocentric;

static const char *show_base_60( const double ival, char *buff, const int is_ra)
{

   if( show_decimal_degrees)
      {
      snprintf( buff, 30, (is_ra ? "%012.8f" : "%011.8f "), ival);
      }
   else
      {
      const unsigned long zas = (unsigned long)( ival * (is_ra ? 2400. : 3600.) * 1000.);
      const unsigned long mas = zas / (is_ra ? 10L : 1L);

      snprintf( buff, 30, "%02lu %02lu %02lu.%03lu",
               mas / 3600000UL, (mas / 60000UL) % 60UL,
               (mas / 1000UL) % 60UL, mas % 1000UL);
      if( is_ra)
         snprintf( buff + 9, 5, "%04lu", zas % 10000UL);
      }
   return( buff);
}

static double cartesian_to_polar( const double *coords, double *theta, double *phi)
{
   const double cyl2 = coords[0] * coords[0] + coords[1] * coords[1];
   const double r = sqrt( cyl2 + coords[2] * coords[2]);

   *theta = atan2( coords[1], coords[0]);
   *phi = atan( coords[2] / sqrt( cyl2));
   return( r);
}

typedef struct
{
   char obj_desig[4], international_desig[12], type[30];
   double j2000_topo[3], j2000_geo[3];
   double topo_r;
   double ra, dec, alt, az, elong;
   double motion, posn_ang;
   bool in_shadow, is_from_tle;
   int norad;
} gps_ephem_t;

static void set_ra_dec( gps_ephem_t *loc, const double year)
{
   loc->topo_r = cartesian_to_polar( loc->j2000_topo, &loc->ra, &loc->dec);
   compute_aberration( year / 100., &loc->ra, &loc->dec);
   if( loc->ra < 0.)
      loc->ra += PI + PI;
   if( loc->ra > PI + PI)
      loc->ra -= PI + PI;
}

int get_earth_loc( const double t_millennia, double *results);
                                       /* eart2000.cpp */

static void get_unit_vector_to_sun( const double year, double *vect)
{
   double tvect[6];
   int i;

   get_earth_loc( year / 1000., tvect);
   ecliptic_to_equatorial( tvect);
   for( i = 0; i < 3; i++)
      vect[i] = tvect[i] / tvect[5];
}

static double minimum_altitude = 0.;   /* only show objs above the horizon */

static char *fgets_trimmed( char *buff, const size_t max_bytes, FILE *ifile)
{
   char *rval = fgets( buff, max_bytes, ifile);
   size_t i;

   if( rval)
      {
      for( i = 0; buff[i] != 13 && buff[i] != 10; i++)
         ;
      buff[i] = '\0';
      }
   return( rval);
}

extern const char *names_filename;

static void set_designations( const size_t n_sats, gps_ephem_t *loc,
                                 const int mjd_utc)
{
   FILE *ifile = fopen( names_filename, "rb");
   size_t i;

   for( i = 0; i < n_sats; i++)     /* Ensure there are default values */
      {
      strcpy( loc[i].international_desig, "Unknown  ");
      loc[i].type[0] = '\0';
      }

   if( ifile)
      {
      char buff[100];

      while( fgets_trimmed( buff, sizeof( buff), ifile))
         if( atoi( buff) <= mjd_utc && atoi( buff + 5) > mjd_utc)
            for( i = 0; i < n_sats; i++)
               if( !memcmp( loc[i].obj_desig, buff + 12, 3))
                  {
                  memcpy( loc[i].international_desig, buff + 21, 9);
                  loc[i].international_desig[9] = '\0';
                  strcpy( loc[i].type, buff + 39);
                  loc[i].norad = atoi( buff + 33);
                  }
      fclose( ifile);
      }
}

#define USE_TLES_ONLY         0
#define USE_SP3_ONLY          1
#define USE_TLES_AND_SP3      2

int tle_usage = USE_TLES_AND_SP3;

#ifdef CGI_VERSION
const char *tle_path =    "../../tles/all_tle.txt";
#else
const char *tle_path =    "../tles/all_tle.txt";
#endif
        /* Above paths are defaults for my ISP's server and my own desktop,
           respectively.  Alter to suit file paths on your machine. */


static int compute_gps_satellite_locations_minus_motion( gps_ephem_t *locs,
         const double jd_utc, const mpc_code_t *cdata)
{
   const double tdt = jd_utc + td_minus_utc( jd_utc) / seconds_per_day;
   const double tdt_minus_gps = 51.184;
   const double gps_time = tdt - tdt_minus_gps / seconds_per_day;
   int err_code;
   int i, rval = 0;
   double sat_locs[MAX_N_GPS_SATS * 3];
   double *tptr = sat_locs, observer_loc[3], sun_vect[3];
   double precess_matrix[9], alt_az_matrix[9];
   const double j2000 = 2451545.;
   const double year = (tdt - j2000) / 365.25;

   observer_loc[0] = cos( cdata->lon) * cdata->rho_cos_phi * EARTH_SEMIMAJOR_AXIS;
   observer_loc[1] = sin( cdata->lon) * cdata->rho_cos_phi * EARTH_SEMIMAJOR_AXIS;
   observer_loc[2] =                    cdata->rho_sin_phi * EARTH_SEMIMAJOR_AXIS;
   err_code = setup_precession_with_nutation_eops( precess_matrix,
                                  2000. + year);
   if( err_code)
      {
      printf( "Precession failed: err code %d\n", err_code);
      printf( "Either you're trying to predict too far into the future,  or\n");
      printf( "the Earth orientation data is out of date and needs to be updated.\n");
      return( -1);
      }

   alt_az_matrix[0] = -cos( cdata->lon) * sin( cdata->lat);
   alt_az_matrix[1] = -sin( cdata->lon) * sin( cdata->lat);
   alt_az_matrix[2] =                     cos( cdata->lat);
   alt_az_matrix[3] = -sin( cdata->lon);
   alt_az_matrix[4] = cos( cdata->lon);
   alt_az_matrix[5] = 0.;
   alt_az_matrix[6] = cos( cdata->lon) * cos( cdata->lat);
   alt_az_matrix[7] = sin( cdata->lon) * cos( cdata->lat);
   alt_az_matrix[8] =                    sin( cdata->lat);

   if( tle_usage != USE_TLES_ONLY)
      err_code = get_gps_positions( sat_locs, observer_loc, gps_time - 2400000.5);
   else
      for( i = 0; i < MAX_N_GPS_SATS * 3; i++)
         tptr[i] = 0.;

   if( err_code)
      {
      printf( "Couldn't get satellite positions : %d\n", err_code);
#ifdef CGI_VERSION
      printf( "This shouldn't happen.  Please notify the owner of this site.\n"
              "It could be that the sites providing GNSS positions have changed\n"
              "addresses or moved files around (this has happened before).\n");
#endif
      return( err_code);
      }

   if( tle_usage != USE_SP3_ONLY)
/*    if( curr_jd( ) < jd_utc + 3. || tle_usage == USE_TLES_ONLY)    */
         get_gps_positions_from_tle( tle_path, sat_locs,
                     gps_time - 2400000.5);
   get_unit_vector_to_sun( year, sun_vect);

   for( i = 0; i < MAX_N_GPS_SATS; i++, tptr += 3)
      if( tptr[0] || tptr[1] || tptr[2])
         {
         int j;
         double alt_az_vect[3];
         extern char is_from_tle[];

         deprecess_vector( precess_matrix, tptr, locs->j2000_geo);
         for( j = 0; j < 3; j++)
            tptr[j] -= observer_loc[j];
         deprecess_vector( precess_matrix, tptr, locs->j2000_topo);
         precess_vector( alt_az_matrix, tptr, alt_az_vect);
         cartesian_to_polar( alt_az_vect, &locs->az, &locs->alt);
         strcpy( locs->obj_desig, desig_from_index( i));
         set_ra_dec( locs, year);
/*       if( locs->alt > minimum_altitude)      */
            {
            const double dot_prod = dot_product( locs->j2000_geo, sun_vect);
            const double geocentric_dist_2 = dot_product( locs->j2000_geo,
                                                          locs->j2000_geo);
            const double cos_elong = -dot_product( locs->j2000_topo, sun_vect) / locs->topo_r;

            locs->elong = acos( cos_elong);
            if( locs->az < 0.)
               locs->az += PI + PI;
            locs->in_shadow = ( dot_prod > 0. && geocentric_dist_2 <
                 dot_prod * dot_prod + EARTH_SEMIMAJOR_AXIS * EARTH_SEMIMAJOR_AXIS);
            locs->is_from_tle = is_from_tle[i];
            locs++;
            rval++;
            }
         }
   return( rval);
}

/* To compute apparent motion of the satellites,  compute their positions at
two closely spaced times (a second apart) and look at how far they moved.
Resulting motion is therefore in radians/second. */

static int compute_gps_satellite_locations( gps_ephem_t *locs,
         const double jd_utc, const mpc_code_t *cdata)
{
   const int n_sats = compute_gps_satellite_locations_minus_motion(
                        locs, jd_utc, cdata);

   if( n_sats > 0)
      {
      gps_ephem_t locs2[MAX_N_GPS_SATS];
      int i;
      const int n_sats2 = compute_gps_satellite_locations_minus_motion(
                        locs2, jd_utc + 1. / seconds_per_day, cdata);

      if( n_sats != n_sats2)
         {
         printf( "Internal error: n_sats = %d, n_sats2 = %d\n", n_sats, n_sats2);
         return( 0);
         }
      for( i = 0; i < n_sats; i++)
         calc_dist_and_posn_ang( &locs[i].ra, &locs2[i].ra, &locs[i].motion, &locs[i].posn_ang);
      set_designations( n_sats, locs, (int)( jd_utc - 2400000.5));
      }
   return( n_sats);
}

bool creating_fake_astrometry = false;
bool asterisk_has_been_shown = false;

static void display_satellite_info( const gps_ephem_t *loc, const bool show_ids)
{
   char ra_buff[30], dec_buff[30];
   char obuff[200];
   const char *ra_dec_fmt = (creating_fake_astrometry ? "%.12s%c%.11s" :
                                                        "%s %c%s");

   if( show_ids)
      snprintf( obuff, sizeof( obuff), "%s: ", loc->obj_desig);
   else
      *obuff = '\0';

   if( !creating_fake_astrometry)
      {
      snprintf_append( obuff, sizeof( obuff), loc->is_from_tle ? "*" : " ");
      if( loc->is_from_tle)
         asterisk_has_been_shown = true;
      }
   snprintf_append( obuff, sizeof( obuff), ra_dec_fmt,
               show_base_60( loc->ra * 180. / PI, ra_buff, 1),
               (loc->dec < 0. ? '-' : '+'),
               show_base_60( fabs( loc->dec * 180. / PI), dec_buff, 0));
   if( !creating_fake_astrometry)
      {
      const double arcsec_per_sec = loc->motion * 3600. * 180. / PI;
      const char *format = (arcsec_per_sec > 99. ? "%6.0f %5.1f" : "%6.2f %5.1f");

      if( show_decimal_degrees)
         snprintf_append( obuff, sizeof( obuff), " ");
      snprintf_append( obuff, sizeof( obuff), " %12.5f", loc->topo_r);
      if( is_topocentric)
         snprintf_append( obuff, sizeof( obuff), " %6.1f%6.1f", loc->az * 180. / PI, loc->alt * 180. / PI);
      if( loc->in_shadow)
         strcat( obuff, " Sha");
      else
         snprintf_append( obuff, sizeof( obuff), " %3.0f", (180 / PI) * loc->elong);
      snprintf_append( obuff, sizeof( obuff), format, arcsec_per_sec,
                  360. - loc->posn_ang * 180. / PI);
      if( show_ids)
         snprintf_append( obuff, sizeof( obuff), " %s %s", loc->international_desig, loc->type);
      strcat( obuff, "\n");
      }
   printf( "%s", obuff);
   if( log_file)
      fprintf( log_file, "%s", obuff);
}

/* We don't have many satellites.  A stupid O(n^2) sort will do. */

static void sort_sat_info( const int n_sats, gps_ephem_t *locs, const int criterion)
{
   int i, j;

   for( i = 0; i < n_sats; i++)
      for( j = i + 1; j < n_sats; j++)
         {
         int compare = 0;

         switch( abs( criterion))
            {
            case 1:
               compare = (locs[i].elong > locs[j].elong ? 1 : -1);
               break;
            case 2:
               compare = (locs[i].ra > locs[j].ra ? 1 : -1);
               break;
            case 3:
               compare = (locs[i].alt > locs[j].alt ? 1 : -1);
               break;
            case 4:
               compare = strcmp( locs[i].obj_desig, locs[j].obj_desig);
               break;
            case 5:
               compare = strcmp( locs[i].international_desig, locs[j].international_desig);
               break;
            case 6:
               compare = (locs[i].dec > locs[j].dec ? 1 : -1);
               break;
            case 7:
               compare = (locs[i].topo_r > locs[j].topo_r ? 1 : -1);
               break;
            }
         if( criterion < 0)       /* negative 'criterion' = descending order */
            compare = -compare;
         if( compare > 0)
            {
            const gps_ephem_t temp = locs[i];

            locs[i] = locs[j];
            locs[j] = temp;
            }
         }
}

int sort_order = 1;          /* default = sort by elongation */

#define ERR_CODE_TOO_FAR_IN_FUTURE        -903
#define ERR_CODE_TOO_FAR_IN_PAST          -904
#define ERR_CODE_OBSERVATORY_UNKNOWN      -905

static const char *asterisk_message =
   "Objects marked with an asterisk have less accurate positions.  They're\n"
   "good enough to let you find the object (usually within an arcminute or\n"
   "two).  Observe them and try to get ephemerides again a few days later,\n"
   "and precise positions will probably be available.\n"
#ifdef CGI_VERSION
   "<a href='https://www.projectpluto.com/gps_expl.htm#tles'>"
   "Click here for more information.</a>\n";
#else
   "Visit https://www.projectpluto.com/gps_expl.htm#tles for more info.\n";
#endif

static const char *get_arg( const int argc, const char **argv, const int idx)
{
   if( argv[idx][2] || idx == argc - 1)
      return( argv[idx] + 2);
   else
      return( argv[idx + 1]);
}

/*
CSS field sizes,  from Eric Christensen,  2017 Mar.  Updated by
Rob Seaman,  2017 Nov 04.

703 field size, 2003-2016: 2.85 x 2.85 deg.
G96, 2004 - May 2016: 1.1 x 1.1 deg.
E12 : 2.05 x 2.05 deg.

10K cameras:
G96 - May 2016 - present: 2.2 x 2.2 deg.
703 - Dec. 2016 - present: 4.4 x 4.4 deg.

(566) had a 4096-square CCD at 1.43 arcsec/pixel.

(644): The Palomar NEAT "Tri-Camera" had three separate 4096x4096
pixel CCDs, running north to south,  pixel scale 1.01 arcsec/pixel.
The images have similar RAs (probably almost identical in RA of
date),  and decs spaced out by about 1.3 degrees.  The pointing log
gives three lines for any given time,  corresponding to the northern,
middle,  and southern images.

(691) Spacewatch size roughly from MPC sky coverage files,  plus
some info from Bob McMillan.  The actual shape is a little more
complicated than this -- it's a mosaic of eight chips --  but
the 1.85 x 1.73 degree rectangle is close enough to do a rough
cut as to whether we got the object.  */

static void get_field_size( double *width, double *height, const double jd,
                        const char *obs_code)
{
   const double dec_02_2016 = 2457724.5;
   const double may_26_2016 = 2457534.5;
   const double jun_24_2005 = 2453545.5;     /* see J95 */
   static char bad_code[10];
   const int code = atoi( obs_code + 1) +
               100 * mutant_hex_char_to_int( obs_code[0]);

   *height = 0.;
   switch( code)
      {
      case 291:         /* (291) Spacewatch, 1.8-m : 20' square */
         *width = *height = 1. / 3.;
         break;
      case 566:         /* (566) Haleakala-NEAT/GEODSS  */
         *width = 4096. * 1.43 / 3600.;
         break;
      case 691:         /* (691) Spacewatch */
//       *width = 1.85;      /* if treated as four small rectangles */
//       *height = 1.73;
         *width = *height = 2.9;    /* if treated as one big square */
         break;
      case 644:         /* (644) NEAT at Palomar Sam'l Oschbin Schmidt */
         *width = 4096. * 1.01 / 3600.;
         break;
      case 703:       /* (703) Catalina Sky Survey */
         *width = (jd < dec_02_2016 ? 2.85 : 4.4);
         break;
      case 1696:      /* (G96) Mt. Lemmon */
         *width = (jd < may_26_2016 ? 1.1 : 2.25);
         break;
      case 1412:        /* (E12) Siding Spring */
         *width = 2.05;
         break;
      case 1852:        /* I52:  33' field of view;  some loss in corners */
         *width = 33. / 60.;
         break;
      case 1995:       /* J95:  25' to 2005 jun 22, 18' for 2005 jun 27 on */
         *width = (jd < jun_24_2005 ? 25. / 60. : 18. / 60.);
         break;
      case 2905:         /* ATLAS (T05), (T08) : 7.4 degree FOV */
      case 2908:
         *width = 7.4;
         break;
      case 3100:         /* (V00) Bok */
         *width = 1. / 12.;               /* Spacewatch */
//       *width = 1.16;                   /* CSS */
         break;
      case 3106:          /* (V06) */
         *width = 580. / 3600.;
         break;
      default:
         *width = 0.;
         if( memcmp( bad_code, obs_code, 4))
            {
            printf( "Bad code '%.3s', shouldn't be here\n", obs_code);
            memcpy( bad_code, obs_code, 4);
            }
         break;
      }
   if( !*height)           /* square field indicated */
      *height = *width;
   *width *= PI / 180.;
   *height *= PI / 180.;
}

/* Searching to see if a long trail may have crossed an image can be a little
problematic.  We assume that the endpoints are at (x0, y0),  (x1, y1),
expressed in units of the image width and height.  The segment can be
described parametrically as x = x0 + t(x1 - x0), y = y0 + t(y1 - y0), with
0 <= t <= 1. We're interested in finding if any t in that range runs
through the unit square from -1 < x < 1 and similarly for y,  i.e.,  does
the segment run through the image?

I did this by clipping against _one_ edge (the x=-1 one) and rotating 90
degrees four times.  At any of those four,  the line segment may be totally
clipped (in which case we return t=-1),  or partly clipped (compute the
intersection point and replace one endpoint and its t),  or nothing may
happen (for example,  if both points are on the image,  we'll just rotate
the endpoints four times,  do nothing to them,  and return 0.5.)       */

static double trail_within_image( double x0, double y0, double x1, double y1)
{
   int loop;
   double t0 = 0., t1 = 1.;

   for( loop = 4; loop; loop--)
      {
      double new_y, new_t;

      if( x0 < -1. && x1 < -1.)
         return( -1.);
      else if( x0 < -1. || x1 < -1.)
         {
         new_t = (t0 * (x1 + 1.) - t1 * (x0 + 1.)) / (x1 - x0);
         new_y = (y0 * (x1 + 1.) - y1 * (x0 + 1.)) / (x1 - x0);
         if( x0 < -1.)         /* First point is clipped */
            {
            x0 = -1.;
            y0 = new_y;
            t0 = new_t;
            }
         else              /* ...or second endpoint is clipped */
            {
            x1 = -1.;
            y1 = new_y;
            t1 = new_t;
            }
         }
      new_y = -x0;        /* Rotate both endpoints 90 degrees around origin */
      x0 = y0;
      y0 = new_y;
      new_y = -x1;
      x1 = y1;
      y1 = new_y;
      }
                     /* At least part of the line segment goes through the */
                     /* unit square.  Return the 't' for the midpoint.     */
   return( (t0 + t1) / 2.);
}

/* Computes tangent plane coords,  in radians,  gnomonic projection.  Returns
0 if the result is 'OK' and the specified point really is on the tangent plane
(i.e.,  within 90 degrees of the projection point).  Returns -1 otherwise. */

static int compute_tangent_plane_coords( const double dec, const double dec0,
                     const double delta_ra, double *xi, double *eta)
{
   const double x = sin( delta_ra) * cos( dec);
   const double y = cos( delta_ra) * cos( dec);
   const double z = sin( dec);
   const double d = cos( dec0) * y + sin( dec0) * z;

   *xi = x / d;
   *eta = (-sin( dec0) * y + cos( dec0) * z) / d;
   return( d > 0. ? 0 : -1);
}

#define ASTROMETRY 1
#define FIELD_DATA 2

const double start_gps_jd = 2448793.500000;           /* 1992 Jun 20  0:00:00 UTC */
double min_jd = start_gps_jd, max_jd = start_gps_jd + 365. * 100.;
            /* In 2092,  somebody may have to revise this */

static bool show_sats_in_shadow = true;

static void test_astrometry( const char *ifilename)
{
   FILE *ifile = fopen( ifilename, "rb");
   char buff[700];
   double sum_along = 0., sum_cross = 0.;
   double sum_along2 = 0., sum_cross2 = 0.;
   int n_found = 0;
   int data_type = 0, addenda_start;
   double exposure = 0., tilt = 0., override_field_size = 0.;
   void *ades_context = init_ades2mpc( );
   unsigned n_five_digit_times = 0, n_one_second_times = 0;
   mpc_code_t rover_data;

   assert( ifile);
   assert( ades_context);
   memset( &rover_data, 0, sizeof( rover_data));
   while( fgets_with_ades_xlation( buff, sizeof( buff), ades_context, ifile))
      {
      double ra, dec, jd = 0.;
      double altitude_adjustment = 0.;
      char mpc_code[4], time_str[25];
      static int count;
      size_t i;

      if( get_xxx_location_info( &rover_data, buff) == -2)
         {
#ifdef CGI_VERSION
         printf( "<b>Failure to read roving observer line:\n%s\n"
                 "<a href='https://www.projectpluto.com/xxx.htm'>Click here for information about how to fix this.\n"
                 "The lat/lon has to be provided in a very specific format.</b>\n", buff);
#else
         printf( "Failure to read roving observer line:\n%s\n"
                 "See https://www.projectpluto.com/xxx.htm.  The lat/lon has\n"
                 "to be provided in a very specific format.\n", buff);
#endif
         }
      if( !memcmp( buff, "COM ignore obs", 14))
         while( fgets_trimmed( buff, sizeof( buff), ifile))
            if( !memcmp( buff, "COM end ignore obs", 18))
               break;
      if( !memcmp( buff, "# Tilt: ", 8))
         tilt = atof( buff + 8) * PI / 180.;
      if( !memcmp( buff, "# Exposure: ", 12))
         exposure = atof( buff + 12) / seconds_per_day;
      if( !memcmp( buff, "# Field size : ", 15))
         override_field_size = atof( buff + 15) * PI / 180.;
      if( !memcmp( buff, "COM MGEX", 8))
         {
         extern bool use_mgex_data;

         use_mgex_data = false;
         printf( "Not using MGEX data\n");
         }
      for( i = 0; buff[i]; i++)
         if( buff[i] == ',')
            buff[i] = ' ';
      if( strlen( buff) >= 80)
         {
         const char removed_char = buff[80];
         unsigned time_format;

         if( strlen( buff) > 81 && (buff[80] == '+' || buff[80] == '-'))
            altitude_adjustment = atof( buff + 80);
         buff[80] = '\0';
         jd = extract_date_from_mpc_report( buff, &time_format);
         if( jd)
            {
            get_ra_dec_from_mpc_report( buff, NULL, &ra, NULL,
                                              NULL, &dec, NULL);
            strcpy( mpc_code, buff + 77);
            data_type = ASTROMETRY;
            if( time_format == 5)
               n_five_digit_times++;
            if( time_format == 20)
               n_one_second_times++;
            }
         buff[80] = removed_char;
         }
      if( data_type != ASTROMETRY && sscanf( buff, "%lf %lf %23s %3s%n",
                    &ra, &dec, time_str, mpc_code, &addenda_start) == 4)
         {
         jd = get_time_from_string( 0, time_str, FULL_CTIME_YMD, NULL);
         data_type = FIELD_DATA;
         ra *= PI / 180.;
         dec *= PI / 180.;
//       jd -= exposure / 2.;
         count++;
         }
      if( jd > min_jd && jd < max_jd)
         {
         mpc_code_t cdata;
         gps_ephem_t loc[MAX_N_GPS_SATS];
         double xi0[MAX_N_GPS_SATS], eta0[MAX_N_GPS_SATS];
         int i, n_sats, pass, err_code;
         const double earth_radius = 6378140.;  /* equatorial, in meters */
         const double TOL = 600.;                /* ten arcmin */
         double width, height;
         const double radians_to_arcsec = 3600. * 180. / PI;
         bool match_found = false;

         if( data_type == ASTROMETRY)
            height = width = TOL;
         else
            {
            if( override_field_size)
               width = height = override_field_size;
            else
               get_field_size( &width, &height, jd, mpc_code);
#ifdef CURRENTLY_UNUSED_DEBUGGING_STATEMENTS
            if( count < 10)
               printf( "%f x %f deg FOV\n", width * 180. / PI, height * 180. / PI);
#endif
            height *= radians_to_arcsec / 2.;
            width *= radians_to_arcsec / 2.;
            }

         if( !strcmp( mpc_code, "XXX"))
            {
            err_code = (strcmp( rover_data.code, "XXX") ? -1 : 0);
            if( err_code)
#ifdef CGI_VERSION
               printf( "<b>No position found for code (XXX)."
                       "<a href='https://www.projectpluto.com/xxx.htm'> Click here for information\n"
                       "on how to specify a <tt>COD Long.</tt> line in your astrometry file.</a></b>\n");
#else
               printf( "No position found for code (XXX).  See\n"
                       "https://www.projectpluto.com/xxx.htm for information\n"
                       "on how to specify one in your astrometry file.\n");
#endif
            else
               cdata = rover_data;
            }
         else if( !strcmp( mpc_code, "247"))
            {
            char loc_buff[100];

            if( fgets_with_ades_xlation( loc_buff, sizeof( loc_buff), ades_context, ifile))
               {
               if( 3 != get_mpc_code_info( &cdata, loc_buff))
                  printf( "ERROR : didn't parse the location line for a roving observer correctly\n");
               }
            else
               printf( "ERROR : didn't get a location line for a roving observer\n");
            }
         else
            {
            err_code = get_observer_loc( &cdata, mpc_code);
            if( err_code)
               printf( "\nCouldn't find observer '%s': err %d\n", mpc_code,
                                       err_code);
            }
         cdata.rho_cos_phi *= 1. + altitude_adjustment / earth_radius;
         cdata.rho_sin_phi *= 1. + altitude_adjustment / earth_radius;
         if( data_type == ASTROMETRY)
            printf( "%s", buff);
         for( pass = 0; pass < (exposure ? 2 : 1); pass++)
            {
            const double jd_new = jd + ((double)pass - 0.5) * exposure;

            n_sats = compute_gps_satellite_locations( loc, jd_new, &cdata);
            for( i = 0; i < n_sats; i++)
               {
               double xi, eta, jd_to_show = jd_new;
               bool is_a_match = false;
               const double BAD_PROJECTION = 99999.;

               if( compute_tangent_plane_coords( dec, loc[i].dec, ra - loc[i].ra,
                              &xi, &eta))
                  xi = BAD_PROJECTION;      /* place safely outside of contention */
               xi  *= radians_to_arcsec;
               eta *= radians_to_arcsec;
               if( tilt)
                  {
                  const double tval = cos( tilt) * xi - sin( tilt) * eta;

                  eta = cos( tilt) * eta - sin( tilt) * xi;
                  xi = tval;
                  }
               if( fabs( xi) < width && fabs( eta) < height)
                  is_a_match = true;
               if( !pass)
                  {
                  xi0[i] = xi / width;
                  eta0[i] = eta / height;
                  if( xi == BAD_PROJECTION)
                     xi0[i] = BAD_PROJECTION;
                  }
               else if( data_type != ASTROMETRY && !is_a_match
                              && xi != BAD_PROJECTION && xi0[i] != BAD_PROJECTION)
                  {
                  const double t = trail_within_image( xi0[i], eta0[i],
                           xi / width, eta / height);

                  if( t > 0.)       /* part of the trail does cross the image */
                     {
                     gps_ephem_t temp_loc[MAX_N_GPS_SATS];

                     is_a_match = true;
                     xi = xi0[i] + (xi - xi0[i]) * t;
                     eta = eta0[i] + (eta - eta0[i]) * t;
                     jd_to_show = jd + (t - 0.5) * exposure;
                     compute_gps_satellite_locations( temp_loc, jd_to_show, &cdata);
                     loc[i] = temp_loc[i];
                     }
                  }
               if( is_a_match)
                  {
                  const double motion = loc[i].motion * radians_to_arcsec;
                  const double sin_ang = sin( loc[i].posn_ang);
                  const double cos_ang = cos( loc[i].posn_ang);
                  const double cross_res = cos_ang * xi + sin_ang * eta;
                  double along_res = cos_ang * eta - sin_ang * xi;

                  along_res /= motion;
                  if( data_type == ASTROMETRY)
                     {
                     printf( "  %c xresid %10.6f\"  along %11.7fs  ",
                                    (loc[i].is_from_tle ? '*' : ' '),
                                    cross_res, along_res);
                     printf( "%s %s\n", loc[i].obj_desig, loc[i].international_desig);
                     if( loc[i].is_from_tle)
                        asterisk_has_been_shown = true;
                     }
                  else if( !loc[i].in_shadow || show_sats_in_shadow)
                     {
                     full_ctime( time_str, jd_to_show, FULL_CTIME_YMD
                                 | FULL_CTIME_MONTHS_AS_DIGITS
                                 | FULL_CTIME_LEADING_ZEROES);
                     printf( "%s ", time_str);
                     if( log_file)
                        fprintf( log_file, "%s ", time_str);
                     display_satellite_info( loc + i, true);
                     printf( "%s %s\n", mpc_code, buff + addenda_start);
                     if( log_file)
                        fprintf( log_file, "%s %s\n", mpc_code, buff + addenda_start);
                     }
                  n_found++;
                  match_found = true;
                  sum_along += along_res;
                  sum_along2 += along_res * along_res;
                  sum_cross += cross_res;
                  sum_cross2 += cross_res * cross_res;
                  }
               }
            }
         if( data_type == ASTROMETRY && !match_found)
            printf( "   (no matching satellite found)\n");
         }
      }
   free_ades2mpc_context( ades_context);
   if( n_found > 1 && data_type == ASTROMETRY)
      {
      printf( "\n%d observations found\n\n", n_found);
      sum_along /= (double)n_found;
      sum_along2 /= (double)n_found;
      sum_cross /= (double)n_found;
      sum_cross2 /= (double)n_found;
      printf( "Avg cross-track : %10.6f +/- %.6f\"\n",
               sum_cross, sqrt( sum_cross2 - sum_cross * sum_cross));
      printf( "Avg along-track (timing): %11.7f +/- %.7f seconds\n",
               sum_along, sqrt( sum_along2 - sum_along * sum_along));
      if( sum_along > 0.)
         printf( "Positive along-track errors mean your clock was 'behind' the actual time;\n"
              "i.e.,  the times reported in the astrometry are earlier than the positions\n"
              "of the GPS satellites would indicate.\n");
      else
         printf( "Negative along-track errors mean your clock was 'ahead' of the actual time;\n"
              "i.e.,  the times reported in the astrometry are later than the positions\n"
              "of the GPS satellites would indicate.\n");
      }
   if( !data_type)
      printf( "Didn't find any astrometry in the input data.  Check the data\n"
#ifdef CGI_VERSION
            "to be sure it is in <a href='astromet.htm'>either MPC80 or ADES"
            "format</a>"
#else
            "to be sure it is in either MPC80 or ADES format"
#endif
            " and try again.\n");
   if( n_five_digit_times)
      printf( "\nWARNING : %u of your observations had times given to five digits.\n"
              "This isn't terrible,  but it does limit those times to have a precision\n"
              "of 0.864 seconds.  Record a sixth digit to get 86.4 millisecond precision.\n"
              "Or (preferred solution) use the ADES format.\n", n_five_digit_times);

   if( n_one_second_times)
      printf( "\nWARNING : %u of your observations had times given to a precision of\n"
              "one second.  This isn't terrible,  but it would be better to provide at\n"
              "least 0.1-second precision.\n", n_one_second_times);

   if( n_five_digit_times || n_one_second_times)
      printf(
#ifdef CGI_VERSION
              "<a href='https://www.projectpluto.com/gps_ast.htm#tips'>"
              "Click here for information about increasing the reported precision.</a>\n"
#else
              "Visit https://www.projectpluto.com/gps_ast.htm#tips for information\n"
              "about increasing the reported timing precision.\n"
#endif
              );

   fclose( ifile);
}

const char *google_map_url =
   "<a title='Click for map' href='http://maps.google.com/maps?q=%+.5f,%+.5f'>";

/* See 'dailyize.c' for info about 'finals.mix'.  Note that 'finals.all'
may also be available at ftp://maia.usno.navy.mil/ser7/finals.all.  */

int dummy_main( const int argc, const char **argv)
{
   const char *ephem_step = NULL, *ephem_target = NULL;
   bool desig_not_found = false;
   int n_ephem_steps = 20;
   char observatory_code[20];
   const char *legend =
          "RA      (J2000)     dec     dist (km)    Azim   Alt Elo  Rate  PA ";
   const char *geocentric_legend =
          "RA      (J2000)     dec     dist (km)  Elo  Rate  PA ";
   const double jan_1_1970 = 2440587.5;
   const double curr_t = jan_1_1970 + (double)time( NULL) / seconds_per_day;
   const double utc = get_time_from_string( curr_t,
               (argc > 1 ? argv[1] : "+0"), FULL_CTIME_YMD, NULL);
   int i, n_sats, eop_file_mjd;
   mpc_code_t cdata;
   gps_ephem_t loc[MAX_N_GPS_SATS];
   char tbuff[80];
   int err_code = load_earth_orientation_params( "finals.mix", &eop_file_mjd);
   FILE *geo_rect_file;

   if( err_code <= 0)
      err_code = load_earth_orientation_params( "finals.all", &eop_file_mjd);

   full_ctime( tbuff, curr_t, FULL_CTIME_YMD);
   printf( "Current time = %s UTC\n", tbuff);
   printf( "Version 2024 Oct 30\n");
   if( err_code <= 0)
      {
      printf( "\nProblem loading EOPs (Earth Orientation Parameters):  rval %d\n", err_code);
#ifdef CGI_VERSION
      printf( "Please notify the owner of this site.  New Earth Orientation Parameters\n"
              "need to be uploaded every few months (they can't be predicted far in\n"
              "advance).  The owner appears to have forgotten to do this.\n");
#else
      printf( "You probably need to download the EOP file\n"
              "https://datacenter.iers.org/data/latestVersion/finals.all.iau1980.txt\n"
              "and rename it to 'finals.all'.\n"
              "This needs to be downloaded every few months (the earth's orientation\n"
              "can't be predicted far in advance).  Get a current 'finals.all',  and\n"
              "this error will probably go away.\n\n");
#endif
      }
   else
      {
      full_ctime( tbuff, 2400000.5 + eop_file_mjd,
                             FULL_CTIME_YMD | FULL_CTIME_DATE_ONLY);
      printf( "Earth rotation parameter file date %s\n", tbuff);
      }

   for( i = 2; i < argc; i++)
      if( argv[i][0] == '-')
         {
         const char *arg = get_arg( argc, argv, i);

         switch( argv[i][1])
            {
            case 'a': case 'A':
               minimum_altitude = atof( arg) * PI / 180.;
               break;
            case 'd': case 'D':
               show_decimal_degrees = true;
               break;
            case 'i': case 'I':
               ephem_step = arg;
               break;
            case 'l':
               min_jd = get_time_from_string( curr_t, arg, FULL_CTIME_YMD,
                                 NULL);
               printf( "Min JD reset to %f\n", min_jd);
               break;
            case 'L':
               log_file = fopen( arg, "wb");
               break;
            case 'n':
               n_ephem_steps = atoi( arg);
               break;
            case 'N':
               names_filename = arg;
               break;
            case 'o':
               ephem_target = get_name_data( arg, (int)( utc - 2400000.5));
               if( !ephem_target)
                  {
                  printf( "SATELLITE '%s' NOT FOUND\n", arg);
                  printf( "Designations can be of any of these three forms:\n"
                          "   YYYY-NNNL   (International designation : year,  three digits,  a letter)\n"
                          "   YYNNNL      ('short form' international,  with a two-digit year)\n"
                          "   LNN         (GNSS designation)\n"
                          "   LNNN        (Alternative,  rarely used system for GNSS designation)\n"
#ifdef CGI_VERSION
   "<a href='https://www.projectpluto.com/gps_expl.htm#desigs'>"
   "Click here for more information.</a>\n"
#else
   "   See https://www.projectpluto.com/gps_expl.htm#desigs for details.\n"
#endif
                                                               );
                  desig_not_found = true;
                  }
               else
                  ephem_target += 12;     /* skip MJD range data at start */
               break;
            case 'p':
               {
               extern const char *ephem_data_path;

               ephem_data_path = arg;       /* see 'gps.cpp' */
               }
               break;
            case 'r':
               strncpy( relocation, arg, sizeof( relocation) - 1);
               break;
            case 's': case 'S':
               sort_order = atoi( arg);
               break;
            case 't':
               tle_usage = atoi( arg);
               break;
            case 'T':
               tle_path = arg;
               break;
            case 'u':
               max_jd = get_time_from_string( curr_t, arg, FULL_CTIME_YMD,
                                 NULL);
               break;
            case 'v': case 'V':
               {
               extern int gps_verbose;

               gps_verbose = 1;
               }
               break;
            case 'z':
               creating_fake_astrometry = true;
               break;
            default:
               printf( "Command line option '%s' not understood\n", argv[i]);
               break;
            }
         }
   if( argc >= 2 && argv[1][0] == '-' && argv[1][1] == 'f')
      {
      test_astrometry( get_arg( argc, argv, 1));
      if( asterisk_has_been_shown && tle_usage != USE_TLES_ONLY)
         printf( "\n%s", asterisk_message);
      return( 0);
      }

   if(  argc < 3)
      {
      printf( "Usage possibilities are:\n\n"
              "list_gps (date/time) (MPC station)   (to get a list of sats)\n"
              "list_gps (date/time) (MPC station) -o(target) -i(ephem step)\n"
              "list_gps -f (filename)\n\n"
              "-a(alt)     Set minimum altitude (default=0)\n"
              "-d          RA/decs shown in decimal degrees\n"
              "-f          Filename contains astrometry;  get an evaluation of\n"
              "            cross/along-track errors\n"
              "-n(#)       Set number of ephemeris steps shown\n"
              "-s(#)       Set sort order (1=elong, 2=RA, 3=alt, 4=desig, 5=COSPAR,\n"
              "            6=dec, 7=dist)\n"
              "-t(#)       Set TLE usage: 1=don't use them, 0=use only TLEs\n"
              "-v          Verbose mode\n"
              "-z          Ephemerides are simulated 80-column MPC astrometry\n");
      return( -1);
      }
   strcpy( observatory_code, argv[2]);
   if( strlen( observatory_code) < 3)
      {
      printf( "You must specify a three-character MPC code for your site.\n"
              "Use 500 for the geocenter.  If your site lacks an observatory\n"
              "code,  send me your latitude,  longitude,  and altitude,  and\n"
              "I'll add an (unofficial) MPC observatory code for you.  I can\n"
              "be contacted at\n" MANGLED_EMAIL "\n"
              "(remove diacritics if you're not a spammer).\n");
      return( -1);
      }
   i = (int)strlen( observatory_code) - 2;
   if( i > 0 && !strcmp( observatory_code + i, " m"))
      {
      extern bool use_mgex_data;

      use_mgex_data = false;
      observatory_code[i] = '\0';
      }
   full_ctime( tbuff, utc, FULL_CTIME_YMD | FULL_CTIME_MILLISECS);
   printf( "GPS positions for JD %f = %s UTC\n", utc, tbuff);
   if( utc < start_gps_jd)     /* 1992 Jun 20  0:00:00 UTC */
      {
      printf( "GPS/GLONASS ephemerides only extend back to 1992 June 20.\n");
      return( ERR_CODE_TOO_FAR_IN_PAST);
      }
   err_code = get_observer_loc( &cdata, observatory_code);
   if( err_code)
      {
      printf( "Couldn't find observer '%s': err %d\n",
                     observatory_code, err_code);
#ifdef CGI_VERSION
      printf( "If you think that code really does exist,  notify the owner of this\n"
              "page.  The list of observatories probably needs to be updated.\n");
#endif
      err_code = ERR_CODE_OBSERVATORY_UNKNOWN;
      }
   if( err_code)
      return( err_code);

   geo_rect_file = fopen( "geo_rect.txt", "rb");
   if( geo_rect_file)
      {
      extract_region_data_for_lat_lon( geo_rect_file, tbuff,
                           cdata.lat * 180. / PI, cdata.lon * 180. / PI);
      fclose( geo_rect_file);
      }
   printf( "Observatory (%s) %s %s\n", observatory_code, cdata.name, tbuff);
   if( cdata.lon > PI)
      cdata.lon -= PI + PI;
   is_topocentric = (cdata.rho_cos_phi || cdata.rho_sin_phi);
   if( is_topocentric)
      {
      printf( "Longitude %f, latitude %f  alt %.2f m\n",
            cdata.lon * (180. / PI), cdata.lat * (180. / PI), cdata.alt);
#ifdef CGI_VERSION
      printf( google_map_url, cdata.lat * (180. / PI), cdata.lon * (180. / PI));
      printf( "Click here for a Google Map for this site.</a>  If you have doubts\n");
      printf( "about the lat/lon/alt for this observatory,  check the above.  Google\n");
      printf( "Maps is usually right to about five or ten meters.\n");
#endif
      }
   else
      legend = geocentric_legend;

   if( ephem_target && ephem_step)
      {
      double step_size = atof( ephem_step);
      const char end_char = ephem_step[strlen( ephem_step) - 1];
      int j;
      int time_format = FULL_CTIME_YMD
                                    | FULL_CTIME_LEADING_ZEROES
                                    | FULL_CTIME_MONTHS_AS_DIGITS;

      if( end_char == 'm')
         {
         step_size /= minutes_per_day;
         time_format |= FULL_CTIME_FORMAT_HH_MM | FULL_CTIME_5_PLACES;
         }
      else if( end_char == 'd')
         time_format |= FULL_CTIME_FORMAT_DAY | FULL_CTIME_8_PLACES;
      else        /* assume seconds */
         {
         step_size /= seconds_per_day;
         time_format |= FULL_CTIME_MILLISECS;
         }
      if( creating_fake_astrometry)
         printf( "COM Time sigma 1e-9\n");
      else
         {
         printf( "Target object: %s\n", ephem_target);
         printf( "UTC date/time             %s\n", legend);
         }
      for( i = 0; i < n_ephem_steps; i++)
         {
         const double curr_utc = utc + (double)i * step_size;
         bool got_data = false;

         if( creating_fake_astrometry)
            {
            full_ctime( tbuff, curr_utc, FULL_CTIME_YMD | FULL_CTIME_MICRODAYS
                                    | FULL_CTIME_LEADING_ZEROES
                                    | FULL_CTIME_ROUNDING
                                    | FULL_CTIME_MONTHS_AS_DIGITS);
            printf( "     GNS%.3s   C%s", ephem_target, tbuff);
            }
         else
            {
            full_ctime( tbuff, curr_utc, time_format | FULL_CTIME_ROUNDING);
            printf( "%-23s", tbuff);
            }
         n_sats = compute_gps_satellite_locations( loc, curr_utc, &cdata);

         for( j = 0; j < n_sats; j++)
            if( !memcmp( loc[j].obj_desig, ephem_target, 3))
               {
               got_data = true;
               display_satellite_info( loc + j, false);
               }
         if( !got_data)
            printf( "  Object not found\n");
         if( creating_fake_astrometry)
            printf( "                Fake %s\n", observatory_code);
         }
      }
   else if( !desig_not_found)       /* just list all the satellites */
      {
      n_sats = compute_gps_satellite_locations( loc, utc, &cdata);
      if( n_sats <= 0)
         {
         printf( "No satellites found\n");
         if( utc > curr_t + 4.)
            {
            printf( "Precise predictions are only available for about five days\n"
                    "in advance.  You should probably try again a few days before\n"
                    "your planned observation time.\n");
            }
         }
      else
         {
         sort_sat_info( n_sats, loc, sort_order);
         printf( " Nr:    %s   Desig\n", legend);
         for( i = 0; i < n_sats; i++)
            if( loc[i].alt > minimum_altitude || !is_topocentric)
               display_satellite_info( loc + i, true);
         }
      }
   load_earth_orientation_params( NULL, NULL);   /* free up memory */
   free_cached_gps_positions( );
   if( asterisk_has_been_shown)
      printf( "%s", asterisk_message);
   return( 0);
}

#ifndef CGI_VERSION     /* "standard" command-line test version */
int main( const int argc, const char **argv)
{
   return( dummy_main( argc, argv));
}
#endif
