/* $Id: main.h,v 1.37 2003/07/16 15:35:02 snazbaz Exp $ */

#ifndef NOEXTERN
#define EXTERN extern
#else
#define EXTERN
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "cfg.h"

#ifdef FREEBSD
	#include <gnuregex.h>
#else
	#include <regex.h>
#endif

#define SOCK_NULL 0
#define SOCK_READY 1
#define SOCK_BROKEN 2
#define SOCK_CONNECTING 3
#define SOCK_LISTEN 4

typedef unsigned int u_32bit_t;
typedef u_32bit_t IP;

struct socket {
	char type;
	char status;
	int id;
	int fd;
	int last_ts;
	int con_ts;
	char checkLink;
	char buffer[4096];
	struct socket *next;
};

struct Server {
	char *hostname;
	int port;
	char *password;
	int numtries;
	int lastconnect;
	int lastfail;
};

struct repeat {
	int id;
	int ts;
	char count;
};

struct criminalRecord {
	char nick[NICKSIZE];
	char *identhost;
	char longTerm;
	char shortTerm;
	int laststamp;
	int lastJoin;
	int njoins;
	struct repeat trackrepeat[REPEAT_HISTORY_SIZE];
	struct criminalRecord *prev;
	struct criminalRecord *next;
};

struct rBot {
	char name[32];
	char set_nick[32];
	char set_network[32];
	char flags[FLAGLEN];
	struct rBot *bots;
	struct rBot *next;
};

struct rMember {
	int id;
	char nick[32];
	char bot[32];
	char *host;
	struct rMember *next;
	struct rMember *prev;
};

struct linkline {
	int id;
	char type;
	char *name;
	char *password;
	char *ip;
	int port;
	char *flags;
	char burst;
	struct rBot *bots;
	struct linkline *next;
};

struct dcc {
	int id;
	char *user;
	char *host;
	struct socket *con;
	struct sadmin *adm;
};

struct softBan {
	char *banmask;
	char *reason;
	char type;
	char *setby;
	int expires;
	struct softBan *next;
	struct softBan *prev;
};

struct chanRec {
	char *name;
	char flags[FLAGLEN+1];
	char key[KEYLEN+1];
	char goodModes[MODELEN+1];
	char badModes[MODELEN+1];
	struct schan *physical;
	struct softBan *softbans;
	struct chanRec *next;
	struct chanRec *prev;
};

struct sadmin {
	char nick[NICKSIZE];
	char pass[PASSLEN+1];
	char actualNick[NICKSIZE];
	int level;
	struct sadmin *next;
	struct sadmin *prev;
};

struct ircUser
{
	char nick[NICKSIZE];
	char ident[IDENTSIZE];
	char host[HOSTSIZE];
	char mask[MASKSIZE];
	char gnuUser[32];
};

struct suser {
	char nick[NICKSIZE];
	char *ident;
	char *host;
	char *mask;
	char *realname;
	char isVoice;
	char isOp;
	int lastMsg;
	int flines;
	int joinTime;
	int nickchanges;
	int lastNick;
	int idleMsg;
	struct suser *next;
	struct suser *prev;
};

struct sban {
	char *mask;
	char *setby;
	int ts;
	struct sban *next;
	struct sban *prev;
};

struct schan {
	char *name;
	char topic[TOPICLEN+1];
	char modes[MODELEN+1];
	char key[KEYLEN+1];
	int limit;
	int tsLim;
	struct criminalRecord *nortyusers;
	struct sban *bans;
	struct suser *user;
	int userSize;
	int banSize;
	int flood_last;
	int flood_lines;
	struct schan *next;
	struct schan *prev;
	struct chanRec *record;
};

struct sial {
	struct schan *channel;
//	int chanSize;
};

struct badUrl {
	char *match;
	int count;
	struct badUrl *next;
};

#ifdef INCLUDE_ALLCOMP
struct tagRet {
	int counted_users;
	int num_tags;
	char topTags[300];
};

struct tagList {
	char tag[NICKSIZE];
	int count;
};
#endif

typedef struct myConfig CONFIG;
typedef struct ircUser sUser;

//GLOBAL VARS
EXTERN CONFIG config;
EXTERN CONFIG lastcfg;
EXTERN int gotworkingcfg;
EXTERN char cNick[32];
EXTERN int tStart;
EXTERN int tConMax;
EXTERN int lastMsg;
EXTERN int bper;
EXTERN struct sial ial;
EXTERN struct sadmin admins;
EXTERN struct chanRec chans;
EXTERN FILE *out;
EXTERN char redoAdm[513];
EXTERN char myip[16];
EXTERN char myNetwork[32];
EXTERN char argmodes[32];
EXTERN int redoAdmTs;
EXTERN regex_t lamehost_5, lamehost_10, lamehost_15, safehosts;
EXTERN struct Server servers[MAX_SERVERS];
EXTERN int numserves;
EXTERN struct Server *serv;

