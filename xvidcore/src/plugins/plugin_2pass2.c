/******************************************************************************
 *
 * XviD Bit Rate Controller Library
 * - VBR 2 pass bitrate controler implementation -
 *
 * Copyright (C)      2002 Foxer <email?>
 *                    2002 Dirk Knop <dknop@gwdg.de>
 *               2002-2003 Edouard Gomez <ed.gomez@free.fr>
 *                    2003 Pete Ross <pross@xvid.org>
 *
 * This curve treatment algorithm is the one originally implemented by Foxer
 * and tuned by Dirk Knop for the XviD vfw frontend.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: plugin_2pass2.c,v 1.1.2.7 2003-05-22 10:57:33 edgomez Exp $
 *
 *****************************************************************************/

#include <stdio.h>
#include <math.h>

#define RAD2DEG 57.295779513082320876798154814105
#define DEG2RAD 0.017453292519943295769236907684886

#include "../xvid.h"
#include "../image/image.h"

typedef struct {
    int type;               /* first pass type */
    int quant;              /* first pass quant */
	int blks[3];			/* k,m,y blks */
    int length;             /* first pass length */
    int scaled_length;      /* scaled length */
    int desired_length;     /* desired length; calcuated during encoding */

    int zone_mode;   /* XVID_ZONE_xxx */
    double weight;
} stat_t;




/* context struct */
typedef struct
{
    xvid_plugin_2pass2_t param;

    /* constant statistical data */
	int num_frames;
    int num_keyframes;
    uint64_t target;	/* target filesize */
	
    int count[3];   /* count of each frame types */
    uint64_t tot_length[3];  /* total length of each frame types */
    double avg_length[3];   /* avg */
    int min_length[3];  /* min frame length of each frame types */
    uint64_t tot_scaled_length[3];  /* total scaled length of each frame type */
    int max_length;     /* max frame size */

    /* zone statistical data */
    double avg_weight;  /* average weight */
    int64_t tot_quant;   /* total length used by XVID_ZONE_QUANT zones */


    double curve_comp_scale;
    double movie_curve;

	double alt_curve_low;
	double alt_curve_high;
	double alt_curve_low_diff;
	double alt_curve_high_diff;
    double alt_curve_curve_bias_bonus;
	double alt_curve_mid_qual;
	double alt_curve_qual_dev;

    /* dynamic */

    int * keyframe_locations;
    stat_t * stats;
    
    double pquant_error[32];
    double bquant_error[32];
    int quant_count[32];
    int last_quant[3];

    double curve_comp_error;
    int overflow;
    int KFoverflow;
    int KFoverflow_partial;
    int KF_idx;

    double fq_error;
} rc_2pass2_t;



#define BUF_SZ 1024
#define MAX_COLS    5


/* open stats file, and count num frames */

static int det_stats_length(rc_2pass2_t * rc, char * filename)
{
    FILE * f;
    int n, ignore;
    char type;

    rc->num_frames = 0;
    rc->num_keyframes = 0;

    if ((f = fopen(filename, "rt")) == NULL)
        return 0;

    while((n = fscanf(f, "%c %d %d %d %d %d %d\n",
        &type, &ignore, &ignore, &ignore, &ignore, &ignore, &ignore)) != EOF) {
        if (type == 'i') {
            rc->num_frames++;
            rc->num_keyframes++;
        }else if (type == 'p' || type == 'b' || type == 's') {
            rc->num_frames++;
        }
    }

    fclose(f);

    return 1;
}



/* open stats file(s) and read into rc->stats array */

static int load_stats(rc_2pass2_t *rc, char * filename)
{
    FILE * f;
    int i, not_scaled;
 

    if ((f = fopen(filename, "rt"))==NULL)
        return 0;
    
    i = 0;
	not_scaled = 0;
    while(i < rc->num_frames) {
        stat_t * s = &rc->stats[i];
        int n;
        char type;

		s->scaled_length = 0;
        n = fscanf(f, "%c %d %d %d %d %d %d\n", &type, &s->quant, &s->blks[0], &s->blks[1], &s->blks[2], &s->length, &s->scaled_length);
        if (n == EOF) break;
		if (n < 7) {
			not_scaled = 1;
		}

        if (type == 'i') {
            s->type = XVID_TYPE_IVOP;
        }else if (type == 'p' || type == 's') {
            s->type = XVID_TYPE_PVOP;
        }else if (type == 'b') {
            s->type = XVID_TYPE_BVOP;
        }else{  /* unknown type */
            DPRINTF(XVID_DEBUG_RC, "unknown stats frame type; assuming pvop");
            s->type = XVID_TYPE_PVOP;
        }

        i++;
    }

    rc->num_frames = i;

	fclose(f);

    return 1;
}



#if 0
static void print_stats(rc_2pass2_t * rc)
{
    int i;
    for (i = 0; i < rc->num_frames; i++) {
        stat_t * s = &rc->stats[i];
        DPRINTF(XVID_DEBUG_RC, "%i %i %i %i\n", s->type, s->quant, s->length, s->scaled_length);
    }
}
#endif

/* pre-process the statistics data 
    - for each type, count, tot_length, min_length, max_length
    - set keyframes_locations
*/

