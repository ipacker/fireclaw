/* $Id: cfg.h,v 1.40 2003/06/26 17:24:10 snazbaz Exp $ */

#include "os.h"

#define DEFAULT_CONFIG "claw.conf"
#define DEFAULT_PID "claw.pid"
#define DEFAULT_ADMIN "admins.txt"
#define DEFAULT_CHANS "chans.txt"
#define DEFAULT_BANS "bans.txt"
#define DEFAULT_CHANFLAGS "jea"	

#define DEBUG_MODE
#define DEBUG_EXTENDED

/* Allow anonymous uptime reporting to uptime.eggheads.org - this involves
	sending off a single udp packet once every 6 hours. */
#define REPORT_UPTIME

/* Strict swear matching? */
//#define STRICT_SWEAR

/* using gnuworld with +x support in the ircd? */
#define GNU_X_HOST ".users.netgamers.org"

/* this value is critical to fireclaw avoiding excess flood 
		generally it should be ~0.75 of the server's CLIENT_FLOOD */
#define CLIENT_FLOOD 800

/* this should be CLIENT_FLOOD * 1.25 rounded to an int */
#define CLIENT_FLOOD_AUX 1000

/* this should be CLIENT_FLOOD * 0.75 rounded to an int */
#define CLIENT_FLOOD_LOW 600

#define REPEAT_HISTORY_SIZE 30
#define BANLIST_LIMIT 10
#define INIT_USERMASK 8
#define INCLUDE_ALLCOMP
#define ALLCOMP_FILE "rawtags.html"
#define ALLCOMP_FINAL "tags.html"
#define ALLCOMP_EXCLUDE "exclude.txt"

#ifdef STRICT_SWEAR
	#define SWEAR_MATCH_MAIN "f[uv\\*]+[c\\*]+k|sh+[i\\*]+t|c[uv]+n+t"
	#define SWEAR_MATCH_MILD "twat|bi+tch|c[o\\*]+ck|as+ho+le|arseho+le|bollock|bastard|fki+ng|wank"
#else
	#define SWEAR_MATCH_MAIN "f[uv\\*]+[c\\*]+k|sh+[i\\*]+t|c[uv]+n+t"
	#define SWEAR_MATCH_MILD "c[o\\*]+ck|as+ho+le|arseho+le|bollock|bastard|wank"
#endif

#define EXCEPTIONS "\\.(edu|ac\\.uk|pol\\.co\\.uk|blueyonder\\.co\\.uk|netgamers\\.org|chello\\.nl|online\\.no|ntl\\.com|aol\\.com|t-dialin\\.net)$"
#define LAMEHOST_5 "^(by|all)$"
#define LAMEHOST_10 "^(at|in|you|if|just|like|ein|bei|my|it|get)$"
#define LAMEHOST_15 "^(is|the|for|your|a|that|and|are|suck|i|a|dont|ist)$"

/* 
 * 
 * Do not change anything below unless you know what you are doing 
 *******************************************************************/

#define HIGHMSG_DELAY 1
#define LOWMSG_DELAY 2

/* outgoing message queue sizes (cost = memory) */
#define Q_LOWSIZE 256
#define Q_HIGHSIZE 128

#define SOCKET_BUFFER 4096

#define BUFSIZE 1024
#define INCBUF 4096
#define MAXTOK 256

#define NICKSIZE 33
#define IDENTSIZE 33
#define HOSTSIZE 256
#define MASKSIZE 512

#define TOPICLEN 500
#define MODELEN 32
#define KEYLEN 64
#define PASSLEN 33

#define FLAGLEN 32

#define MAX_SERVERS 32
#define MAX_DCC 256
#define MAX_AUTHWAIT 10
#define GUEST_SLOTS 200

// These are defaults that get set if you dont specifiy them in the config file.
// SO DO NOT CHANGE THEM IN THIS FILE.
#define M_USERMODES "+i"
#define W_TEMPLATE "NOTICE %s :\002Warning\002 (%s) %s - next time you will be kicked."
#define DCC_ID_OFSET 1000
#define DEFAULT_PORT 6667

#define VALIDFLAGS "ABIOVabcdefhijklmnoprstxyz"
#define MODES_ARGS "klhovb"

#define SHORT_VER "0.93"
#define VERSION_STR "0.93 (C) 2003 Ian Packer"

struct myConfig
{
	char nick[NICKSIZE];
	char ident[16];
	char realname[64];
	char vhost[20];
	char relaychan[64];


	char servicePrefix[128];
	char serviceAuth[128];
	char serviceGhost[128];
	char serviceOp[128];

	char warntemplate[256];

	char usermodes[32];

	int loglevel;

	int maxmodes;
	int banlim;

	int caps_max;
	int ctrlchar_max;
	int repeatchar_lim;

	int maxkicks;

	int float_period;
	int float_thresh;
		int float_step;  // hidden
		int float_step4; // hidden

	int flood_global_period;
	int flood_global_limit;
	int flood_period;
	int flood_limit;
	int flood_nick_period;
	int flood_nick_limit;
	int	flood_nick_mperiod;
	int flood_nick_mlimit;
	int flood_join_period;
	int flood_join_limit;

	int repeat_limit;

	int contry;
	int condelay;

	int linkport;
	char linkname[32];

	int slowkicks;

	int clonelimit;

	int dcc_allowguests;
	int dcc_isolate;
	int dcc_timeout;
	int dcc_listenport;
	char dcc_listenip[16];

	int idle_limit;
	int idlevoice;

	int showusermsgs;
	int noctcps;

	int cleanbanlist;

	int nooverride;

    int oj_period;
	int oj_listenperiod;

	char msg_badurl[128];
	char msg_colour[128];
	char msg_onjoin[128];
	char msg_flood[128];
	char msg_lamehost[128];
	char msg_chanctcp[128];
	char msg_kickclones[128];
	char msg_badmask[128];
	char msg_notice[128];
	char msg_caps[128];
	char msg_repeat[128];
	char msg_swear[128];
	char msg_ctrlchar[128];
	char msg_repeatchar[128];
	char msg_joinflood[128];
	char msg_nickflood[128];

	int len_badurl;
	int len_colour;
	int len_onjoin;
	int len_flood;
	int len_lamehost;
	int len_chanctcp;
	int len_kickclones;
	int len_badmask;
	int len_notice;
	int len_caps;
	int len_repeat;
	int len_swear;
	int len_ctrlchar;
	int len_repeatchar;
	int len_joinflood;
	int len_nickflood;

};
