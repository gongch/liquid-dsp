/*
 * Copyright (c) 2007, 2008, 2009, 2010 Joseph Gaeddert
 * Copyright (c) 2007, 2008, 2009, 2010 Virginia Polytechnic
 *                                      Institute & State University
 *
 * This file is part of liquid.
 *
 * liquid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * liquid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with liquid.  If not, see <http://www.gnu.org/licenses/>.
 */

//
// polynomial methods
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "liquid.internal.h"


// finds the complex roots of the polynomial
void POLY(_findroots)(T * _p,
                      unsigned int _k,
                      TC * _roots)
{
    POLY(_findroots_bairstow)(_p,_k,_roots);
}

// finds the complex roots of the polynomial using the Durand-Kerner method
void POLY(_findroots_durandkerner)(T * _p,
                                   unsigned int _k,
                                   TC * _roots)
{
    if (_k < 2) {
        fprintf(stderr,"%s_findroots(), order must be greater than 0\n", POLY_NAME);
        exit(1);
    } else if (_p[_k-1] != 1) {
        fprintf(stderr,"%s_findroots(), _p[_k-1] must be equal to 1\n", POLY_NAME);
        exit(1);
    }

    unsigned int i;
    unsigned int num_roots = _k-1;
    T r0[num_roots];
    T r1[num_roots];

    // find intial magnitude
    float g     = 0.0f;
    float gmax  = 0.0f;
    for (i=0; i<_k; i++) {
        g = cabsf(_p[i]);
        if (i==0 || g > gmax)
            gmax = g;
    }

    // initialize roots
    T t0 = 0.9f * (1 + gmax) * cexpf(_Complex_I*1.1526f);
    T t  = 1.0f;
    for (i=0; i<num_roots; i++) {
        r0[i] = t;
        t *= t0;
    }

    unsigned int max_num_iterations = 50;
    int continue_iterating = 1;
    unsigned int j, k;
    T f;
    T fp;
    //for (i=0; i<num_iterations; i++) {
    i = 0;
    while (continue_iterating) {
#if 0
        printf("%s_findroots(), i=%3u :\n", POLY_NAME, i);
        for (j=0; j<num_roots; j++)
            printf("  r[%3u] = %12.8f + j*%12.8f\n", j, crealf(r0[j]), cimagf(r0[j]));
#endif
        for (j=0; j<num_roots; j++) {
            f = POLY(val)(_p,_k,r0[j]);
            fp = 1;
            for (k=0; k<num_roots; k++) {
                if (k==j) continue;
                fp *= r0[j] - r0[k];
            }
            r1[j] = r0[j] - f / fp;
        }

        // stop iterating if roots have settled
        float delta=0.0f;
        T e;
        for (j=0; j<num_roots; j++) {
            e = r0[j] - r1[j];
            delta += crealf(e*conjf(e));
        }
        delta /= num_roots * gmax;
#if 0
        printf("delta[%3u] = %12.4e\n", i, delta);
#endif

        if (delta < 1e-6f || i == max_num_iterations)
            continue_iterating = 0;

        memmove(r0, r1, num_roots*sizeof(T));
        i++;
    }

    for (i=0; i<_k; i++)
        _roots[i] = r1[i];
}

// finds the complex roots of the polynomial using Bairstow's method
void POLY(_findroots_bairstow)(T * _p,
                               unsigned int _k,
                               TC * _roots)
{
    T p0[_k];       // buffer 0
    T p1[_k];       // buffer 1
    T * p   = NULL; // input polynomial
    T * pr  = NULL; // output (reduced) polynomial

    unsigned int i, k=0;
    memmove(p0, _p, _k*sizeof(T));

    T u, v;

    unsigned int n = _k;
    unsigned int r = _k % 2;
    unsigned int L = (_k-r)/2;
    for (i=0; i<L-1+r; i++) {
        p  = (i % 2) == 0 ? p0 : p1;
        pr = (i % 2) == 0 ? p1 : p0;

        // initial estimates for u, v
        // TODO : ensure no division by zero
        u = p[n-2] / p[n-1];
        v = p[n-3] / p[n-1];

        // compute factor using Bairstow's recursion
        POLY(_findroots_bairstow_recursion)(p,n,pr,&u,&v);

        float complex r0 = 0.5f*(-u + csqrtf(u*u - 4*v));
        float complex r1 = 0.5f*(-u - csqrtf(u*u - 4*v));

        _roots[k++] = r0;
        _roots[k++] = r1;

#if 0
        unsigned int j;
        printf("initial polynomial:\n");
        for (j=0; j<n; j++)
            printf("  p[%3u]  = %12.8f + j*%12.8f\n", j, crealf(p[j]), cimagf(p[j]));

        printf("polynomial factor: x^2 + u*x + v\n");
        printf("  u : %12.8f + j*%12.8f\n", crealf(u), cimagf(u));
        printf("  v : %12.8f + j*%12.8f\n", crealf(v), cimagf(v));

        printf("roots:\n");
        printf("  r0 : %12.8f + j*%12.8f\n", crealf(r0), cimagf(r0));
        printf("  r1 : %12.8f + j*%12.8f\n", crealf(r1), cimagf(r1));

        printf("reduced polynomial:\n");
        for (j=0; j<n-2; j++)
            printf("  pr[%3u] = %12.8f + j*%12.8f\n", j, crealf(pr[j]), cimagf(pr[j]));
#endif

        n -= 2;
    }

    if (r==0) {
        assert(n==2);
        _roots[k++] = -pr[0]/pr[1];
    }
    //assert( k == _k-1 );
}

