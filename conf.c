// Fireclaw Copyright (C) 2003 Ian Packer <snazbaz at fireclaw.org>
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// 
// $Id: conf.c,v 1.30 2003/06/25 19:05:07 snazbaz Exp $
//

#include <ctype.h>

#include "main.h"

static void syntax_err(int line, char *reason);
void add_badurl(char *match);
void reset_badurls();

struct confset {
	char *var;
	void *memory;
	char type;
	int length;
	char *DEFAULT;
};

static struct confset setmap[] = 
{
	{ "nick", &config.nick, 's', NICKSIZE-1, NULL },
	{ "ident", &config.ident, 's', 15, "fireclaw" },
	{ "realname", &config.realname, 's', 63, "http://www.fireclaw.org" },
	{ "vhost", &config.vhost, 's', 19, "" },
	{ "relaychan", &config.relaychan, 's', 63, "" },
	{ "servicePrefix", &config.servicePrefix, 's', 127, "" },
	{ "serviceAuth", &config.serviceAuth, 's', 127, "" },
	{ "serviceGhost", &config.serviceGhost, 's', 127, "" },
	{ "serviceOp", &config.serviceOp, 's', 127, "" },
	{ "warntemplate", &config.warntemplate, 's', 255, W_TEMPLATE },
	{ "usermodes", &config.usermodes, 's', 63, M_USERMODES },
	{ "maxmodes", &config.maxmodes, 'i', 6, NULL },
	{ "banlim", &config.banlim, 'i', 45, NULL },
	{ "caps_max", &config.caps_max, 'i', 70, NULL },
	{ "ctrlchar_max", &config.ctrlchar_max, 'i', 60, NULL },
	{ "repeatchar_lim", &config.repeatchar_lim, 'i', 15, NULL },
	{ "maxkicks", &config.maxkicks, 'i', 10, NULL },
	{ "float.thresh", &config.float_thresh, 'i', 10, NULL },
	{ "float.period", &config.float_period, 'i', 3, NULL },
	{ "flood.period", &config.flood_period, 'i', 7, NULL },
	{ "flood.limit", &config.flood_limit, 'i', 7, NULL },
	{ "flood.global.period", &config.flood_global_period, 'i', 20, NULL },
	{ "flood.global.limit", &config.flood_global_limit, 'i', 40, NULL },
	{ "flood.join.period", &config.flood_join_period, 'i', 25, NULL },
	{ "flood.join.limit", &config.flood_join_limit, 'i', 3, NULL },
	{ "flood.nick.period", &config.flood_nick_period, 'i', 80, NULL },
	{ "flood.nick.limit", &config.flood_nick_limit, 'i', 4, NULL },
	{ "flood.nick.+m.period", &config.flood_nick_mperiod, 'i', 10, NULL },
	{ "flood.nick.+m.limit", &config.flood_nick_mlimit, 'i', 2, NULL },
	{ "repeat.limit", &config.repeat_limit, 'i', 3, NULL },
	{ "connect.attempts", &config.contry, 'i', 0, NULL },
	{ "connect.delay", &config.condelay, 'i', 60, NULL },
	{ "slowkicks", &config.slowkicks, 'i', 0, NULL },
	{ "dcc.allowguests", &config.dcc_allowguests, 'i', 0, NULL },
	{ "dcc.timeout", &config.dcc_timeout, 'i', 7200, NULL },
	{ "dcc.listenport", &config.dcc_listenport, 'i', 0, NULL },
	{ "dcc.isolate", &config.dcc_isolate, 'i', 0, NULL },
	{ "dcc.listenip", &config.dcc_listenip, 's', 15, "0" },		
	{ "link.port", &config.linkport, 'i', 0, NULL },
	{ "link.name", &config.linkname, 's', 31, NULL },		
	{ "idle.limit", &config.idle_limit, 'i', 3600, NULL },
	{ "idle.devoice", &config.idlevoice, 'i', 1, NULL },
	{ "clonelimit", &config.clonelimit, 'i', 4, NULL },
	{ "showusermsgs", &config.showusermsgs, 'i', 0, NULL },
	{ "noctcps", &config.noctcps, 'i', 0, NULL },
	{ "cleanbanlist", &config.cleanbanlist, 'i', 0, NULL },
	{ "nooverride", &config.nooverride, 'i', 0, NULL },
	{ "onjoin.listenperiod", &config.oj_listenperiod, 'i', 60, NULL },
	{ "onjoin.period", &config.oj_period, 'i', 600, NULL },
	{ "badurl.msg", &config.msg_badurl, 's', 127, "BadURL: Spamming these is not tolerated." },
	{ "colour.msg", &config.msg_colour, 's', 127, "No colours please!" },
	{ "onjoin.msg", &config.msg_onjoin, 's', 127, "On-join message detected." },
	{ "flood.msg", &config.msg_flood, 's', 127, "No flooding please!" },
	{ "lamehost.msg", &config.msg_lamehost, 's', 127, "Lameness filter: Bad hostname detected." },
	{ "chanctcp.msg", &config.msg_chanctcp, 's', 127, "No channel CTCPs please!" },
	{ "kickclones.msg", &config.msg_kickclones, 's', 127, "Too many clones from your host." },
	{ "badmask.msg", &config.msg_badmask, 's', 127, "Offensive nick, ident or hostname." },
	{ "notice.msg", &config.msg_notice, 's', 127, "No channel notices please!" },
	{ "caps.msg", &config.msg_caps, 's', 127, "No excesive CAPS please!" },
	{ "repeat.msg", &config.msg_repeat, 's', 127, "No need to repeat yourself." },
	{ "swear.msg", &config.msg_swear, 's', 127, "No offensive language please!" },
	{ "ctrlchar.msg", &config.msg_ctrlchar, 's', 127, "No excessive use of control characters please!" },
	{ "repeatchar.msg", &config.msg_repeatchar, 's', 127, "Lameness filter: Repetition of 1 char detected." },
	{ "joinflood.msg", &config.msg_joinflood, 's', 127, "Oops! Having a problem with the revolving door?" },
	{ "nickflood.msg", &config.msg_nickflood, 's', 127, "Excessive nick changes." },
	{ "badurl.length", &config.len_badurl, 'i', 86400, NULL },
	{ "colour.length", &config.len_colour, 'i', 300, NULL },
	{ "onjoin.length", &config.len_onjoin, 'i', 1800, NULL },
	{ "flood.length", &config.len_flood, 'i', 600, NULL },
	{ "lamehost.length", &config.len_lamehost, 'i', 43200, NULL },
	{ "chanctcp.length", &config.len_chanctcp, 'i', 300, NULL },
	{ "kickclones.length", &config.len_kickclones, 'i', 600, NULL },
	{ "badmask.length", &config.len_badmask, 'i', 100, NULL },
	{ "notice.length", &config.len_notice, 'i', 600, NULL },
	{ "caps.length", &config.len_caps, 'i', 300, NULL },
	{ "repeat.length", &config.len_repeat, 'i', 600, NULL },
	{ "swear.length", &config.len_swear, 'i', 600, NULL },
	{ "ctrlchar.length", &config.len_ctrlchar, 'i', 300, NULL },
	{ "repeatchar.length", &config.len_repeatchar, 'i', 300, NULL },
	{ "joinflood.length", &config.len_joinflood, 'i', 300, NULL },
	{ "nickflood.length", &config.len_nickflood, 'i', 300, NULL },
	{ 0, 0, 0, 0 }
};

