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

	int	frames = 0, credits_frames = 0, i_frames = 0;
	__int64 total_ext = 0, total = 0, i_total = 0, i_boost_total = 0, start = 0, end = 0, start_curved = 0, end_curved = 0;
	__int64 desired = (__int64)codec->config.desired_size * 1024;

	double total1 = 0.0;
	double total2 = 0.0;

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

				if (!codec_is_in_credits(&codec->config, frames))
				{
					if (twopass->nns1.quant & NNSTATS_KEYFRAME)
					{
						i_boost_total += twopass->nns2.bytes * codec->config.keyframe_boost / 100;
						i_total += twopass->nns2.bytes;
						twopass->keyframe_locations[i_frames] = frames;
						++i_frames;
					}

					total += twopass->nns1.bytes;
					total_ext += twopass->nns2.bytes;
				}
				else
					++credits_frames;

				++frames;
			}
			twopass->keyframe_locations[i_frames] = frames;

			twopass->movie_curve = ((double)(total_ext + i_boost_total) / total_ext);
			twopass->average_frame = ((double)(total_ext - i_total) / (frames - credits_frames - i_frames) / twopass->movie_curve);

			SetFilePointer(twopass->stats1, sizeof(DWORD), 0, FILE_BEGIN);
			SetFilePointer(twopass->stats2, sizeof(DWORD), 0, FILE_BEGIN);

			// perform prepass to compensate for over/undersizing
			frames = 0;

			if (codec->config.use_alt_curve)
			{
				twopass->alt_curve_low = twopass->average_frame - twopass->average_frame * (double)codec->config.alt_curve_low_dist / 100.0;
				twopass->alt_curve_low_diff = twopass->average_frame - twopass->alt_curve_low;
				twopass->alt_curve_high = twopass->average_frame + twopass->average_frame * (double)codec->config.alt_curve_high_dist / 100.0;
				twopass->alt_curve_high_diff = twopass->alt_curve_high - twopass->average_frame;
				if (codec->config.alt_curve_use_auto)
				{
					if (total > total_ext)
					{
						codec->config.alt_curve_min_rel_qual = (int)(100.0 - (100.0 - 100.0 / ((double)total / (double)total_ext)) * (double)codec->config.alt_curve_auto_str / 100.0);
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
							sin(DEG2RAD * (twopass->average_frame * 90.0 / twopass->alt_curve_low_diff)));
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							sin(DEG2RAD * (twopass->average_frame * 90.0 / twopass->alt_curve_low_diff));
						break;
					case 1: // Linear (medium aggressiveness)
						twopass->alt_curve_qual_dev *= 2.0 / (1.0 +
							twopass->average_frame / twopass->alt_curve_low_diff);
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							twopass->average_frame / twopass->alt_curve_low_diff;
						break;
					case 0: // Cosine Curve (low aggressiveness)
						twopass->alt_curve_qual_dev *= 2.0 / (1.0 + 
							(1.0 - cos(DEG2RAD * (twopass->average_frame * 90.0 / twopass->alt_curve_low_diff))));
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							(1.0 - cos(DEG2RAD * (twopass->average_frame * 90.0 / twopass->alt_curve_low_diff)));
					}
				}
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

				if (!codec_is_in_credits(&codec->config, frames) &&
					!(twopass->nns1.quant & NNSTATS_KEYFRAME))
				{
					double dbytes = twopass->nns2.bytes / twopass->movie_curve;
					total1 += dbytes;

					if (codec->config.use_alt_curve)
					{
						if (dbytes > twopass->average_frame)
						{
							if (dbytes >= twopass->alt_curve_high)
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev);
							else
							{
								switch(codec->config.alt_curve_type)
								{
								case 2:
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									sin(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_high_diff)));
									break;
								case 1:
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(dbytes - twopass->average_frame) / twopass->alt_curve_high_diff);
									break;
								case 0:
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_high_diff))));
								}
							}
						}
						else
						{
							if (dbytes <= twopass->alt_curve_low)
								total2 += dbytes;
							else
							{
								switch(codec->config.alt_curve_type)
								{
								case 2:
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									sin(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_low_diff)));
									break;
								case 1:
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(dbytes - twopass->average_frame) / twopass->alt_curve_low_diff);
									break;
								case 0:
								total2 += dbytes * (twopass->alt_curve_mid_qual + twopass->alt_curve_qual_dev *
									(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_low_diff))));
								}
							}
						}
					}
					else
					{
						if (dbytes > twopass->average_frame)
						{
							total2 += ((double)dbytes + (twopass->average_frame - dbytes) *
								codec->config.curve_compression_high / 100.0);
						}
						else
						{
							total2 += ((double)dbytes + (twopass->average_frame - dbytes) *
								codec->config.curve_compression_low / 100.0);
						}
					}
				}

				++frames;
			}

			twopass->curve_comp_scale = total1 / total2;

			if (!codec->config.use_alt_curve)
			{
				int asymmetric_average_frame;
				char s[100];

				asymmetric_average_frame = (int)(twopass->average_frame * twopass->curve_comp_scale);
				wsprintf(s, "middle frame size for asymmetric curve compression: %i", asymmetric_average_frame);
				DEBUG2P(s);
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
					total += twopass->nns1.bytes * codec->config.keyframe_boost / 100;
					twopass->keyframe_locations[i_frames] = frames;
					++i_frames;
				}

				total += twopass->nns1.bytes;

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
					((double)total / desired) * ((double)100 / codec->config.credits_rate);

				start_curved = (__int64)(start / twopass->credits_start_curve);
				end_curved = (__int64)(end / twopass->credits_end_curve);

				// movie curve = (total - credits) / (desired_size - curved credits)
				twopass->movie_curve = (double)
					(total - start - end) /
					(desired - start_curved - end_curved);

				break;

			case CREDITS_MODE_QUANT :

				// movie curve = (total - credits) / (desired_size - credits)
				twopass->movie_curve = (double)
					(total - start - end) / (desired - start - end);

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
					(total - start - end) /
					(desired - start_curved - end_curved);

				break;
			}

			// average frame size = (desired - curved credits - curved keyframes) /
			//	(frames - credits frames - keyframes)
			twopass->average_frame = (double)
				(desired - start_curved - end_curved - (i_total / twopass->movie_curve)) /
				(frames - credits_frames - i_frames);

			SetFilePointer(twopass->stats1, sizeof(DWORD), 0, FILE_BEGIN);

			// perform prepass to compensate for over/undersizing
			frames = 0;

			if (codec->config.use_alt_curve)
			{
				twopass->alt_curve_low = twopass->average_frame - twopass->average_frame * (double)codec->config.alt_curve_low_dist / 100.0;
				twopass->alt_curve_low_diff = twopass->average_frame - twopass->alt_curve_low;
				twopass->alt_curve_high = twopass->average_frame + twopass->average_frame * (double)codec->config.alt_curve_high_dist / 100.0;
				twopass->alt_curve_high_diff = twopass->alt_curve_high - twopass->average_frame;
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
							sin(DEG2RAD * (twopass->average_frame * 90.0 / twopass->alt_curve_low_diff)));
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							sin(DEG2RAD * (twopass->average_frame * 90.0 / twopass->alt_curve_low_diff));
						break;
					case 1: // Linear (medium aggressiveness)
						twopass->alt_curve_qual_dev *= 2.0 / (1.0 +
							twopass->average_frame / twopass->alt_curve_low_diff);
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							twopass->average_frame / twopass->alt_curve_low_diff;
						break;
					case 0: // Cosine Curve (low aggressiveness)
						twopass->alt_curve_qual_dev *= 2.0 / (1.0 + 
							(1.0 - cos(DEG2RAD * (twopass->average_frame * 90.0 / twopass->alt_curve_low_diff))));
						twopass->alt_curve_mid_qual = 1.0 - twopass->alt_curve_qual_dev *
							(1.0 - cos(DEG2RAD * (twopass->average_frame * 90.0 / twopass->alt_curve_low_diff)));
					}
				}
			}

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

				if (!codec_is_in_credits(&codec->config, frames) &&
					!(twopass->nns1.quant & NNSTATS_KEYFRAME))
				{
					double dbytes = twopass->nns1.bytes / twopass->movie_curve;
					total1 += dbytes;

					if (codec->config.use_alt_curve)
					{
						if (dbytes > twopass->average_frame)
						{
							if (dbytes >= twopass->alt_curve_high)
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev);
							else
							{
								switch(codec->config.alt_curve_type)
								{
								case 2:
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									sin(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_high_diff)));
									break;
								case 1:
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(dbytes - twopass->average_frame) / twopass->alt_curve_high_diff);
									break;
								case 0:
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_high_diff))));
								}
							}
						}
						else
						{
							if (dbytes <= twopass->alt_curve_low)
								total2 += dbytes;
							else
							{
								switch(codec->config.alt_curve_type)
								{
								case 2:
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									sin(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_low_diff)));
									break;
								case 1:
								total2 += dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
									(dbytes - twopass->average_frame) / twopass->alt_curve_low_diff);
									break;
								case 0:
								total2 += dbytes * (twopass->alt_curve_mid_qual + twopass->alt_curve_qual_dev *
									(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_low_diff))));
								}
							}
						}
					}
					else
					{
						if (dbytes > twopass->average_frame)
						{
							total2 += ((double)dbytes + (twopass->average_frame - dbytes) *
								codec->config.curve_compression_high / 100.0);
						}
						else
						{
							total2 += ((double)dbytes + (twopass->average_frame - dbytes) *
								codec->config.curve_compression_low / 100.0);
						}
					}
				}

				++frames;
			}

			twopass->curve_comp_scale = total1 / total2;

			if (!codec->config.use_alt_curve)
			{
				int asymmetric_average_frame;
				char s[100];

				asymmetric_average_frame = (int)(twopass->average_frame * twopass->curve_comp_scale);
				wsprintf(s, "middle frame size for asymmetric curve compression: %i", asymmetric_average_frame);
				DEBUG2P(s);
			}

			SetFilePointer(twopass->stats1, sizeof(DWORD), 0, FILE_BEGIN);
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

				wsprintf(s, "avg scaled framesize:%i", (int)(twopass->average_frame));
				DEBUG2P(s);

				wsprintf(s, "bias bonus:%i bytes", (int)(twopass->curve_bias_bonus));
				DEBUG2P(s);

				for (i=1; i <= (int)(twopass->alt_curve_high*2)+1; i++)
				{
					dbytes = i;
					if (dbytes > twopass->average_frame)
					{
						if (dbytes >= twopass->alt_curve_high)
							curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev);
						else
						{
							switch(codec->config.alt_curve_type)
							{
							case 2:
							curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
								sin(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_high_diff)));
								break;
							case 1:
							curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
								(dbytes - twopass->average_frame) / twopass->alt_curve_high_diff);
								break;
							case 0:
							curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
								(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_high_diff))));
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
								sin(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_low_diff)));
								break;
							case 1:
							curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
								(dbytes - twopass->average_frame) / twopass->alt_curve_low_diff);
								break;
							case 0:
							curve_temp = dbytes * (twopass->alt_curve_mid_qual + twopass->alt_curve_qual_dev *
								(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_low_diff))));
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
							percent = (int)((i - twopass->average_frame) * 100.0 / twopass->average_frame);
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


