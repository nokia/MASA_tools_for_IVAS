/*--------------------------------------------------------------------------------*
 * MASA analyzer                                                                  *
 * ----------------------------------                                             *
 * (C) 2024 Nokia Technologies Ltd.. See LICENSE.md for license.                  *
 *                                                                                *
 *--------------------------------------------------------------------------------*/

#include <stdint.h>
#include <assert.h>
#include "sphericalIndexing_nokia.h"

#define MASA_NO_INDEX            32767
#define NO_POINTS_EQUATOR        430
#define ANGLE_AT_EQUATOR         0.012894427382667f
#define ANGLE_AT_EQUATOR_DEG     0.738796268264740f
#define INV_ANGLE_AT_EQUATOR_DEG 1.353553128183453f

void generateSphericalGrid_nokia(SphericalGridData_nokia* data)
{
    short i;
    int32_t* n;

    int16_t n1[122];
    int16_t n_plus[] = { 18,    29,    45,    61,    71,    85,    88,    92,    95,   107 };
    int16_t n_minus[] = { 15,    19,    62,    65,    83,    89,    91,    98,   106,   109 };


    n = data->no_phi;
    n1[0] = NO_POINTS_EQUATOR;
    for (i = 1; i < SPH_CB_SIZE_16BIT - 1; i++)
    {
        n1[i] = (int16_t)roundf(422.546f * cosf(ANGLE_AT_EQUATOR * i));
    }

    for (i = 0; i < 10; i++)
    {
        n1[n_plus[i]] += 1;
        n1[n_minus[i]] -= 1;
    }

    n[0] = n1[0];
    for (i = 1; i < SPH_CB_SIZE_16BIT - 1; i++)
    {
        n[i] = n[i - 1] + 2 * n1[i]; /* number of azimuth points for the current elevation codeword */
    }

    n[i] = n[i - 1] + 2;
    data->no_theta = i + 1;

}

void deindexDirection_nokia(
    unsigned short sphIndex, /* (i) - Spherical index */
    const SphericalGridData_nokia* data, /* (i) - Prepared spherical grid */
    float* theta,                  /* (o) - Elevation */
    float* phi                     /* (o) - Azimuth */
)
{

    unsigned short id_th = 0;
    short sign_theta = 1;
    short id_phi = 0;
    short no_th = data->no_theta;
    const int32_t* n = data->no_phi;


    float ba[2] = { 1.698533296919981e+02f,  1.223976966547744e+02f }; /* -b/2a*/
    float del[2] = { 8.667598146620304e+05f, 1.364579281970680e+06f }; /* delta*/
    float div[2] = { -0.181726144905283f,  -0.096108553968620f }; /* 1/2a */
    float a4[2] = { -11.005571053314377f,  -20.809802222734728f }; /* 4a */
    float ba_crt, del_crt, div_crt, a4_crt;
    float estim;
    int base_low, base_up;
    int cnt = 0;

    int16_t n_crt;

    if (sphIndex >= 57300)
    {
        ba_crt = ba[1];
        div_crt = div[1];
        a4_crt = a4[1];
        del_crt = del[1];
    }
    else
    {
        ba_crt = ba[0];
        div_crt = div[0];
        a4_crt = a4[0];
        del_crt = del[0];
    }
    estim = ba_crt + div_crt * sqrtf(del_crt + a4_crt * sphIndex);

    if (estim > SPH_CB_SIZE_16BIT - 1)
    {
        estim = SPH_CB_SIZE_16BIT - 1;
    }

    assert(estim > 0);
    id_th = (short)roundf(estim);
    if (id_th < 0)
    {
        id_th = 0;
    }

    if (id_th == 0)
    {
        base_low = 0;
        base_up = n[0];
        n_crt = base_up;
    }
    else
    {
        base_low = n[id_th - 1];
        base_up = n[id_th];
        n_crt = (base_up - base_low) >> 1;
    }

    sign_theta = 1;
    id_phi = 0;


    if (sphIndex < base_low)
    {
        id_th--;

        if (id_th == 0)
        {
            base_low = 0;
            base_up = n[0];
            n_crt = base_up;
        }
        else
        {
            base_up = base_low;
            base_low = n[id_th - 1];
            n_crt = (base_up - base_low) >> 1;
        }

        assert(sphIndex >= base_low);
    }
    else if (sphIndex >= base_up)
    {
        id_th++;
        base_low = base_up;
        base_up = n[id_th];
        n_crt = (base_up - base_low) >> 1;
        assert(sphIndex < base_up);
    }

    id_phi = sphIndex - base_low;
    if (sphIndex - base_low >= n_crt)
    {
        id_phi -= n_crt;
        sign_theta = -1;
    }

    if (id_th == 0)
    {
        *theta = 0.f;
        *phi = (float)sphIndex * 360 / (float)n_crt - 180;
    }
    else
    {
        if (id_th == no_th - 1)
        {
            id_phi = 0;
            *phi = -180;
            *theta = 90 * (float)sign_theta;
        }
        else
        {
            *theta = id_th * ANGLE_AT_EQUATOR_DEG * (float)sign_theta;
            if (id_th % 2 == 0)
            {
                *phi = (float)id_phi * 360 / (float)n_crt - 180;
            }
            else
            {
                *phi = ((float)id_phi + 0.5f) * 360 / (float)n_crt - 180;
            }
        }
    }


}