void pre_process0(rc_2pass2_t * rc)
{
    int i,j;

    for (i=0; i<3; i++) {
        rc->count[i]=0;
        rc->tot_length[i] = 0;
        rc->last_quant[i] = 0;
    }

    for (i=j=0; i<rc->num_frames; i++) {
        stat_t * s = &rc->stats[i];

        rc->count[s->type-1]++;
        rc->tot_length[s->type-1] += s->length;
       
        if (i == 0 || s->length < rc->min_length[s->type-1]) {
            rc->min_length[s->type-1] = s->length;
        }

        if (i == 0 || s->length > rc->max_length) {
            rc->max_length = s->length;
        }

        if (s->type == XVID_TYPE_IVOP) {
            rc->keyframe_locations[j] = i;
            j++;
        }
    }

	/*
	 * The "per sequence" overflow system considers a natural sequence to be
	 * formed by all frames between two iframes, so if we want to make sure
	 * the system does not go nuts during last sequence, we force the last
	 * frame to appear in the keyframe locations array.
	 */
    rc->keyframe_locations[j] = i;
}

    
/* calculate zone weight "center" */

static void zone_process(rc_2pass2_t *rc, const xvid_plg_create_t * create)
{
    int i,j;
    int n = 0;

    rc->avg_weight = 0.0;
    rc->tot_quant = 0;


    if (create->num_zones == 0) {
        for (j = 0; j < rc->num_frames; j++) {
            rc->stats[j].zone_mode = XVID_ZONE_WEIGHT;
            rc->stats[j].weight = 1.0;
        }
        rc->avg_weight += rc->num_frames * 1.0;
        n += rc->num_frames;
    }


    for(i=0; i < create->num_zones; i++) {
        
        int next = (i+1<create->num_zones) ? create->zones[i+1].frame : rc->num_frames;

        if (i==0 && create->zones[i].frame > 0) {
            for (j = 0; j < create->zones[i].frame && j < rc->num_frames; j++) {
                rc->stats[j].zone_mode = XVID_ZONE_WEIGHT;
                rc->stats[j].weight = 1.0;
            }
            rc->avg_weight += create->zones[i].frame * 1.0;
            n += create->zones[i].frame;
        }

        if (create->zones[i].mode == XVID_ZONE_WEIGHT) {
            for (j = create->zones[i].frame; j < next && j < rc->num_frames; j++ ) {
                rc->stats[j].zone_mode = XVID_ZONE_WEIGHT;
                rc->stats[j].weight = (double)create->zones[i].increment / (double)create->zones[i].base;
            }
            next -= create->zones[i].frame;
            rc->avg_weight += (double)(next * create->zones[i].increment) / (double)create->zones[i].base;
            n += next;
        }else{  // XVID_ZONE_QUANT
            for (j = create->zones[i].frame; j < next && j < rc->num_frames; j++ ) {
                rc->stats[j].zone_mode = XVID_ZONE_QUANT;
                rc->stats[j].weight = (double)create->zones[i].increment / (double)create->zones[i].base;
                rc->tot_quant += rc->stats[j].length;
            }
        }
    }
    rc->avg_weight = n>0 ? rc->avg_weight/n : 1.0;

    DPRINTF(XVID_DEBUG_RC, "center_weight: %f (for %i frames);   fixed_bytes: %i\n", rc->avg_weight, n, rc->tot_quant);
}


/* scale the curve */

static void internal_scale(rc_2pass2_t *rc)
{
	int64_t target  = rc->target - rc->tot_quant;
	int64_t pass1_length = rc->tot_length[0] + rc->tot_length[1] + rc->tot_length[2] - rc->tot_quant;
	int min_size[3];
	double scaler;
	int i;


	/* perform an initial scale pass.
	   if a frame size is scaled underneath our hardcoded minimums, then we force the
	   frame size to the minimum, and deduct the original & scaled frmae length from the
	   original and target total lengths */

	min_size[0] = ((rc->stats[0].blks[0]*22) + 240) / 8;
	min_size[1] = (rc->stats[0].blks[0] + 88) / 8;
	min_size[2] = 8;

	scaler = (double)target / (double)pass1_length;

	if (target <= 0 || pass1_length <= 0 || target >= pass1_length) {
		DPRINTF(XVID_DEBUG_RC, "undersize warning\n");
        scaler = 1.0;
	}

    DPRINTF(XVID_DEBUG_RC, "target=%i, tot_length=%i, scaler=%f\n", (int)target, (int)pass1_length, scaler);

	for (i=0; i<rc->num_frames; i++) {
		stat_t * s = &rc->stats[i];
		int len;

        if (s->zone_mode == XVID_ZONE_QUANT) {
            s->scaled_length = s->length;
        }else {
		    len = (int)((double)s->length * scaler * s->weight / rc->avg_weight);
		    if (len < min_size[s->type-1]) {		/* force frame size */
			    s->scaled_length = min_size[s->type-1];
			    target -= s->scaled_length;
			    pass1_length -= s->length;
		    }else{
			    s->scaled_length = 0;
		    }
        }
	}

    scaler = (double)target / (double)pass1_length;
    if (target <= 0 || pass1_length <= 0 || target >= pass1_length) {
		DPRINTF(XVID_DEBUG_RC,"undersize warning\n");
		scaler = 1.0;
	}

	DPRINTF(XVID_DEBUG_RC, "target=%i, tot_length=%i, scaler=%f\n", (int)target, (int)pass1_length, scaler);

	for (i=0; i<rc->num_frames; i++) {
		stat_t * s = &rc->stats[i];

		if (s->scaled_length==0) {	/* ignore frame with forced frame sizes */
			s->scaled_length = (int)((double)s->length * scaler * s->weight / rc->avg_weight);
		}
	}
}




