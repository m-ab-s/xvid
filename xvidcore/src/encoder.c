// 14.04.2002	added FrameCodeB()

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "encoder.h"
#include "prediction/mbprediction.h"
#include "global.h"
#include "utils/timer.h"
#include "image/image.h"
#include "bitstream/cbp.h"
#include "utils/mbfunctions.h"
#include "bitstream/bitstream.h"
#include "bitstream/mbcoding.h"
#include "utils/ratecontrol.h"
#include "utils/emms.h"
#include "bitstream/mbcoding.h"
#include "quant/adapt_quant.h"
#include "quant/quant_matrix.h"
#include "utils/mem_align.h"

#define ENC_CHECK(X) if(!(X)) return XVID_ERR_FORMAT
#define SWAP(A,B)    { void * tmp = A; A = B; B = tmp; }


static int FrameCodeI(Encoder * pEnc, Bitstream * bs, uint32_t *pBits);
static int FrameCodeP(Encoder * pEnc, Bitstream * bs, uint32_t *pBits, bool force_inter, bool vol_header);

static int DQtab[4] = 
{
	-1, -2, 1, 2
};

static int iDQtab[5] = 
{
	1, 0, NO_CHANGE, 2, 3
};


void static image_null(IMAGE * image)
{
	image->y = image->u = image->v = NULL;
}


