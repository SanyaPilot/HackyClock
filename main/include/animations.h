#ifndef __ANIMATIONS_H__
#define __ANIMATIONS_H__

#include <stdint.h>
#include "canvas.h"

void anim_fade_out(struct canvas *cv, uint32_t duration_ms, uint32_t delay_ms);
void anim_fade_in(struct canvas *cv, struct canvas *cv_new, uint32_t duration_ms, uint32_t delay_ms);

#endif
