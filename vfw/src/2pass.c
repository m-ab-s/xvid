/**************************************************************************
 *
 *	XVID 2PASS CODE
 *	codec
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *************************************************************************/

/**************************************************************************
 *
 *	History:
 *
 *	17.04.2002	changed 1st pass quant to always be 2 (2pass_update())
 *	07.04.2002	added max bitrate constraint, overflow controls (foxer)
 *	31.03.2002	inital version;
 *
 *************************************************************************/

#include <windows.h>
#include <math.h>

#include "2pass.h"


int codec_2pass_init(CODEC* codec)
{
	TWOPASS *twopass = &codec->twopass;
	DWORD version = -20;
	DWORD read, wrote;

	int	frames = 0, bframes = 0, pframes = 0, credits_frames = 0, i_frames = 0, recminbsize = 0, recminpsize = 0, recminisize = 0;
	__int64 bframe_total_ext = 0, pframe_total_ext = 0, pframe_total = 0, bframe_total = 0, i_total = 0, i_total_ext = 0, i_boost_total = 0, start = 0, end = 0, start_curved = 0, end_curved = 0;
	__int64 desired = (__int64)codec->config.desired_size * 1024;

	double total1 = 0.0;
	double total2 = 0.0;
	double dbytes, dbytes2;

	/* ensure free() is called safely */
	codec->twopass.hintstream = NULL;
	twopass->nns1_array = NULL;
	twopass->nns2_array = NULL;

	if (codec->config.hinted_me)
	{
		codec->twopass.hintstream = malloc(100000);

		if (codec->twopass.hintstream == NULL)
		{
			DEBUGERR("couldn't allocate memory for mv hints");
			return ICERR_ERROR;
		}
	}

	switch (codec->config.mode)
	{
	case DLG_MODE_2PASS_1 :
		twopass->stats1 = CreateFile(codec->config.stats1, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		if (twopass->stats1 == INVALID_HANDLE_VALUE) 
		{
			DEBUGERR("2pass init error - couldn't create stats1");
			return ICERR_ERROR;
		}
		if (WriteFile(twopass->stats1, &version, sizeof(DWORD), &wrote, 0) == 0 || wrote != sizeof(DWORD)) 
		{
			CloseHandle(twopass->stats1);
			twopass->stats1 = INVALID_HANDLE_VALUE;
			DEBUGERR("2pass init error - couldn't write to stats1");
			return ICERR_ERROR;
		}
		break;
	
	case DLG_MODE_2PASS_2_INT :
	case DLG_MODE_2PASS_2_EXT :
		twopass->stats1 = CreateFile(codec->config.stats1, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (twopass->stats1 == INVALID_HANDLE_VALUE) 
		{
			DEBUGERR("2pass init error - couldn't open stats1");
			return ICERR_ERROR;
		}
		if (ReadFile(twopass->stats1, &version, sizeof(DWORD), &read, 0) == 0 || read != sizeof(DWORD)) 
		{
			CloseHandle(twopass->stats1);
			twopass->stats1 = INVALID_HANDLE_VALUE;
			DEBUGERR("2pass init error - couldn't read from stats1");
			return ICERR_ERROR;
		}
		if (version != -20)
		{
			CloseHandle(twopass->stats1);
			twopass->stats1 = INVALID_HANDLE_VALUE;
			DEBUGERR("2pass init error - wrong .stats version");
			return ICERR_ERROR;
		}

		twopass->nns1_array = (NNSTATS*)malloc(sizeof(NNSTATS) * 10240);
		twopass->nns2_array = (NNSTATS*)malloc(sizeof(NNSTATS) * 10240);
		twopass->nns_array_size = 10240;
		twopass->nns_array_length = 0;
		twopass->nns_array_pos = 0;

		// read the stats file(s) into array(s) and reorder them so they
		// correctly represent the frames that the encoder will receive.
		if (codec->config.mode == DLG_MODE_2PASS_2_EXT)
		{
			if (twopass->stats2 != INVALID_HANDLE_VALUE)
			{
				CloseHandle(twopass->stats2);
			}

			twopass->stats2 = CreateFile(codec->config.stats2, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

			if (twopass->stats2 == INVALID_HANDLE_VALUE) 
			{
				CloseHandle(twopass->stats1);
				twopass->stats1 = INVALID_HANDLE_VALUE;
				DEBUGERR("2pass init error - couldn't open stats2");
				return ICERR_ERROR;
			}
			if (ReadFile(twopass->stats2, &version, sizeof(DWORD), &read, 0) == 0 || read != sizeof(DWORD)) 
			{
				CloseHandle(twopass->stats1);
				twopass->stats1 = INVALID_HANDLE_VALUE;
				CloseHandle(twopass->stats2);
				twopass->stats2 = INVALID_HANDLE_VALUE;
				DEBUGERR("2pass init error - couldn't read from stats2");
				return ICERR_ERROR;
			}
			if (version != -20)
			{	
				CloseHandle(twopass->stats1);
				twopass->stats1 = INVALID_HANDLE_VALUE;
				CloseHandle(twopass->stats2);
				twopass->stats2 = INVALID_HANDLE_VALUE;
				DEBUGERR("2pass init error - wrong .stats version");
				return ICERR_ERROR;
			}

			while (1)
			{
				if (!ReadFile(twopass->stats1, &twopass->nns1, sizeof(NNSTATS), &read, NULL) || read != sizeof(NNSTATS) ||
					!ReadFile(twopass->stats2, &twopass->nns2, sizeof(NNSTATS), &read, NULL) || read != sizeof(NNSTATS))
				{
					DWORD err = GetLastError();

					if (err == ERROR_HANDLE_EOF || err == ERROR_SUCCESS)
					{
						break;
					}
					else
					{
						CloseHandle(twopass->stats1);
						CloseHandle(twopass->stats2);
						twopass->stats1 = INVALID_HANDLE_VALUE;
						twopass->stats2 = INVALID_HANDLE_VALUE;
						DEBUGERR("2pass init error - incomplete stats1/stats2 record?");
						return ICERR_ERROR;
					}
				}

				// increase the allocated memory if necessary
				if (frames >= twopass->nns_array_size)
				{
					twopass->nns1_array = (NNSTATS*)realloc(twopass->nns1_array,
						sizeof(NNSTATS) * (twopass->nns_array_size * 5 / 4 + 1));
					twopass->nns2_array = (NNSTATS*)realloc(twopass->nns2_array,
						sizeof(NNSTATS) * (twopass->nns_array_size * 5 / 4 + 1));
					twopass->nns_array_size = twopass->nns_array_size * 5 / 4 + 1;
				}

				// copy this frame's stats into the arrays
				memcpy (&twopass->nns1_array[frames], &twopass->nns1, sizeof(NNSTATS));
				memcpy (&twopass->nns2_array[frames], &twopass->nns2, sizeof(NNSTATS));
				frames++;
			}

			SetFilePointer(twopass->stats1, sizeof(DWORD), 0, FILE_BEGIN);
			SetFilePointer(twopass->stats2, sizeof(DWORD), 0, FILE_BEGIN);
		}
		else	// DLG_MODE_2PASS_2_INT
		{
			while (1)
			{
				if (!ReadFile(twopass->stats1, &twopass->nns1, sizeof(NNSTATS), &read, NULL) || read != sizeof(NNSTATS))
				{
					DWORD err = GetLastError();

					if (err == ERROR_HANDLE_EOF || err == ERROR_SUCCESS)
					{
						break;
					}
					else
					{
						CloseHandle(twopass->stats1);
						twopass->stats1 = INVALID_HANDLE_VALUE;
						DEBUGERR("2pass init error - incomplete stats2 record?");
						return ICERR_ERROR;
					}
				}

				// increase the allocated memory if necessary
				if (frames >= twopass->nns_array_size)
				{
					twopass->nns1_array = (NNSTATS*)realloc(twopass->nns1_array,
						sizeof(NNSTATS) * (twopass->nns_array_size * 5 / 4 + 1));
					twopass->nns_array_size = twopass->nns_array_size * 5 / 4 + 1;
				}

				// copy this frame's stats into the array
				memcpy (&twopass->nns1_array[frames], &twopass->nns1, sizeof(NNSTATS));
				frames++;
			}

			SetFilePointer(twopass->stats1, sizeof(DWORD), 0, FILE_BEGIN);
		}
		twopass->nns1_array = (NNSTATS*)realloc(twopass->nns1_array, sizeof(NNSTATS) * frames);
		twopass->nns2_array = (NNSTATS*)realloc(twopass->nns2_array, sizeof(NNSTATS) * frames);
		twopass->nns_array_size = frames;
		twopass->nns_array_length = frames;
		frames = 0;

/* // this isn't necessary with the current core.
		// reorder the array(s) so they are in the order that they were received
		// IPBBPBB to
		// IBBPBBP
		for (i=0; i<twopass->nns_array_length; i++)
		{
			NNSTATS temp_nns, temp_nns2;
			int k, num_bframes;
			if (twopass->nns1_array[i].dd_v & NNSTATS_BFRAME)
			{
				num_bframes = 1;
				for (k=i+1; k<twopass->nns_array_length; k++)
				{
					if (twopass->nns1_array[k].dd_v & NNSTATS_BFRAME)
						num_bframes++;
					else
						k=twopass->nns_array_length;
				}

				i--;
				memcpy (&temp_nns, &twopass->nns1_array[i], sizeof(NNSTATS));
				if (codec->config.mode == DLG_MODE_2PASS_2_EXT)
					memcpy (&temp_nns2, &twopass->nns2_array[i], sizeof(NNSTATS));

				for (k=0; k<num_bframes; k++)
				{
					memcpy(&twopass->nns1_array[i], &twopass->nns1_array[i+1], sizeof(NNSTATS));
					if (codec->config.mode == DLG_MODE_2PASS_2_EXT)
						memcpy(&twopass->nns2_array[i], &twopass->nns2_array[i+1], sizeof(NNSTATS));
					i++;
				}

				memcpy(&twopass->nns1_array[i], &temp_nns, sizeof(NNSTATS));
				if (codec->config.mode == DLG_MODE_2PASS_2_EXT)
					memcpy(&twopass->nns2_array[i], &temp_nns2, sizeof(NNSTATS));
			}
		}
*/
		// continue with the initialization..
		if (codec->config.mode == DLG_MODE_2PASS_2_EXT)
		{
			while (1)
			{
				if (twopass->nns_array_pos >= twopass->nns_array_length)
				{
					twopass->nns_array_pos = 0;
					break;
				}

				memcpy(&twopass->nns1, &twopass->nns1_array[twopass->nns_array_pos], sizeof(NNSTATS));
				memcpy(&twopass->nns2, &twopass->nns2_array[twopass->nns_array_pos], sizeof(NNSTATS));
				twopass->nns_array_pos++;

				// skip unnecessary frames.
				if (twopass->nns1.dd_v & NNSTATS_SKIPFRAME ||
					twopass->nns1.dd_v & NNSTATS_PADFRAME ||
					twopass->nns1.dd_v & NNSTATS_DELAYFRAME)
					continue;

				if (!codec_is_in_credits(&codec->config, frames))
				{
					if (twopass->nns1.quant & NNSTATS_KEYFRAME)
					{
						i_total += twopass->nns2.bytes;
						i_boost_total += twopass->nns2.bytes * codec->config.keyframe_boost / 100;
						twopass->keyframe_locations[i_frames] = frames;
						++i_frames;
					}
					else
					{
						if (twopass->nns1.dd_v & NNSTATS_BFRAME)
						{
							bframe_total += twopass->nns1.bytes;
							bframe_total_ext += twopass->nns2.bytes;
							bframes++;
						}
						else
						{
							pframe_total += twopass->nns1.bytes;
							pframe_total_ext += twopass->nns2.bytes;
							pframes++;
						}
					}
				}
				else
					++credits_frames;

				if (twopass->nns1.quant & NNSTATS_KEYFRAME)
				{
					// this test needs to be corrected..
					if (!(twopass->nns1.kblk + twopass->nns1.mblk))
						recminisize = twopass->nns1.bytes;
				}
				else if (twopass->nns1.dd_v & NNSTATS_BFRAME)
				{
					if (!(twopass->nns1.kblk + twopass->nns1.mblk))
						recminbsize = twopass->nns1.bytes;
				}
				else
				{
					if (!(twopass->nns1.kblk + twopass->nns1.mblk))
						recminpsize = twopass->nns1.bytes;
				}

				++frames;
			}
			twopass->keyframe_locations[i_frames] = frames;

			twopass->movie_curve = ((double)(bframe_total_ext + pframe_total_ext + i_boost_total) /
				(bframe_total_ext + pframe_total_ext));

			if (bframes)
				twopass->average_bframe = (double)bframe_total_ext / bframes / twopass->movie_curve;

			if (pframes)
				twopass->average_pframe = (double)pframe_total_ext / pframes / twopass->movie_curve;
			else
				if (bframes)
					twopass->average_pframe = twopass->average_bframe;  // b-frame packed bitstream fix
				else
				{
					DEBUGERR("ERROR:  No p-frames or b-frames were present in the 1st pass.  Rate control cannot function properly!");
					return ICERR_ERROR;
				}



			// perform prepass to compensate for over/undersizing
			frames = 0;

			if (codec->config.use_alt_curve)
			{
				twopass->alt_curve_low = twopass->average_pframe - twopass->average_pframe * (double)codec->config.alt_curve_low_dist / 100.0;
				twopass->alt_curve_low_diff = twopass->average_pframe - twopass->alt_curve_low;
				twopass->alt_curve_high = twopass->average_pframe + twopass->average_pframe * (double)codec->config.alt_curve_high_dist / 100.0;
				twopass->alt_curve_high_diff = twopass->alt_curve_high - twopass->average_pframe;
				if (codec->config.alt_curve_use_auto)
				{
					if (bframe_total + pframe_total > bframe_total_ext + pframe_total_ext)
					{
						codec->config.alt_curve_min_rel_qual = (int)(100.0 - (100.0 - 100.0 /
							((double)(bframe_total + pframe_total) / (double)(bframe_total_ext + pframe_total_ext))) *
							(double)codec->config.alt_curve_auto_str / 100.0);

						if (codec->config.alt_curve_min_rel_qual < 20)
							codec->config.alt_curve_min_rel_qual = 20;
					}
					else
						codec->config.alt_curve_min_rel_qual = 100;
				}
				twopass->alt_curve_mid_qual = (1.0 + (double)codec->config.alt_curve_min_rel_qual / 100.0) / 2.0;
				twopass->alt_curve_qual_dev = 1.0 - twopass->alt_curve_mid_qual;
				if (codec->config.alt_curve_low_dist > 100)
				{
					switch(codec->config.alt_curve_type)
					{
					case 2: // Sine Curve (high aggressiveness)
						twopass->alt_curve_qual_dev *= 2.0 / (1.0 + 
							sin(DEG2RAD * (twopass->average_pframe * 90.0 / twopass->alt_curve_low_diff)));
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							sin(DEG2RAD * (twopass->average_pframe * 90.0 / twopass->alt_curve_low_diff));
						break;
					case 1: // Linear (medium aggressiveness)
						twopass->alt_curve_qual_dev *= 2.0 / (1.0 +
							twopass->average_pframe / twopass->alt_curve_low_diff);
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							twopass->average_pframe / twopass->alt_curve_low_diff;
						break;
					case 0: // Cosine Curve (low aggressiveness)
						twopass->alt_curve_qual_dev *= 2.0 / (1.0 + 
							(1.0 - cos(DEG2RAD * (twopass->average_pframe * 90.0 / twopass->alt_curve_low_diff))));
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							(1.0 - cos(DEG2RAD * (twopass->average_pframe * 90.0 / twopass->alt_curve_low_diff)));
					}
				}
			}

			while (1)
			{
				if (twopass->nns_array_pos >= twopass->nns_array_length)
				{
					twopass->nns_array_pos = 0;
					break;
				}

				memcpy(&twopass->nns1, &twopass->nns1_array[twopass->nns_array_pos], sizeof(NNSTATS));
				memcpy(&twopass->nns2, &twopass->nns2_array[twopass->nns_array_pos], sizeof(NNSTATS));
				twopass->nns_array_pos++;

				if (frames == 0)
				{
					twopass->minbsize = (twopass->nns1.kblk + 88) / 8;
					twopass->minpsize = (twopass->nns1.kblk + 88) / 8;
					twopass->minisize = ((twopass->nns1.kblk * 22) + 240) / 8;
					if (recminbsize > twopass->minbsize)
						twopass->minbsize = recminbsize;
					if (recminpsize > twopass->minpsize)
						twopass->minpsize = recminpsize;
					if (recminisize > twopass->minisize)
						twopass->minisize = recminisize;
				}

				// skip unnecessary frames.
				if (twopass->nns1.dd_v & NNSTATS_SKIPFRAME ||
					twopass->nns1.dd_v & NNSTATS_PADFRAME ||
					twopass->nns1.dd_v & NNSTATS_DELAYFRAME)
					continue;

				if (!codec_is_in_credits(&codec->config, frames) &&
					!(twopass->nns1.quant & NNSTATS_KEYFRAME))
				{
					dbytes = twopass->nns2.bytes / twopass->movie_curve;
					total1 += dbytes;

					if (twopass->nns1.dd_v & NNSTATS_BFRAME)
						dbytes *= twopass->average_pframe / twopass->average_bframe;

					if (codec->config.use_alt_curve)
					{
						if (dbytes > twopass->average_pframe)
						{
							if (dbytes >= twopass->alt_curve_high)
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev);
							else
							{
								switch(codec->config.alt_curve_type)
								{
								case 2:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									sin(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_high_diff)));
									break;
								case 1:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(dbytes - twopass->average_pframe) / twopass->alt_curve_high_diff);
									break;
								case 0:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_high_diff))));
								}
							}
						}
						else
						{
							if (dbytes <= twopass->alt_curve_low)
								dbytes2 = dbytes;
							else
							{
								switch(codec->config.alt_curve_type)
								{
								case 2:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									sin(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_low_diff)));
									break;
								case 1:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(dbytes - twopass->average_pframe) / twopass->alt_curve_low_diff);
									break;
								case 0:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual + twopass->alt_curve_qual_dev *
									(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_low_diff))));
								}
							}
						}
					}
					else
					{
						if (dbytes > twopass->average_pframe)
						{
							dbytes2 = ((double)dbytes + (twopass->average_pframe - dbytes) *
								codec->config.curve_compression_high / 100.0);
						}
						else
						{
							dbytes2 = ((double)dbytes + (twopass->average_pframe - dbytes) *
								codec->config.curve_compression_low / 100.0);
						}
					}

					if (twopass->nns1.dd_v & NNSTATS_BFRAME)
					{
						dbytes2 *= twopass->average_bframe / twopass->average_pframe;
						if (dbytes2 < twopass->minbsize)
							dbytes2 = twopass->minbsize;
					}
					else
					{
						if (dbytes2 < twopass->minpsize)
							dbytes2 = twopass->minpsize;
					}

					total2 += dbytes2;
				}

				++frames;
			}

			twopass->curve_comp_scale = total1 / total2;

			if (!codec->config.use_alt_curve)
			{
				int asymmetric_average_frame;
				char s[100];

				asymmetric_average_frame = (int)(twopass->average_pframe * twopass->curve_comp_scale);
				wsprintf(s, "middle frame size for asymmetric curve compression: %i", asymmetric_average_frame);
				DEBUG2P(s);
			}
		}
		else	// DLG_MODE_2PASS_2_INT
		{
			while (1)
			{
				if (twopass->nns_array_pos >= twopass->nns_array_length)
				{
					twopass->nns_array_pos = 0;
					break;
				}

				memcpy(&twopass->nns1, &twopass->nns1_array[twopass->nns_array_pos], sizeof(NNSTATS));
				twopass->nns_array_pos++;

				// skip unnecessary frames.
				if (twopass->nns1.dd_v & NNSTATS_SKIPFRAME ||
					twopass->nns1.dd_v & NNSTATS_PADFRAME ||
					twopass->nns1.dd_v & NNSTATS_DELAYFRAME)
					continue;

				if (codec_is_in_credits(&codec->config, frames) == CREDITS_START)
				{
					start += twopass->nns1.bytes;
					++credits_frames;
				}
				else if (codec_is_in_credits(&codec->config, frames) == CREDITS_END)
				{
					end += twopass->nns1.bytes;
					++credits_frames;
				}
				else if (twopass->nns1.quant & NNSTATS_KEYFRAME)
				{
					i_total += twopass->nns1.bytes + twopass->nns1.bytes * codec->config.keyframe_boost / 100;
					twopass->keyframe_locations[i_frames] = frames;
					++i_frames;
				}
				else
				{
					if (twopass->nns1.dd_v & NNSTATS_BFRAME)
					{
						bframe_total += twopass->nns1.bytes;
						bframes++;
					}
					else
					{
						pframe_total += twopass->nns1.bytes;
						pframes++;
					}
				}

				if (twopass->nns1.quant & NNSTATS_KEYFRAME)
				{
					// this test needs to be corrected..
					if (!(twopass->nns1.kblk + twopass->nns1.mblk))
						recminisize = twopass->nns1.bytes;
				}
				else if (twopass->nns1.dd_v & NNSTATS_BFRAME)
				{
					if (!(twopass->nns1.kblk + twopass->nns1.mblk))
						recminbsize = twopass->nns1.bytes;
				}
				else
				{
					if (!(twopass->nns1.kblk + twopass->nns1.mblk))
						recminpsize = twopass->nns1.bytes;
				}

				++frames;
			}
			twopass->keyframe_locations[i_frames] = frames;

			// compensate for avi frame overhead
			desired -= frames * 24;

			switch (codec->config.credits_mode)
			{
			case CREDITS_MODE_RATE :

				// credits curve = (total / desired_size) * (100 / credits_rate)
				twopass->credits_start_curve = twopass->credits_end_curve =
					((double)(bframe_total + pframe_total + i_total + start + end) / desired) *
					((double)100 / codec->config.credits_rate);

				start_curved = (__int64)(start / twopass->credits_start_curve);
				end_curved = (__int64)(end / twopass->credits_end_curve);

				// movie curve = (total - credits) / (desired_size - curved credits)
				twopass->movie_curve = (double)
					(bframe_total + pframe_total + i_total) /
					(desired - start_curved - end_curved);

				break;

			case CREDITS_MODE_QUANT :

				// movie curve = (total - credits) / (desired_size - credits)
				twopass->movie_curve = (double)
					(bframe_total + pframe_total + i_total) / (desired - start - end);

				// aid the average asymmetric frame calculation below
				start_curved = start;
				end_curved = end;

				break;

			case CREDITS_MODE_SIZE :

				// start curve = (start / start desired size)
				twopass->credits_start_curve = (double)
					(start / 1024) / codec->config.credits_start_size;

				// end curve = (end / end desired size)
				twopass->credits_end_curve = (double)
					(end / 1024) / codec->config.credits_end_size;

				start_curved = (__int64)(start / twopass->credits_start_curve);
				end_curved = (__int64)(end / twopass->credits_end_curve);

				// movie curve = (total - credits) / (desired_size - curved credits)
				twopass->movie_curve = (double)
					(bframe_total + pframe_total + i_total) /
					(desired - start_curved - end_curved);

				break;
			}

			if (bframes)
				twopass->average_bframe = (double)bframe_total / bframes / twopass->movie_curve;

			if (pframes)
				twopass->average_pframe = (double)pframe_total / pframes / twopass->movie_curve;
			else
				if (bframes)
					twopass->average_pframe = twopass->average_bframe;  // b-frame packed bitstream fix
				else
				{
					DEBUGERR("ERROR:  No p-frames or b-frames were present in the 1st pass.  Rate control cannot function properly!");
					return ICERR_ERROR;
				}



			// perform prepass to compensate for over/undersizing
			frames = 0;

			if (codec->config.use_alt_curve)
			{
				twopass->alt_curve_low = twopass->average_pframe - twopass->average_pframe * (double)codec->config.alt_curve_low_dist / 100.0;
				twopass->alt_curve_low_diff = twopass->average_pframe - twopass->alt_curve_low;
				twopass->alt_curve_high = twopass->average_pframe + twopass->average_pframe * (double)codec->config.alt_curve_high_dist / 100.0;
				twopass->alt_curve_high_diff = twopass->alt_curve_high - twopass->average_pframe;
				if (codec->config.alt_curve_use_auto)
				{
					if (twopass->movie_curve > 1.0)
					{
						codec->config.alt_curve_min_rel_qual = (int)(100.0 - (100.0 - 100.0 / twopass->movie_curve) * (double)codec->config.alt_curve_auto_str / 100.0);
						if (codec->config.alt_curve_min_rel_qual < 20)
							codec->config.alt_curve_min_rel_qual = 20;
					}
					else
						codec->config.alt_curve_min_rel_qual = 100;
				}
				twopass->alt_curve_mid_qual = (1.0 + (double)codec->config.alt_curve_min_rel_qual / 100.0) / 2.0;
				twopass->alt_curve_qual_dev = 1.0 - twopass->alt_curve_mid_qual;
				if (codec->config.alt_curve_low_dist > 100)
				{
					switch(codec->config.alt_curve_type)
					{
					case 2: // Sine Curve (high aggressiveness)
						twopass->alt_curve_qual_dev *= 2.0 / (1.0 + 
							sin(DEG2RAD * (twopass->average_pframe * 90.0 / twopass->alt_curve_low_diff)));
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							sin(DEG2RAD * (twopass->average_pframe * 90.0 / twopass->alt_curve_low_diff));
						break;
					case 1: // Linear (medium aggressiveness)
						twopass->alt_curve_qual_dev *= 2.0 / (1.0 +
							twopass->average_pframe / twopass->alt_curve_low_diff);
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							twopass->average_pframe / twopass->alt_curve_low_diff;
						break;
					case 0: // Cosine Curve (low aggressiveness)
						twopass->alt_curve_qual_dev *= 2.0 / (1.0 + 
							(1.0 - cos(DEG2RAD * (twopass->average_pframe * 90.0 / twopass->alt_curve_low_diff))));
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							(1.0 - cos(DEG2RAD * (twopass->average_pframe * 90.0 / twopass->alt_curve_low_diff)));
					}
				}
			}

			while (1)
			{
				if (twopass->nns_array_pos >= twopass->nns_array_length)
				{
					twopass->nns_array_pos = 0;
					break;
				}

				memcpy(&twopass->nns1, &twopass->nns1_array[twopass->nns_array_pos], sizeof(NNSTATS));
				twopass->nns_array_pos++;

				if (frames == 0)
				{
					twopass->minbsize = (twopass->nns1.kblk + 88) / 8;
					twopass->minpsize = (twopass->nns1.kblk + 88) / 8;
					twopass->minisize = ((twopass->nns1.kblk * 22) + 240) / 8;
					if (recminbsize > twopass->minbsize)
						twopass->minbsize = recminbsize;
					if (recminpsize > twopass->minpsize)
						twopass->minpsize = recminpsize;
					if (recminisize > twopass->minisize)
						twopass->minisize = recminisize;
				}

				// skip unnecessary frames.
				if (twopass->nns1.dd_v & NNSTATS_SKIPFRAME ||
					twopass->nns1.dd_v & NNSTATS_PADFRAME ||
					twopass->nns1.dd_v & NNSTATS_DELAYFRAME)
					continue;

				if (!codec_is_in_credits(&codec->config, frames) &&
					!(twopass->nns1.quant & NNSTATS_KEYFRAME))
				{
					dbytes = twopass->nns1.bytes / twopass->movie_curve;
					total1 += dbytes;

					if (twopass->nns1.dd_v & NNSTATS_BFRAME)
						dbytes *= twopass->average_pframe / twopass->average_bframe;

					if (codec->config.use_alt_curve)
					{
						if (dbytes > twopass->average_pframe)
						{
							if (dbytes >= twopass->alt_curve_high)
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev);
							else
							{
								switch(codec->config.alt_curve_type)
								{
								case 2:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									sin(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_high_diff)));
									break;
								case 1:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(dbytes - twopass->average_pframe) / twopass->alt_curve_high_diff);
									break;
								case 0:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_high_diff))));
								}
							}
						}
						else
						{
							if (dbytes <= twopass->alt_curve_low)
								dbytes2 = dbytes;
							else
							{
								switch(codec->config.alt_curve_type)
								{
								case 2:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									sin(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_low_diff)));
									break;
								case 1:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(dbytes - twopass->average_pframe) / twopass->alt_curve_low_diff);
									break;
								case 0:
								dbytes2 = dbytes * (twopass->alt_curve_mid_qual + twopass->alt_curve_qual_dev *
									(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_low_diff))));
								}
							}
						}
					}
					else
					{
						if (dbytes > twopass->average_pframe)
						{
							dbytes2 = ((double)dbytes + (twopass->average_pframe - dbytes) *
								codec->config.curve_compression_high / 100.0);
						}
						else
						{
							dbytes2 = ((double)dbytes + (twopass->average_pframe - dbytes) *
								codec->config.curve_compression_low / 100.0);
						}
					}

					if (twopass->nns1.dd_v & NNSTATS_BFRAME)
					{
						dbytes2 *= twopass->average_bframe / twopass->average_pframe;
						if (dbytes2 < twopass->minbsize)
							dbytes2 = twopass->minbsize;
					}
					else
					{
						if (dbytes2 < twopass->minpsize)
							dbytes2 = twopass->minpsize;
					}

					total2 += dbytes2;
				}

				++frames;
			}

			twopass->curve_comp_scale = total1 / total2;

			if (!codec->config.use_alt_curve)
			{
				int asymmetric_average_frame;
				char s[100];

				asymmetric_average_frame = (int)(twopass->average_pframe * twopass->curve_comp_scale);
				wsprintf(s, "middle frame size for asymmetric curve compression: %i", asymmetric_average_frame);
				DEBUG2P(s);
			}
		}

		if (codec->config.use_alt_curve)
		{
			if (codec->config.alt_curve_use_auto_bonus_bias)
				codec->config.alt_curve_bonus_bias = codec->config.alt_curve_min_rel_qual;

			twopass->curve_bias_bonus = (total1 - total2) * (double)codec->config.alt_curve_bonus_bias / 100.0 / (double)(frames - credits_frames - i_frames);
			twopass->curve_comp_scale = ((total1 - total2) * (1.0 - (double)codec->config.alt_curve_bonus_bias / 100.0) + total2) / total2;


			// special info for alt curve:  bias bonus and quantizer thresholds, 
			{
				double curve_temp, dbytes;
				char s[100];
				int i, newquant, percent;
				int oldquant = 1;

				wsprintf(s, "avg scaled framesize:%i", (int)(twopass->average_pframe));
				DEBUG2P(s);

				wsprintf(s, "bias bonus:%i bytes", (int)(twopass->curve_bias_bonus));
				DEBUG2P(s);

				for (i=1; i <= (int)(twopass->alt_curve_high*2)+1; i++)
				{
					dbytes = i;
					if (dbytes > twopass->average_pframe)
					{
						if (dbytes >= twopass->alt_curve_high)
							curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev);
						else
						{
							switch(codec->config.alt_curve_type)
							{
							case 2:
							curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
								sin(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_high_diff)));
								break;
							case 1:
							curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
								(dbytes - twopass->average_pframe) / twopass->alt_curve_high_diff);
								break;
							case 0:
							curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
								(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_high_diff))));
							}
						}
					}
					else
					{
						if (dbytes <= twopass->alt_curve_low)
							curve_temp = dbytes;
						else
						{
							switch(codec->config.alt_curve_type)
							{
							case 2:
							curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
								sin(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_low_diff)));
								break;
							case 1:
							curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
								(dbytes - twopass->average_pframe) / twopass->alt_curve_low_diff);
								break;
							case 0:
							curve_temp = dbytes * (twopass->alt_curve_mid_qual + twopass->alt_curve_qual_dev *
								(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_low_diff))));
							}
						}
					}

					if (twopass->movie_curve > 1.0)
						dbytes *= twopass->movie_curve;

					newquant = (int)(dbytes * 2.0 / (curve_temp * twopass->curve_comp_scale + twopass->curve_bias_bonus));
					if (newquant > 1)
					{
						if (newquant != oldquant)
						{
							oldquant = newquant;
							percent = (int)((i - twopass->average_pframe) * 100.0 / twopass->average_pframe);
							wsprintf(s, "quant:%i threshold at %i : %i percent", newquant, i, percent);
							DEBUG2P(s);
						}
					}
				}
			}
		}

		twopass->overflow = 0;
		twopass->KFoverflow = 0;
		twopass->KFoverflow_partial = 0;
		twopass->KF_idx = 1;

		break;
	}

	return ICERR_OK;
}

