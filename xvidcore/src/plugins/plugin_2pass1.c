/******************************************************************************
 *
 * XviD Bit Rate Controller Library
 * - VBR 2 pass bitrate controler implementation -
 *
 * Copyright (C) 2002 Edouard Gomez <ed.gomez@wanadoo.fr>
 *
 * The curve treatment algorithm is the one implemented by Foxer <email?> and
 * Dirk Knop <dknop@gwdg.de> for the XviD vfw dynamic library.
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
 * $Id: plugin_2pass1.c,v 1.1.2.2 2003-04-08 14:01:09 suxen_drol Exp $
 *
 *****************************************************************************/

#include <stdio.h>

#include "../xvid.h"
#include "../image/image.h"


/* context struct */
typedef struct
{
	FILE * stat_file;
} rc_2pass1_t;



static int rc_2pass1_create(xvid_plg_create_t * create, rc_2pass1_t ** handle)
{
    xvid_plugin_2pass1_t * param = (xvid_plugin_2pass1_t *)create->param;
	rc_2pass1_t * rc;

    /* check filename */
    if (param->filename == NULL || param->filename[0] == '\0')
        return XVID_ERR_FAIL;

    /* allocate context struct */
	if((rc = malloc(sizeof(rc_2pass1_t))) == NULL)
		return(XVID_ERR_MEMORY);

    /* Initialize safe defaults for 2pass 1 */ 
    rc->stat_file = NULL;

	/* Open the 1st pass file */
	if((rc->stat_file = fopen(param->filename, "w+")) == NULL)
		return(XVID_ERR_FAIL);

	/*
	 * The File Header
	 */
	/* fprintf(rc->stat_file, "# XviD 2pass stat file\n");
    fprintf(rc->stat_file, "version %i.%i.%i\n",XVID_MAJOR(XVID_VERSION), XVID_MINOR(XVID_VERSION), XVID_PATCH(XVID_VERSION));
	fprintf(rc->stat_file, "start\n");
    fprintf(rc->stat_file, "type quantizer length kblocks mblocks ublocks\n");  */

    *handle = rc;
	return(0);
}


static int rc_2pass1_destroy(rc_2pass1_t * rc, xvid_plg_destroy_t * destroy)
{
    //fprintf(rc->stat_file, "stop\n");
	fclose(rc->stat_file);

	free(rc);
	return(0);
}


static int rc_2pass1_before(rc_2pass1_t * rc, xvid_plg_data_t * data)
{
    data->quant = 2;
    data->type = XVID_TYPE_AUTO;
    return 0;
}


static int rc_2pass1_after(rc_2pass1_t * rc, xvid_plg_data_t * data)
{
	char type;

	/* Frame type in ascii I/P/B */
	switch(data->type) {
	case XVID_TYPE_IVOP:
		type = 'i';
		break;
	case XVID_TYPE_PVOP:
		type = 'p';
		break;
	case XVID_TYPE_BVOP:
		type = 'b';
		break;
	case XVID_TYPE_SVOP:
		type = 's';
		break;
	default: /* Should not go here */
		return(XVID_ERR_FAIL);
	}

	/* write the resulting statistics */

	fprintf(rc->stat_file, "%c %d %d %d %d %d\n",
        type,
		data->quant,
		data->kblks,
        data->mblks,
        data->ublks,
        data->length);

	return(0);
}



int xvid_plugin_2pass1(void * handle, int opt, void * param1, void * param2)
{
    switch(opt)
    {
    case XVID_PLG_INFO :
        return 0;

    case XVID_PLG_CREATE :
        return rc_2pass1_create((xvid_plg_create_t*)param1, param2);

    case XVID_PLG_DESTROY :
        return rc_2pass1_destroy((rc_2pass1_t*)handle, (xvid_plg_destroy_t*)param1);

    case XVID_PLG_BEFORE :
        return rc_2pass1_before((rc_2pass1_t*)handle, (xvid_plg_data_t*)param1);

    case XVID_PLG_AFTER :
        return rc_2pass1_after((rc_2pass1_t*)handle, (xvid_plg_data_t*)param1);
    }

    return XVID_ERR_FAIL;
}
