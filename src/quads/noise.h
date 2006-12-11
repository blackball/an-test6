#ifndef NOISE_H
#define NOISE_H

/**
   angleArcMin: radius in arcminutes.
 */
void sample_star_in_circle(double* center, double angleArcMin,
						   double* point);

void sample_star_in_ring(double* center,
						 double minAngleArcMin,
						 double maxAngleArcMin,
						 double* point);

void sample_field_in_circle(double* center, double radius,
							double* point);

void add_star_noise(const double* real, double noisemag, double* noisy);

void add_field_noise(const double* real, double noisevar, double* noisy);

void compute_star_code(double* A, double* B, double* C, double* D,
					   double* code);

void compute_field_code(double* A, double* B, double* C, double* D,
						double* code, double* scale);

#endif
