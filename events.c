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
// $Id: events.c,v 1.26 2003/07/16 15:35:02 snazbaz Exp $
//

#include "main.h"

static char ebuf[BUFSIZE];

static void on_ctcp(sUser *theUser, char *msg, char *tokens[], int *ctok);

void on_connect()
{
	struct chanRec *i;
	
	service_auth();

	sprintf(ebuf, "MODE %s %s", cNick, config.usermodes);
	highMsg(ebuf);

	/* burst chans */
	i = chans.next;
	while(i)
	{	
		if(strchr(i->flags, 'j'))
		{
			sprintf(ebuf, "JOIN %s %s", i->name, i->key);
			lowMsg(ebuf);
		}
		i = i->next;
	}

	sprintf(ebuf, "USERIP %s", cNick);
	lowMsg(ebuf);

	return;
}

void on_privmsg(sUser *theUser, char *target, char *message, char *tokens[], int *ctok)
{

	if(target[0] == '#') {
		
		chan_msg(theUser, target, message, tokens, ctok, 0);

	} else if(!strcmp(target, cNick)) {
		if(message[0] == '\001' && message[strlen(message)-1] == '\001')
		{
			on_ctcp(theUser, message, tokens, ctok);
			return;
		} else
			admin_msg(theUser, message, tokens, ctok, 0, 0);
	}

	return;
}

static void on_ctcp(sUser *theUser, char *msg, char *tokens[], int *ctok)
{
	char type[10];
	char response[64];

	if(config.noctcps == 1)
		return;

	if(strlen(msg) > 60) /* stupidly long, doing this also avoids buffer overflows. */
	{
		msg[60] = '\001';
		msg[61] = '\0';
	}

	type[0] = 0;

	if(strcmp(msg, "\001VERSION\001") == 0)	{
		sprintf(response, "Fireclaw %s", VERSION_STR);
		strcpy(type, "VERSION");
	} else if(strcmp(msg, "\001FINGER\001") == 0) {
		strcpy(response, "kinky!");
		strcpy(type, "FINGER");
	} else if(tokens[4] && strcmp(tokens[3], ":\001PING") == 0) {
		mystrncpy(response, msg + (tokens[4] - tokens[3]) - 1, 64);
		response[strlen(response)-1] = '\0';
		strcpy(type, "PING");
	} else if(tokens[7] && !strcmp(tokens[3], ":\001DCC") && !strcmp(tokens[4], "CHAT")) {
		unsigned long dip;
		struct socket *con;
		struct sadmin *theAdmin = findActualAdmin(theUser->nick);
		tokens[7][strlen(tokens[7])-1] = 0;

		snprintf(response, 63, "DCC Chat from %s (%s)", theUser->nick, theUser->host);
		sRelay(response);

		if(!config.dcc_allowguests && !theAdmin) {
			snprintf(response, 63, "Chat from %s denied. (no guests)", theUser->nick);
			sRelay(response);
			return;
		}

		if(!theAdmin && get_next_dcc_id() > GUEST_SLOTS) {
			snprintf(response, 63, "Chat from %s denied. (no more guests)", theUser->nick);
			sRelay(response);
			return;
		}

		if(get_next_dcc_id() >= MAX_DCC) {
			snprintf(response, 63, "Chat from %s denied. (no slots left)", theUser->nick);
			sRelay(response);
			return;
		}

		if(sscanf(tokens[6], "%lu", &dip) == 1) {
			con = add_socket('d', get_next_dcc_id() + DCC_ID_OFSET, NULL, atoi(tokens[7]), "", dip);
			if(!con) {
				fprintf(out, "Error, couldnt connect.\n");
				snprintf(response, 63, "Error: Couldn't create chat with %s.", theUser->nick);
				sRelay(response);
			} else {
				add_dcc_chat(theUser->nick, theUser->host, theAdmin, con);
			}
		}
		return;
	}


	if(type[0] != 0)
	{
		sprintf(ebuf, "NOTICE %s :\001%s %s\001", theUser->nick, type, response);
		quickMsg(ebuf);
		snprintf(ebuf, 510, "[\002CTCP\002] CTCP '%s' from %s (%s@%s)", 
			type,
			theUser->nick,
			theUser->ident,
			theUser->host );
		sRelay(ebuf);
	} else
	{
		snprintf(ebuf, 510, "[\002CTCP\002] Unknown CTCP string: '%s' from %s (%s@%s)", 
			msg,
			theUser->nick,
			theUser->ident,
			theUser->host );
		sRelay(ebuf);
	}

	return;
}