void indexDirection_nokia(
    uint16_t* sphIndex,                  /* (o) - output index for direction */
    float theta,                         /* (i) - input elevation to be indexed */
    float phi,                           /* (i) - input azimuth to be indexed  */
    const SphericalGridData_nokia* gridData    /* (i) - generated grid data */
)
{
    float abs_theta,  phi_q;
    short sign_th, id_phi, id_th;
    int idx_sph;
    int cum_n_pos, cum_n_neg;


    phi = phi + 180;
    if (theta < 0)
    {
        abs_theta = -theta;
        sign_th = -1;
    }
    else
    {
        abs_theta = theta;
        sign_th = 1;
    }

    quantize_theta_phi_nokia(gridData->no_theta, gridData->no_phi, abs_theta, &id_phi,
                             phi, &id_th, &phi_q);


    /* Starting from Equator, alternating positive and negative */

    if (id_th > 0)
    {
        cum_n_pos = gridData->no_phi[id_th - 1];
        cum_n_neg = cum_n_pos + ((gridData->no_phi[id_th] - gridData->no_phi[id_th - 1]) >> 1);
    }
    else
    {
        cum_n_pos = 0;
        cum_n_neg = 0;
    }


    if (id_th == 0)
    {
        idx_sph = id_phi;
    }
    else
    {
        if (id_th == gridData->no_theta - 1)
        {
            id_phi = 0;
        }

        if (sign_th > 0)
        {
            idx_sph = cum_n_pos + id_phi;
        }
        else
        {
            idx_sph = cum_n_neg + id_phi;
        }


    }


    *sphIndex = idx_sph;

}

int16_t quantize_theta_nokia( /* o  : index of quantized value */
    float x,            /* i  : theta value to be quantized */
    int16_t no_cb,      /* i  : number of codewords */
    float *xhat         /* o  : quantized value */
)
{
    int16_t imin;

    float diff1, diff2;


    imin = (int16_t) (x * INV_ANGLE_AT_EQUATOR_DEG + 0.5f);

    if (imin >= no_cb - 1)
    {
        imin = no_cb - 1;
        diff1 = x - 90;
        diff2 = x - ANGLE_AT_EQUATOR_DEG * (imin - 1);
        if (fabsf(diff1) > fabsf(diff2))
        {
            imin--;
            *xhat = imin * ANGLE_AT_EQUATOR_DEG;
        }
        else
        {
            *xhat = 90;
        }
    }
    else
    {
        *xhat = imin * ANGLE_AT_EQUATOR_DEG;
    }

    return imin;
}


