/* Implementation of common functions */
/* The current date is: 25/01/2013  */
#include "thornifix.h"

int thor_interpol(const double* x, const double* y, int n, double* z, double* fz, int m)
{
    int i, j, k;
    double* _tbl, *_coef;
    double _t;

    /* allocate storage */
    _tbl = (double*) calloc(n, sizeof(double));
    if(_tbl == NULL)
	return 1;
    _coef = (double*) calloc(n, sizeof(double));
    if(_coef == NULL)
	{
	    free(_tbl);
	    return 1;
	}
    /* initialise coefficnets */
    for(i=0; i<n; i++)
	_tbl[i] = y[i];

    /* work out the coefficients of the interpolating polynomial */
    _coef[0] = _tbl[0];

    for(k=1; k<n; k++)
	{
	    for(i=0; i<n-k; i++)
		{
		    j=i+k;
		    _tbl[i] = (_tbl[i+1] - _tbl[i]) / (x[j] - x[i]);
		}
	    _coef[k] = _tbl[0];
	}
    free(_tbl);

    /* work out interpolating polynomial specified points */
    for(k=0; k<m; k++)
	{
	    fz[k] = _coef[0];
	    for(j=1; j<n; j++)
		{
		    _t = _coef[j];
		    for(i=0; i<j; i++)
			_t = _t * (z[k] - x[i]);

		    fz[k] += _t;
		}
	}
    free(_coef);
    return 0;
}