void override_config(char *type, int *location, int newint)
{
	char eb[300];
	if(config.nooverride == 0 && *location != newint) {
		sprintf(eb, "Overriding config setting for '%s', as server has told us a new value (%d).", type, newint);
		sRelay(eb);
		fprintf(out, "%s\n", eb);
		*location = newint;		
	}
	return;
}

void attempt_reload_conf(char *filename)
{
		/* wipe previous config */
	reset_linklines();
	wipe_ojdata();
	kill_servers();
	reset_badurls();

		/* loadnew */
	load_config(filename);

		/* check and revert back to old if necessary */
	check_config();
	return;
}

static void bad_config(int i)
{
	char eb[BUFSIZE];
	sprintf(eb, "Bad config, missing required field: '%s'", setmap[i].var);
	if(gotworkingcfg) {
		strcat(eb, "Reverting to previous config.");
		sRelay(eb);
		config = lastcfg;
	} else {
		strcat(eb, "\n");
		fprintf(out, eb);
		exit(0);
	}
	return;
}

void check_config()
{
	int i;

		for(i = 0; setmap[i].var; i++)
		{
			// ints
			if(setmap[i].type == 'i') {
				if(*(int*)setmap[i].memory == 0) {
					if(setmap[i].length == -1)
						{ bad_config(i); return; }
					else
						*(int*)setmap[i].memory = setmap[i].length;
				}
				continue;
			}
			// string
			if(setmap[i].type == 's') {
				if(!(*(char*)setmap[i].memory)) {
					if(setmap[i].DEFAULT != NULL)
						strcpy(setmap[i].memory, setmap[i].DEFAULT);
					else
						{ bad_config(i); return; }
				}
				continue;
			}
		}
	
	/* setup float vars. This saves us calculating these values each time
		we check the limit... actually this optimisation is probably a bit OTT */
	config.float_step = (int)(config.float_thresh/2);
	config.float_step4 = 3 * config.float_step;

	/* ojperiod must be more than 5 mins */
	if(config.oj_period < 300)
		config.oj_period = 300;

	/* set dcc_listenip to our vhost */
	config.dcc_listenip[15] = 0;
	mystrncpy(config.dcc_listenip, config.vhost, 16);

	/* lets re-open our listening socket, since port might have changed etc */
	kill_dcc_listen();
	disconnect_all_links("ERROR Config rehash");

	if(config.dcc_listenport > 0) {
		if(add_listen('D', 3, config.dcc_listenport, config.dcc_listenip)) {
			fprintf(out, "Listening for connections on %s:%d\n", config.dcc_listenip, config.dcc_listenport);
		}
	}
	if(config.linkport > 0) {
		if(add_listen('F', 4, config.linkport, config.vhost)) {
			fprintf(out, "Listening for connections on %s:%d\n", config.vhost, config.linkport);
		}
	}

	/* everything is fine! */
	gotworkingcfg = 1;
	lastcfg = config;
	return;
}

