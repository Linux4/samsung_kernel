/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <net/genetlink.h>
#include <scsc/scsc_logring.h>

/**
 * Attributes are fields of data your messages will contain.
 * The designers of Netlink really want you to use these instead of just dumping
 * data to the packet payload... and I have really mixed feelings about it.
 */
enum attributes {
	/*
	 * The first one has to be a throwaway empty attribute; I don't know
	 * why.
	 * If you remove it, ATTR_HELLO (the first one) stops working, because
	 * it then becomes the throwaway.
	 */
	ATTR_UNSPEC,

	ATTR_STR,
	ATTR_INT,

	/* This must be last! */
	__ATTR_MAX,
};

/**
 * Message type codes. All you need is a hello sorta function, so that's what
 * I'm defining.
 */
enum events {
	/* must be first */
	EVENT_UNSPEC,

	EVENT_PANIC,

	/* This must be last! */
	__EVENT_MAX,
};

static const struct genl_multicast_group scsc_mdp_mcgrp[] = {
	{ .name = "scsc_mdp_grp", },
};

int scsc_wlbtd_init(void);
int scsc_wlbtd_deinit(void);
int coredump_wlbtd(void);