int encoder_create(XVID_ENC_PARAM * pParam)
{
	Encoder *pEnc;
	uint32_t i;

	pParam->handle = NULL;

	ENC_CHECK(pParam);

	ENC_CHECK(pParam->width > 0 && pParam->width <= 1920);
	ENC_CHECK(pParam->height > 0 && pParam->height <= 1280);
	ENC_CHECK(!(pParam->width % 2));
	ENC_CHECK(!(pParam->height % 2));

	if (pParam->fincr <= 0 || pParam->fbase <= 0)
	{
		pParam->fincr = 1;
		pParam->fbase = 25;
	}

	// simplify the "fincr/fbase" fraction
	// (neccessary, since windows supplies us with huge numbers)

	i = pParam->fincr;
	while (i > 1)
	{
		if (pParam->fincr % i == 0 && pParam->fbase % i == 0)
		{
			pParam->fincr /= i;
			pParam->fbase /= i;
			i = pParam->fincr;
			continue;
		}
		i--;
	}

	if (pParam->fbase > 65535)
	{
		float div = (float)pParam->fbase / 65535;
		pParam->fbase = (int)(pParam->fbase / div);
		pParam->fincr = (int)(pParam->fincr / div);
	}

	if (pParam->rc_bitrate <= 0)
		pParam->rc_bitrate = 900000;

	if (pParam->rc_reaction_delay_factor <= 0)
		pParam->rc_reaction_delay_factor = 16;

	if (pParam->rc_averaging_period <= 0)
		pParam->rc_averaging_period = 100;

	if (pParam->rc_buffer <= 0)
		pParam->rc_buffer = 100;

	if ((pParam->min_quantizer <= 0) || (pParam->min_quantizer > 31))
		pParam->min_quantizer = 1;

	if ((pParam->max_quantizer <= 0) || (pParam->max_quantizer > 31))
		pParam->max_quantizer = 31;

	if (pParam->max_key_interval == 0)		/* 1 keyframe each 10 seconds */ 
		pParam->max_key_interval = 10 * pParam->fincr / pParam->fbase;
					
	if (pParam->max_quantizer < pParam->min_quantizer)
		pParam->max_quantizer = pParam->min_quantizer;

	if ((pEnc = (Encoder *) xvid_malloc(sizeof(Encoder), CACHE_LINE)) == NULL)
		return XVID_ERR_MEMORY;

	/* Fill members of Encoder structure */

	pEnc->mbParam.width = pParam->width;
	pEnc->mbParam.height = pParam->height;

	pEnc->mbParam.mb_width = (pEnc->mbParam.width + 15) / 16;
	pEnc->mbParam.mb_height = (pEnc->mbParam.height + 15) / 16;

	pEnc->mbParam.edged_width = 16 * pEnc->mbParam.mb_width + 2 * EDGE_SIZE;
	pEnc->mbParam.edged_height = 16 * pEnc->mbParam.mb_height + 2 * EDGE_SIZE;

	pEnc->sStat.fMvPrevSigma = -1;

	/* Fill rate control parameters */

	pEnc->bitrate = pParam->rc_bitrate;

	pEnc->iFrameNum = 0;
	pEnc->iMaxKeyInterval = pParam->max_key_interval;

	/* try to allocate frame memory */

	pEnc->current = NULL;
	pEnc->reference = NULL;
	if ( (pEnc->current = xvid_malloc(sizeof(FRAMEINFO), CACHE_LINE)) == NULL ||
		 (pEnc->reference = xvid_malloc(sizeof(FRAMEINFO), CACHE_LINE)) == NULL)
	{
		if (pEnc->current) xvid_free(pEnc->current);
		xvid_free(pEnc);
		return XVID_ERR_MEMORY;
	}

	/* try to allocate mb memory */

	pEnc->current->mbs = NULL;
	pEnc->reference->mbs = NULL;

#ifdef _DEBUG
#ifdef WIN32
OutputDebugString("malloc mbs");
#endif
#endif 

	if ((pEnc->current->mbs = xvid_malloc(sizeof(MACROBLOCK) * pEnc->mbParam.mb_width * pEnc->mbParam.mb_height, CACHE_LINE)) == NULL ||
		(pEnc->reference->mbs = xvid_malloc(sizeof(MACROBLOCK) * pEnc->mbParam.mb_width * pEnc->mbParam.mb_height, CACHE_LINE)) == NULL)
	{
		if (pEnc->current->mbs) xvid_free(pEnc->current->mbs);
		xvid_free(pEnc->current);
		xvid_free(pEnc->reference);
		xvid_free(pEnc);
	}

	/* try to allocate image memory */

#ifdef _DEBUG
	image_null(&pEnc->sOriginal);
#endif
	image_null(&pEnc->current->image);
	image_null(&pEnc->reference->image);
	image_null(&pEnc->vInterH);
	image_null(&pEnc->vInterV);
	image_null(&pEnc->vInterVf);
	image_null(&pEnc->vInterHV);
	image_null(&pEnc->vInterHVf);
	
#ifdef _DEBUG
#ifdef WIN32
OutputDebugString("malloc images");
#endif
#endif
	if (
#ifdef _DEBUG
		image_create(&pEnc->sOriginal, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height) < 0 ||
#endif
		image_create(&pEnc->current->image, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height) < 0 ||
		image_create(&pEnc->reference->image, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height) < 0 ||
		image_create(&pEnc->vInterH, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height) < 0 ||
		image_create(&pEnc->vInterV, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height) < 0 ||
		image_create(&pEnc->vInterVf, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height) < 0 ||
		image_create(&pEnc->vInterHV, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height) < 0 ||
		image_create(&pEnc->vInterHVf, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height) < 0)
	{
#ifdef _DEBUG
		image_destroy(&pEnc->sOriginal, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
#endif
		image_destroy(&pEnc->current->image, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
		image_destroy(&pEnc->reference->image, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
		image_destroy(&pEnc->vInterH, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
		image_destroy(&pEnc->vInterV, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
		image_destroy(&pEnc->vInterVf, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
		image_destroy(&pEnc->vInterHV, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
		image_destroy(&pEnc->vInterHVf, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);

		xvid_free(pEnc->current);
		xvid_free(pEnc->reference);
		xvid_free(pEnc);
		return XVID_ERR_MEMORY;
	}

	pParam->handle = (void *)pEnc;

	if (pParam->rc_bitrate)
	{
		RateControlInit(pParam->rc_bitrate, pParam->rc_reaction_delay_factor,
			pParam->rc_averaging_period, pParam->rc_buffer, pParam->fbase * 1000 / pParam->fincr,
			pParam->max_quantizer, pParam->min_quantizer);
	}

	init_timer();

	return XVID_ERR_OK;
}


int encoder_destroy(Encoder * pEnc)
{
	ENC_CHECK(pEnc);

	image_destroy(&pEnc->current->image, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
	image_destroy(&pEnc->reference->image, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterH, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterV, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterVf, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterHV, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
	image_destroy(&pEnc->vInterHVf, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
#ifdef _DEBUG
		image_destroy(&pEnc->sOriginal, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height);
#endif
	xvid_free(pEnc->current->mbs);
	xvid_free(pEnc->current);

	xvid_free(pEnc->reference->mbs);
	xvid_free(pEnc->reference);

	xvid_free(pEnc);
	return XVID_ERR_OK;
}

int encoder_encode(Encoder * pEnc, XVID_ENC_FRAME * pFrame, XVID_ENC_STATS * pResult)
{
	uint16_t x, y;
	Bitstream bs;
	uint32_t bits;
	uint16_t write_vol_header = 0;
#ifdef _DEBUG
	float psnr;
	uint8_t temp[100];
#endif

	start_global_timer();

	ENC_CHECK(pEnc);
	ENC_CHECK(pFrame);
	ENC_CHECK(pFrame->bitstream);
	ENC_CHECK(pFrame->image);

	SWAP(pEnc->current, pEnc->reference);

	pEnc->current->global_flags = pFrame->general;
	pEnc->current->motion_flags = pFrame->motion;
	pEnc->mbParam.hint = &pFrame->hint;

	start_timer();
	if (image_input(&pEnc->current->image, pEnc->mbParam.width, pEnc->mbParam.height, pEnc->mbParam.edged_width,
			pFrame->image, pFrame->colorspace))
	{
		return XVID_ERR_FORMAT;
	}
	stop_conv_timer();

#ifdef _DEBUG
	image_copy(&pEnc->sOriginal, &pEnc->sCurrent, pEnc->mbParam.edged_width, pEnc->mbParam.height);
#endif

	EMMS();

	BitstreamInit(&bs, pFrame->bitstream, 0);

	if (pFrame->quant == 0)
	{
		pEnc->current->quant = RateControlGetQ(0);
	}
	else
	{
		pEnc->current->quant = pFrame->quant;
	}

	if ((pEnc->current->global_flags & XVID_LUMIMASKING))
	{
		int * temp_dquants = (int *) xvid_malloc(pEnc->mbParam.mb_width * pEnc->mbParam.mb_height * sizeof(int), CACHE_LINE);
		
		pEnc->current->quant = adaptive_quantization(pEnc->current->image.y,
							    pEnc->mbParam.edged_width,	// stride
							    temp_dquants,
							    pEnc->current->quant,
							    pEnc->current->quant,	// min_quant
							    2*pEnc->current->quant,	// max_quant
							    pEnc->mbParam.mb_width,
							    pEnc->mbParam.mb_height);
			
		for (y = 0; y < pEnc->mbParam.mb_height; y++)
			for (x = 0; x < pEnc->mbParam.mb_width; x++)
			{
				MACROBLOCK *pMB = &pEnc->current->mbs[x + y * pEnc->mbParam.mb_width];
				pMB->dquant = iDQtab[(temp_dquants[y * pEnc->mbParam.mb_width + x] + 2)];
			}
		xvid_free(temp_dquants);
	}

	if (pEnc->current->global_flags & XVID_H263QUANT) {
		if(pEnc->mbParam.m_quant_type != H263_QUANT)
			write_vol_header = 1;
		pEnc->mbParam.m_quant_type = H263_QUANT;
	}
	else if(pEnc->current->global_flags & XVID_MPEGQUANT) {
		int ret1, ret2;

		ret1 = ret2 = 0;

		if(pEnc->mbParam.m_quant_type != MPEG4_QUANT)
			write_vol_header = 1;
		
		pEnc->mbParam.m_quant_type = MPEG4_QUANT;
		
		if ((pEnc->current->global_flags & XVID_CUSTOM_QMATRIX) > 0) {
			if(pFrame->quant_intra_matrix != NULL)
				ret1 = set_intra_matrix(pFrame->quant_intra_matrix);
			if(pFrame->quant_inter_matrix != NULL)
				ret2 = set_inter_matrix(pFrame->quant_inter_matrix);
		}
		else {
			ret1 = set_intra_matrix(get_default_intra_matrix());
			ret2 = set_inter_matrix(get_default_inter_matrix());
		}
		if(write_vol_header == 0)
			write_vol_header = ret1 | ret2;
	}

	if (pFrame->intra < 0)
	{
		if ((pEnc->iFrameNum == 0) || ((pEnc->iMaxKeyInterval > 0) 
					       && (pEnc->iFrameNum >= pEnc->iMaxKeyInterval)))

			pFrame->intra = FrameCodeI(pEnc, &bs, &bits);
		else
			pFrame->intra = FrameCodeP(pEnc, &bs, &bits, 0, write_vol_header);
	}
	else
	{
		if (pFrame->intra == 1)
			pFrame->intra = FrameCodeI(pEnc, &bs, &bits);
		else
			pFrame->intra = FrameCodeP(pEnc, &bs, &bits, 1, write_vol_header);
	}

	BitstreamPutBits(&bs, 0xFFFF, 16);
	BitstreamPutBits(&bs, 0xFFFF, 16);
	BitstreamPad(&bs);
	pFrame->length = BitstreamLength(&bs);

	if (pResult)
	{
		pResult->quant = pEnc->current->quant;
		pResult->hlength = pFrame->length - (pEnc->sStat.iTextBits / 8);
		pResult->kblks = pEnc->sStat.kblks;
		pResult->mblks = pEnc->sStat.mblks;
		pResult->ublks = pEnc->sStat.ublks;
	}
   
	EMMS();

	if (pFrame->quant == 0)
	{
		RateControlUpdate(pEnc->current->quant, pFrame->length, pFrame->intra);
	}

#ifdef _DEBUG
	psnr = image_psnr(&pEnc->sOriginal, &pEnc->current->image, pEnc->mbParam.edged_width,
				pEnc->mbParam.width, pEnc->mbParam.height);

	sprintf(temp, "PSNR: %f\n", psnr);
	DEBUG(temp);
#endif

	pEnc->iFrameNum++;
	
	stop_global_timer();
	write_timer();

	return XVID_ERR_OK;
}


static __inline void CodeIntraMB(Encoder *pEnc, MACROBLOCK *pMB) {

	pMB->mode = MODE_INTRA;

	/* zero mv statistics */
	pMB->mvs[0].x = pMB->mvs[1].x = pMB->mvs[2].x = pMB->mvs[3].x = 0;
	pMB->mvs[0].y = pMB->mvs[1].y = pMB->mvs[2].y = pMB->mvs[3].y = 0;
	pMB->sad8[0] = pMB->sad8[1] = pMB->sad8[2] = pMB->sad8[3] = 0;
	pMB->sad16 = 0;

	if ((pEnc->current->global_flags & XVID_LUMIMASKING)) {
		if(pMB->dquant != NO_CHANGE)
		{
			pMB->mode = MODE_INTRA_Q;
			pEnc->current->quant += DQtab[pMB->dquant];
		
			if (pEnc->current->quant > 31) pEnc->current->quant = 31;
			if (pEnc->current->quant < 1) pEnc->current->quant = 1;
		}
	}

	pMB->quant = pEnc->current->quant;
}


#define FCODEBITS	3
#define MODEBITS	5

void HintedMESet(Encoder * pEnc, int * intra)
{
	HINTINFO * hint;
	Bitstream bs;
	int length, high;
	uint32_t x, y;

	hint = pEnc->mbParam.hint;

	if (hint->rawhints)
	{
		*intra = hint->mvhint.intra;
	}
	else
	{
		BitstreamInit(&bs, hint->hintstream, hint->hintlength);
		*intra = BitstreamGetBit(&bs);
	}

	if (*intra)
	{
		return;
	}

	pEnc->current->fcode = (hint->rawhints) ? hint->mvhint.fcode : BitstreamGetBits(&bs, FCODEBITS);

	length	= pEnc->current->fcode + 5;
	high	= 1 << (length - 1);

	for (y=0 ; y<pEnc->mbParam.mb_height ; ++y)
	{
		for (x=0 ; x<pEnc->mbParam.mb_width ; ++x)
		{
			MACROBLOCK * pMB = &pEnc->current->mbs[x + y * pEnc->mbParam.mb_width];
			MVBLOCKHINT * bhint = &hint->mvhint.block[x + y * pEnc->mbParam.mb_width];
			VECTOR pred[4];
			VECTOR tmp;
			int dummy[4];
			int vec;

			pMB->mode = (hint->rawhints) ? bhint->mode : BitstreamGetBits(&bs, MODEBITS);

			pMB->mode = (pMB->mode == MODE_INTER_Q) ? MODE_INTER : pMB->mode;
			pMB->mode = (pMB->mode == MODE_INTRA_Q) ? MODE_INTRA : pMB->mode;

			if (pMB->mode == MODE_INTER)
			{
				tmp.x  = (hint->rawhints) ? bhint->mvs[0].x : BitstreamGetBits(&bs, length);
				tmp.y  = (hint->rawhints) ? bhint->mvs[0].y : BitstreamGetBits(&bs, length);
				tmp.x -= (tmp.x >= high) ? high*2 : 0;
				tmp.y -= (tmp.y >= high) ? high*2 : 0;

				get_pmvdata(pEnc->current->mbs, x, y, pEnc->mbParam.mb_width, 0, pred, dummy);

				for (vec=0 ; vec<4 ; ++vec)
				{
					pMB->mvs[vec].x  = tmp.x;
					pMB->mvs[vec].y  = tmp.y;
					pMB->pmvs[vec].x = pMB->mvs[0].x - pred[0].x;
					pMB->pmvs[vec].y = pMB->mvs[0].y - pred[0].y;
				}
			}
			else if (pMB->mode == MODE_INTER4V)
			{
				for (vec=0 ; vec<4 ; ++vec)
				{
					tmp.x  = (hint->rawhints) ? bhint->mvs[vec].x : BitstreamGetBits(&bs, length);
					tmp.y  = (hint->rawhints) ? bhint->mvs[vec].y : BitstreamGetBits(&bs, length);
					tmp.x -= (tmp.x >= high) ? high*2 : 0;
					tmp.y -= (tmp.y >= high) ? high*2 : 0;

					get_pmvdata(pEnc->current->mbs, x, y, pEnc->mbParam.mb_width, vec, pred, dummy);

					pMB->mvs[vec].x  = tmp.x;
					pMB->mvs[vec].y  = tmp.y;
					pMB->pmvs[vec].x = pMB->mvs[vec].x - pred[0].x;
					pMB->pmvs[vec].y = pMB->mvs[vec].y - pred[0].y;
				}
			}
			else	// intra / stuffing / not_coded
			{
				for (vec=0 ; vec<4 ; ++vec)
				{
					pMB->mvs[vec].x  = pMB->mvs[vec].y  = 0;
				}
			}

			if (pMB->mode == MODE_INTER4V &&
				(pEnc->current->global_flags & XVID_LUMIMASKING) && pMB->dquant != NO_CHANGE)
			{
				pMB->mode = MODE_INTRA;

				for (vec=0 ; vec<4 ; ++vec)
				{
					pMB->mvs[vec].x = pMB->mvs[vec].y = 0;
				}
			}
		}
	}
}


void HintedMEGet(Encoder * pEnc, int intra)
{
	HINTINFO * hint;
	Bitstream bs;
	uint32_t x, y;
	int length, high;

	hint = pEnc->mbParam.hint;

	if (hint->rawhints)
	{
		hint->mvhint.intra = intra;
	}
	else
	{
		BitstreamInit(&bs, hint->hintstream, 0);
		BitstreamPutBit(&bs, intra);
	}

	if (intra)
	{
		if (!hint->rawhints)
		{
			BitstreamPad(&bs);
			hint->hintlength = BitstreamLength(&bs);
		}
		return;
	}

	length	= pEnc->current->fcode + 5;
	high	= 1 << (length - 1);

	if (hint->rawhints)
	{
		hint->mvhint.fcode = pEnc->current->fcode;
	}
	else
	{
		BitstreamPutBits(&bs, pEnc->current->fcode, FCODEBITS);
	}

	for (y=0 ; y<pEnc->mbParam.mb_height ; ++y)
	{
		for (x=0 ; x<pEnc->mbParam.mb_width ; ++x)
		{
			MACROBLOCK * pMB = &pEnc->current->mbs[x + y * pEnc->mbParam.mb_width];
			MVBLOCKHINT * bhint = &hint->mvhint.block[x + y * pEnc->mbParam.mb_width];
			VECTOR tmp;

			if (hint->rawhints)
			{
				bhint->mode = pMB->mode;
			}
			else
			{
				BitstreamPutBits(&bs, pMB->mode, MODEBITS);
			}

			if (pMB->mode == MODE_INTER || pMB->mode == MODE_INTER_Q)
			{
				tmp.x  = pMB->mvs[0].x;
				tmp.y  = pMB->mvs[0].y;
				tmp.x += (tmp.x < 0) ? high*2 : 0;
				tmp.y += (tmp.y < 0) ? high*2 : 0;

				if (hint->rawhints)
				{
					bhint->mvs[0].x = tmp.x;
					bhint->mvs[0].y = tmp.y;
				}
				else
				{
					BitstreamPutBits(&bs, tmp.x, length);
					BitstreamPutBits(&bs, tmp.y, length);
				}
			}
			else if (pMB->mode == MODE_INTER4V)
			{
				int vec;

				for (vec=0 ; vec<4 ; ++vec)
				{
					tmp.x  = pMB->mvs[vec].x;
					tmp.y  = pMB->mvs[vec].y;
					tmp.x += (tmp.x < 0) ? high*2 : 0;
					tmp.y += (tmp.y < 0) ? high*2 : 0;

					if (hint->rawhints)
					{
						bhint->mvs[vec].x = tmp.x;
						bhint->mvs[vec].y = tmp.y;
					}
					else
					{
						BitstreamPutBits(&bs, tmp.x, length);
						BitstreamPutBits(&bs, tmp.y, length);
					}
				}
			}
		}
	}

	if (!hint->rawhints)
	{
		BitstreamPad(&bs);
		hint->hintlength = BitstreamLength(&bs);
	}
}


static int FrameCodeI(Encoder * pEnc, Bitstream * bs, uint32_t *pBits)
{

	DECLARE_ALIGNED_MATRIX(dct_codes, 6, 64, int16_t, CACHE_LINE);
	DECLARE_ALIGNED_MATRIX(qcoeff,    6, 64, int16_t, CACHE_LINE);

	uint16_t x, y;

	pEnc->iFrameNum = 0;
	pEnc->mbParam.m_rounding_type = 1;
	pEnc->current->rounding_type = pEnc->mbParam.m_rounding_type;
	pEnc->current->coding_type = I_VOP;

	BitstreamWriteVolHeader(bs, &pEnc->mbParam, pEnc->current);
	BitstreamWriteVopHeader(bs, &pEnc->mbParam, pEnc->current);

	*pBits = BitstreamPos(bs);

	pEnc->sStat.iTextBits = 0;
	pEnc->sStat.kblks = pEnc->mbParam.mb_width * pEnc->mbParam.mb_height;
	pEnc->sStat.mblks = pEnc->sStat.ublks = 0;

	for (y = 0; y < pEnc->mbParam.mb_height; y++)
		for (x = 0; x < pEnc->mbParam.mb_width; x++)
		{
			MACROBLOCK *pMB = &pEnc->current->mbs[x + y * pEnc->mbParam.mb_width];

			CodeIntraMB(pEnc, pMB);

			MBTransQuantIntra(&pEnc->mbParam, pEnc->current, pMB, x, y, dct_codes, qcoeff);

			start_timer();
			MBPrediction(pEnc->current, x, y, pEnc->mbParam.mb_width, qcoeff);
			stop_prediction_timer();

			start_timer();
			MBCoding(pEnc->current, pMB, qcoeff, bs, &pEnc->sStat);
			stop_coding_timer();
		}

	emms();

	*pBits = BitstreamPos(bs) - *pBits;
	pEnc->sStat.fMvPrevSigma = -1;
	pEnc->sStat.iMvSum = 0;
	pEnc->sStat.iMvCount = 0;
	pEnc->mbParam.m_fcode = 2;

	if (pEnc->current->global_flags & XVID_HINTEDME_GET)
	{
		HintedMEGet(pEnc, 1);
	}

	return 1;					 // intra
}


#define INTRA_THRESHOLD 0.5

static int FrameCodeP(Encoder * pEnc, Bitstream * bs, uint32_t *pBits, bool force_inter, bool vol_header)
{
	float fSigma;

	DECLARE_ALIGNED_MATRIX(dct_codes, 6, 64, int16_t, CACHE_LINE);
	DECLARE_ALIGNED_MATRIX(qcoeff,    6, 64, int16_t, CACHE_LINE);

	int iLimit;
	uint32_t x, y;
	int iSearchRange;
	bool bIntra;

	IMAGE *pCurrent = &pEnc->current->image;
	IMAGE *pRef = &pEnc->reference->image;

	start_timer();
	image_setedges(pRef,
		       pEnc->mbParam.edged_width,
		       pEnc->mbParam.edged_height,
		       pEnc->mbParam.width,
		       pEnc->mbParam.height,
		       pEnc->current->global_flags & XVID_INTERLACING);
	stop_edges_timer();

	pEnc->mbParam.m_rounding_type = 1 - pEnc->mbParam.m_rounding_type;
	pEnc->current->rounding_type = pEnc->mbParam.m_rounding_type;
	pEnc->current->fcode = pEnc->mbParam.m_fcode;

	if (!force_inter)
		iLimit = (int)(pEnc->mbParam.mb_width * pEnc->mbParam.mb_height * INTRA_THRESHOLD);
	else
		iLimit = pEnc->mbParam.mb_width * pEnc->mbParam.mb_height + 1;

	if ((pEnc->current->global_flags & XVID_HALFPEL)) {
		start_timer();
		image_interpolate(pRef, &pEnc->vInterH, &pEnc->vInterV, &pEnc->vInterHV,
				  pEnc->mbParam.edged_width, pEnc->mbParam.edged_height,
				  pEnc->current->rounding_type);
		stop_inter_timer();
	}

	start_timer();
	if (pEnc->current->global_flags & XVID_HINTEDME_SET)
	{
		HintedMESet(pEnc, &bIntra);
	}
	else
	{
		bIntra = MotionEstimation(
			&pEnc->mbParam, 
			pEnc->current, 
			pEnc->reference, 
			&pEnc->vInterH, 
			&pEnc->vInterV,
			&pEnc->vInterHV, 
			iLimit);
	}
	stop_motion_timer();

	if (bIntra == 1)
	{
		return FrameCodeI(pEnc, bs, pBits);
	}

	pEnc->current->coding_type = P_VOP;

	if(vol_header)
		BitstreamWriteVolHeader(bs, &pEnc->mbParam, pEnc->current);

	BitstreamWriteVopHeader(bs, &pEnc->mbParam, pEnc->current);

	*pBits = BitstreamPos(bs);

	pEnc->sStat.iTextBits = 0;
	pEnc->sStat.iMvSum = 0;
	pEnc->sStat.iMvCount = 0;
	pEnc->sStat.kblks = pEnc->sStat.mblks = pEnc->sStat.ublks = 0;

	for(y = 0; y < pEnc->mbParam.mb_height; y++)
	{
		for(x = 0; x < pEnc->mbParam.mb_width; x++)
		{
			MACROBLOCK * pMB = &pEnc->current->mbs[x + y * pEnc->mbParam.mb_width];

			bIntra = (pMB->mode == MODE_INTRA) || (pMB->mode == MODE_INTRA_Q);

			if (!bIntra)
			{
				start_timer();
				MBMotionCompensation(pMB,
						     x, y,
						     &pEnc->reference->image,
						     &pEnc->vInterH,
						     &pEnc->vInterV,
						     &pEnc->vInterHV,
						     &pEnc->current->image,
						     dct_codes,
						     pEnc->mbParam.width,
						     pEnc->mbParam.height,
						     pEnc->mbParam.edged_width,
						     pEnc->current->rounding_type);
				stop_comp_timer();

				if ((pEnc->current->global_flags & XVID_LUMIMASKING)) {
					if(pMB->dquant != NO_CHANGE) {
						pMB->mode = MODE_INTER_Q;
						pEnc->current->quant += DQtab[pMB->dquant];
						if (pEnc->current->quant > 31) pEnc->current->quant = 31;
						else if(pEnc->current->quant < 1) pEnc->current->quant = 1;
					}
				}
				pMB->quant = pEnc->current->quant;

				pMB->field_pred = 0;

				pMB->cbp = MBTransQuantInter(&pEnc->mbParam, pEnc->current, pMB, x, y, dct_codes, qcoeff);
			}
			else 
			{
				CodeIntraMB(pEnc, pMB);
				MBTransQuantIntra(&pEnc->mbParam, pEnc->current, pMB, x, y, dct_codes, qcoeff);
			}

			start_timer();
			MBPrediction(pEnc->current, x, y, pEnc->mbParam.mb_width, qcoeff);			
			stop_prediction_timer();

			if (pMB->mode == MODE_INTRA || pMB->mode == MODE_INTRA_Q)
			{
				pEnc->sStat.kblks++;
			}
			else if (pMB->cbp || 
				 pMB->mvs[0].x || pMB->mvs[0].y ||
				 pMB->mvs[1].x || pMB->mvs[1].y ||
				 pMB->mvs[2].x || pMB->mvs[2].y ||
				 pMB->mvs[3].x || pMB->mvs[3].y)
			{
				pEnc->sStat.mblks++;
			}
			else
			{
				pEnc->sStat.ublks++;
			}

			start_timer();
			MBCoding(pEnc->current, pMB, qcoeff, bs, &pEnc->sStat);
			stop_coding_timer();
		}
	}

	emms();

	if (pEnc->current->global_flags & XVID_HINTEDME_GET)
	{
		HintedMEGet(pEnc, 0);
	}

	if (pEnc->sStat.iMvCount == 0)
		pEnc->sStat.iMvCount = 1;

	fSigma = (float)sqrt((float) pEnc->sStat.iMvSum / pEnc->sStat.iMvCount);

	iSearchRange = 1 << (3 + pEnc->mbParam.m_fcode);

	if ((fSigma > iSearchRange / 3) 
	    && (pEnc->mbParam.m_fcode <= 3))	// maximum search range 128
	{
		pEnc->mbParam.m_fcode++;
		iSearchRange *= 2;
	}
	else if ((fSigma < iSearchRange / 6)
		 && (pEnc->sStat.fMvPrevSigma >= 0)
		 && (pEnc->sStat.fMvPrevSigma < iSearchRange / 6)
		 && (pEnc->mbParam.m_fcode >= 2))	// minimum search range 16
	{
		pEnc->mbParam.m_fcode--;
		iSearchRange /= 2;
	}

	pEnc->sStat.fMvPrevSigma = fSigma;
    
	*pBits = BitstreamPos(bs) - *pBits;

	return 0;					 // inter
}



/*
static void FrameCodeB(Encoder * pEnc, FRAMEINFO * frame, Bitstream * bs, uint32_t *pBits)
{
    int16_t dct_codes[6][64];
    int16_t qcoeff[6][64];
    uint32_t x, y;
	VECTOR forward;
	VECTOR backward;

    IMAGE *f_ref = &pEnc->reference->image;
	IMAGE *b_ref = &pEnc->current->image;

	// forward 
	image_setedges(f_ref, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height, pEnc->mbParam.width, pEnc->mbParam.height);
	start_timer();
	image_interpolate(f_ref, &pEnc->f_refh, &pEnc->f_refv, &pEnc->f_refhv,
		pEnc->mbParam.edged_width, pEnc->mbParam.edged_height, 0);
	stop_inter_timer();

	// backward
	image_setedges(b_ref, pEnc->mbParam.edged_width, pEnc->mbParam.edged_height, pEnc->mbParam.width, pEnc->mbParam.height);
    start_timer();
	image_interpolate(b_ref, &pEnc->vInterH, &pEnc->vInterV, &pEnc->vInterHV,
		pEnc->mbParam.edged_width, pEnc->mbParam.edged_height, 0);
	stop_inter_timer();

	start_timer();
	MotionEstimationBVOP(&pEnc->mbParam, frame, 
		pEnc->reference->mbs, f_ref, &pEnc->f_refh, &pEnc->f_refv, &pEnc->f_refhv,
		pEnc->current->mbs, b_ref, &pEnc->vInterH, &pEnc->vInterV, &pEnc->vInterHV);

	stop_motion_timer();
	
	if (test_quant_type(&pEnc->mbParam, pEnc->current))
	{
		BitstreamWriteVolHeader(bs, pEnc->mbParam.width, pEnc->mbParam.height, pEnc->mbParam.quant_type);
	}

    frame->coding_type = B_VOP;
    BitstreamWriteVopHeader(bs, B_VOP, frame->tick, 0,
			frame->quant, frame->fcode, frame->bcode);

    *pBits = BitstreamPos(bs);

    pEnc->sStat.iTextBits = 0;
    pEnc->sStat.iMvSum = 0;
    pEnc->sStat.iMvCount = 0;
	pEnc->sStat.kblks = pEnc->sStat.mblks = pEnc->sStat.ublks = 0;


    for (y = 0; y < pEnc->mbParam.mb_height; y++)
	{
		// reset prediction

		forward.x = 0;
		forward.y = 0;
		backward.x = 0;
		backward.y = 0;

		for (x = 0; x < pEnc->mbParam.mb_width; x++)
		{
			MACROBLOCK * f_mb = &pEnc->reference->mbs[x + y * pEnc->mbParam.mb_width];
			MACROBLOCK * b_mb = &pEnc->current->mbs[x + y * pEnc->mbParam.mb_width];
			MACROBLOCK * mb = &frame->mbs[x + y * pEnc->mbParam.mb_width];

			// decoder ignores mb when refence block is INTER(0,0), CBP=0 
			if (mb->mode == MODE_NOT_CODED)
			{
				mb->mvs[0].x = 0;
				mb->mvs[0].y = 0;
				continue;
			}

			MBMotionCompensationBVOP(&pEnc->mbParam, mb, x, y, &frame->image,
					f_ref, &pEnc->f_refh, &pEnc->f_refv, &pEnc->f_refhv, 
					b_ref, &pEnc->vInterH, &pEnc->vInterV, &pEnc->vInterHV, 
					dct_codes);
	
			mb->quant = frame->quant;
			mb->cbp = MBTransQuantInter(&pEnc->mbParam, frame, x, y, dct_codes, qcoeff);
			//mb->cbp = MBTransQuantBVOP(&pEnc->mbParam, x, y, dct_codes, qcoeff, &frame->image, frame->quant);


			if ((mb->mode == MODE_INTERPOLATE || mb->mode == MODE_DIRECT) &&
				mb->cbp == 0 &&
				mb->mvs[0].x == 0 && 
				mb->mvs[0].y == 0)
			{
				mb->mode = 5;  // skipped
			}

			if (mb->mode == MODE_INTERPOLATE || mb->mode == MODE_FORWARD)
			{
				mb->pmvs[0].x = mb->mvs[0].x - forward.x;
				mb->pmvs[0].y = mb->mvs[0].y - forward.y;
				forward.x = mb->mvs[0].x;
				forward.y = mb->mvs[0].y;
			}
			
			if (mb->mode == MODE_INTERPOLATE || mb->mode == MODE_BACKWARD)
			{
				mb->b_pmvs[0].x = mb->b_mvs[0].x - backward.x;
				mb->b_pmvs[0].y = mb->b_mvs[0].y - backward.y;
				backward.x = mb->b_mvs[0].x;
				backward.y = mb->b_mvs[0].y;
			}

//			printf("[%i %i] M=%i CBP=%i MVX=%i MVY=%i %i,%i  %i,%i\n", x, y, pMB->mode, pMB->cbp, pMB->mvs[0].x, bmb->pmvs[0].x, bmb->pmvs[0].y, forward.x, forward.y);

			start_timer();			
			MBCodingBVOP(frame, mb, qcoeff, bs, &pEnc->sStat);
			stop_coding_timer();
		}
	}

	emms();

	// TODO: dynamic fcode/bcode ???

	*pBits = BitstreamPos(bs) - *pBits;

}

*/
