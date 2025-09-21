
#ifndef COORDINATES_TRANSLATION_FUNCTIONS_H
#define COORDINATES_TRANSLATION_FUNCTIONS_H

void geodetic_to_ecef(const GeodeticPosition* geo, double* ecef_x, double* ecef_y, double* ecef_z);
void eci_to_ecef(const double* eci_pos, double julian_date, double* ecef_pos);
void ecef_to_topo(const double* observer_ecef, const double* satellite_ecef, const GeodeticPosition* observer_geo, double* azimuth, double* elevation, double* range);
double unix_to_julian(time_t unix_time);

#endif // COORDINATES_TRANSLATION_FUNCTIONS_H
