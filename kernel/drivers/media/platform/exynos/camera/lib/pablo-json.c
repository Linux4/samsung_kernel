// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "[" KBUILD_MODNAME "] Pablo-JSON: " fmt

#include <linux/types.h>
#include <linux/kernel.h>

#include "pablo-json.h"

/**
  * chtoa() - Add a character at the end of a string.
  *
  * @dst:	[in]	Pointer to the null character of the string
  * @ch:	[in]	Value to be added
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A pointer to the null character of the dstination string.
  */
static char *chtoa(char *dst, char ch, size_t *rem)
{
	if (*rem != 0) {
		--*rem;
		*dst = ch;
		*++dst = '\0';
	}

	return dst;
}

/**
  * atoa() - Copy a null-terminated string.
  *
  * @dst:	[in]	Destination memory block
  * @src:	[in]	Source string
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A pointer to the null character of the destination string.
  */
static char *atoa(char *dst, const char *src, size_t *rem)
{
	for (; *src != '\0' && *rem != 0; ++dst, ++src, --*rem)
		*dst = *src;
	*dst = '\0';

	return dst;
}

char *pablo_json_object_open(char *dst, const char *name, size_t *rem)
{
	if (name) {
		dst = chtoa(dst, '\"', rem);
		dst = atoa(dst, name, rem);
		dst = atoa(dst, "\":{", rem);
	} else {
		dst = chtoa(dst, '{', rem);
	}

	return dst;
}
EXPORT_SYMBOL_GPL(pablo_json_object_open);

char *pablo_json_object_close(char *dst, size_t *rem)
{
	if (dst[-1] == ',') {
		--dst;
		++*rem;
	}

	return atoa(dst, "},", rem);
}
EXPORT_SYMBOL_GPL(pablo_json_object_close);

char *pablo_json_end(char *dst, size_t *rem)
{
	if (',' == dst[-1]) {
		dst[-1] = '\0';
		--dst;
		++*rem;
	}

	return dst;
}
EXPORT_SYMBOL_GPL(pablo_json_end);

char *pablo_json_array_open(char *dst, const char *name, size_t *rem)
{
	if (name) {
		dst = chtoa(dst, '\"', rem);
		dst = atoa(dst, name, rem);
		dst = atoa(dst, "\":[", rem);
	} else {
		dst = chtoa(dst, '[', rem);
	}

	return dst;
}
EXPORT_SYMBOL_GPL(pablo_json_array_open);

char *pablo_json_array_close(char *dst, size_t *rem)
{
	if (dst[-1] == ',') {
		--dst;
		++*rem;
	}

	return atoa(dst, "],", rem);
}
EXPORT_SYMBOL_GPL(pablo_json_array_close);

/**
  * strname() - Add the name of a text property.
  * @dst:	[in]	Destination memory
  * @name:	[in]	The name of the property
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A pointer to the next char. */
static char *strname(char *dst, const char *name, size_t *rem)
{
	dst = chtoa(dst, '\"', rem);

	if (name) {
		dst = atoa(dst, name, rem);
		dst = atoa(dst, "\":\"", rem);
	}

	return dst;
}

static int nibbletoch(int nibble)
{
	return "0123456789ABCDEF"[nibble % 16u];
}

/**
  * escape() - Get the escape character of a non-printable.
  * @ch:	[in]	Character source
  *
  * Return: The escape character or null character if error.
  */
static int escape(int ch)
{
	int i;
	static struct {
		char code;
		char ch;
	} const pair[] = {
		{ '\"', '\"' }, { '\\', '\\' }, { '/', '/' },  { 'b', '\b' },
		{ 'f', '\f' },	{ 'n', '\n' },	{ 'r', '\r' }, { 't', '\t' },
	};

	for (i = 0; i < sizeof pair / sizeof *pair; ++i)
		if (ch == pair[i].ch)
			return pair[i].code;

	return '\0';
}