void on_join(sUser *theUser, char *channel)
{
	struct schan *theChan = findiChan(channel);

	if(!strcmp(theUser->nick, cNick))
	{
		struct chanRec *cr = fetchChanRec(channel);
		if(!cr || !strchr(cr->flags, 'j'))
		{
			sprintf(ebuf, "PART %s :huh?", channel);
			quickMsg(ebuf);
			return;
		}
		cl_add(channel);
		sprintf(ebuf, "WHO %s %%cuhnfr\nMODE %s b", channel, channel);
		highMsg(ebuf);
		sprintf(ebuf, "MODE %s", channel);
		lowMsg(ebuf);
	} else {
		struct suser *u;
		if(!theChan)
			return;
		u = u_add(theChan, theUser->nick, theUser->ident, theUser->host);
		chkfor_ban(theChan, u);
		if(theChan->record)
		{ 
			if(!isOp(theChan, cNick))
				return;
			if(strchr(theChan->record->flags, 'l'))
				checkLimit(theChan);
			if(strchr(theChan->record->flags, 'A'))
			{
				struct sadmin *theAdmin = findActualAdmin(theUser->nick);
				if(theAdmin)
				{
					if(theAdmin->level >= 100)
						sprintf(ebuf, "MODE %s +o %s", theChan->name, theUser->nick);
					else
						sprintf(ebuf, "MODE %s +v %s", theChan->name, theUser->nick);
					lowMsg(ebuf);
				}
			}
			if(strchr(theChan->record->flags, 'O')) {
				sprintf(ebuf, "MODE %s +o %s", theChan->name, theUser->nick);
				lowMsg(ebuf);
			} else if(strchr(theChan->record->flags, 'V')) {
				sprintf(ebuf, "MODE %s +v %s", theChan->name, theUser->nick);
				lowMsg(ebuf);
			}
			if(strchr(theChan->record->flags, 'h'))
				checkLamehost(theChan, theUser);
			if(strchr(theChan->record->flags, 'm'))
				checkBadHost(theChan, theUser);
			if(strchr(theChan->record->flags, 'y')) {
				struct criminalRecord *p = findCriminal(theChan, theUser->nick);
				if(!p) {
					p = nu_add(theChan, theUser->nick, theUser->ident, theUser->host);
					if(!p) return;
				}
				if(time(0) - p->lastJoin > config.flood_join_period) {
					p->lastJoin = time(0);
					p->njoins = 1;
				} else if(++p->njoins >= config.flood_join_limit) {
					punish_user(theChan->record, u, 'y', 0, 0);
				}
			}
			if(strchr(theChan->record->flags, 'k') && count_clones(theChan, theUser->mask) >= config.clonelimit)
				punish_user(theChan->record, u, 'k', 0, 0);
		} /* chan has record? */
	} /* is it me joining? */

	return;
}

void on_kick(sUser *theUser, char *chan, char *victim, char *kickmsg)
{
	struct schan *theChan = findChan(chan);
	if(!theChan || !theChan->record)
		return;

	if(!strcmp(victim, cNick))
	{
		sprintf(ebuf, "JOIN %s %s", theChan->name, theChan->record->key);
		lowMsg(ebuf);
		sprintf(ebuf, "I have been kicked from %s by %s (%s)", chan, theUser->nick, kickmsg);
		sRelay(ebuf);
		cl_del(chan);
		/* check i still have a chan in common w/ logged in admins. */
		checkAllAdmins();
	} else {
		struct suser *pUser = findUser(theChan, victim);
		if(!pUser)
			return;
		u_del(theChan, pUser);
		checkCommonAdmin(victim);
	}
	return;
}

