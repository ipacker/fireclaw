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
// $Id: admin_msg.c,v 1.25 2003/06/25 23:37:41 snazbaz Exp $
//

#include <ctype.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "main.h"

extern struct dcc dcclist[MAX_DCC];
extern struct socket *sockets;
extern struct rMember *rusers;

static char reply[BUFSIZE];
static char *NO_REASON = "No reason supplied.";

static int decipher_duration(char *duration);

#ifdef REPORT_UPTIME
void set_uptime_status(char *str);
#endif

#ifdef INCLUDE_ALLCOMP
struct tagRet *count_tags(struct schan *theChan, int no_spam);
#endif

void send_punish_list(char *nick);

void send_reply(sUser *theUser, char *reply, char urgency, struct socket *dcc)
{
	char final[513];
	if(dcc == NULL) {
		snprintf(final, 510, "NOTICE %s :%s", theUser->nick, reply);
		switch(urgency) {
			case 'h':
				highMsg(final);
				break;
			case 'l':
				lowMsg(final);
				break;
			case 'q':
				quickMsg(final);
				break;
		}
	} else {
		sockwrite(dcc, reply);
	}
}

void admin_msg(sUser *theUser, char *message, char *tokens[], int *ctok, struct socket *dcc, struct sadmin *adm)
{
	static int lastWhois = 0;
	int i, rtok;
	char cmd[32];
	char *pch;
	struct sadmin *theAdmin;
	struct schan *theChan;
	struct suser *iuser;

	if(strlen(tokens[3]) > 31)	/* no cmds this long, and no space to store them */
		return;

//	if(!main_irc)
//		return;
	
	for(i = 1; i <= strlen(tokens[3]); i++)
		cmd[i-1] = tolower(tokens[3][i]);

	rtok = *ctok - 3;

	if(!strcmp(cmd, "login"))
	{
		if(rtok < 3)
			return;

		theAdmin = checkLogin(theUser->nick, tokens[4], tokens[5], dcc);
		if(theAdmin)
		{
			sprintf(reply, "Login granted. Access level: %d", theAdmin->level);
			send_reply(theUser, reply, 'h', dcc);
			sprintf(reply, "[\002login\002] %s logged in as %s.", *theUser->nick ? theUser->nick : "Unauthed user", theAdmin->nick);
			sRelay_butone(reply, dcc);
			if(dcc) {
				set_dcc_adm(dcc->id, theAdmin);
				sprintf(reply, "Your DCC access level is elevated to %d, this does not affect your IRC client's access.", theAdmin->level);
				send_reply(theUser, reply, 'h', dcc);
			}
		} else {
			sprintf(reply, "Login denied.");
			send_reply(theUser, reply, 'l', dcc);
			sprintf(reply, "[\002login\002] %s %sfailed login.", *theUser->nick ? theUser->nick : "Unauthed user", dcc ? "(dcc) " : "");
			sRelay_butone(reply, dcc);
		}
		return;
	} 

	theAdmin = dcc ? adm : findActualAdmin(theUser->nick);

	if(theAdmin || config.showusermsgs == 1) {
		if(!strcmp(cmd, "newpass"))
			sprintf(reply, "[%s(\002%s\002)] newpass ******", theUser->nick, (theAdmin) ? theAdmin->nick : "User");
		else if(!strcmp(cmd, "newadmin"))
			snprintf(reply, 510, "[%s(\002%s\002)] newadmin %s ****** %s", theUser->nick, (theAdmin) ? theAdmin->nick : "User",
					tokens[4] ? tokens[4] : "", tokens[6] ? tokens[6] : "");
		else
			snprintf(reply, 510, "[%s(\002%s\002)] %s", theUser->nick, (theAdmin) ? theAdmin->nick : "User", message);
		sRelay_butone(reply, dcc);
	}

	if(!strcmp(cmd, "who") && (dcc || theAdmin))
	{
		struct rMember *ri;
		sprintf(reply, "DCC users: %d...", get_next_dcc_id());
		send_reply(theUser, reply, 'h', dcc);
		for(i = 0; i < get_next_dcc_id(); i++) {
			if(dcclist[i].adm)
				sprintf(reply, "[%d] Admin: %s (%d) from %s.", i+1, dcclist[i].adm->nick, dcclist[i].adm->level, dcclist[i].host);
			else
				sprintf(reply, "[%d] Guest: %s from %s.", i+1, dcclist[i].user ? dcclist[i].user : "Unauthed user", dcclist[i].host);
			send_reply(theUser, reply, 'l', dcc);
		}
		if(rusers) {
			ri = rusers;
			sprintf(reply, "Remote Users:");
			send_reply(theUser, reply, 'l', dcc);
			while(ri) {
				sprintf(reply, "[%s] User: %s (%s)", ri->bot, ri->nick, ri->host);
				send_reply(theUser, reply, 'l', dcc);
				ri = ri->next;
			}
		}
		return;
	}

	if(!theAdmin && dcc)
		return;

	if(!theAdmin)
	{
		if(time(0) - lastWhois > 10) {
			lastWhois = time(0);
			sprintf(reply, "WHOIS %s", theUser->nick);
			highMsg(reply);
			redoAdmTs = time(NULL);
			sprintf(redoAdm, "%s PRIVMSG %s :%s", theUser->mask, cNick, message);
		}
		return;
	}

	if(!strcmp(cmd, "whoami"))
	{
		sprintf(reply, "You are %s with access level %d.", theAdmin->nick, theAdmin->level);
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "commands"))
	{
		cmdList(theUser, dcc);
		return;
	}

	if(!strcmp(cmd, "showflags"))
	{
		showFlags(theUser, dcc);
		return;
	}

	if(!strcmp(cmd, "help"))
	{
		mainHelp(theUser, tokens[4], dcc);
		return;
	}

	if(!strcmp(cmd, "status"))
	{
		int iter;
		#ifndef SOLARIS
			struct mallinfo mem = mallinfo();
		#endif
		sprintf(reply, "\002Fireclaw\002 %s running %s", VERSION_STR, fixed_duration(time(0) - tStart));
		send_reply(theUser, reply, 'h', dcc);
		#ifndef SOLARIS
			sprintf(reply, "Memory> Heap: \002%d\002kB, Used: \002%d\002kB", 
				(int)((float)mem.arena/1024.0), (int)((float)mem.uordblks/1024.0));
			send_reply(theUser, reply, 'h', dcc);
		#endif
		sprintf(reply, "CPUTime: \002%.2f\002s, Connected time: %s (to: %s)", my_cputime(), 
				main_irc ? fixed_duration(time(0) - main_irc->con_ts) : "Not connected.", myNetwork );
		send_reply(theUser, reply, 'h', dcc);
		for(iter = 0; iter < numserves; iter++)
		{
			sprintf(reply, "Server: (%03d attempts) %s %s", servers[iter].numtries, servers[iter].hostname, (&servers[iter] == serv) ? "(\002Current\002)" : "");
			send_reply(theUser, reply, 'h', dcc);
		}
		#ifdef REPORT_UPTIME
		reply[0] = 0;
		set_uptime_status(reply);
		send_reply(theUser, reply, 'h', dcc);
		#endif
		return;
	}

	if(!strcmp(cmd, "transfer"))
	{
		sprintf(reply, "Main socket: \002%.2f\002kB in, \002%.2f\002kB out.", ((float)count_main_in/1024.0), ((float)count_main_out/1024.0));
		send_reply(theUser, reply, 'h', dcc);
		sprintf(reply, "DCC Chats: \002%.2f\002kB in, \002%.2f\002kB out.", ((float)count_dcc_in/1024.0), ((float)count_dcc_out/1024.0));
		send_reply(theUser, reply, 'h', dcc);
		sprintf(reply, "Bot links: \002%.2f\002kB in, \002%.2f\002kB out.", ((float)count_link_in/1024.0), ((float)count_link_out/1024.0));
		send_reply(theUser, reply, 'h', dcc);
		sprintf(reply, "On-join detector: \002%.2f\002kB in, \002%.2f\002kB out.", ((float)count_oj_in/1024.0), ((float)count_oj_out/1024.0));
		send_reply(theUser, reply, 'h', dcc);
		sprintf(reply, "Total: \002%.2f\002kB in, \002%.2f\002kB out.", 
			((float)(count_oj_in+count_main_in+count_dcc_in+count_link_in)/1024.0), 
			((float)(count_oj_out+count_dcc_out+count_main_out+count_link_out)/1024.0));
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "sockets"))
	{
		struct sockaddr getname;
		socklen_t getlen;
		struct sockaddr_in *data;
		struct socket *i = sockets;
		while(i) {
			if(!getpeername(i->fd, &getname, &getlen) || !getsockname(i->fd, &getname, &getlen)) {
				data = (struct sockaddr_in *)&getname;
				switch(i->type) {
					case 'm':
						sprintf(reply, "Socket (main) - ID: %d, with: %s - connected for: %s", i->id, inet_ntoa(data->sin_addr), fixed_duration(time(0) - i->con_ts));
						break;
					case 'd':
						sprintf(reply, "Socket (DCC) - ID: %d, with %s (%s) - connected for %s", i->id, get_dcc_user(i->id), inet_ntoa(data->sin_addr), fixed_duration(time(0) - i->con_ts));
						break;
					case 'f':
						sprintf(reply, "Socket (BotLink) - ID: %d, with %s (%s) - connected for %s", i->id, get_dcc_user(i->id), inet_ntoa(data->sin_addr), fixed_duration(time(0) - i->con_ts));
						break;
					case 'J':
						sprintf(reply, "Socket (onjoin) - ID: %d, with: %s - connected for: %s", i->id, inet_ntoa(data->sin_addr), fixed_duration(time(0) - i->con_ts));
						break;
					case 'F':
						sprintf(reply, "Socket (Link Listen) - ID: %d, port: %d - listening for: %s", i->id, config.linkport, fixed_duration(time(0) - i->con_ts));
						break;
					case 'D':
						sprintf(reply, "Socket (DCC Listen) - ID: %d, port: %d - listening for: %s", i->id, config.dcc_listenport, fixed_duration(time(0) - i->con_ts));
						break;
				}
				send_reply(theUser, reply, 'h', dcc);
			}
			i = i->next;
		}
		return;
	}

	if(!strcmp(cmd, "listchans"))
	{
		int count;
		struct chanRec *c = chans.next;
		count = 0;
		while(c)
		{
			if(c->physical)
				sprintf(reply, "\002%s\002> Flags: \002%s\002, Modes: \002%s\002, Users: \002%d\002, SoftBans: \002%d\002, HardBans, \002%d\002",
					c->name, c->flags, c->physical->modes, c->physical->userSize, count_softBansOn(c), c->physical->banSize);
			else
				sprintf(reply, "\002%s\002> Flags: \002%s\002, SoftBans, \002%d\002 (I'm not in this channel)",
					c->name, c->flags, count_softBansOn(c));
			send_reply(theUser, reply, 'l', dcc);
			count++;
			c = c->next;
		}
		sprintf(reply, "Listing \002%d\002 channels...", count);
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "listlinks")) {
		if(dcc) {
			send_listlinks(dcc);
		} else {
			sprintf(reply, "This is a DCC command only.");
			send_reply(theUser, reply, 'h', dcc);
		}
		return;
	}

	if(!strcmp(cmd, "linktree")) {
		if(dcc) {
			send_tree(dcc);
		} else {
			sprintf(reply, "This is a DCC command only.");
			send_reply(theUser, reply, 'h', dcc);
		}
		return;
	}

	if(!strcmp(cmd, "listadmins"))
	{
		int count;
		struct sadmin *a = admins.next;
		count = 0;
		while(a)
		{
			sprintf(reply, "\002%s\002> Level: \002%d\002, Current Nick: \002%s\002",
					a->nick, a->level, (a->actualNick[0]) ? a->actualNick : "Not logged in");
			send_reply(theUser, reply, 'l', dcc);
			count++;
			a = a->next;
		}
		sprintf(reply, "Listing \002%d\002 admins...", count);
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "criminalstats"))
	{
		sprintf(reply, "Offence counters: ");
		write_cstat_msg(reply);
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "shutdown"))
	{
		if(theAdmin->level < 500)
			return;
		stop_running();
		sendQUIT("shutdown");
		return;
	}

	if(!strcmp(cmd, "hop"))
	{
		if(theAdmin->level < 400)
			return;
		doHop = 1;
		sendQUIT("changing server");
		sleep(1);
		irc_disconnect();
		return;
	}

	if(!strcmp(cmd, "rehash"))
	{
		if(theAdmin->level < 500)
			return;
		attempt_reload_conf(DEFAULT_CONFIG);
		sprintf(reply, "\002%s\002 rehashing configuration.", theUser->nick);
		sRelay(reply);
		return;
	}

	if(!strcmp(cmd, "dccme") && !dcc)
	{
		if(config.dcc_listenport > 0) {
			sprintf(reply, "PRIVMSG %s :\001DCC CHAT chat %lu %d\001", theUser->nick, get_listen_ip(), config.dcc_listenport);
		} else {
			sprintf(reply, "NOTICE %s :Reverse DCC support not enabled. Set dcc.listenport in the configuration.", theUser->nick);
		}
		highMsg(reply);
		return;
	}