void quantize_theta_phi_nokia(
    int16_t no_th,       /* i  : elevation codebook size */
    const int32_t* no_phi_loc, /* i  : number of azimuth values for each elevation codeword */
    float abs_theta,     /* i  : absolute value of elevation to be quantized */
    int16_t *id_phi,           /* o  : azimuth index */
    float phi,           /* i  : input azimuth value; to be quantized  */
    int16_t *id_theta,         /* o  : elevation index */
    float *phi_q               /* o  : rotated quantized azimuth */
)
{
    float theta_hat, theta_hat1, phi_hat, phi_hat1;
    int16_t id_th, id_th1, id_th2, id_ph, id_ph1;
    float d, d1;
    float diff1, diff2;
    uint16_t no_phi;

    theta_hat = 0; /* to remove warning */
    id_th = quantize_theta_nokia(abs_theta, no_th, &theta_hat);
    if (no_th > 1)
    {
        if (id_th == 0)
        {
            id_th1 = 1;
        }
        else if (id_th == no_th - 1)
        {
            id_th1 = no_th - 2;
        }
        else
        {
            id_th1 = id_th - 1;
            id_th2 = id_th + 1;

            diff1 = abs_theta - id_th1 * ANGLE_AT_EQUATOR_DEG;
            diff2 = abs_theta - ANGLE_AT_EQUATOR_DEG * id_th2;
            if (id_th2 == no_th - 1)
            {
                diff2 = abs_theta - 90;
            }
            if (fabsf(diff1) > fabsf(diff2))
            {
                id_th1 = id_th2;
            }
        }

        theta_hat1 = ANGLE_AT_EQUATOR_DEG * id_th1;

        if (id_th1 == no_th - 1)
        {
            theta_hat1 = 90;
        }


        if (no_phi_loc[id_th] > 1)
        {
            if (id_th > 0)
            {
                no_phi = ((no_phi_loc[id_th] - no_phi_loc[id_th - 1]) >> 1);
            }
            else
            {
                no_phi = no_phi_loc[id_th];
            }
            id_ph = quantize_phi_nokia(phi, (id_th % 2 == 1), &phi_hat, no_phi);
        }
        else
        {
            id_ph = MASA_NO_INDEX;
            phi_hat = 180;
            *phi_q = 0;
        }
        if ((no_phi_loc[id_th1] > 1) && (id_ph < MASA_NO_INDEX))
        {
            if (id_th1 > 0)
            {
                no_phi = ((no_phi_loc[id_th1] - no_phi_loc[id_th1 - 1]) >> 1);
            }
            else
            {
                no_phi = no_phi_loc[id_th1];
            }
            id_ph1 = quantize_phi_nokia(phi, (id_th1 % 2 == 1), &phi_hat1, no_phi);
            d = direction_distance_nokia(abs_theta, theta_hat, phi, phi_hat);
            d1 = direction_distance_nokia(abs_theta, theta_hat1, phi, phi_hat1);
            if (d1 > d)
            {
                id_ph = id_ph1;
                id_th = id_th1; /*theta_hat = ANGLE_AT_EQUATOR_DEG * id_th1; */
            }
        }
    }
    else
    {
        if (id_th > 0)
        {
            no_phi = ((no_phi_loc[id_th] - no_phi_loc[id_th - 1]) >> 1);
        }
        else
        {
            no_phi = no_phi_loc[id_th];
        }
        id_ph = quantize_phi_nokia(phi, (id_th % 2 == 1), &phi_hat, no_phi);
    }

    *id_phi = id_ph;
    *id_theta = id_th;
}


int16_t quantize_phi_nokia(   /* o  : index azimuth */
    float phi,          /* i  : azimuth value */
    int16_t flag_delta, /* i  : flag indicating if the azimuth codebook is translated or not */
    float *phi_hat,     /* o  : quantized azimuth */
    int16_t n     /* i  : azimuth codebook size */
)
{
    int16_t id_phi;
    float dd;
    float delta_phi;

    delta_phi = 360.0f / (float) n;

    if (n == 1)
    {
        *phi_hat = 0;

        return 0;
    }

    if (flag_delta == 1)
    {
        dd = delta_phi / 2.0f;
    }
    else
    {
        dd = 0;
    }

    id_phi = (int16_t) ((phi - dd + delta_phi / 2.0f) / (float) delta_phi);

    if (id_phi == n)
    {
        id_phi = 0;
    }

    if (id_phi == -1)
    {
        id_phi = n - 1;
    }

    *phi_hat = id_phi * delta_phi + dd;

    return id_phi;
}

float direction_distance_nokia(    /* o  : distortion value */
    float theta,             /* i  : elevation absolute value */
    float theta_hat,         /* i  : quantized elevation value in absolute value */
    const float phi,         /* i  : azimuth value */
    const float phi_hat      /* i  : quantized azimuth value */
)
{
    float d;

    theta *= MASA_PI / 180;
    theta_hat *= MASA_PI / 180;

    d = sinf(theta) * sinf(theta_hat) + cosf(theta) * cosf(theta_hat) * cosf((phi - phi_hat) * MASA_PI / 180);

    return d;
}