void on_part(sUser *theUser, char *channel)
{
	struct schan *theChan = findChan(channel);
	if(!theChan)
		return;

	if(!strcmp(theUser->nick, cNick))
	{
		cl_del(channel);
		checkAllAdmins();
	} else {
		struct suser *pUser = findUser(theChan, theUser->nick);
		if(!pUser)
			return;
		u_del(theChan, pUser);
		checkCommonAdmin(theUser->nick);
	}
	return;
}

void on_quit(sUser *theUser, char *msg)
{
	struct schan *c;
	struct suser *u;
	struct sadmin *a;

	c = ial.channel;
	while(c != NULL)
	{
		u = findUser(c, theUser->nick);
		if(u)
		{
			u_del(c, u);
		}
		c = c->next;
	}

	a = findActualAdmin(theUser->nick);
	if(a)
		a->actualNick[0] = 0;

	if(strmi(theUser->nick, config.nick))
	{
		sprintf(ebuf, "NICK %s", config.nick);
		highMsg(ebuf);
	}

	return;
}


void on_nick(sUser *theUser, char *newnick)
{
	struct schan *c;
	struct suser *u;
	struct sadmin *a;
	struct criminalRecord *p;
	sUser newUser = *theUser;
	mystrncpy(newUser.nick, newnick, NICKSIZE);
	sprintf(newUser.mask, "%s!%s@%s", newnick, theUser->ident, theUser->host);

	/* if it's me, update what i think my nick is */
	if(!strcmp(theUser->nick, cNick)) {
		strcpy(cNick, newnick);
		sprintf(ebuf, "SET %s NICK %s", config.linkname, cNick);
		send_to_all_links(ebuf);
	}
	
	/* update adminr records to reflect change.. */
	a = findActualAdmin(theUser->nick);
	if(a)
		mystrncpy(a->actualNick, newnick, NICKSIZE);

	/* cycle channels */
	c = ial.channel;
	while(c != NULL)
	{
		/* change nick of criminalrecords */
		p = findCriminal(c, theUser->nick);
		if(p) {
			mystrncpy(p->nick, newnick, NICKSIZE);
		}
		/* change ial nicks */
		u = findUser(c, theUser->nick);
		if(u) {
			mystrncpy(u->nick, newnick, NICKSIZE);
			/* check if the user now matches a badmask or an existing ban now they've changed mask */
			if(c->record) {
				chkfor_ban(c, u);
				if(strchr(c->record->flags, 'm'))
					checkBadHost(c, &newUser);
				if(strchr(c->record->flags, 'z')) {
					int pe, li;
					pe = (strchr((c)->modes, 'm') && config.flood_nick_mperiod > 0) ? config.flood_nick_mperiod : config.flood_nick_period;
					li = (strchr((c)->modes, 'm') && config.flood_nick_mlimit > 0) ? config.flood_nick_mlimit : config.flood_nick_limit;
					printf("debug: %d %d\n", pe, li);
					/* deal with nickflood */
					if(time(0) - u->lastNick > pe) {
						u->lastNick = time(0);
						u->nickchanges = 1;
					} else if(++u->nickchanges >= li) {
						punish_user(c->record, u, 'z', 0, 0);
					}
				}
			}
			/* phew */
		}
		c = c->next;
	}

	return;
}

void on_topic(sUser *theUser, char *channel, char *topic)
{
	struct schan *theChan = findChan(channel);
	if(!theChan)
		return;

	if(topic)
		mystrncpy(theChan->topic, topic, TOPICLEN);
	else theChan->topic[0] = 0;

	return;
}

