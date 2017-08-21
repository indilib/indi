#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "libdspau.h"

struct Coefficient {
	double e;
	double p;
	double d0;
	double d1;
	double d2;
	double x0;
	double x1;
	double x2;
	double y0;
	double y1;
	double y2;
};

double* dspau_squarelawfilter(double* y, int len)
{
	double mn = DBL_MAX, mx = DBL_MIN;
	for(int i = 0; i < len; i++) {
		mn = (y[i] < mn ? y[i] : mn);
		mx = (y[i] > mx ? y[i] : mx);
	}
	double mean = (double)((mx - mn) / 2 + mn);
	double* ret = y;
	int val = 0;
	for (int i = 0; i < len; i++) {
		val = ret [i] - mean;
		ret [i] = (ushort)(abs (val) + mean);
	}
	return ret;
}

double* dspau_lowpassfilter(double* y, int len, double SamplingFrequency, double Frequency, double Q)
{
	double* ret = (double*)malloc(sizeof(double) * len);
	double RC = 1.0 / (Frequency * 2.0 * M_PI); 
	double dt = 1.0 / SamplingFrequency; 
	double alpha = dt / (RC + dt) / Q;
	ret [0] = y [0];
	for (int i = 1; i < len; ++i) { 
		ret [i] = (ushort)(ret [i - 1] + (alpha * (y [i] - ret [i - 1])));
	}
	return ret;
}

double* dspau_highpassfilter(double* y, int len, double SamplingFrequency, double Frequency, double Q)
{
	double* ret = (double*)malloc(sizeof(double) * len);
	double RC = 1.0 / (Frequency * 2.0 * M_PI); 
	double dt = 1.0 / SamplingFrequency; 
	double alpha = dt / (RC + dt) / Q;
	ret [0] = y [0];
	for (int i = 1; i < len; ++i) { 
		ret [i] = (ushort)(y[i] - (ret [i - 1] + (alpha * (y [i] - ret [i - 1]))));
	}
	return ret;
}

double dspau_singlefilter(double yin, struct Coefficient *coefficient) {
	double yout = 0.0;
	coefficient->x0 = coefficient->x1; 
	coefficient->x1 = coefficient->x2; 
	coefficient->x2 = yin;

	coefficient->y0 = coefficient->y1; 
	coefficient->y1 = coefficient->y2; 
	coefficient->y2 = coefficient->d0 * coefficient->x2 - coefficient->d1 * coefficient->x1 + coefficient->d0 * coefficient->x0 + coefficient->d1 * coefficient->y1 - coefficient->d2 * coefficient->y0;

	yout = coefficient->y2;
	return yout;
}

double* dspau_bandrejectfilter(double* y, int len, double SamplingFrequency, double Frequency, double Q) {
	double* ret = (double*)malloc(sizeof(double) * len);
	double wo = 2.0 * M_PI * Frequency / SamplingFrequency;

	struct Coefficient coefficient;
	coefficient.e = 1 / (1 + tan (wo / (Q * 2)));
	coefficient.p = cos (wo);
	coefficient.d0 = coefficient.e;
	coefficient.d1 = 2 * coefficient.e * coefficient.p;
	coefficient.d2 = (2 * coefficient.e - 1);
	for (int x = 0; x < len; x ++) {
		ret [x + 0] = dspau_singlefilter (y [x], &coefficient);
	}
	return ret;
}

double* dspau_bandpassfilter(double* y, int len, double SamplingFrequency, double Frequency, double Q) {
	double* ret = dspau_bandrejectfilter (y, len, SamplingFrequency, Frequency, Q);
	for (int x = 0; x < len; x ++) {
		ret [x] = y [x] - ret [x];
	}
	return ret;
}