static void dealwith_line(int line, char *cmd, char *var, char *value)
{
	int i;

	if(strmi(cmd, "die"))
		exit(0);

	if(strmi(cmd, "set"))
	{
		for(i = 0; setmap[i].var; i++)
		{
			if(strmi(setmap[i].var, var))
			{
				if(setmap[i].type == 's') {
					if(strlen(value) <= setmap[i].length)
						mystrncpy((char*)setmap[i].memory, value, setmap[i].length+1);
					else
						syntax_err(line, "Value too long");
				} else if(setmap[i].type == 'i') {
					(*(int*)setmap[i].memory) = atoi(value);
				}
				return;
			}
		}
		syntax_err(line, "Variable name not recognised");
		return;
	}

	if(strmi(var, "server")) {
		if(!add_server(value))
			fprintf(out, "Warning, MAX_SERVERS exceeded.\n");
	} else if(strmi(var, "badurl")) {
		add_badurl(value);
	} else if(strmi(var, "word")) {
		add_word(value);
	} else if(strmi(var, "vhost")) {
		add_vhost(value);
	} else if(strmi(var, "link")) {
		add_link(value, 'f');
	}

	return;
}

static void syntax_err(int line, char *reason)
{
	char eb[BUFSIZE];
	sprintf(eb, "Syntax warning, line %d: %s", line, reason);
	sRelay(eb);
	strcat(eb, "\n");
	fprintf(out, eb);
	return;
}

int check_cmdstr(int line, char *cmd)
{
	if(strmi(cmd, "set"))
		return 1;
	if(strmi(cmd, "add"))
		return 1;
	if(strmi(cmd, "die"))
		return 1;
	syntax_err(line, "Invalid command");
	return 0;
}

void load_config(char *filename)
{
	FILE *fp;
	char filebuf[BUFSIZE];
	char *cmd, *var, *value;
	int line = 0;

	memset(&config, 0, sizeof(config));

	fp = fopen(filename, "r");

	if(fp == NULL)
	{
		fprintf(out, "Cannot open %s\n", filename);
		exit(0);
	}

	while(!feof(fp))
	{
		line++;
		fgets(filebuf, BUFSIZE-1, fp);
		cmd = filebuf;		
		/* skip whitespace */
		while(*cmd == ' ' || '\t' == *cmd)
			cmd++;

		if(strlen(cmd) < 5)
			continue;

		if(!isalpha((int)*cmd))
			continue;

		/* cmd is done */
		var = strchr(cmd, ' ');
		if(!var) {
			syntax_err(line, "No variable name or value specified.");
			continue;
		}

		*var++ = '\0';
		if(!check_cmdstr(line, cmd))
			continue;

		value = strchr(var, ' ');
		if(!value) {
			syntax_err(line, "Missing a value.");
			continue;
		}

		*value++ = '\0';

		if(value[strlen(value)-2] == '\r' || value[strlen(value)-2] == '\n')
			value[strlen(value)-2] = '\0';
		else
			value[strlen(value)-1] = '\0';

		dealwith_line(line, cmd, var, value);
	}


	fclose(fp);
	return;
}

