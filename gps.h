
void free_cached_gps_positions( void);
int get_gps_positions( double *output_coords, const double mjd_gps);
char *desig_from_index( const int idx);

#define MAX_N_GPS_SATS 100
