/******************************************************************************
 *
 * XviD Video-for-Windows Frontend
 * Quantizer histogram and encoding status window
 *
 * Copyright (C) 2003 Peter Ross <pross@xvid.org>
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
 * $Id: status.c,v 1.1.2.3 2004-01-22 14:43:39 syskin Exp $
 *
 *****************************************************************************/


#include <windows.h>
#include "resource.h"
#include "codec.h"
#include "status.h"

#include "debug.h"


#define CLR_BG			0
#define CLR_FG			1
#define CLR_QUANT_I		2
#define CLR_QUANT_P		3
#define CLR_QUANT_B		4

static void set_bic(RGBQUAD * rgb, int index, int r, int g, int b)
{
	rgb[index].rgbRed = r;
	rgb[index].rgbGreen = g;
	rgb[index].rgbBlue = b;
}

/*
	draw graph into buffer
*/
static void draw_graph(status_t *s)
{
	unsigned int i,j;

	memset(s->buffer, CLR_BG, s->width*s->stride);

	for (j=0; j<s->height; j++)
	for (i=0; i<31; i++)
		s->buffer[ j*s->stride + i*s->width31 ] = CLR_FG;

	if (s->count[0]>0) {
		for (i=0; i<31; i++) {
			/* i-vops */
			unsigned int j_height = (s->height-s->tm.tmHeight)*s->quant[0][i]/s->max_quant_frames;
			if (j_height==0 && s->quant[1][i]>0) j_height=1;

			for(j=0; j < j_height; j++) {
				memset(s->buffer + (s->tm.tmHeight+j)*s->stride + i*s->width31 + 1,
						CLR_QUANT_I, s->width31-1); 
			}
			/* p/s-vops */
			j_height += (s->height-s->tm.tmHeight)*s->quant[1][i]/s->max_quant_frames;
			if (j_height==0 && s->quant[1][i]>0) j_height=1;

			for(; j < j_height; j++) {
				memset(s->buffer + (s->tm.tmHeight+j)*s->stride + i*s->width31 + 1,
						CLR_QUANT_P, s->width31-1); 
			}
			/* b-vops */
			j_height += (s->height-s->tm.tmHeight)*s->quant[2][i]/s->max_quant_frames;
			if (j_height==0 && s->quant[2][i]>0) j_height=1;

			for(; j < j_height; j++) {
				memset(s->buffer + (s->tm.tmHeight+j)*s->stride + i*s->width31 + 1,
						CLR_QUANT_B, s->width31-1); 
			}
		}
	}
}


static const char * number[31] = {
	"1", "2", "3", "4", "5", "6", "7", "8", "9",
	"10","11","12","13","14","15","16","17","18","19",
	"20","21","22","23","24","25","26","27","28","29",
	"30","31"
};


/* status window proc handlder */