void pre_process1(rc_2pass2_t * rc)
{
    int i;
    double total1, total2;
    uint64_t ivop_boost_total;

    ivop_boost_total = 0;
    rc->curve_comp_error = 0;

    for (i=0; i<3; i++) {
        rc->tot_scaled_length[i] = 0;
    }

    for (i=0; i<rc->num_frames; i++) {
        stat_t * s = &rc->stats[i];

        rc->tot_scaled_length[s->type-1] += s->scaled_length;
       
        if (s->type == XVID_TYPE_IVOP) {
            ivop_boost_total += s->scaled_length * rc->param.keyframe_boost / 100;
        }
    }

    rc->movie_curve = ((double)(rc->tot_scaled_length[XVID_TYPE_PVOP-1] + rc->tot_scaled_length[XVID_TYPE_BVOP-1] + ivop_boost_total) /
			                (rc->tot_scaled_length[XVID_TYPE_PVOP-1] + rc->tot_scaled_length[XVID_TYPE_BVOP-1]));

    for(i=0; i<3; i++) {
        if (rc->count[i] == 0 || rc->movie_curve == 0) {
            rc->avg_length[i] = 1;
        }else{
            rc->avg_length[i] = rc->tot_scaled_length[i] / rc->count[i] / rc->movie_curve;
        }
    }

    /* alt curve stuff here */

    if (rc->param.use_alt_curve) {
        const double avg_pvop = rc->avg_length[XVID_TYPE_PVOP-1];
        const uint64_t tot_pvop = rc->tot_length[XVID_TYPE_PVOP-1];
        const uint64_t tot_bvop = rc->tot_length[XVID_TYPE_BVOP-1];
        const uint64_t tot_scaled_pvop = rc->tot_scaled_length[XVID_TYPE_PVOP-1];
        const uint64_t tot_scaled_bvop = rc->tot_scaled_length[XVID_TYPE_BVOP-1];

		rc->alt_curve_low = avg_pvop - avg_pvop * (double)rc->param.alt_curve_low_dist / 100.0;
		rc->alt_curve_low_diff = avg_pvop - rc->alt_curve_low;
		rc->alt_curve_high = avg_pvop + avg_pvop * (double)rc->param.alt_curve_high_dist / 100.0;
		rc->alt_curve_high_diff = rc->alt_curve_high - avg_pvop;

        if (rc->param.alt_curve_use_auto) {
            if (tot_bvop + tot_pvop > tot_scaled_bvop + tot_scaled_pvop) {
				rc->param.alt_curve_min_rel_qual = (int)(100.0 - (100.0 - 100.0 /
					((double)(tot_pvop + tot_bvop) / (double)(tot_scaled_pvop + tot_scaled_bvop))) * (double)rc->param.alt_curve_auto_str / 100.0);

				if (rc->param.alt_curve_min_rel_qual < 20)
					rc->param.alt_curve_min_rel_qual = 20;
            }else{
				rc->param.alt_curve_min_rel_qual = 100;
            }
        }
		rc->alt_curve_mid_qual = (1.0 + (double)rc->param.alt_curve_min_rel_qual / 100.0) / 2.0;
		rc->alt_curve_qual_dev = 1.0 - rc->alt_curve_mid_qual;
			
        if (rc->param.alt_curve_low_dist > 100) {
			switch(rc->param.alt_curve_type) {
            case XVID_CURVE_SINE: // Sine Curve (high aggressiveness)
				rc->alt_curve_qual_dev *= 2.0 / (1.0 + sin(DEG2RAD * (avg_pvop * 90.0 / rc->alt_curve_low_diff)));
				rc->alt_curve_mid_qual = 1.0 - rc->alt_curve_qual_dev * sin(DEG2RAD * (avg_pvop * 90.0 / rc->alt_curve_low_diff));
				break;
			case XVID_CURVE_LINEAR: // Linear (medium aggressiveness)
				rc->alt_curve_qual_dev *= 2.0 / (1.0 + avg_pvop / rc->alt_curve_low_diff);
				rc->alt_curve_mid_qual = 1.0 - rc->alt_curve_qual_dev * avg_pvop / rc->alt_curve_low_diff;
				break;
			case XVID_CURVE_COSINE: // Cosine Curve (low aggressiveness)
				rc->alt_curve_qual_dev *= 2.0 / (1.0 + (1.0 - cos(DEG2RAD * (avg_pvop * 90.0 / rc->alt_curve_low_diff))));
				rc->alt_curve_mid_qual = 1.0 - rc->alt_curve_qual_dev * (1.0 - cos(DEG2RAD * (avg_pvop * 90.0 / rc->alt_curve_low_diff)));
			}
		}
    }
    /* --- */


    total1=total2=0;
    for (i=0; i<rc->num_frames; i++) {
        stat_t * s = &rc->stats[i];

        if (s->type != XVID_TYPE_IVOP) {
            double dbytes,dbytes2;

            dbytes = s->scaled_length / rc->movie_curve;
            dbytes2 = 0; /* XXX: warning */
            total1 += dbytes;
            if (s->type == XVID_TYPE_BVOP)
                dbytes *= rc->avg_length[XVID_TYPE_PVOP-1] / rc->avg_length[XVID_TYPE_BVOP-1];

            if (rc->param.use_alt_curve) {
                if (dbytes > rc->avg_length[XVID_TYPE_PVOP-1]) {

                    if (dbytes >= rc->alt_curve_high) {
						dbytes2 = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev);
                    }else{
						switch(rc->param.alt_curve_type) {
                        case XVID_CURVE_SINE :
						    dbytes2 = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * sin(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_high_diff)));
							break;
                        case XVID_CURVE_LINEAR :
						    dbytes2 = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * (dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) / rc->alt_curve_high_diff);
							break;
						case XVID_CURVE_COSINE :
						    dbytes2 = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * (1.0 - cos(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_high_diff))));
						}
					}
                }else{
                    if (dbytes <= rc->alt_curve_low) {
						dbytes2 = dbytes;
                    }else{
						switch(rc->param.alt_curve_type) {
						case XVID_CURVE_SINE :
						    dbytes2 = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * sin(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_low_diff)));
							break;
						case XVID_CURVE_LINEAR :
						    dbytes2 = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * (dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) / rc->alt_curve_low_diff);
							break;
						case XVID_CURVE_COSINE :
						    dbytes2 = dbytes * (rc->alt_curve_mid_qual + rc->alt_curve_qual_dev * (1.0 - cos(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_low_diff))));
						}
					}

                }


            }else{
                if (dbytes > rc->avg_length[XVID_TYPE_PVOP-1]) {
                    dbytes2=((double)dbytes + (rc->avg_length[XVID_TYPE_PVOP-1] - dbytes) * rc->param.curve_compression_high / 100.0);
                }else{
			        dbytes2 = ((double)dbytes + (rc->avg_length[XVID_TYPE_PVOP-1] - dbytes) * rc->param.curve_compression_low / 100.0);
                }
            }

            if (s->type == XVID_TYPE_BVOP) {
			    dbytes2 *= rc->avg_length[XVID_TYPE_BVOP-1] / rc->avg_length[XVID_TYPE_PVOP-1];
			    if (dbytes2 < rc->min_length[XVID_TYPE_BVOP-1])
				    dbytes2 = rc->min_length[XVID_TYPE_BVOP-1];
            }else{
			    if (dbytes2 < rc->min_length[XVID_TYPE_PVOP-1])
				    dbytes2 = rc->min_length[XVID_TYPE_PVOP-1];
            }
            total2 += dbytes2;
        }
    }

    rc->curve_comp_scale = total1 / total2;

    if (!rc->param.use_alt_curve) {
        DPRINTF(XVID_DEBUG_RC, "middle frame size for asymmetric curve compression: %i\n",
            (int)(rc->avg_length[XVID_TYPE_PVOP-1] * rc->curve_comp_scale));
    }

    if (rc->param.use_alt_curve) {
        int bonus_bias = rc->param.alt_curve_bonus_bias;
        int oldquant = 1;

	    if (rc->param.alt_curve_use_auto_bonus_bias)
		    bonus_bias = rc->param.alt_curve_min_rel_qual;

	    rc->alt_curve_curve_bias_bonus = (total1 - total2) * (double)bonus_bias / 100.0 / (double)(rc->num_frames /* - credits_frames */ - rc->num_keyframes);
	    rc->curve_comp_scale = ((total1 - total2) * (1.0 - (double)bonus_bias / 100.0) + total2) / total2;


        /* special info for alt curve:  bias bonus and quantizer thresholds */

		DPRINTF(XVID_DEBUG_RC, "avg scaled framesize:%i", (int)rc->avg_length[XVID_TYPE_PVOP-1]);
		DPRINTF(XVID_DEBUG_RC, "bias bonus:%i bytes", (int)rc->alt_curve_curve_bias_bonus);

		for (i=1; i <= (int)(rc->alt_curve_high*2)+1; i++) {
            double curve_temp, dbytes;
            int newquant;

            dbytes = i;
			if (dbytes > rc->avg_length[XVID_TYPE_PVOP-1]) {
                if (dbytes >= rc->alt_curve_high) {
					curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev);
                }else{
					switch(rc->param.alt_curve_type)
					{
					case XVID_CURVE_SINE :
						curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * sin(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_high_diff)));
						break;
					case XVID_CURVE_LINEAR :
						curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * (dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) / rc->alt_curve_high_diff);
						break;
					case XVID_CURVE_COSINE :
						curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * (1.0 - cos(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_high_diff))));
					}
				}
			}else{
                if (dbytes <= rc->alt_curve_low) {
					curve_temp = dbytes;
                }else{
					switch(rc->param.alt_curve_type)
					{
					case XVID_CURVE_SINE :
						curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * sin(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_low_diff)));
						break;
					case XVID_CURVE_LINEAR :
						curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * (dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) / rc->alt_curve_low_diff);
						break;
					case XVID_CURVE_COSINE :
						curve_temp = dbytes * (rc->alt_curve_mid_qual + rc->alt_curve_qual_dev * (1.0 - cos(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_low_diff))));
					}
				}
			}

			if (rc->movie_curve > 1.0)
				dbytes *= rc->movie_curve;

			newquant = (int)(dbytes * 2.0 / (curve_temp * rc->curve_comp_scale + rc->alt_curve_curve_bias_bonus));
			if (newquant > 1) {
				if (newquant != oldquant) {
                    int percent = (int)((i - rc->avg_length[XVID_TYPE_PVOP-1]) * 100.0 / rc->avg_length[XVID_TYPE_PVOP-1]);
					oldquant = newquant;
					DPRINTF(XVID_DEBUG_RC, "quant:%i threshold at %i : %i percent", newquant, i, percent);
				}
			}
		}

    }
   
    rc->overflow = 0;
    rc->KFoverflow = 0;
    rc->KFoverflow_partial = 0;
    rc->KF_idx = 1;
}