EXTERN int timeSkewWait;
EXTERN char doHop;
EXTERN int lastConnect;
EXTERN struct socket *main_irc;

/* counters */
EXTERN int count_main_in;
EXTERN int count_main_out;
EXTERN int count_dcc_in;
EXTERN int count_dcc_out;
EXTERN int count_oj_in;
EXTERN int count_oj_out;
EXTERN int count_link_in;
EXTERN int count_link_out;

void *myalloc(size_t size);
#ifndef SOLARIS
void printmem();
#endif

// irc stuff
void sRelay(char *message);
void sRelay_butone(char *message, struct socket *tso);
void override_config(char *type, int *location, int newint);
void load_config(char *filename);
void check_config();
void attempt_reload_conf(char *filename);
void service_auth();


// signals.c

void setup_signals();
void write_pid(char *file);

// queue
void setup_queue();
void highMsg(char *message);
void lowMsg(char *message);
void quickMsg(char *message);
void popMsg();
void reset_queue();

// ial
int count_criminals(struct schan *c);
int userExists(char *nick);
void cl_add(char *name);
void cl_del(char *name);
struct suser *u_add(struct schan *channel, char *nick, char *ident, char *host);
void u_del(struct schan *theChan, struct suser *theUser);
void clear_users(struct schan *c);
void clear_ial();
struct suser *findUser(struct schan *channel, char *nick);
struct suser *findiUser(struct schan *channel, char *nick);
struct schan *findChan(char *name);
struct schan *findiChan(char *name);
void removeMode(struct schan *theChan, char mode);
void addMode(struct schan *theChan, char mode);
struct sban *findBan(struct schan *channel, char *mask);
struct sban *bl_add(struct schan *channel, char *mask, char *setby, int ts);
void bl_del(struct schan *theChan, struct sban *theBan);
void clear_nortyusers(struct schan *c);
struct criminalRecord *nu_add(struct schan *channel, char *nick, char *ident, char *host);
void nu_del(struct schan *theChan, struct criminalRecord *criminal);
struct criminalRecord *findCriminal(struct schan *channel, char *nick);

// socket stuff
void kill_dcc_listen();
unsigned long get_listen_ip();
void fixBrokenSocks();
void stop_running();
void setup_socks();
int sockwrite(struct socket *outsock, char *output);
struct socket *add_listen(char type, int id, int port, char *vhost);
struct socket *add_socket(char type, int id, char *addr, int port, char *vhost, unsigned long ulip);
int irc_bind(int fd, char *vhost, int port);
void irc_disconnect();
int irc_write(char *output);
int io_loop();
void clean_all_socks();

// tools
char *cropnick(const char *nick, int nicklen);
char* mystrncpy(char* s1, const char* s2, size_t n);
int tokenize( char *line, char *separators, char *tokens[], int max );
int write_memory(char *file, char *begin, unsigned size);
int read_memory(char *file, char *begin, unsigned size);
char *fixed_duration(int duration);
int world_uptime();
int strmi(char *str1, char *str2);
double my_cputime();
int wildcmp(char *wild, char *string);
int wildicmp(char *wild, char *string);
char checkSimChars(char *str1, char *str2);
char *small_duration(int duration);
void str_remove(char *str, char *remove);
void modifyFlags(char *flags, char *changes);

// handle
void parseLine(char *in);
void make_user_struct(char *mask, sUser *usr);

void checkLastPointer(struct schan *removal);

// admin.c
void clear_admins();
void logout_all_admins();
int load_admins(char *filename);
int save_admins(char *filename);
int adm_add(char *user, char *pass, int level);
int adm_del(char *admin);
char *passCrypt(char *in);
struct sadmin *findActualAdmin(char *name);
struct sadmin *checkLogin(char *nick, char *user, char *pass, struct socket *dcc);
struct sadmin *findAdmin(char *nick);
struct sadmin *automagic_login(char *account, char *nick);
void checkCommonAdmin(char *nick);
void checkAllAdmins();
int chPass(struct sadmin *theAdmin, char *newpass);
int count_admins();
void giveAdminStatus(char *nick, int *level);

// events.c

void on_connect();
void on_privmsg(sUser *theUser, char *target, char *message, char *tokens[], int *ctok);
void on_join(sUser *theUser, char *channel);
void on_kick(sUser *theUser, char *chan, char *victim, char *kickmsg);
void on_part(sUser *theUser, char *channel);
void on_quit(sUser *theUser, char *msg);
void on_nick(sUser *theUser, char *newnick);
void on_topic(sUser *theUser, char *channel, char *topic);
void on_mode(sUser *theUser, char *channel, char sign, char mode, char *argument);
void on_invite(sUser *theUser, char *channel);


void admin_msg(sUser *theUser, char *message, char *tokens[], int *ctok, struct socket *dcc, struct sadmin *adm);

// help
void cmdList(sUser *theUser, struct socket *dcc);
void syntaxHelp(sUser *theUser, char *cmd, struct socket *dcc);
void mainHelp(sUser *theUser, char *tok2, struct socket *dcc);
void showFlags(sUser *theUser, struct socket *dcc);

