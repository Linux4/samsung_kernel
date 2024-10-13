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

#ifndef PABLO_JSON_H
#define PABLO_JSON_H

/**
  * pablo_json_object_open() - Open a JSON object in a JSON string.
  *
  * @dst:	[in]	Pointer to the end of JSON under construction
  * @name:	[in]	Pointer to null-terminated string or null for unnamed
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A pointer to the new end of JSON under construction.
  */
char *pablo_json_object_open(char *dst, const char *name, size_t *rem);

/**
  * pablo_json_object_close() - Close a JSON object in a JSON string.
  *
  * @dst:	[in]	Pointer to the end of JSON under construction
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A Pointer to the new end of JSON under construction.
  */
char *pablo_json_object_close(char *dst, size_t *rem);

/**
  * pablo_json_end() - Used to finish the root JSON object.
  * @dst:	[in]	Pointer to the end of JSON under construction
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A Pointer to the new end of JSON under construction.
  */
char *pablo_json_end(char *dst, size_t *rem);

/**
  * pablo_json_array_open() - Open an array in a JSON string.
  * @dst:	[in]	Pointer to the end of JSON under construction
  * @name:	[in]	Pointer to null-terminated string or null for unnamed
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A Pointer to the new end of JSON under construction.
  */
char *pablo_json_array_open(char *dst, const char *name, size_t *rem);

/**
  * pablo_json_array_close() - Close an array in a JSON string.
  * @dst:	[in]	Pointer to the end of JSON under construction
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A Pointer to the new end of JSON under construction.
  */
char *pablo_json_array_close(char *dst, size_t *rem);

/**
  * pablo_json_nstr() - Add a text property in a JSON string.
  *
  * @dst:	[in]	Pointer to the end of JSON under construction
  * @name:	[in]	Pointer to null-terminated string or null for unnamed
  * @val:	[in]	A valid null-terminated string with the val
  *			Backslash escapes will be added for special characters
  * @len:	[in]	Max length of source < 0 for unlimit
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A Pointer to the new end of JSON under construction.
  */
char *pablo_json_nstr(char *dst, const char *name, const char *val, int len, size_t *rem);

/**
  * pablo_json_str() - Add a text property in a JSON string.
  *
  * @dst:	[in]	Pointer to the end of JSON under construction
  * @name:	[in]	Pointer to null-terminated string or null for unnamed
  * @val:	[in]	A valid null-terminated string with the val
  *			Backslash escapes will be added for special characters
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A Pointer to the new end of JSON under construction.
  */
static inline char *pablo_json_str(char *dst, const char *name, const char *val, size_t *rem)
{
	return pablo_json_nstr(dst, name, val, -1, rem);
}

/**
  * pablo_json_bool() - Add a boolean property in a JSON string.
  *
  * @dst:	[in]	Pointer to the end of JSON under construction
  * @name:	[in]	Pointer to null-terminated string or null for unnamed
  * @val:	[in]	A valid boolean value
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A Pointer to the new end of JSON under construction.
  */
char *pablo_json_bool(char *dst, const char *name, bool val, size_t *rem);

/**
  * pablo_json_null() - Add a null property in a JSON string.
  *
  * @dst:	[in]	Pointer to the end of JSON under construction
  * @name:	[in]	Pointer to null-terminated string or null for unnamed
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A Pointer to the new end of JSON under construction.
  */
char *pablo_json_null(char *dst, const char *name, size_t *rem);

/**
  * pablo_json_TYPE() - Add an each type property in a JSON string.
  *
  * @dst:	[in]	Pointer to the end of JSON under construction
  * @name:	[in]	Pointer to null-terminated string or null for unnamed
  * @val:	[in]	A valid value for each type
  * @rem:	[in]	Pointer to remaining length of destination
  *
  * Return: A Pointer to the new end of JSON under construction.
  */
char *pablo_json_int(char *dst, const char *name, int val, size_t *rem);
char *pablo_json_uint(char *dst, const char *name, unsigned int val, size_t *rem);
char *pablo_json_long(char *dst, const char *name, long val, size_t *rem);
char *pablo_json_ulong(char *dst, const char *name, unsigned long val, size_t *rem);
char *pablo_json_llong(char *dst, const char *name, long long val, size_t *rem);

#endif /* PABLO_JSON_H */