/**
  * atoesc() - Copy a null-terminated string inserting escape characters if needed.
  *
  * @dst:	[in]	Destination memory block
  * @src:	[in]	Source string
  * @len:	[in]	Max length of source < 0 for unlimit
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A pointer to the null character of the destination string.
  */
static char *atoesc(char *dst, const char *src, int len, size_t *rem)
{
	int esc, i;

	for (i = 0; src[i] != '\0' && (i < len || 0 > len) && *rem != 0;
	     ++dst, ++i, --*rem) {
		if (src[i] >= ' ' && src[i] != '\"' && src[i] != '\\' && src[i] != '/') {
			*dst = src[i];
		} else {
			if (*rem != 0) {
				*dst++ = '\\';
				--*rem;
				esc = escape(src[i]);
				if (esc) {
					if (*rem != 0)
						*dst = esc;
				} else {
					if (*rem != 0) {
						--*rem;
						*dst++ = 'u';
					}
					if (*rem != 0) {
						--*rem;
						*dst++ = '0';
					}
					if (*rem != 0) {
						--*rem;
						*dst++ = '0';
					}
					if (*rem != 0) {
						--*rem;
						*dst++ = nibbletoch(src[i] / 16);
					}
					if (*rem != 0) {
						--*rem;
						*dst++ = nibbletoch(src[i]);
					}
				}
			}
		}

		if (*rem == 0)
			break;
	}

	*dst = '\0';

	return dst;
}

char *pablo_json_nstr(char *dst, const char *name, const char *val, int len, size_t *rem)
{
	dst = strname(dst, name, rem);
	dst = atoesc(dst, val, len, rem);
	dst = atoa(dst, "\",", rem);

	return dst;
}
EXPORT_SYMBOL_GPL(pablo_json_nstr);

/**
  * primitivename() - Add the name of a primitive property.
  *
  * @dst:	[in]	Destination memory
  * @name:	[in]	The name of the property
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A pointer to the next char.
  */
static char *primitivename(char *dst, const char *name, size_t *rem)
{
	if (!name)
		return dst;

	dst = chtoa(dst, '\"', rem);
	dst = atoa(dst, name, rem);
	dst = atoa(dst, "\":", rem);

	return dst;
}

char *pablo_json_bool(char *dst, const char *name, bool val, size_t *rem)
{
	dst = primitivename(dst, name, rem);
	dst = atoa(dst, val ? "true," : "false,", rem);

	return dst;
}
EXPORT_SYMBOL_GPL(pablo_json_bool);

char *pablo_json_null(char *dst, const char *name, size_t *rem)
{
	dst = primitivename(dst, name, rem);
	dst = atoa(dst, "null,", rem);

	return dst;
}
EXPORT_SYMBOL_GPL(pablo_json_null);

#define pablo_json_num(func, type, fmt)					\
	char *func(char *dst, const char *name, type val, size_t *rem)	\
	{								\
		int digitlen;						\
									\
		dst = primitivename(dst, name, rem);			\
		digitlen = snprintf(dst, *rem, fmt, val);		\
									\
		if (digitlen >= (int)*rem + 1)				\
			digitlen = (int)*rem;				\
									\
		*rem -= (size_t)digitlen;				\
		dst += digitlen;					\
		dst = chtoa(dst, ',', rem);				\
									\
		return dst;						\
	}

pablo_json_num(pablo_json_int, int, "%d")
EXPORT_SYMBOL_GPL(pablo_json_int);

pablo_json_num(pablo_json_uint, unsigned int, "%u")
EXPORT_SYMBOL_GPL(pablo_json_uint);

pablo_json_num(pablo_json_long, long, "%ld")
EXPORT_SYMBOL_GPL(pablo_json_long);

pablo_json_num(pablo_json_ulong, unsigned long, "%lu")
EXPORT_SYMBOL_GPL(pablo_json_ulong);

pablo_json_num(pablo_json_llong, long long, "%lld")
EXPORT_SYMBOL_GPL(pablo_json_llong);