static int rc_2pass2_create(xvid_plg_create_t * create, rc_2pass2_t ** handle)
{
    xvid_plugin_2pass2_t * param = (xvid_plugin_2pass2_t *)create->param;
    rc_2pass2_t * rc;
    int i;

    rc = malloc(sizeof(rc_2pass2_t));
    if (rc == NULL) 
        return XVID_ERR_MEMORY;

    rc->param = *param;

    if (rc->param.keyframe_boost <= 0) rc->param.keyframe_boost = 0;
    if (rc->param.payback_method <= 0) rc->param.payback_method = XVID_PAYBACK_PROP;
    if (rc->param.bitrate_payback_delay <= 0) rc->param.bitrate_payback_delay = 250;
    if (rc->param.curve_compression_high <= 0) rc->param.curve_compression_high = 0;
    if (rc->param.curve_compression_low <= 0) rc->param.curve_compression_low = 0;
    if (rc->param.max_overflow_improvement <= 0) rc->param.max_overflow_improvement = 60;
    if (rc->param.max_overflow_degradation <= 0) rc->param.max_overflow_degradation = 60;

    if (rc->param.use_alt_curve <= 0) rc->param.use_alt_curve = 0;
    if (rc->param.alt_curve_high_dist <= 0) rc->param.alt_curve_high_dist = 500;
    if (rc->param.alt_curve_low_dist <= 0) rc->param.alt_curve_low_dist = 90;
    if (rc->param.alt_curve_use_auto <= 0) rc->param.alt_curve_use_auto = 1;
    if (rc->param.alt_curve_auto_str <= 0) rc->param.alt_curve_auto_str = 30;
    if (rc->param.alt_curve_type <= 0) rc->param.alt_curve_type = XVID_CURVE_LINEAR;
    if (rc->param.alt_curve_min_rel_qual <= 0) rc->param.alt_curve_min_rel_qual = 50;
    if (rc->param.alt_curve_use_auto_bonus_bias <= 0) rc->param.alt_curve_use_auto_bonus_bias = 1;
    if (rc->param.alt_curve_bonus_bias <= 0) rc->param.alt_curve_bonus_bias = 50;

    if (rc->param.kftreshold <= 0) rc->param.kftreshold = 10;
    if (rc->param.kfreduction <= 0) rc->param.kfreduction = 20;
    if (rc->param.min_key_interval <= 0) rc->param.min_key_interval = 300;

    if (!det_stats_length(rc, param->filename)){
        DPRINTF(XVID_DEBUG_RC,"fopen %s failed\n", param->filename);
        free(rc);
        return XVID_ERR_FAIL;
    }

    if ((rc->stats = malloc(rc->num_frames * sizeof(stat_t))) == NULL) {
        free(rc);
        return XVID_ERR_MEMORY;
    }

    /*
	 * We need an extra location because we do as if the last frame were an
	 * IFrame. This is needed because our code consider that frames between
	 * 2 IFrames form a natural sequence. So we store last frame as a
	 * keyframe location.
	 */
    if ((rc->keyframe_locations = malloc((rc->num_keyframes + 1) * sizeof(int))) == NULL) {
        free(rc->stats);
        free(rc);
        return XVID_ERR_MEMORY;
    }

    if (!load_stats(rc, param->filename)) {
        DPRINTF(XVID_DEBUG_RC,"fopen %s failed\n", param->filename);
        free(rc->keyframe_locations);
        free(rc->stats);
        free(rc);
        return XVID_ERR_FAIL;
    }

    /* pre-process our stats */

	if (rc->num_frames  < create->fbase/create->fincr) {
		rc->target = rc->param.bitrate / 8;	/* one second */
	}else{
		rc->target = (rc->param.bitrate * rc->num_frames * create->fincr) / (create->fbase * 8);
	}

    DPRINTF(XVID_DEBUG_RC, "rc->target : %i\n", rc->target);

#if 0
	rc->target -= rc->num_frames*24;	/* avi file header */
#endif
    

	pre_process0(rc);

	if (rc->param.bitrate) {
        zone_process(rc, create);
		internal_scale(rc);
    }else{
        /* external scaler: ignore zone */
        for (i=0;i<rc->num_frames;i++) {
            rc->stats[i].zone_mode = XVID_ZONE_WEIGHT;
            rc->stats[i].weight = 1.0;
        }
        rc->avg_weight = 1.0;
        rc->tot_quant = 0;
    }
	pre_process1(rc);

    for (i=0; i<32;i++) {
        rc->pquant_error[i] = 0;
        rc->bquant_error[i] = 0;
        rc->quant_count[i] = 0;
    }

    rc->fq_error = 0;
    
    *handle = rc;
	return(0);
}


