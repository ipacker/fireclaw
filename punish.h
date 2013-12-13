
/* $Id: punish.h,v 1.17 2003/06/21 12:43:11 snazbaz Exp $ */

struct punishment {
	char weight;
	char *shortName;
	char *reason;
	int *banlen;
	int counter;
};

struct punishment pmap[26] =
{
	{ 0, "", "", 0, 0 },		/* a ( TAKEN )*/
	{ 3, "badurl", config.msg_badurl, &config.len_badurl, 0 },			/* b */
	{ 1, "colour", config.msg_colour, &config.len_colour, 0 },									/* c */
	{ 3, "onjoin", config.msg_onjoin, &config.len_onjoin, 0 },			/* d */
	{ 0, "", "", 0, 0 },		/* e ( TAKEN )*/
	{ 2, "flood", config.msg_flood, &config.len_flood, 0 },									/* f */
	{ 0, "", "", 0, 0 },		/* g */
	{ 3, "lamehost", config.msg_lamehost, &config.len_lamehost, 0 },			/* h */
	{ 1, "chanctcp", config.msg_chanctcp, &config.len_chanctcp, 0 },							/* i */
	{ 0, "", "", 0, 0 },		/* j ( TAKEN ) */
	{ 3, "kickclones", config.msg_kickclones, &config.len_kickclones, 0 },					/* k */
	{ 0, "", "", 0, 0 },		/* l ( TAKEN ) */
	{ 3, "badmask", config.msg_badmask, &config.len_badmask, 0 },				/* m */
	{ 2, "notice", config.msg_notice, &config.len_notice, 0 },							/* n */
	{ 0, "", "", 0, 0 },		/* o ( TAKEN ) */
	{ 1, "caps", config.msg_caps, &config.len_caps, 0 },								/* p */
	{ 0, "", "", 0, 0 },		/* q */
	{ 2, "repeat", config.msg_repeat, &config.len_repeat, 0 },							/* r */
	{ 1, "swear", config.msg_swear, &config.len_swear, 0 },						/* s */
	{ 1, "ctrlchar", config.msg_ctrlchar, &config.len_ctrlchar, 0 },	/* t */
	{ 0, "", "", 0, 0 },		/* u */
	{ 0, "", "", 0, 0 },		/* v */
	{ 0, "", "", 0, 0 },		/* w */
	{ 1, "repeatchar", config.msg_repeatchar, &config.len_repeatchar, 0 },	/* x */
	{ 3, "joinflood", config.msg_joinflood, &config.len_joinflood, 0 },	/* y */
	{ 3, "nickflood", config.msg_nickflood, &config.len_nickflood, 0 }							/* z */
};