int codec_2pass_get_quant(CODEC* codec, XVID_ENC_FRAME* frame)
{
	static double quant_error[32];
	static double curve_comp_error;
	static int last_quant;

	TWOPASS * twopass = &codec->twopass;

	DWORD read;
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
			quant_error[i] = 0.0;
			twopass->quant_count[i] = 0;
		}

		curve_comp_error = 0.0;
		last_quant = 0;
	}

	if (ReadFile(twopass->stats1, &twopass->nns1, sizeof(NNSTATS), &read, 0) == 0 || read != sizeof(NNSTATS)) 
	{
		DEBUGERR("2ndpass quant: couldn't read from stats1");
		return ICERR_ERROR;	
	}
	if (codec->config.mode == DLG_MODE_2PASS_2_EXT)
	{
		if (ReadFile(twopass->stats2, &twopass->nns2, sizeof(NNSTATS), &read, 0) == 0 || read != sizeof(NNSTATS)) 
		{
			DEBUGERR("2ndpass quant: couldn't read from stats2");
			return ICERR_ERROR;	
		}
	}
		
	bytes1 = twopass->nns1.bytes;
	overflow = twopass->overflow / 8;

	// override codec i-frame choice (reenable in credits)
	frame->intra = (twopass->nns1.quant & NNSTATS_KEYFRAME);

	if (frame->intra)
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
			else
				bytes2 = twopass->nns2.bytes;
		}
	}
	else	// Foxer: apply curve compression outside credits
	{
		double dbytes, curve_temp;

		bytes2 = (codec->config.mode == DLG_MODE_2PASS_2_INT) ? bytes1 : twopass->nns2.bytes;

		if (frame->intra)
		{
			dbytes = ((int)(bytes2 + bytes2 * codec->config.keyframe_boost / 100)) /
				twopass->movie_curve;
		}
		else
		{
			dbytes = bytes2 / twopass->movie_curve;
		}

		// spread the compression error across payback_delay frames
		if (codec->config.bitrate_payback_method == 0)
		{
			bytes2 = (int)(curve_comp_error / codec->config.bitrate_payback_delay);
		}
		else
		{
			bytes2 = (int)(curve_comp_error * dbytes /
				twopass->average_frame / codec->config.bitrate_payback_delay);

			if (labs(bytes2) > fabs(curve_comp_error))
			{
				bytes2 = (int)curve_comp_error;
			}
		}

		curve_comp_error -= bytes2;

		if (codec->config.use_alt_curve)
		{
			if (!frame->intra)
			{
				if (dbytes > twopass->average_frame)
				{
					if (dbytes >= twopass->alt_curve_high)
						curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev);
					else
					{
						switch(codec->config.alt_curve_type)
						{
						case 2:
						curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
							sin(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_high_diff)));
							break;
						case 1:
						curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
							(dbytes - twopass->average_frame) / twopass->alt_curve_high_diff);
							break;
						case 0:
						curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
							(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_high_diff))));
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
							sin(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_low_diff)));
							break;
						case 1:
						curve_temp = dbytes * (twopass->alt_curve_mid_qual - twopass->alt_curve_qual_dev *
							(dbytes - twopass->average_frame) / twopass->alt_curve_low_diff);
							break;
						case 0:
						curve_temp = dbytes * (twopass->alt_curve_mid_qual + twopass->alt_curve_qual_dev *
							(1.0 - cos(DEG2RAD * ((dbytes - twopass->average_frame) * 90.0 / twopass->alt_curve_low_diff))));
						}
					}
				}
				curve_temp = curve_temp * twopass->curve_comp_scale + twopass->curve_bias_bonus;

				bytes2 += ((int)curve_temp);
				curve_comp_error += curve_temp - ((int)curve_temp);
			}
			else
			{
				curve_comp_error += dbytes - ((int)dbytes);
				bytes2 += ((int)dbytes);
			}
		}
		else if ((codec->config.curve_compression_high + codec->config.curve_compression_low) &&
			!frame->intra)
		{
			if (dbytes > twopass->average_frame)
			{
				curve_temp = twopass->curve_comp_scale *
					((double)dbytes + (twopass->average_frame - dbytes) *
					codec->config.curve_compression_high / 100.0);
			}
			else
			{
				curve_temp = twopass->curve_comp_scale *
					((double)dbytes + (twopass->average_frame - dbytes) *
					codec->config.curve_compression_low / 100.0);
			}

			bytes2 += ((int)curve_temp);
			curve_comp_error += curve_temp - ((int)curve_temp);
		}
		else
		{
			curve_comp_error += dbytes - ((int)dbytes);
			bytes2 += ((int)dbytes);
		}

		// cap bytes2 to first pass size, lowers number of quant=1 frames
		if (bytes2 > bytes1)
		{
			curve_comp_error += bytes2 - bytes1;
			bytes2 = bytes1;
		}
		else if (bytes2 < 1)
		{
			curve_comp_error += --bytes2;
			bytes2 = 1;
		}
	}

	twopass->desired_bytes2 = bytes2;

	// if this keyframe is too close to the next,
	// reduce it's byte allotment
	if (frame->intra && !credits_pos)
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
	overflow = (int)((double)overflow * bytes2 / twopass->average_frame);

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

	if (bytes2 < 1) 
	{
		bytes2 = 1;
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
	else if (!frame->intra)
	{
		// Foxer: aid desired quantizer precision by accumulating decision error
		quant_error[frame->quant] += ((double)((twopass->nns1.quant & ~NNSTATS_KEYFRAME) * 
			bytes1) / bytes2) - frame->quant;

		if (quant_error[frame->quant] >= 1.0)
		{
			quant_error[frame->quant] -= 1.0;
			++frame->quant;
		}
	}

	// we're done with credits
	if (codec_is_in_credits(&codec->config, codec->framenum))
	{
		return ICERR_OK;
	}

	if (frame->intra)
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
		if (last_quant && capped_to_max_framesize == 0)
		{
			if (frame->quant > last_quant + 2)
			{
				frame->quant = last_quant + 2;
				DEBUG2P("P-frame quantizer prevented from rising too steeply");
			}
			if (frame->quant < last_quant - 2)
			{
				frame->quant = last_quant - 2;
				DEBUG2P("P-frame quantizer prevented from falling too steeply");
			}
		}
	}

	if (capped_to_max_framesize == 0)
		last_quant = frame->quant;

	if (codec->config.quant_type == QUANT_MODE_MOD)
	{
		frame->general |= (frame->quant < 4) ? XVID_MPEGQUANT : XVID_H263QUANT;
		frame->general &= (frame->quant < 4) ? ~XVID_H263QUANT : ~XVID_MPEGQUANT;
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
		nns1.dd_v = stats->hlength;	// header bytes

		nns1.dd_u = nns1.dd_y = 0;
		nns1.dk_v = nns1.dk_u = nns1.dk_y = 0;
		nns1.md_u = nns1.md_y = 0;
		nns1.mk_u = nns1.mk_y = 0;

//		nns1.quant = stats->quant;
		nns1.quant = 2;				// ugly fix for lumi masking in 1st pass returning new quant
		if (frame->intra) 
		{
			nns1.quant |= NNSTATS_KEYFRAME;
		}
		nns1.kblk = stats->kblks;
		nns1.mblk = stats->mblks;
		nns1.ublk = stats->ublks;
		nns1.lum_noise[0] = nns1.lum_noise[1] = 1;

		total_size += frame->length;

		DEBUG1ST(frame->length, (int)total_size/1024, frame->intra, frame->quant, quant_type, stats->kblks, stats->mblks)
		
		if (WriteFile(codec->twopass.stats1, &nns1, sizeof(NNSTATS), &wrote, 0) == 0 || wrote != sizeof(NNSTATS))
		{
			DEBUGERR("stats1: WriteFile error");
			return ICERR_ERROR;	
		}
		break;

	case DLG_MODE_2PASS_2_INT :
	case DLG_MODE_2PASS_2_EXT :
		credits_pos = codec_is_in_credits(&codec->config, codec->framenum);
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

		DEBUG2ND(frame->quant, quant_type, frame->intra, codec->twopass.bytes1, codec->twopass.desired_bytes2, frame->length, codec->twopass.overflow, credits_pos)
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