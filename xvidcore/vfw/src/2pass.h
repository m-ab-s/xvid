#ifndef _2PASS_H_
#define _2PASS_H_

#include "codec.h"

int codec_2pass_init(CODEC *);
int codec_2pass_get_quant(CODEC *, xvid_enc_frame_t *);
int codec_2pass_update(CODEC *, xvid_enc_stats_t *);
void codec_2pass_finish(CODEC *);

#endif // _2PASS_H_