////////////////////////////////////////////////////////////////////
	if(rtok < 2) 
	{ 
		syntaxHelp(theUser, cmd, dcc); 
		return; 
	}
////////////////////////////////////////////////////////////////////

	if(!strcmp(cmd, "connect"))
	{
		if(theAdmin->level < 100)
			return;
		if(!firelink_wildconnect(tokens[4])) {
			sprintf(reply, "No unconnected links matching that mask.");
			send_reply(theUser, reply, 'h', dcc);
		}
		return;
	}

	if(!strcmp(cmd, "boot"))
	{
		i = atoi(tokens[4]) - 1;
		if(i >= get_next_dcc_id() || i < 0) {
			sprintf(reply, "Couldn't match a DCC session with id: %s.", tokens[4]);
		} else if(dcclist[i].adm && dcclist[i].adm->level >= theAdmin->level) {
			sprintf(reply, "Can't boot %s, they have equal or more access.", dcclist[i].adm->nick);
		} else {
			char tbuf[64];
			sprintf(reply, "Booted by %s.", theAdmin->nick);
			mystrncpy(tbuf, dcclist[i].user, 64);
			dcc_close_user(i, reply);
			sprintf(reply, "%s was booted.", tbuf);
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

#ifdef INCLUDE_ALLCOMP
	if(!strcmp(cmd, "tagcount") || !strcmp(cmd, "fakecount"))
	{
		struct tagRet *res;
		theChan = findiChan(tokens[4]);
		if(!theChan || !theChan->record)
			sprintf(reply, "I'm not in that channel.");
		else {
			res = count_tags(theChan, (cmd[0] == 't') ? 0 : 1);
			if(res)
				sprintf(reply, "Checked %d nicks, found %d tags and wrote the output to %s. Top 3 summary: %s", 
					res->counted_users, res->num_tags, ALLCOMP_FINAL, res->topTags);
			else 
				sprintf(reply, "Error, couldn't open files for writing.");
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}
#endif

	if(!strcmp(cmd, "op") || !strcmp(cmd, "deop") || !strcmp(cmd, "voice") || !strcmp(cmd, "devoice"))
	{
		char arguments[500];
		if(theAdmin->level < 100 && strlen(cmd) <= 4)
			return;
		theChan = findiChan(tokens[4]);
		if(!theChan || !theChan->record) {
			sprintf(reply, "I'm not in that channel.");
		} else if(!isOp(theChan, cNick)) {
			sprintf(reply, "I'm not op in that channel.");
		} else {
			int count, iter;
			char *pch = arguments;
			if(!tokens[5]) {
				count = 1;
				*pch++ = ' ';
				strcpy(pch, theUser->nick);
			} else {
				arguments[0] = count = 0;
				for(iter = 5; tokens[iter]; iter++)
				{
					strcat(arguments, " ");					
					strcat(arguments, tokens[iter]);
					if(++count >= config.maxmodes) {
						massMode(theChan, (cmd[0] == 'd') ? '-' : '+', (strlen(cmd) > 4) ? 'v' : 'o', count, arguments);
						arguments[0] = count = 0;
					}
				}
			}
			massMode(theChan, (cmd[0] == 'd') ? '-' : '+', (strlen(cmd) > 4) ? 'v' : 'o', count, arguments);
			return;
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "newpass"))
	{
		if(strlen(tokens[4]) > PASSLEN)
			sprintf(reply, "New password must be no more than %d characters.", PASSLEN);
		else if(chPass(theAdmin, tokens[4]))
			sprintf(reply, "Password changed.");
		else
			sprintf(reply, "Error changing password!");
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "chaninfo"))
	{
		theChan = findiChan(tokens[4]);
		if(!theChan || !theChan->record)
			sprintf(reply, "I'm not in that channel.");
		else {
			int realnames, ops, voices;
			realnames = ops = voices = 0;
			iuser = theChan->user;
			while(iuser != NULL)
			{
				if(iuser->realname)
					realnames++;
				if(iuser->isOp)
					ops++;
				if(iuser->isVoice)
					voices++;
				iuser = iuser->next;
			}
			sprintf(reply, "Channel: \002%s\002, Modes: \002%s\002, HardKey: \002%s\002, Limit: \002%d\002, Flags: \002%s\002, SoftKey: \002%s\002",
				 theChan->name, theChan->modes, (strlen(theChan->key) > 0) ? theChan->key : "None", 
							theChan->limit, theChan->record->flags, theChan->record->key);
			send_reply(theUser, reply, 'h', dcc);
			sprintf(reply, "GoodModes: \002%s\002, BadModes: \002%s\002, Criminal records: \002%d\002",
					theChan->record->goodModes, theChan->record->badModes, count_criminals(theChan));
			send_reply(theUser, reply, 'h', dcc);
			sprintf(reply, "Users: \002%d\002, Known Realnames: \002%d\002, Ops: \002%d\002, Voices: \002%d\002, Hardbans: \002%d\002, SoftBans: \002%d\002", 
					theChan->userSize, realnames, ops, voices, theChan->banSize, count_softBansOn(theChan->record));
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "deladmin"))
	{
		if(theAdmin->level < 500)
			return;
		if(strlen(tokens[4]) >= NICKSIZE)
			sprintf(reply, "Admin doesn't exist.");
		else if(adm_del(tokens[4]))
		{
			sprintf(reply, "Admin removed.");
			save_admins(DEFAULT_ADMIN);
		} else {
			sprintf(reply, "Admin doesn't exist.");
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "delchan"))
	{
		if(theAdmin->level < 400)
			return;
		if(cr_del(tokens[4]))
		{
			sprintf(reply, "PART %s :zap!", tokens[4]);
			quickMsg(reply);
			sprintf(reply, "Channel removed.");
			save_chans(DEFAULT_CHANS);
		} else {
			sprintf(reply, "Channel doesn't exist.");
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "invite"))
	{
		if(theAdmin->level < 100)
			return;
		theChan = findiChan(tokens[4]);
		if(!theChan) {
			sprintf(reply, "Not in that channel.");
		} else {
			struct suser *me = findUser(theChan, cNick);
			if(me && me->isOp) {
				/* should this be INVITE %s :%s ? */
				sprintf(reply, "INVITE %s %s", theUser->nick, tokens[4]);
				highMsg(reply);
				return;
			} else {
				sprintf(reply, "I'm not an operator in that channel.");
			}
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "newchan"))
	{
		if(theAdmin->level < 400)
			return;
		if(cr_add(tokens[4], DEFAULT_CHANFLAGS, "0", "+", "-"))
		{
			sprintf(reply, "JOIN %s", tokens[4]);
			lowMsg(reply);
			sprintf(reply, "Channel added with default flags (%s)", DEFAULT_CHANFLAGS);
			save_chans(DEFAULT_CHANS);
		} else {
			sprintf(reply, "Channel already exists or name was too long (64+ chars)!");
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}	

	if(!strcmp(cmd, "raw"))
	{
		if(theAdmin->level < 500)
			return;
		pch = message + (tokens[4] - tokens[3]) - 1;
		if(pch)
			highMsg(pch);
		return;
	}

////////////////////////////////////////////////////////////////////
	if(rtok < 3) 
	{ 
		syntaxHelp(theUser, cmd, dcc); 
		return; 
	}
////////////////////////////////////////////////////////////////////

	if(!strcmp(cmd, "mode"))
	{
		if(theAdmin->level < 100)
			return;
		pch = message + (tokens[5] - tokens[3]) - 1;
		if(pch)
		{
			sprintf(reply, "MODE %s %s", tokens[4], pch);
			highMsg(reply);
		}
		return;
	}

	if(!strcmp(cmd, "say"))
	{
		if(theAdmin->level < 400)
			return;
		pch = message + (tokens[5] - tokens[3]) - 1;
		if(pch)
		{
			sprintf(reply, "PRIVMSG %s :%s", tokens[4], pch);
			highMsg(reply);
		}
		return;
	}

	if(!strcmp(cmd, "setkey"))
	{
		struct chanRec *theRec = fetchChanRec(tokens[4]);
		if(theAdmin->level < 100)
			return;
		if(!theRec)
			sprintf(reply, "Channel is not in my database.");
		else if(strlen(tokens[5]) > KEYLEN)
			sprintf(reply, "Key too long.");
		else
		{
			strcpy(theRec->key, tokens[5]);
			save_chans(DEFAULT_CHANS);
			sprintf(reply, "Key updated.");
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "setflags"))
	{
		struct chanRec *theRec = fetchChanRec(tokens[4]);
		if(theAdmin->level < 400)
			return;
		if(!theRec)
			sprintf(reply, "Channel is not in my database.");
		else if(tokens[5][0] != '+' && tokens[5][0] != '-')
			sprintf(reply, "You must add or subtract modes. e.g +je-l");
		else
		{
			modifyFlags(theRec->flags, tokens[5]);
//			strcpy(theRec->flags, tokens[5]);
			save_chans(DEFAULT_CHANS);
			if(!strchr(theRec->flags, 'j') && theRec->physical) {
				sprintf(reply, "PART %s", theRec->name);
				lowMsg(reply);
			}
			sprintf(reply, "Flags updated to '%s' for %s.", theRec->flags, theRec->name);
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "setgoodmodes"))
	{
		struct chanRec *theRec = fetchChanRec(tokens[4]);
		if(theAdmin->level < 100)
			return;
		if(!theRec)
			sprintf(reply, "Channel is not in my database.");
		else if(strlen(tokens[5]) > MODELEN)
			sprintf(reply, "Modestring too long.");
		else if(tokens[5][0] != '+')
			sprintf(reply, "Syntax requires modes start with a '+', e.g +nt");
		else if(checkSimChars(tokens[5]+1, MODES_ARGS))
			sprintf(reply, "The mode '%c' has arguments and therefore is not suitable here.", checkSimChars(tokens[5]+1, MODES_ARGS));
		else
		{
			strcpy(theRec->goodModes, tokens[5]);
			save_chans(DEFAULT_CHANS);
			sprintf(reply, "GoodModes updated to '%s' for %s.", tokens[5], theRec->name);
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "setbadmodes"))
	{
		struct chanRec *theRec = fetchChanRec(tokens[4]);
		if(theAdmin->level < 100)
			return;
		if(!theRec)
			sprintf(reply, "Channel is not in my database.");
		else if(strlen(tokens[5]) > MODELEN)
			sprintf(reply, "Modestring too long.");
		else if(tokens[5][0] != '-')
			sprintf(reply, "Syntax requires modes start with a '-', e.g -iml");
		else if(checkSimChars(tokens[5]+1, MODES_ARGS))
			sprintf(reply, "The mode '%c' has arguments and therefore is not suitable here.", checkSimChars(tokens[5]+1, MODES_ARGS));
		else
		{
			strcpy(theRec->badModes, tokens[5]);
			save_chans(DEFAULT_CHANS);
			sprintf(reply, "BadModes updated to '%s' for %s.", tokens[5], theRec->name);
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "me") || !strcmp(cmd, "action"))
	{
		if(theAdmin->level < 400)
			return;
		pch = message + (tokens[5] - tokens[3]) - 1;
		if(pch)
		{
			sprintf(reply, "PRIVMSG %s :\001ACTION %s\001", tokens[4], pch);
			highMsg(reply);
		}
		return;
	}

	if(!strcmp(cmd, "unban"))
	{
		int *count;
		struct chanRec *c = fetchChanRec(tokens[4]);
		if(theAdmin->level < 100)
			return;	
		if(!c)
			sprintf(reply, "Error, channel doesn't exist.");
		else if((count = wildunban(c, tokens[5])) >= 0)
			sprintf(reply, "Removed %d real bans and %d internal bans matching that mask.", count[0], count[1]);

		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "kick")) /* to make karmulian happy and joyful */
	{
		struct suser *u;
		struct chanRec *c = fetchChanRec(tokens[4]);
		if(theAdmin->level < 100)
			return;
		if(tokens[6])
			pch = message + (tokens[6] - tokens[3]) - 1;
		else
			pch = NO_REASON;
		u = NULL;
		if(c && c->physical)
			u = findUser(c->physical, tokens[5]);
		if(!c)
			sprintf(reply, "Error, channel does not exist!");
		else if(!u)
			sprintf(reply, "Cannot find %s on %s", tokens[5], tokens[4]);
		else if(u->isOp && !strchr(c->flags, 'o'))
			sprintf(reply, "%s is an op on %s!", tokens[5], tokens[4]);
		else
		{
			sprintf(reply, "KICK %s %s :%s (%s)", tokens[4], tokens[5], pch, theAdmin->nick);
			quickMsg(reply);
			return;
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "listbans"))
	{
		int count, tcount;
		struct softBan *sb;
		struct chanRec *c = fetchChanRec(tokens[4]);
		if(!c)
			sprintf(reply, "Channel doesn't exist.");
		else {
			sb = c->softbans;
			count = tcount = 0;
			while(sb)
			{
				tcount++;
				if((dcc || count < BANLIST_LIMIT) && wildicmp(tokens[5], sb->banmask)) {
					sprintf(reply, "\002%s\002 %sby %s, expires in %s. Reason: %s",
						sb->banmask, (sb->type == 'r') ? "(realname) " : "", sb->setby, 
						(sb->expires - time(NULL) > 0) ? fixed_duration(sb->expires - time(NULL)) : "< 2mins", sb->reason);
					send_reply(theUser, reply, 'l', dcc);
					count++;
				}
				sb = sb->next;
			}
			if(count > 0)
				sprintf(reply, "Listing \002%d\002 of %d internal bans on \002%s\002...", count, tcount, tokens[4]);
			else
				sprintf(reply, "No bans matching that mask.");
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "namescan"))
	{
		struct suser *i;
		int count, totn;
		struct chanRec *c = fetchChanRec(tokens[4]);
		if(!c || !c->physical)
			sprintf(reply, "Error, channel does not exist!");
		else {
			i = c->physical->user;
			count = totn = 0;
			while(i)
			{
				if(i->realname)
				{
					if(wildicmp(tokens[5], i->realname))
						count++;
					totn++;
				}
				i = i->next;
			}
			sprintf(reply, "Matched \002%d\002/%d record(s)", count, totn);
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}

////////////////////////////////////////////////////////////////////
	if(rtok < 4) 
	{ 
		syntaxHelp(theUser, cmd, dcc); 
		return; 
	}
////////////////////////////////////////////////////////////////////

	if(!strcmp(cmd, "newadmin"))
	{
		char *hashed = NULL;
		int newLevel = atoi(tokens[6]);
		
		if(theAdmin->level < 500)
			return;

		if(strlen(tokens[5]) <= PASSLEN)
			hashed = passCrypt(tokens[5]);

		if(strlen(tokens[4]) >= NICKSIZE)
			sprintf(reply, "Username must be under %d characters.", NICKSIZE);
		else if(strlen(tokens[5]) > PASSLEN)
			sprintf(reply, "Password is to long (>%d characters).", PASSLEN);
		else if(newLevel < 1 || newLevel > theAdmin->level)
			sprintf(reply, "Level must be between 1 - 500");
		else if(adm_add(tokens[4], hashed, newLevel))
		{
			sprintf(reply, "Admin added.");
			save_admins(DEFAULT_ADMIN);
		} else {
			sprintf(reply, "Admin already exists or username too long (>128 chars)!");
		}
		send_reply(theUser, reply, 'h', dcc);
		return;
	}	

	if(!strcmp(cmd, "ban"))
	{
		struct chanRec *c;
		char *abm;
		int count;
		int duration = decipher_duration(tokens[6]);
		if(theAdmin->level < 100)
			return;
		
		if(tokens[7])
			pch = message + (tokens[7] - tokens[3]) - 1;
		else
			pch = NO_REASON;

		c = fetchChanRec(tokens[4]);
		abm = NULL;

		if(!strchr(tokens[5], '!') || !strchr(tokens[5], '@'))
		{
			if(c && c->physical)
			{
				struct suser *u = findiUser(c->physical, tokens[5]);
				if(u)
				{
					abm = (char*)myalloc(strlen(u->host) + 5);
					sprintf(abm, "*!*@%s", u->host);
				}
			}
		} else 
			abm = tokens[5];
		
		if(duration <= 0)
			sprintf(reply, "Error, invalid duration. Use <x>[w|d|h|m], e.g \"3h\".");
		else if(!c)
			sprintf(reply, "Error, channel does not exist!");
		else if(!abm)
			sprintf(reply, "Cannot find %s on %s", tokens[5], tokens[4]);
		else if((count = normal_ban(c, abm, theAdmin->nick, duration, pch)) >= 0)
			sprintf(reply, "Ban added, affecting %d %s.", count, (count == 1) ? "person" : "people");
		else
			sprintf(reply, "Error adding that ban, maybe it already exists?");

		if(abm && abm != tokens[5])
			free(abm);

		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	if(!strcmp(cmd, "realnameban"))
	{
		struct chanRec *c;
		int count;
		int duration = decipher_duration(tokens[6]);
		if(theAdmin->level < 100)
			return;
		if(tokens[7])
			pch = message + (tokens[7] - tokens[3]) - 1;
		else
			pch = NO_REASON;
		c = fetchChanRec(tokens[4]);
		
		if(duration <= 0)
			sprintf(reply, "Error, invalid duration. Use <x>[w|d|h|m], e.g \"3h\".");
		else if(!c)
			sprintf(reply, "Error, channel does not exist!");
		else if(strlen(tokens[5]) > 32)
			sprintf(reply, "Mask must be under 32 characters.");
		else if((count = rn_ban(c, tokens[5], theAdmin->nick, duration, pch)) >= 0)
			sprintf(reply, "Ban added, affecting %d %s.", count, (count == 1) ? "person" : "people");
		else
			sprintf(reply, "Error adding that ban, maybe it already exists?");

		send_reply(theUser, reply, 'h', dcc);
		return;
	}

	return;
}

static int decipher_duration(char *duration)
{
	int ret;
	char type;

//	if(atoi(duration) > 0)
//		return atoi(duration);

	if(strlen(duration) < 2)
		return 0;

	type = duration[strlen(duration)-1];

	if(type >= '0' && type <= '9')
		return atoi(duration);

	duration[strlen(duration)-1] = '\0';

	ret = atoi(duration);

	if(ret <= 0)
		return 0;

	switch(type)
	{
		case 'w':
		{
			ret = (ret <= 250) ? ret * (86400*7) : 151200000;
			break;
		}
		case 'd': 
		{
			ret = (ret <= 250*7) ? ret * 86400 : 151200000; 
			break;
		}
		case 'h': 
		{
			ret = (ret <= 250*7*3600) ? ret * 3600 : 151200000; 
			break;
		}
		case 'm': 
		{
			ret = (ret <= 151200000) ? ret * 60 : 151200000; 
			break;
		}
	}

	return ret;
}