// NOTE: codec_2pass_get_quant() should be called for all the frames that are in the stats file(s)
int codec_2pass_get_quant(CODEC* codec, XVID_ENC_FRAME* frame)
{
	static double bquant_error[32];
	static double pquant_error[32];
	static double curve_comp_error;
	static int last_bquant, last_pquant;

	TWOPASS * twopass = &codec->twopass;

//	DWORD read;
	int bytes1, bytes2;
	int overflow;
	int credits_pos;
	int capped_to_max_framesize = 0;
	int KFdistance, KF_min_size;

	if (codec->framenum == 0)
	{
		int i;

		for (i=0 ; i<32 ; ++i)
		{
			bquant_error[i] = 0.0;
			pquant_error[i] = 0.0;
			twopass->quant_count[i] = 0;
		}

		curve_comp_error = 0.0;
		last_bquant = 0;
		last_pquant = 0;
	}

	if (twopass->nns_array_pos >= twopass->nns_array_length)
	{
		// fix for VirtualDub 1.4.13 bframe handling
		if (codec->config.max_bframes > 0 &&
			codec->framenum < twopass->nns_array_length + codec->config.max_bframes)
		{
			return ICERR_OK;
		}
		else
		{
			DEBUGERR("ERROR: VIDEO EXCEEDS 1ST PASS!!!");
			return ICERR_ERROR;
		}
	}

	memcpy(&twopass->nns1, &twopass->nns1_array[twopass->nns_array_pos], sizeof(NNSTATS));
	if (codec->config.mode == DLG_MODE_2PASS_2_EXT)
		memcpy(&twopass->nns2, &twopass->nns2_array[twopass->nns_array_pos], sizeof(NNSTATS));
	twopass->nns_array_pos++;

	bytes1 = twopass->nns1.bytes;

	// skip unnecessary frames.
	if (twopass->nns1.dd_v & NNSTATS_SKIPFRAME)
	{
		twopass->bytes1 = bytes1;
		twopass->bytes2 = bytes1;
		twopass->desired_bytes2 = bytes1;
		frame->intra = 3;
		return ICERR_OK;
	}
	else if (twopass->nns1.dd_v & NNSTATS_PADFRAME)
	{
		twopass->bytes1 = bytes1;
		twopass->bytes2 = bytes1;
		twopass->desired_bytes2 = bytes1;
		frame->intra = 4;
		return ICERR_OK;
	}
	else if (twopass->nns1.dd_v & NNSTATS_DELAYFRAME)
	{
		twopass->bytes1 = bytes1;
		twopass->bytes2 = bytes1;
		twopass->desired_bytes2 = bytes1;
		frame->intra = 5;
		return ICERR_OK;
	}
		
	overflow = twopass->overflow / 8;

	// override codec i-frame choice (reenable in credits)
	if (twopass->nns1.quant & NNSTATS_KEYFRAME)
		frame->intra=1;
	else if (twopass->nns1.dd_v & NNSTATS_BFRAME)
		frame->intra=2;
	else
		frame->intra=0;

	if (frame->intra==1)
	{
		overflow = 0;
	}

	credits_pos = codec_is_in_credits(&codec->config, codec->framenum);

	if (credits_pos)
	{
		if (codec->config.mode == DLG_MODE_2PASS_2_INT)
		{
			switch (codec->config.credits_mode)
			{
			case CREDITS_MODE_RATE :
			case CREDITS_MODE_SIZE :
				if (credits_pos == CREDITS_START)
				{
					bytes2 = (int)(bytes1 / twopass->credits_start_curve);
				}
				else // CREDITS_END
				{
					bytes2 = (int)(bytes1 / twopass->credits_end_curve);
				}

				frame->intra = -1;
				break;

			case CREDITS_MODE_QUANT :
				if (codec->config.credits_quant_i != codec->config.credits_quant_p)
				{
					frame->quant = frame->intra ?
						codec->config.credits_quant_i :
						codec->config.credits_quant_p;
				}
				else
				{
					frame->quant = codec->config.credits_quant_p;
					frame->intra = -1;
				}

				twopass->bytes1 = bytes1;
				twopass->bytes2 = bytes1;
				twopass->desired_bytes2 = bytes1;
				return ICERR_OK;
			}
		}
		else	// DLG_MODE_2PASS_2_EXT
		{
			if (codec->config.credits_mode == CREDITS_MODE_QUANT)
			{
				if (codec->config.credits_quant_i != codec->config.credits_quant_p)
				{
					frame->quant = frame->intra == 1 ?
						codec->config.credits_quant_i :
						codec->config.credits_quant_p;
				}
				else
				{
					frame->quant = codec->config.credits_quant_p;
					frame->intra = -1;
				}

				twopass->bytes1 = bytes1;
				twopass->bytes2 = bytes1;
				twopass->desired_bytes2 = bytes1;
				return ICERR_OK;				
			}
			else
				bytes2 = twopass->nns2.bytes;
		}
	}
	else	// Foxer: apply curve compression outside credits
	{
		double dbytes, curve_temp;

		bytes2 = (codec->config.mode == DLG_MODE_2PASS_2_INT) ? bytes1 : twopass->nns2.bytes;

		if (frame->intra==1)
		{
			dbytes = ((int)(bytes2 + bytes2 * codec->config.keyframe_boost / 100)) /
				twopass->movie_curve;
		}
		else
		{
			dbytes = bytes2 / twopass->movie_curve;
		}

		if (twopass->nns1.dd_v & NNSTATS_BFRAME)
			dbytes *= twopass->average_pframe / twopass->average_bframe;

		// spread the compression error across payback_delay frames
		if (codec->config.bitrate_payback_method == 0)
		{
			bytes2 = (int)(curve_comp_error / codec->config.bitrate_payback_delay);
		}
		else
		{
			bytes2 = (int)(curve_comp_error * dbytes /
				twopass->average_pframe / codec->config.bitrate_payback_delay);

			if (labs(bytes2) > fabs(curve_comp_error))
			{
				bytes2 = (int)curve_comp_error;
			}
		}

		curve_comp_error -= bytes2;

		if (codec->config.use_alt_curve)
		{
			if (!(frame->intra==1))
			{
				if (dbytes > twopass->average_pframe)
				{
					if (dbytes >= twopass->alt_curve_high)
						curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev);
					else
					{
						switch(codec->config.alt_curve_type)
						{
						case 2:
						curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
							sin(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_high_diff)));
							break;
						case 1:
						curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
							(dbytes - twopass->average_pframe) / twopass->alt_curve_high_diff);
							break;
						case 0:
						curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
							(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_high_diff))));
						}
					}
				}
				else
				{
					if (dbytes <= twopass->alt_curve_low)
						curve_temp = dbytes;
					else
					{
						switch(codec->config.alt_curve_type)
						{
						case 2:
						curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
							sin(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_low_diff)));
							break;
						case 1:
						curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
							(dbytes - twopass->average_pframe) / twopass->alt_curve_low_diff);
							break;
						case 0:
						curve_temp = dbytes * (twopass->alt_curve_mid_qual + twopass->alt_curve_qual_dev *
							(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_pframe) * 90.0 / twopass->alt_curve_low_diff))));
						}
					}
				}
				if (twopass->nns1.dd_v & NNSTATS_BFRAME)
					curve_temp *= twopass->average_bframe / twopass->average_pframe;

				curve_temp = curve_temp * twopass->curve_comp_scale + twopass->curve_bias_bonus;

				bytes2 += ((int)curve_temp);
				curve_comp_error += curve_temp - ((int)curve_temp);
			}
			else
			{
				if (twopass->nns1.dd_v & NNSTATS_BFRAME)
					dbytes *= twopass->average_bframe / twopass->average_pframe;

				bytes2 += ((int)dbytes);
				curve_comp_error += dbytes - ((int)dbytes);
			}
		}
		else if ((codec->config.curve_compression_high + codec->config.curve_compression_low) &&
			!(frame->intra==1))
		{
			if (dbytes > twopass->average_pframe)
			{
				curve_temp = twopass->curve_comp_scale *
					((double)dbytes + (twopass->average_pframe - dbytes) *
					codec->config.curve_compression_high / 100.0);
			}
			else
			{
				curve_temp = twopass->curve_comp_scale *
					((double)dbytes + (twopass->average_pframe - dbytes) *
					codec->config.curve_compression_low / 100.0);
			}

			if (twopass->nns1.dd_v & NNSTATS_BFRAME)
				curve_temp *= twopass->average_bframe / twopass->average_pframe;

			bytes2 += ((int)curve_temp);
			curve_comp_error += curve_temp - ((int)curve_temp);
		}
		else
		{
			if (twopass->nns1.dd_v & NNSTATS_BFRAME)
				dbytes *= twopass->average_bframe / twopass->average_pframe;

			bytes2 += ((int)dbytes);
			curve_comp_error += dbytes - ((int)dbytes);
		}

		// cap bytes2 to first pass size, lowers number of quant=1 frames
		if (bytes2 > bytes1)
		{
			curve_comp_error += bytes2 - bytes1;
			bytes2 = bytes1;
		}
		else
		{
			if (frame->intra==1)
			{
				if (bytes2 < twopass->minisize)
				{
					curve_comp_error -= twopass->minisize - bytes2;
					bytes2 = twopass->minisize;
				}
			}
			else if (twopass->nns1.dd_v & NNSTATS_BFRAME)
			{
				if (bytes2 < twopass->minbsize)
					bytes2 = twopass->minbsize;
			}
			else
			{
				if (bytes2 < twopass->minpsize)
					bytes2 = twopass->minpsize;
			}
		}
	}

	twopass->desired_bytes2 = bytes2;

	// if this keyframe is too close to the next,
	// reduce it's byte allotment
	if ((frame->intra==1) && !credits_pos)
	{
		KFdistance = codec->twopass.keyframe_locations[codec->twopass.KF_idx] -
			codec->twopass.keyframe_locations[codec->twopass.KF_idx - 1];

		if (KFdistance < codec->config.kftreshold)
		{
			KFdistance = KFdistance - codec->config.min_key_interval;

			if (KFdistance >= 0)
			{
				KF_min_size = bytes2 * (100 - codec->config.kfreduction) / 100;
				if (KF_min_size < 1)
					KF_min_size = 1;

				bytes2 = KF_min_size + (bytes2 - KF_min_size) * KFdistance /
					(codec->config.kftreshold - codec->config.min_key_interval);

				if (bytes2 < 1)
					bytes2 = 1;
			}
		}
	}

	// Foxer: scale overflow in relation to average size, so smaller frames don't get
	// too much/little bitrate
	overflow = (int)((double)overflow * bytes2 / twopass->average_pframe);

	// Foxer: reign in overflow with huge frames
	if (labs(overflow) > labs(twopass->overflow))
	{
		overflow = twopass->overflow;
	}

	// Foxer: make sure overflow doesn't run away
	if (overflow > bytes2 * codec->config.twopass_max_overflow_improvement / 100)
	{
		bytes2 += (overflow <= bytes2) ? bytes2 * codec->config.twopass_max_overflow_improvement / 100 :
			overflow * codec->config.twopass_max_overflow_improvement / 100;
	}
	else if (overflow < bytes2 * codec->config.twopass_max_overflow_degradation / -100)
	{
		bytes2 += bytes2 * codec->config.twopass_max_overflow_degradation / -100;
	}
	else
	{
		bytes2 += overflow;
	}

	if (bytes2 > twopass->max_framesize)
	{
		capped_to_max_framesize = 1;
		bytes2 = twopass->max_framesize;
	}

	// make sure to not scale below the minimum framesize
	if (twopass->nns1.quant & NNSTATS_KEYFRAME)
	{
		if (bytes2 < twopass->minisize)
			bytes2 = twopass->minisize;
	}
	else if (twopass->nns1.dd_v & NNSTATS_BFRAME)
	{
		if (bytes2 < twopass->minbsize)
			bytes2 = twopass->minbsize;
	}
	else
	{
		if (bytes2 < twopass->minpsize)
			bytes2 = twopass->minpsize;
	}

	twopass->bytes1 = bytes1;
	twopass->bytes2 = bytes2;

	// very 'simple' quant<->filesize relationship
	frame->quant = ((twopass->nns1.quant & ~NNSTATS_KEYFRAME) * bytes1) / bytes2;

	if (frame->quant < 1)
	{
		frame->quant = 1;
	}
	else if (frame->quant > 31)
	{
		frame->quant = 31;
	}
	else if (!(frame->intra==1))
	{
		// Foxer: aid desired quantizer precision by accumulating decision error
		if (twopass->nns1.dd_v & NNSTATS_BFRAME)
		{
			bquant_error[frame->quant] += ((double)((twopass->nns1.quant & ~NNSTATS_KEYFRAME) * 
				bytes1) / bytes2) - frame->quant;

			if (bquant_error[frame->quant] >= 1.0)
			{
				bquant_error[frame->quant] -= 1.0;
				++frame->quant;
			}
		}
		else
		{
			pquant_error[frame->quant] += ((double)((twopass->nns1.quant & ~NNSTATS_KEYFRAME) * 
				bytes1) / bytes2) - frame->quant;

			if (pquant_error[frame->quant] >= 1.0)
			{
				pquant_error[frame->quant] -= 1.0;
				++frame->quant;
			}
		}
	}

	// we're done with credits
	if (codec_is_in_credits(&codec->config, codec->framenum))
	{
		return ICERR_OK;
	}

	if ((frame->intra==1))
	{
		if (frame->quant < codec->config.min_iquant)
		{
			frame->quant = codec->config.min_iquant;
			DEBUG2P("I-frame quantizer raised");
		}
		if (frame->quant > codec->config.max_iquant)
		{
			frame->quant = codec->config.max_iquant;
			DEBUG2P("I-frame quantizer lowered");
		}
	}
	else
	{
		if (frame->quant > codec->config.max_pquant)
		{
			frame->quant = codec->config.max_pquant;
		}
		if (frame->quant < codec->config.min_pquant)
		{
			frame->quant = codec->config.min_pquant;
		}

		// subsequent frame quants can only be +- 2
		if ((last_pquant || last_bquant) && capped_to_max_framesize == 0)
		{
			if (twopass->nns1.dd_v & NNSTATS_BFRAME)
			{
				// this bframe quantizer variation
				// restriction needs to be redone.
				if (frame->quant > last_bquant + 2)
				{
					frame->quant = last_bquant + 2;
					DEBUG2P("B-frame quantizer prevented from rising too steeply");
				}
				if (frame->quant < last_bquant - 2)
				{
					frame->quant = last_bquant - 2;
					DEBUG2P("B-frame quantizer prevented from falling too steeply");
				}
			}
			else
			{
				if (frame->quant > last_pquant + 2)
				{
					frame->quant = last_pquant + 2;
					DEBUG2P("P-frame quantizer prevented from rising too steeply");
				}
				if (frame->quant < last_pquant - 2)
				{
					frame->quant = last_pquant - 2;
					DEBUG2P("P-frame quantizer prevented from falling too steeply");
				}
			}
		}
	}

	if (capped_to_max_framesize == 0)
	{
		if (twopass->nns1.quant & NNSTATS_KEYFRAME)
		{
			last_bquant = frame->quant;
			last_pquant = frame->quant;
		}
		else if (twopass->nns1.dd_v & NNSTATS_BFRAME)
			last_bquant = frame->quant;
		else
			last_pquant = frame->quant;
	}

	if (codec->config.quant_type == QUANT_MODE_MOD)
	{
		frame->general |= (frame->quant < 4) ? XVID_MPEGQUANT : XVID_H263QUANT;
		frame->general &= (frame->quant < 4) ? ~XVID_H263QUANT : ~XVID_MPEGQUANT;
	}

	if (codec->config.quant_type == QUANT_MODE_MOD_NEW)
	{
		frame->general |= (frame->quant < 4) ? XVID_H263QUANT : XVID_MPEGQUANT;
		frame->general &= (frame->quant < 4) ? ~XVID_MPEGQUANT : ~XVID_H263QUANT;
	}

	return ICERR_OK;
}