static BOOL CALLBACK status_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	status_t * s = (status_t*)GetWindowLong(hDlg, GWL_USERDATA);

	switch (uMsg)
	{
	case WM_INITDIALOG :
		SetWindowLong(hDlg, GWL_USERDATA, lParam);
		s = (status_t*)lParam;
		
		s->hGraph = GetDlgItem(hDlg, IDC_STATUS_GRAPH);
		s->hDc = GetDC(s->hGraph);
		{
			RECT rect;
			GetWindowRect(s->hGraph, &rect);
			s->width = rect.right - rect.left;  
			s->height = rect.bottom - rect.top;
		}
		s->width31 = s->width/31;
		s->stride = (s->width/4+1)*4;

		s->buffer = malloc(s->width * s->stride);

		s->bi = malloc(sizeof(BITMAPINFOHEADER) + 256*sizeof(RGBQUAD));
		memset(s->bi, 0, sizeof(BITMAPINFOHEADER) + 256*sizeof(RGBQUAD));

		s->bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		s->bi->bmiHeader.biWidth  = s->stride;
		s->bi->bmiHeader.biHeight = s->height;
		s->bi->bmiHeader.biPlanes = 1;
		s->bi->bmiHeader.biBitCount = 8;
		s->bi->bmiHeader.biCompression = BI_RGB;

		set_bic(s->bi->bmiColors, CLR_BG,	   0,   0,   0);
		set_bic(s->bi->bmiColors, CLR_FG,	 128, 128, 128);
		set_bic(s->bi->bmiColors, CLR_QUANT_I,  255, 0,   0);
		set_bic(s->bi->bmiColors, CLR_QUANT_P,  0, 0,   255);
		set_bic(s->bi->bmiColors, CLR_QUANT_B,  0, 192,   0);

		SelectObject(s->hDc, GetStockObject(SYSTEM_FONT));
		SetBkColor(s->hDc, *(DWORD*)&s->bi->bmiColors[CLR_BG]);
		SetTextColor(s->hDc, *(DWORD*)&s->bi->bmiColors[CLR_FG]);
		GetTextMetrics(s->hDc, &s->tm);

		draw_graph(s);
		SetTimer(hDlg, IDC_STATUS_GRAPH, 1000, NULL);	/* 1 second */
 		break;

	case WM_DESTROY :
		free(s->buffer);
		free(s->bi);
		KillTimer(hDlg, IDC_STATUS_GRAPH);
		s->hDlg = NULL;
		break;

	case WM_DRAWITEM :
		if (wParam==IDC_STATUS_GRAPH) {
			int i;

			/* copy buffer into dc */
			SetDIBitsToDevice(s->hDc, 
				0, 0, s->width, s->height,
				0, 0, 0, s->height,
				s->buffer, s->bi, DIB_RGB_COLORS);

			SetTextAlign(s->hDc, GetTextAlign(s->hDc)|TA_CENTER);

			for (i=0; i<31; i++) {
				TextOut(s->hDc, i*s->width31 + s->width/62, 
					s->height-s->tm.tmHeight, number[i], strlen(number[i]));
			}
		}
		break;

	case WM_TIMER :
		if (wParam==IDC_STATUS_GRAPH) {

			SetDlgItemInt(hDlg, IDC_STATUS_I_NUM, s->count[1], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_P_NUM, s->count[2], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_B_NUM, s->count[3], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_NUM, s->count[0], FALSE);
			
			SetDlgItemInt(hDlg, IDC_STATUS_IQ_MIN, s->min_quant[1], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_IQ_MAX, s->max_quant[1], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_PQ_MIN, s->min_quant[2], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_PQ_MAX, s->max_quant[2], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_BQ_MIN, s->min_quant[3], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_BQ_MAX, s->max_quant[3], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_Q_MIN, s->min_quant[0], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_Q_MAX, s->max_quant[0], FALSE);

			SetDlgItemInt(hDlg, IDC_STATUS_IL_MIN, s->min_length[1], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_IL_MAX, s->max_length[1], FALSE);
			if (s->count[1]>0)
				SetDlgItemInt(hDlg, IDC_STATUS_IL_AVG, s->tot_length[1]/s->count[1], FALSE);
			else
				SetDlgItemInt(hDlg, IDC_STATUS_IL_AVG, 0, FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_IL_TOT, s->tot_length[1]/1024, FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_PL_MIN, s->min_length[2], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_PL_MAX, s->max_length[2], FALSE);
			if (s->count[2]>0)
				SetDlgItemInt(hDlg, IDC_STATUS_PL_AVG, s->tot_length[2]/s->count[2], FALSE);
			else
				SetDlgItemInt(hDlg, IDC_STATUS_PL_AVG, 0, FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_PL_TOT, s->tot_length[2]/1024, FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_BL_MIN, s->min_length[3], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_BL_MAX, s->max_length[3], FALSE);
			if (s->count[3]>0)
				SetDlgItemInt(hDlg, IDC_STATUS_BL_AVG, s->tot_length[3]/s->count[3], FALSE);
			else
				SetDlgItemInt(hDlg, IDC_STATUS_BL_AVG, 0, FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_BL_TOT, s->tot_length[3]/1024, FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_L_MIN, s->min_length[0], FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_L_MAX, s->max_length[0], FALSE);
			if (s->count[0]>0)
				SetDlgItemInt(hDlg, IDC_STATUS_L_AVG, s->tot_length[0]/s->count[0], FALSE);
			else
				SetDlgItemInt(hDlg, IDC_STATUS_L_AVG, 0, FALSE);
			SetDlgItemInt(hDlg, IDC_STATUS_L_TOT, s->tot_length[0]/1024, FALSE);

			if (s->count[0]>0) {
				uint64_t kbits = 8*s->tot_length[0]/1000;
				double secs = (double)s->count[0]/s->fps;
			   SetDlgItemInt(hDlg, IDC_STATUS_KBPS, (int)(kbits/secs), FALSE);
			}else{
				SetDlgItemInt(hDlg, IDC_STATUS_KBPS, 0, FALSE);
			}

			draw_graph(s);
			InvalidateRect(s->hGraph, NULL, FALSE);
		}
		break;

	case WM_COMMAND :
		if (LOWORD(wParam)==IDCANCEL) {
			DestroyWindow(hDlg);
		}
		break;

	default :
		return FALSE;
	}

	return TRUE;
}


