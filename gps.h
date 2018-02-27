
void free_cached_gps_positions( void);
int get_gps_positions( double *output_coords, const double *observer_loc,
                            const double mjd_gps);

char *desig_from_index( const int idx);
int get_gps_positions_from_tle( const char *tle_filename,
                        double *output_coords, const double mjd_gps);

#define MAX_N_GPS_SATS 200
