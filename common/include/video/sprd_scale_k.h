/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define SCALE_IO_MAGIC                             'S'
#define SCALE_IO_INPUT_SIZE                        _IOW(SCALE_IO_MAGIC, SCALE_INPUT_SIZE,         struct scale_size)
#define SCALE_IO_INPUT_RECT                        _IOW(SCALE_IO_MAGIC, SCALE_INPUT_RECT,         struct scale_rect)
#define SCALE_IO_INPUT_FORMAT                      _IOW(SCALE_IO_MAGIC, SCALE_INPUT_FORMAT,       enum scale_fmt)
#define SCALE_IO_INPUT_ADDR                        _IOW(SCALE_IO_MAGIC, SCALE_INPUT_ADDR,         struct scale_addr)
#define SCALE_IO_INPUT_ENDIAN                      _IOW(SCALE_IO_MAGIC, SCALE_INPUT_ENDIAN,       struct scale_endian_sel)
#define SCALE_IO_OUTPUT_SIZE                       _IOW(SCALE_IO_MAGIC, SCALE_OUTPUT_SIZE,        struct scale_size)
#define SCALE_IO_OUTPUT_FORMAT                     _IOW(SCALE_IO_MAGIC, SCALE_OUTPUT_FORMAT,      enum scale_fmt)
#define SCALE_IO_OUTPUT_ADDR                       _IOW(SCALE_IO_MAGIC, SCALE_OUTPUT_ADDR,        struct scale_addr)
#define SCALE_IO_OUTPUT_ENDIAN                     _IOW(SCALE_IO_MAGIC, SCALE_OUTPUT_ENDIAN,      struct scale_endian_sel)
#define SCALE_IO_TEMP_BUFF                         _IOW(SCALE_IO_MAGIC, SCALE_TEMP_BUFF,          struct scale_addr)
#define SCALE_IO_SCALE_MODE                        _IOW(SCALE_IO_MAGIC, SCALE_SCALE_MODE,         enum scle_mode)
#define SCALE_IO_SLICE_SCALE_HEIGHT                _IOW(SCALE_IO_MAGIC, SCALE_SLICE_SCALE_HEIGHT, uint32_t)
#define SCALE_IO_START                             _IO(SCALE_IO_MAGIC,  SCALE_START)
#define SCALE_IO_CONTINUE                          _IO(SCALE_IO_MAGIC,  SCALE_CONTINUE)
#define SCALE_IO_STOP                              _IO(SCALE_IO_MAGIC,  SCALE_STOP)
#define SCALE_IO_INIT                              _IO(SCALE_IO_MAGIC,  SCALE_INIT)
#define SCALE_IO_DEINIT                            _IO(SCALE_IO_MAGIC,  SCALE_DEINIT)
#define SCALE_IO_IS_DONE                           _IOR(SCALE_IO_MAGIC, SCALE_IS_DONE,            struct scale_frame)