// iterate over Bairstow's method
void POLY(_findroots_bairstow_recursion)(T * _p,
                                         unsigned int _k,
                                         T * _p1,
                                         T * _u,
                                         T * _v)
{
    // validate length
    if (_k < 3) {
        fprintf(stderr,"findroots_bairstow_recursion(), invalid polynomial length: %u\n", _k);
        exit(1);
    }

    // initial estimates for u, v
    T u = *_u;
    T v = *_v;
    
    unsigned int n = _k-1;
    T c,d,g,h;
    T q;
    T du, dv;

    // reduced polynomials
    T b[_k];
    T f[_k];
    b[n] = b[n-1] = 0;
    f[n] = f[n-1] = 0;

    int i;
    unsigned int k=0;
    unsigned int max_num_iterations=50;
    int continue_iterating = 1;

    while (continue_iterating) {
        // update reduced polynomial coefficients
        for (i=n-2; i>=0; i--) {
            b[i] = _p[i+2] - u*b[i+1] - v*b[i+2];
            f[i] =  b[i+2] - u*f[i+1] - v*f[i+2];
        }
        c = _p[1] - u*b[0] - v*b[1];
        g =  b[1] - u*f[0] - v*f[1];
        d = _p[0] - v*b[0];
        h =  b[0] - v*f[0];

        // compute scaling factor
        q  = 1/(v*g*g + h*(h-u*g));

        // compute u, v steps
        du = - q*(-h*c   + g*d);
        dv = - q*(-g*v*c + (g*u-h)*d);

#if 0
        printf("bairstow [%u] :\n", k);
        printf("  u     : %12.4e + j*%12.4e\n", crealf(u), cimagf(u));
        printf("  v     : %12.4e + j*%12.4e\n", crealf(v), cimagf(v));
        printf("  b     : \n");
        for (i=0; i<n-2; i++)
            printf("      %12.4e + j*%12.4e\n", crealf(b[i]), cimagf(b[i]));
        printf("  fb    : \n");
        for (i=0; i<n-2; i++)
            printf("      %12.4e + j*%12.4e\n", crealf(f[i]), cimagf(f[i]));
        printf("  c     : %12.4e + j*%12.4e\n", crealf(c), cimagf(c));
        printf("  g     : %12.4e + j*%12.4e\n", crealf(g), cimagf(g));
        printf("  d     : %12.4e + j*%12.4e\n", crealf(d), cimagf(d));
        printf("  h     : %12.4e + j*%12.4e\n", crealf(h), cimagf(h));
        printf("  q     : %12.4e + j*%12.4e\n", crealf(q), cimagf(q));
        printf("  du    : %12.4e + j*%12.4e\n", crealf(du), cimagf(du));
        printf("  dv    : %12.4e + j*%12.4e\n", crealf(dv), cimagf(dv));

        printf("  step : %12.4e + j*%12.4e\n", crealf(du+dv), cimagf(du+dv));
#endif

        // adjust u, v
        if (isnan(du) || isnan(dv)) {
            u *= 0.5f;
            v *= 0.5f;
        } else {
            u += du;
            v += dv;
        }

        // increment iteration counter
        k++;

        // exit conditions
        if (cabsf(du+dv) < 1e-6f || k == max_num_iterations)
            continue_iterating = 0;
    }

    for (i=0; i<_k-2; i++)
        _p1[i] = b[i];

    *_u = u;
    *_v = v;

}