/* destroy status window
   (however if the auto-close box is unchecked, dont destroy) */

void status_destroy(status_t *s)
{
	if (s->hDlg && IsDlgButtonChecked(s->hDlg,IDC_STATUS_DESTROY)==BST_CHECKED) {
		DestroyWindow(s->hDlg);
	}
}


/* destroy status window, alwasys */

void status_destroy_always(status_t *s)
{
	if (s->hDlg) {
		DestroyWindow(s->hDlg);
	}
}


/* create status window */
void status_create(status_t * s, unsigned int fps_inc, unsigned int fps_base)
{
	int i;

	s->fps = fps_base/fps_inc;

	memset(s->quant, 0, 31*sizeof(int));
	s->max_quant_frames = 0;
	for (i=0; i<4; i++) {
		s->count[i] = 0;
		s->min_quant[i] = s->max_quant[i] = 0;
		s->min_length[i] = s->max_length[i] = 0;
		s->tot_length[i] = 0;
	}

	s->hDlg = CreateDialogParam(g_hInst, 
							MAKEINTRESOURCE(IDD_STATUS),
							GetDesktopWindow(),
							status_proc, (LPARAM)s);

	ShowWindow(s->hDlg, SW_SHOW);
}


/* feed stats info into the window */
void status_update(status_t *s, int type, int length, int quant)
{
	if (type == 4) type = 2; /* XVID_TYPE_SVOP to XVID_TYPE_PVOP */
	s->count[0]++;
	s->count[type]++;

	if (s->min_quant[0]==0 || quant<s->min_quant[0]) s->min_quant[0] = quant;
	if (s->max_quant[0]==0 || quant>s->max_quant[0]) s->max_quant[0] = quant;
	if (s->min_quant[type]==0 || quant<s->min_quant[type]) s->min_quant[type] = quant;
	if (s->max_quant[type]==0|| quant>s->max_quant[type]) s->max_quant[type] = quant;

	s->quant[type-1][quant-1]++;
	if (s->quant[0][quant-1] + s->quant[1][quant-1] + s->quant[2][quant-1] > s->max_quant_frames)
		s->max_quant_frames = s->quant[0][quant-1] + s->quant[1][quant-1] + s->quant[2][quant-1];

	if (s->min_length[0]==0 || length<s->min_length[0]) s->min_length[0] = length;
	if (s->max_length[0]==0 || length>s->max_length[0]) s->max_length[0] = length;
	if (s->min_length[type]==0 || length<s->min_length[type]) s->min_length[type] = length;
	if (s->max_length[type]==0|| length>s->max_length[type]) s->max_length[type] = length;
	s->tot_length[0] += length;
	s->tot_length[type] += length;
}