int codec_2pass_update(CODEC* codec, XVID_ENC_FRAME* frame, XVID_ENC_STATS* stats)
{
	static __int64 total_size;

	NNSTATS nns1;
	DWORD wrote;
	int credits_pos, tempdiv;
	char* quant_type;
	char* frame_type;

	if (codec->framenum == 0)
	{
		total_size = 0;
	}

	quant_type = (frame->general & XVID_H263QUANT) ? "H.263" :
		((frame->general & XVID_MPEGQUANT) && (frame->general & XVID_CUSTOM_QMATRIX)) ?
		"Cust" : "MPEG";

	switch (codec->config.mode)
	{
	case DLG_MODE_2PASS_1 :
		nns1.bytes = frame->length;	// total bytes
// THIS small bugger messed up 2pass encoding!
//		nns1.dd_v = stats->hlength;	// header bytes
		nns1.dd_v = 0;
		nns1.dd_u = nns1.dd_y = 0;
		nns1.dk_v = nns1.dk_u = nns1.dk_y = 0;
		nns1.md_u = nns1.md_y = 0;
		nns1.mk_u = nns1.mk_y = 0;

//		nns1.quant = stats->quant;
		nns1.quant = 2;				// ugly fix for lumi masking in 1st pass returning new quant
		nns1.lum_noise[0] = nns1.lum_noise[1] = 1;
		frame_type="inter";
		if (frame->intra==1) {
			nns1.quant |= NNSTATS_KEYFRAME;
			frame_type="intra";
		}
		else if (frame->intra==2) {
			nns1.dd_v |= NNSTATS_BFRAME;
			frame_type="bframe";
		}
		else if (frame->intra==3) {
			nns1.dd_v |= NNSTATS_SKIPFRAME;
			frame_type="skiped";
		}
		else if (frame->intra==4) {
			nns1.dd_v |= NNSTATS_PADFRAME;
			frame_type="padded";
		}
		else if (frame->intra==5) {
			nns1.dd_v |= NNSTATS_DELAYFRAME;
			frame_type="delayed";
		}
		nns1.kblk = stats->kblks;
		nns1.mblk = stats->mblks;
		nns1.ublk = stats->ublks;

		total_size += frame->length;

		DEBUG1ST(frame->length, (int)total_size/1024, frame_type, frame->quant, quant_type, stats->kblks, stats->mblks)
		

		if (WriteFile(codec->twopass.stats1, &nns1, sizeof(NNSTATS), &wrote, 0) == 0 || wrote != sizeof(NNSTATS))
		{
			DEBUGERR("stats1: WriteFile error");
			return ICERR_ERROR;	
		}
		break;

	case DLG_MODE_2PASS_2_INT :
	case DLG_MODE_2PASS_2_EXT :
		credits_pos = codec_is_in_credits(&codec->config, codec->framenum);
		if (!(codec->twopass.nns1.dd_v & NNSTATS_SKIPFRAME) &&
			!(codec->twopass.nns1.dd_v & NNSTATS_PADFRAME) &&
			!(codec->twopass.nns1.dd_v & NNSTATS_DELAYFRAME))
		{
			if (!credits_pos)
			{
				codec->twopass.quant_count[frame->quant]++;
				if ((codec->twopass.nns1.quant & NNSTATS_KEYFRAME))
				{
					// calculate how much to distribute per frame in
					// order to make up for this keyframe's overflow

					codec->twopass.overflow += codec->twopass.KFoverflow;
					codec->twopass.KFoverflow = codec->twopass.desired_bytes2 - frame->length;

					tempdiv = (codec->twopass.keyframe_locations[codec->twopass.KF_idx] -
						codec->twopass.keyframe_locations[codec->twopass.KF_idx - 1]);

					if (tempdiv > 1)
					{
						// non-consecutive keyframes
						codec->twopass.KFoverflow_partial = codec->twopass.KFoverflow / (tempdiv - 1);
					}
					else
					{
						// consecutive keyframes
						codec->twopass.overflow += codec->twopass.KFoverflow;
						codec->twopass.KFoverflow = 0;
						codec->twopass.KFoverflow_partial = 0;
					}
					codec->twopass.KF_idx++;
				}
				else
				{
					// distribute part of the keyframe overflow

					codec->twopass.overflow += codec->twopass.desired_bytes2 - frame->length +
						codec->twopass.KFoverflow_partial;
					codec->twopass.KFoverflow -= codec->twopass.KFoverflow_partial;
				}
			}
			else
			{
				codec->twopass.overflow += codec->twopass.desired_bytes2 - frame->length;

				// ugly fix for credits..
				codec->twopass.overflow += codec->twopass.KFoverflow;
				codec->twopass.KFoverflow = 0;
				codec->twopass.KFoverflow_partial = 0;
				// end of ugly fix.
			}
		}

		frame_type="inter";
		if (frame->intra==1) {
			frame_type="intra";
		}
		else if (codec->twopass.nns1.dd_v & NNSTATS_BFRAME) {
			frame_type="bframe";
		}
		else if (codec->twopass.nns1.dd_v & NNSTATS_SKIPFRAME) {
			frame_type="skipped";
			frame->quant = 2;
			codec->twopass.bytes1 = 1;
			codec->twopass.desired_bytes2 = 1;
			frame->length = 1;
		}
		else if (codec->twopass.nns1.dd_v & NNSTATS_PADFRAME) {
			frame_type="padded";
			frame->quant = 2;
			codec->twopass.bytes1 = 7;
			codec->twopass.desired_bytes2 = 7;
			frame->length = 7;
		}
		else if (codec->twopass.nns1.dd_v & NNSTATS_DELAYFRAME) {
			frame_type="delayed";
			frame->quant = 2;
			codec->twopass.bytes1 = 1;
			codec->twopass.desired_bytes2 = 1;
			frame->length = 1;
		}

		DEBUG2ND(frame->quant, quant_type, frame_type, codec->twopass.bytes1, codec->twopass.desired_bytes2, frame->length, codec->twopass.overflow, credits_pos)
		break;

	default:
		break;
	}

	return ICERR_OK;
}

void codec_2pass_finish(CODEC* codec)
{
	int i;
	char s[100];

	if (codec->twopass.nns1_array)
	{
		free(codec->twopass.nns1_array);
		codec->twopass.nns1_array = NULL;
	}
	if (codec->twopass.nns2_array)
	{
		free(codec->twopass.nns2_array);
		codec->twopass.nns2_array = NULL;
	}
	codec->twopass.nns_array_size = 0;
	codec->twopass.nns_array_length = 0;
	codec->twopass.nns_array_pos = 0;

	if (codec->config.mode == DLG_MODE_2PASS_2_EXT || codec->config.mode == DLG_MODE_2PASS_2_INT)
	{
		// output the quantizer distribution for this encode.

		OutputDebugString("Quantizer distribution for 2nd pass:");
		for (i=1; i<=31; i++)
		{
			if (codec->twopass.quant_count[i])
			{
				wsprintf(s, "Q:%i:%i", i, codec->twopass.quant_count[i]);
				OutputDebugString(s);
			}
		}
		return;
	}
}

