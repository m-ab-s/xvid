#ifndef _REDUCED_H_
#define _REDUCED_H_

#include "../portab.h"

/* decoding prototypes */

typedef void (COPY_UPSAMPLED_8X8_16TO8) (uint8_t *Dst, const int16_t *Src, const int BpS);
typedef void (ADD_UPSAMPLED_8X8_16TO8) (uint8_t *Dst, const int16_t *Src, const int BpS);

/* Note: "Nb"_Blks is the number of 8-pixels blocks to process */
typedef void HFILTER_31(uint8_t *Src1, uint8_t *Src2, int Nb_Blks);
typedef void VFILTER_31(uint8_t *Src1, uint8_t *Src2, const int BpS, int Nb_Blks);


extern COPY_UPSAMPLED_8X8_16TO8 * copy_upsampled_8x8_16to8;
extern COPY_UPSAMPLED_8X8_16TO8 xvid_Copy_Upsampled_8x8_16To8_C;
extern COPY_UPSAMPLED_8X8_16TO8 xvid_Copy_Upsampled_8x8_16To8_mmx;
extern COPY_UPSAMPLED_8X8_16TO8 xvid_Copy_Upsampled_8x8_16To8_xmm;

extern ADD_UPSAMPLED_8X8_16TO8 * add_upsampled_8x8_16to8;
extern ADD_UPSAMPLED_8X8_16TO8 xvid_Add_Upsampled_8x8_16To8_C;
extern ADD_UPSAMPLED_8X8_16TO8 xvid_Add_Upsampled_8x8_16To8_mmx;
extern ADD_UPSAMPLED_8X8_16TO8 xvid_Add_Upsampled_8x8_16To8_xmm;

extern VFILTER_31 * vfilter_31;
extern VFILTER_31 xvid_VFilter_31_C;
extern VFILTER_31 xvid_VFilter_31_x86;

extern HFILTER_31 * hfilter_31;
extern HFILTER_31 xvid_HFilter_31_C;
extern HFILTER_31 xvid_HFilter_31_x86;
extern HFILTER_31 xvid_HFilter_31_mmx;




  /* encoding prototypes */
  /* WARNING! These read 1 pixel outside of the input 16x16 block! */
extern void xvid_Filter_18x18_To_8x8_C(int16_t *Dst, const uint8_t *Src, const int BpS);
extern void xvid_Filter_Diff_18x18_To_8x8_C(int16_t *Dst, const uint8_t *Src, const int BpS);

  /* MMX version */
extern void xvid_Filter_18x18_To_8x8_mmx(int16_t *Dst, const uint8_t *Src, const int BpS);
extern void xvid_Filter_Diff_18x18_To_8x8_mmx(int16_t *Dst, const uint8_t *Src, const int BpS);


#endif /* _REDUCED_H_ */
