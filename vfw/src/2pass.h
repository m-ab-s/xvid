#ifndef _2PASS_H_
#define _2PASS_H_

#include "codec.h"

int codec_2pass_init(CODEC *);
int codec_2pass_get_quant(CODEC *, XVID_ENC_FRAME *);
int codec_2pass_update(CODEC *, XVID_ENC_FRAME *, XVID_ENC_STATS *);
void codec_2pass_finish(CODEC *);

#endif // _2PASS_H_
