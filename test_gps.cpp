#include <stdio.h>
#include <stdlib.h>
#include "gps.h"

int main( const int argc, const char **argv)
{
   if( argc > 1)
      {
      double locs[MAX_N_GPS_SATS * 3];
      double *tptr = locs;
      const double mjd = atof( argv[1]);
      int i, rval;

      for( i = 0; i < MAX_N_GPS_SATS * 3; i++)
         locs[i] = 0.;
      if( argc > 2)
         rval = get_gps_positions_from_tle( argv[2], locs, mjd);
      else
         rval = get_gps_positions( locs, NULL, mjd);
      printf( "rval = %d\n", rval);
      for( i = 0; i < MAX_N_GPS_SATS; i++, tptr += 3)
         if( tptr[0] || tptr[1] || tptr[2])
            printf( "%-4s %14.6f%14.6f%14.6f\n",
                        desig_from_index( i),
                        tptr[0], tptr[1], tptr[2]);
      free_cached_gps_positions( );
      }
   return( 0);
}
