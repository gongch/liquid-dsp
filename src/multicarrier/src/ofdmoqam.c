/*
 * Copyright (c) 2007, 2009 Joseph Gaeddert
 * Copyright (c) 2007, 2009 Virginia Polytechnic Institute & State University
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
//
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "liquid.internal.h"

//#define DEBUG

struct ofdmoqam_s {
    unsigned int num_channels;
    unsigned int m;
    float beta;
    float dt;

    float complex * x0; // time-domain buffer
    float complex * x1; // time-domain buffer

    float complex * X0; // freq-domain buffer
    float complex * X1; // freq-domain buffer

    float complex * x_prime;
    float complex * x_tilda;
    
    firpfbch c0;
    firpfbch c1;

    unsigned int type;  // synthesis/analysis
};

ofdmoqam ofdmoqam_create(unsigned int _num_channels,
                         unsigned int _m,
                         float _beta,
                         float _dt,
                         int _type)
{
    ofdmoqam c = (ofdmoqam) malloc(sizeof(struct ofdmoqam_s));
    c->num_channels = _num_channels;
    c->m = _m;
    c->type = _type;
    c->beta = _beta;
    c->dt   = _dt;

    // validate input
    if ( ((c->num_channels)%2) != 0 ) {
        printf("error: ofdmoqam_create(), invalid channel number %u (must be even)\n", c->num_channels);
        exit(1);
    } else if (c->m < 1) {
        printf("error: ofdmoqam_create(), invalid filter delay %u (must be greater than 1)\n", c->m);
        exit(1);
    }

    // allocate memory for time-domain buffers
    c->x0 = (float complex*) malloc((c->num_channels)*sizeof(float complex));
    c->x1 = (float complex*) malloc((c->num_channels)*sizeof(float complex));

    // allocate memory for freq-domain buffers
    c->X0 = (float complex*) malloc((c->num_channels)*sizeof(float complex));
    c->X1 = (float complex*) malloc((c->num_channels)*sizeof(float complex));

    // allocate memory for time-delay buffer
    c->x_prime = (float complex*) malloc((c->num_channels)*sizeof(float complex));

    // allocate memory for analyzer delay buffer
    c->x_tilda = (float complex*) malloc((c->num_channels)*sizeof(float complex));

    // create filterbank channelizers
    // TODO: use actual prototype (get rid of _slsl input)
    c->c0 = firpfbch_create(_num_channels, c->m, c->beta, c->dt, FIRPFBCH_ROOTNYQUIST, c->type);
    c->c1 = firpfbch_create(_num_channels, c->m, c->beta, c->dt, FIRPFBCH_ROOTNYQUIST, c->type);

    // clear buffers, etc.
    ofdmoqam_clear(c);

    return c;
}

void ofdmoqam_destroy(ofdmoqam _c)
{
    free(_c->c0);
    free(_c->x0);
    free(_c->X0);

    free(_c->c1);
    free(_c->x1);
    free(_c->X1);

    free(_c->x_prime);
    free(_c->x_tilda);

    free(_c);
}

void ofdmoqam_print(ofdmoqam _c)
{
    printf("ofdmoqam: [%u taps]\n", 0);
}

void ofdmoqam_clear(ofdmoqam _c)
{
    // clear filterbank channelizers
    firpfbch_clear(_c->c0);
    firpfbch_clear(_c->c1);

    // clear buffers
    unsigned int i;
    for (i=0; i<_c->num_channels; i++) {
        _c->x0[i] = 0.0f;
        _c->x1[i] = 0.0f;
        _c->X0[i] = 0.0f;
        _c->X1[i] = 0.0f;
        _c->x_prime[i] = 0.0f;
        _c->x_tilda[i] = 0.0f;
    }
}


void ofdmoqam_synthesizer_execute(ofdmoqam _c, float complex * _X, float complex * _x)
{
    unsigned int i;
    unsigned int k2 = (_c->num_channels)/2;

    // prepare signal
    for (i=0; i<_c->num_channels; i+=2) {
        // even channels
        _c->X0[i]   = cimagf(_X[i])*_Complex_I;
        _c->X1[i]   = crealf(_X[i]);

        // odd channels
        _c->X0[i+1] = crealf(_X[i+1]);
        _c->X1[i+1] = cimagf(_X[i+1])*_Complex_I;
    }

    // execute synthesis filter banks
    firpfbch_execute(_c->c0, _c->X0, _c->x0);
    firpfbch_execute(_c->c1, _c->X1, _c->x1);

    // delay the upper branch
    memmove(_c->x_prime + k2, _c->x0, k2*sizeof(float complex));

    for (i=0; i<_c->num_channels; i++)
        _x[i] = _c->x_prime[i] + _c->x1[i];

    // finish delay operation
    memmove(_c->x_prime, _c->x0 + k2, k2*sizeof(float complex));
}

void ofdmoqam_analyzer_execute(ofdmoqam _c, float complex * _x, float complex * _X)
{
    unsigned int i;
    unsigned int k2 = (_c->num_channels)/2;

    memmove(_c->x0, _x, (_c->num_channels)*sizeof(float complex));

    // delay the lower branch
    memmove(_c->x_prime + k2, _x, k2*sizeof(float complex));

    // copy delayed lower branch partition
    memmove(_c->x1, _c->x_prime, (_c->num_channels)*sizeof(float complex));

    // finish delay operation
    memmove(_c->x_prime, _x + k2, k2*sizeof(float complex));

    // execute analysis filter banks
    firpfbch_execute(_c->c0, _c->x0, _c->X0);
    firpfbch_execute(_c->c1, _c->x1, _c->X1);

    // re-combine channels, delay upper branch by one symbol
    for (i=0; i<_c->num_channels; i+=2) {
        _X[i]   = crealf(_c->x_tilda[i])
                + cimagf(_c->X1[i])*_Complex_I;

        _X[i+1] = cimagf(_c->x_tilda[i+1])*_Complex_I
                + crealf(_c->X1[i+1]);
    }

    // complete upper-branch delay operation
    memmove(_c->x_tilda, _c->X0, (_c->num_channels)*sizeof(float complex));
}

void ofdmoqam_execute(ofdmoqam _c, float complex * _x, float complex * _y)
{
    if (_c->type == OFDMOQAM_ANALYZER)
        ofdmoqam_analyzer_execute(_c,_x,_y);
    else
        ofdmoqam_synthesizer_execute(_c,_x,_y);
}

