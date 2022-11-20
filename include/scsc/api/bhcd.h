/****************************************************************************
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/*
 * Tag-Length-Value encoding of Bluetooth Host Configuration Data (BHCD)
 */

#ifndef BHCD_H__
#define BHCD_H__


/** Definition of tags */
enum bhcd_tag {
	/**
	 * The total length of the entire BHCD this tag is contained in, including the size of all
	 * tag, length and value fields.
	 *
	 * This special tag must be present as first item for validation/integrity check purposes,
	 * and to be able to determine the overall length of the BHCD.
	 *
	 * Value length: 4 byte
	 * Value encoding: uint32_t
	 */
	BHCD_TAG_TOTAL_LENGTH = 0x44434842, /* "BHCD" */

	/**
	 * Host communication structure offset/length.
	 *
	 * Value length: 8 byte
	 * Value encoding: bhcd_offset_length
	 */
	BHCD_TAG_PROTOCOL = 1,

	/**
	 * Binary configuration data.
	 *
	 * Value length: N byte
	 * Value encoding: uint8_t[N]
	 */
	BHCD_TAG_CONFIGURATION = 2,

	/**
	 * Bluetooth Address.
	 *
	 * Value length: 8 byte
	 * Value encoding: bhcd_bluetooth_address
	 */
	BHCD_TAG_BLUETOOTH_ADDRESS = 3,

	/**
	 * BT Log Enables.
	 *
	 * Value length: 4N byte
	 * Value encoding: uint32_t[N]
	 */
	BHCD_TAG_BTLOG_ENABLES = 4,

	/**
	 * Entropy
	 *
	 * Value length: N bytes
	 * Value encoding: uint8_t[N]
	 */
	BHCD_TAG_ENTROPY = 6,
};


/**
 * Structure describing the tag and length of each item in the BHCD.
 * The value of the item follows immediately after.
 */
struct bhcd_tag_length {
	uint32_t tag;    /* One of bhcd_tag */
	uint32_t length; /* Length of value in bytes */
};


/**
 * Structure for containing the offset/length of another data structure
 */
struct bhcd_offset_length {
	uint32_t offset;
	uint32_t length;
};


/**
 * A Bluetooth Address in LAP/UAP/NAP format
 */
struct bhcd_bluetooth_address {
	uint32_t lap;    /* Lower Address Part 00..23 */
	uint8_t uap;     /* Upper Address Part 24..31 */
	uint8_t padding;
	uint16_t nap;    /* Non-significant    32..47 */
};


#endif /* BHCD_H__ */