// timers
void reset_timers();
void init_timers();
void check_timers();
void runTimer5();
void checkNiceModes(struct schan *theChan);
void checkLimit(struct schan *theChan);
void expireCriminals();

//chans
int isChanWithJ();
void load_chans(char *filename);
int cr_add(char *name, char *flags, char *key, char *gModes, char *bModes);
struct chanRec *fetchChanRec(char *name);
int cr_del(char *name);
int save_chans(char *filename);
void clear_softbans(struct chanRec *cr);
int br_add(char *channel, char *banmask, char *reason, char type, char *setby, int expires);
int br_del(struct chanRec *theChan, char *banmask);
void clear_chans();
struct softBan *fetchSoftBan(struct chanRec *theChan, char *banmask);
char validFlags(char *str, char *valid);
int count_chans();
int count_softBansOn(struct chanRec *c);

//tasks
void massMode(struct schan *chan, char sign, char mode, int count, char *arguments);
int isOp(struct schan *chan, char *nick);
void check_idle_ops(struct chanRec *theChan);
int count_clones(struct schan *theChan, char *host);
int enforce_ban(struct schan *theChan, char *mask, char *reason, char *setby, int expire);
void unsetBans(struct schan *chan, int count, char *args);
void sendQUIT(char *extra);
void checkLamehost(struct schan *theChan, sUser *theUser);
void checkBadHost(struct schan *theChan, sUser *theUser);
void massDeop(struct schan *chan, int count, char *args);

//bans
int rn_ban(struct chanRec *theChan, char *mask, char *setby, int secs, char *reason);
void chkfor_rn_ban(struct schan *theChan, struct suser *theUser);
void chkfor_ban(struct schan *theChan, struct suser *theUser);
int chan_has_rn_bans(struct chanRec *theChan);
void searchForMissedBans(struct schan *theChan);
void expireBans();
int *wildunban(struct chanRec *theChan, char *mask);
int normal_ban(struct chanRec *theChan, char *mask, char *setby, int secs, char *reason);
void makeBanSpace(struct schan *theChan, int space);
int isHardBan(struct schan *c, char *mask);
int isSoftBan(struct chanRec *c, char *mask);

//chan_msg
void chan_msg(sUser *theUser, char *channel, char *message, char *tokens[], int *ctok, char isNotice);
void init_regex();
void free_regex();
int regmatch(regex_t *re, char *string);

//punish
void write_cstat_msg(char *buffer);
void punish_user(struct chanRec *theChan, struct suser *theUser, char type, char subtype, char *extra);

//servers
void register_self();
void initiate_main_connect();
void init_servers();
void kill_servers();
struct Server *get_next_server();
int add_server(char *confstring);

//dcc

void dcc_close_user(int did, char *msg);
char *get_dcc_user(int id);
char *get_dcc_host(int id);
void set_dcc_adm(int id, struct sadmin *adm);
void dcc_closeall(char *msg);
void admin_deleted(struct sadmin *admin);
void dcc_connected(int id);
int send_to_dcc_butonesock(char *line, struct socket *sock, int for_guest);
int send_to_dcc_butone(int did, char *line);
void zero_dcclist();
struct dcc *add_dcc_chat(char *nick, char *host, struct sadmin *adm, struct socket *con);
void chat_terminated(int id);
int get_next_dcc_id();
int send_to_all_dcc(char *line, int for_guest);
void parseDCC(int id, char *line);


// ojspam

int add_vhost(char *confstring);
int add_word(char *confstring);
void init_oj();
void oj_line(char *message);
void wipe_ojdata();
void checkNeedCon();
void oj_connect();
void oj_dead();

// links

void send_to_all_links(char *buffer);
void send_to_links_butone(int id, char *buffer);
void send_diepartyliner(int dccid, char *nick, char *reason);
void send_newpartyliner(int dccid, char *nick);
void send_partylinemsg(char *nick, char *message);
void send_partylineaction(char *nick, char *message);
void reset_linklines();
int add_link(char *confstring, char type);
int get_next_link_id();
struct linkline *fetch_link_id(int id);
struct linkline *fetch_link(char *name);
void start_links();
void firelink_init();
int get_next_fire_id();
void firelink_new(struct socket *sock, int accepting);
void firelink_parse(struct socket *sock, char *buffer);
void firelink_add(struct socket *sock);
void firelink_die(struct socket *sock);
void firelink_burst(struct socket *sock, struct linkline *linkdat);
int find_unlink_bot(struct rBot **start, char *name);
void unlink_bot(struct rBot *bot);
struct rBot *fetch_bot_from_all(char *name);
struct rBot *fetch_bot_from_line(char *name, struct rBot *linkline);
void send_tree(struct socket *sock);
void disconnect_all_links(char *reason);
int firelink_wildconnect(char *mask);
void send_listlinks(struct socket *sock);
int count_users(char *botname);
int check_dependants(struct schan *theChan);