void on_mode(sUser *theUser, char *channel, char sign, char mode, char *argument)
{
	struct schan *theChan;

	if(!(theChan = findChan(channel)))
		return;

	if(!theChan->record)
		return;

#ifdef DEBUG_MODE
	fprintf(out, "%s: MODE %s %c%c %s\n", theUser->nick, channel, sign, mode, argument);
#endif
	
	switch(mode)
	{
		case 'v':
		{
			struct suser *eUser = findUser(theChan, argument);
			if(!eUser)
				break;
			eUser->isVoice = (sign == '+') ? 1 : 0;
			break;
		}
		case 'o':
		{
			struct suser *eUser = findUser(theChan, argument);
			if(!eUser)
				break;
			eUser->isOp = (sign == '+') ? 1 : 0;
			if(strchr(theChan->modes, 'k') && !strcmp(eUser->nick, cNick)) {
				/* now that we are op we can see the chan key! request it: */
				sprintf(ebuf, "MODE %s", theChan->name);
				lowMsg(ebuf);
			}
			/* bitchmode */
			if(sign == '+') {
				if(!strcmp(eUser->nick, cNick)) {
					char args[512];
					if(strchr(theChan->record->flags, 'B')) {
						int mc = 0;
						args[0] = '\0';
						/* cycle current ops and deop if not an admin */
						eUser = theChan->user;
						while(eUser) {
							if(eUser->isOp && strcmp(eUser->nick, cNick) && !findActualAdmin(eUser->nick)) {
								strcat(args, " ");
								strcat(args, eUser->nick);
								if(++mc >= config.maxmodes) {
									massDeop(theChan, mc, args);
									args[0] = '\0';
									mc = 0;
								}
							}
							eUser = eUser->next;
						}
						if(mc > 0) {
							massDeop(theChan, mc, args);
						}
					}
					/* now tell botnet i have op */
					sprintf(args, "GOTOP %s %s", config.linkname, theChan->name);
					send_to_all_links(args);
				} else if(!findActualAdmin(argument) && strchr(theChan->record->flags, 'B')) {
					sprintf(ebuf, "MODE %s -o %s", theChan->name, argument);
					quickMsg(ebuf);
				}
			}
			break;
		}
		case 'k':
		{
			mystrncpy(theChan->key, (sign == '+') ? argument : "", KEYLEN+1);
			if(sign == '+')
				addMode(theChan, 'k');
			else
				removeMode(theChan, 'k');
			break;
		}
		case 'b':
		{
			if(sign == '+')
			{
				bl_add(theChan, argument, theUser->nick, time(NULL));
				if(strchr(theChan->record->flags, 'e'))
				{
					if(strcmp(theUser->nick, cNick))
						enforce_ban(theChan, argument, "Banned", theUser->nick, config.slowkicks ? -1 : 0);
				}
			}
			else
			{
				struct sban *temp = findBan(theChan, argument);
				if(temp)
					bl_del(theChan, temp);
				if(theChan->record && isSoftBan(theChan->record, argument) && strcmp(theUser->nick, cNick)) {
					snprintf(ebuf, 510, "NOTICE %s :You unbanned \"%s\" from %s, this ban still remains in my internal list. If you wish to remove it properly, you need to /msg %s unban %s %s",
						theUser->nick, argument, theChan->name, cNick, theChan->name, argument);
					lowMsg(ebuf);
				}
			}
			break;
		}
		case 'l':
		{
			theChan->limit = (sign == '+') ? atoi(argument) : 0;
			// No Break.
		}
		default:
		{
			if(sign == '+')
				addMode(theChan, mode);
			else
				removeMode(theChan, mode);
			break;
		}
	}

	return;
}


void on_invite(sUser *theUser, char *channel)
{
	struct chanRec *theChan = fetchChanRec(channel);

	if(theChan && strchr(theChan->flags, 'j'))
	{
		sprintf(ebuf, "JOIN %s %s", channel, theChan->key);
		highMsg(ebuf);
	}

	return;
}