static int rc_2pass2_destroy(rc_2pass2_t * rc, xvid_plg_destroy_t * destroy)
{
    free(rc->keyframe_locations);
    free(rc->stats);
	free(rc);
	return(0);
}



static int rc_2pass2_before(rc_2pass2_t * rc, xvid_plg_data_t * data)
{
    stat_t * s = &rc->stats[data->frame_num];
    int overflow;
    int desired;
    double dbytes;
    double curve_temp;
    int capped_to_max_framesize = 0;

	/*
	 * This function is quite long but easy to understand. In order to simplify
	 * the code path (a bit), we treat 3 cases that can return immediatly.
	 */

	/* First case: Another plugin has already set a quantizer */
    if (data->quant > 0)
		return(0);

	/* Second case: We are in a Quant zone */
	if (s->zone_mode == XVID_ZONE_QUANT) {

		rc->fq_error += s->weight;
		data->quant = (int)rc->fq_error;
		rc->fq_error -= data->quant;
		
		s->desired_length = s->length;

		return(0);

	}

	/* Third case: insufficent stats data */
	if (data->frame_num >= rc->num_frames)
		return 0;

	/*
	 * The last case is the one every normal minded developer should fear to
	 * maintain in a project :-)
	 */

	/* XXX: why by 8 */
	overflow = rc->overflow / 8;

	/*
	 * The rc->overflow field represents the overflow in current scene (between two
	 * IFrames) so we must not forget to reset it if we are enetring a new scene
	 */
	if (s->type == XVID_TYPE_IVOP) {
		overflow = 0;
	}

	desired = s->scaled_length;

	dbytes = desired;
	if (s->type == XVID_TYPE_IVOP) {
		dbytes += desired * rc->param.keyframe_boost / 100;
	}
	dbytes /= rc->movie_curve;

	/*
	 * We are now entering in the hard part of the algo, it was first designed
	 * to work with i/pframes only streams, so the way it computes things is
	 * adapted to pframes only. However we can use it if we just take care to
	 * scale the bframes sizes to pframes sizes using the ratio avg_p/avg_p and
	 * then before really using values depending on frame sizes, scaling the
	 * value again with the inverse ratio
	 */
	if (s->type == XVID_TYPE_BVOP) {
		dbytes *= rc->avg_length[XVID_TYPE_PVOP-1] / rc->avg_length[XVID_TYPE_BVOP-1];
	}

	/*
	 * Apply user's choosen Payback method. Payback helps bitrate to follow the
	 * scaled curve "paying back" past errors in curve previsions.
	 */
	if (rc->param.payback_method == XVID_PAYBACK_BIAS) {
		desired =(int)(rc->curve_comp_error / rc->param.bitrate_payback_delay);
	}else{
		desired = (int)(rc->curve_comp_error * dbytes /
						rc->avg_length[XVID_TYPE_PVOP-1] / rc->param.bitrate_payback_delay);

		if (labs(desired) > fabs(rc->curve_comp_error)) {
			desired = (int)rc->curve_comp_error;
		}
	}

	rc->curve_comp_error -= desired;

	/*
	 * Alt curve treatment is not that hard to understand though the formulas
	 * seem to be huge. Alt treatment is basically a way to soft/harden the
	 * curve flux applying sine/linear/cosine ratios
	 */

	/* XXX: warning */
	curve_temp = 0;

	if (rc->param.use_alt_curve) {
		if (s->type != XVID_TYPE_IVOP)  {
			if (dbytes > rc->avg_length[XVID_TYPE_PVOP-1]) {
				if (dbytes >= rc->alt_curve_high) {
					curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev);
				} else {
					switch(rc->param.alt_curve_type) {
					case XVID_CURVE_SINE :
						curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * sin(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_high_diff)));
						break;
					case XVID_CURVE_LINEAR :
						curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * (dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) / rc->alt_curve_high_diff);
						break;
					case XVID_CURVE_COSINE :
						curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * (1.0 - cos(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_high_diff))));
					}
				}
			} else {
				if (dbytes <= rc->alt_curve_low){
					curve_temp = dbytes;
				} else {
					switch(rc->param.alt_curve_type) {
					case XVID_CURVE_SINE :
						curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * sin(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_low_diff)));
						break;
					case XVID_CURVE_LINEAR :
						curve_temp = dbytes * (rc->alt_curve_mid_qual - rc->alt_curve_qual_dev * (dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) / rc->alt_curve_low_diff);
						break;
					case XVID_CURVE_COSINE :
						curve_temp = dbytes * (rc->alt_curve_mid_qual + rc->alt_curve_qual_dev * (1.0 - cos(DEG2RAD * ((dbytes - rc->avg_length[XVID_TYPE_PVOP-1]) * 90.0 / rc->alt_curve_low_diff))));
					}
				}
			}

			/*
			 * End of code path for curve_temp, as told earlier, we are now
			 * obliged to scale the value to a bframe one using the inverse
			 * ratio applied earlier
			 */
			if (s->type == XVID_TYPE_BVOP)
				curve_temp *= rc->avg_length[XVID_TYPE_BVOP-1] / rc->avg_length[XVID_TYPE_PVOP-1];

			curve_temp = curve_temp * rc->curve_comp_scale + rc->alt_curve_curve_bias_bonus;

			desired += ((int)curve_temp);
			rc->curve_comp_error += curve_temp - (int)curve_temp;
		} else {
			/*
			 * End of code path for dbytes, as told earlier, we are now
			 * obliged to scale the value to a bframe one using the inverse
			 * ratio applied earlier
			 */
			if (s->type == XVID_TYPE_BVOP)
				dbytes *= rc->avg_length[XVID_TYPE_BVOP-1] / rc->avg_length[XVID_TYPE_PVOP-1];

			desired += ((int)dbytes);
			rc->curve_comp_error += dbytes - (int)dbytes;
		}

	} else if ((rc->param.curve_compression_high + rc->param.curve_compression_low) &&	s->type != XVID_TYPE_IVOP) {

		curve_temp = rc->curve_comp_scale;
		if (dbytes > rc->avg_length[XVID_TYPE_PVOP-1]) {
			curve_temp *= ((double)dbytes + (rc->avg_length[XVID_TYPE_PVOP-1] - dbytes) * rc->param.curve_compression_high / 100.0);
		} else {
			curve_temp *= ((double)dbytes + (rc->avg_length[XVID_TYPE_PVOP-1] - dbytes) * rc->param.curve_compression_low / 100.0);
		}

		/*
		 * End of code path for curve_temp, as told earlier, we are now
		 * obliged to scale the value to a bframe one using the inverse
		 * ratio applied earlier
		 */
		if (s->type == XVID_TYPE_BVOP)
			curve_temp *= rc->avg_length[XVID_TYPE_BVOP-1] / rc->avg_length[XVID_TYPE_PVOP-1];

		desired += (int)curve_temp;
		rc->curve_comp_error += curve_temp - (int)curve_temp;
	} else {
		/*
		 * End of code path for dbytes, as told earlier, we are now
		 * obliged to scale the value to a bframe one using the inverse
		 * ratio applied earlier
		 */
		if (s->type == XVID_TYPE_BVOP){
			dbytes *= rc->avg_length[XVID_TYPE_BVOP-1] / rc->avg_length[XVID_TYPE_PVOP-1];
		}

		desired += (int)dbytes;
		rc->curve_comp_error += dbytes - (int)dbytes;
	}


	/*
	 * We can't do bigger frames than first pass, this would be stupid as first
	 * pass is quant=2 and that reaching quant=1 is not worth it. We would lose
	 * many bytes and we would not not gain much quality.
	 */
	if (desired > s->length) {
		rc->curve_comp_error += desired - s->length;
		desired = s->length;
	}else{
		if (desired < rc->min_length[s->type-1]) {
			if (s->type == XVID_TYPE_IVOP){
				rc->curve_comp_error -= rc->min_length[XVID_TYPE_IVOP-1] - desired;
			}
			desired = rc->min_length[s->type-1];
		}
	}

	s->desired_length = desired;

   	        
	/* if this keyframe is too close to the next, reduce it's byte allotment
	   XXX: why do we do this after setting the desired length  */

	if (s->type == XVID_TYPE_IVOP) {
		int KFdistance = rc->keyframe_locations[rc->KF_idx] - rc->keyframe_locations[rc->KF_idx - 1];

		if (KFdistance < rc->param.kftreshold) {
			        
			KFdistance = KFdistance - rc->param.min_key_interval;

			if (KFdistance >= 0) {
				int KF_min_size;

				KF_min_size = desired * (100 - rc->param.kfreduction) / 100;
				if (KF_min_size < 1)
					KF_min_size = 1;

				desired = KF_min_size + (desired - KF_min_size) * KFdistance /
					(rc->param.kftreshold - rc->param.min_key_interval);

				if (desired < 1)
					desired = 1;
			}
		}
	}

	overflow = (int)((double)overflow * desired / rc->avg_length[XVID_TYPE_PVOP-1]);

	/* Reign in overflow with huge frames */
	if (labs(overflow) > labs(rc->overflow)) {
		overflow = rc->overflow;
	}

	/* Make sure overflow doesn't run away */
	if (overflow > desired * rc->param.max_overflow_improvement / 100) {
		desired += (overflow <= desired) ? desired * rc->param.max_overflow_improvement / 100 :
			overflow * rc->param.max_overflow_improvement / 100;
	} else if (overflow < desired * rc->param.max_overflow_degradation / -100){
		desired += desired * rc->param.max_overflow_degradation / -100;
	} else {
		desired += overflow;
	}

	/* Make sure we are not higher than desired frame size */
	if (desired > rc->max_length) {
		capped_to_max_framesize = 1;
		desired = rc->max_length;
	}

	/* Make sure to not scale below the minimum framesize */
	if (desired < rc->min_length[s->type-1])
		desired = rc->min_length[s->type-1];

	/*
	 * Don't laugh at this very 'simple' quant<->filesize relationship, it
	 * proves to be acurate enough for our algorithm
	 */
	data->quant= (s->quant * s->length) / desired;

	/* Let's clip the computed quantizer, if needed */
	if (data->quant < 1) {
		data->quant = 1;
	} else if (data->quant > 31) {
		data->quant = 31;
	} else if (s->type != XVID_TYPE_IVOP) {

		/*
		 * The frame quantizer has not been clipped, this appear to be a good
		 * computed quantizer, however past frames give us some info about how
		 * this quantizer performs against the algo prevision. Let's use this
		 * prevision to increase the quantizer when we observe a too big
		 * accumulated error
		 */
		if (s->type== XVID_TYPE_BVOP) {
			rc->bquant_error[data->quant] += ((double)(s->quant * s->length) / desired) - data->quant;

			if (rc->bquant_error[data->quant] >= 1.0) {
				rc->bquant_error[data->quant] -= 1.0;
				data->quant++;
			}
		} else {
			rc->pquant_error[data->quant] += ((double)(s->quant * s->length) / desired) - data->quant;

			if (rc->pquant_error[data->quant] >= 1.0) {
				rc->pquant_error[data->quant] -= 1.0;
				++data->quant;
			}
		}
	}

	/*
	 * Now we have a computed quant that is in the right quante range, with a
	 * possible +1 correction due to cumulated error. We can now safely clip
	 * the quantizer again with user's quant ranges. "Safely" means the Rate
	 * Control could learn more about this quantizer, this knowledge is useful
	 * for future frames even if it this quantizer won't be really used atm,
	 * that's why we don't perform this clipping earlier.
	 */
	if (data->quant < data->min_quant[s->type-1]) {
		data->quant = data->min_quant[s->type-1];
	} else if (data->quant > data->max_quant[s->type-1]) {
		data->quant = data->max_quant[s->type-1];
	}

	/*
	 * To avoid big quality jumps from frame to frame, we apply a "security"
	 * rule that makes |last_quant - new_quant| <= 2. This rule only applies
	 * to predicted frames (P and B)
	 */
	if (s->type != XVID_TYPE_IVOP && rc->last_quant[s->type-1] && capped_to_max_framesize == 0) {

		if (data->quant > rc->last_quant[s->type-1] + 2) {
			data->quant = rc->last_quant[s->type-1] + 2;
			DPRINTF(XVID_DEBUG_RC, "p/b-frame quantizer prevented from rising too steeply");
		}
		if (data->quant < rc->last_quant[s->type-1] - 2) {
			data->quant = rc->last_quant[s->type-1] - 2;
			DPRINTF(XVID_DEBUG_RC, "p/b-frame quantizer prevented from falling too steeply");
		}
	}

	/*
	 * We don't want to pollute the RC history results when our computed quant
	 * has been computed from a capped frame size
	 */
	if (capped_to_max_framesize == 0) {
		rc->last_quant[s->type-1] = data->quant;
	}

	return 0;
}



