/*
 * Copyright 2020 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __AMDGPU_CWSR_H__
#define __AMDGPU_CWSR_H__

#include "amdgpu_ctx_priv.h"
extern int cwsr_enable;

int amdgpu_cwsr_init(struct amdgpu_ctx *ctx);
void amdgpu_cwsr_deinit(struct amdgpu_ctx *ctx);

int amdgpu_cwsr_dequeue(struct amdgpu_ring *ring);
int amdgpu_cwsr_relaunch(struct amdgpu_ring *ring);

int amdgpu_cwsr_init_queue(struct amdgpu_ring *ring);
void amdgpu_cwsr_deinit_queue(struct amdgpu_ring *ring);
#endif
