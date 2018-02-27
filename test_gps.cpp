#include <stdio.h>
#include <stdlib.h>
#include "gps.h"

int main( const int argc, const char **argv)
{
   if( argc > 1)
      {
      double locs[MAX_N_GPS_SATS * 3];
      double *tptr = locs;
      int i;

      printf( "rval = %d\n", get_gps_positions( locs, NULL, atof( argv[1])));
      for( i = 0; i < MAX_N_GPS_SATS; i++, tptr += 3)
         if( tptr[0] || tptr[1] || tptr[2])
            printf( "%2d: %14.6f%14.6f%14.6f\n", i, tptr[0], tptr[1], tptr[2]);
      free_cached_gps_positions( );
      }
   return( 0);
}