static int rc_2pass2_after(rc_2pass2_t * rc, xvid_plg_data_t * data)
{
    stat_t * s = &rc->stats[data->frame_num];

	/* Insufficent stats data */
    if (data->frame_num >= rc->num_frames)
        return 0;

    rc->quant_count[data->quant]++;

    if (data->type == XVID_TYPE_IVOP) {
        int kfdiff = (rc->keyframe_locations[rc->KF_idx] -	rc->keyframe_locations[rc->KF_idx - 1]);

        rc->overflow += rc->KFoverflow;
        rc->KFoverflow = s->desired_length - data->length;
		
        if (kfdiff > 1) {  // non-consecutive keyframes
            rc->KFoverflow_partial = rc->KFoverflow / (kfdiff - 1);
        }else{ // consecutive keyframes
			rc->overflow += rc->KFoverflow;
			rc->KFoverflow = 0;
			rc->KFoverflow_partial = 0;
        }
        rc->KF_idx++;
    }else{
        // distribute part of the keyframe overflow
        rc->overflow += s->desired_length - data->length + rc->KFoverflow_partial;
        rc->KFoverflow -= rc->KFoverflow_partial;
    }

    DPRINTF(XVID_DEBUG_RC, "[%i] quant:%i stats1:%i scaled:%i actual:%i overflow:%i\n",
        data->frame_num,
        data->quant,
        s->length,
        s->scaled_length,
        data->length,
        rc->overflow);

    return(0);
}



int xvid_plugin_2pass2(void * handle, int opt, void * param1, void * param2)
{
    switch(opt)
    {
    case XVID_PLG_INFO :
        return 0;

    case XVID_PLG_CREATE :
        return rc_2pass2_create((xvid_plg_create_t*)param1, param2);

    case XVID_PLG_DESTROY :
        return rc_2pass2_destroy((rc_2pass2_t*)handle, (xvid_plg_destroy_t*)param1);

    case XVID_PLG_BEFORE :
        return rc_2pass2_before((rc_2pass2_t*)handle, (xvid_plg_data_t*)param1);

    case XVID_PLG_AFTER :
        return rc_2pass2_after((rc_2pass2_t*)handle, (xvid_plg_data_t*)param1);
    }

    return XVID_ERR_FAIL;
}
