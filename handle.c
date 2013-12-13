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
// $Id: handle.c,v 1.22 2003/07/16 15:35:02 snazbaz Exp $
//

#include "main.h"

static char lastChan[256];
static struct schan *lastPointer;
void handleNumeric(char *tokens[], int *numeric, char *end);


/* if someone deletes a channel while we are in the middle of recieving a WHO response, the result
	is that lastPointer is still set and we get a segfault. This will make sure this doesnt happen */
void checkLastPointer(struct schan *removal)
{
	if(lastPointer == removal)
		lastPointer = NULL;
	lastChan[0] = 0;
	return;
}

void parseLine(char *in)
{
	int numtok, numeric;
	char poutbuf[BUFSIZE];
	char line[513];
	char *message;
	char *tokens[MAXTOK];
	sUser theUser;

	mystrncpy(line, in, 513);

	memset(tokens, 0, sizeof(tokens) ); /* dont want old tokenized data causing problems! */
	
	/* split the string into tokens. */
	numtok = tokenize(in, " ", tokens, MAXTOK);

	if(numtok < 2)
		return;

	if(strcmp(tokens[0], "PING") == 0) {
		strcpy(poutbuf, "PONG ");
		strcat(poutbuf, tokens[1]);
		irc_write(poutbuf);
		return;
	}

	if(tokens[0][0] != ':')
		return;

	numeric = atoi(tokens[1]);
	if(strchr(tokens[0], '!') == NULL && numeric > 0) { // server msg
			handleNumeric(tokens, &numeric, strchr(&line[1], ':'));
	} else { // client msg
		
		if(numtok < 3) 
		{
			return;
		}
		
		make_user_struct(tokens[0], &theUser);

		if(strcmp(tokens[1], "JOIN") == 0)
		{
			on_join(	&theUser, 	((tokens[2][0] == '#') ? tokens[2] : (tokens[2]+1)) 	);
			return;
		}

		if(strcmp(tokens[1], "PART") == 0)
		{
			on_part(	&theUser, 	((tokens[2][0] == '#') ? tokens[2] : (tokens[2]+1)) 	);
			return;
		}

		if(strcmp(tokens[1], "QUIT") == 0)
		{
			message = strchr(&line[1], ':') + 1;
			on_quit(&theUser, message);
			return;
		}	

		if(strcmp(tokens[1], "NICK") == 0)
		{
			on_nick(&theUser, ((tokens[2][0] != ':') ? tokens[2] : (tokens[2]+1)) );
			return;
		}

		if(numtok < 4)
			return;

		if(strcmp(tokens[1], "PRIVMSG") == 0)
		{
			message = line + (tokens[3] - tokens[0]) + 1;
			on_privmsg(&theUser, tokens[2], message, tokens, &numtok);
			return;
		}
	
		if(strcmp(tokens[1], "MODE") == 0)
		{
			int argpos, pos;
			char sign = '+';
			argpos = 4;
			for(pos = 0; pos < strlen(tokens[3]); pos++)
			{
				if(tokens[3][pos] == '+' || tokens[3][pos] == '-')
					sign = tokens[3][pos];
				else if( (tokens[3][pos] == 'l') && (sign == '-') )
					on_mode(&theUser, tokens[2], sign, tokens[3][pos], NULL);
				else if(strchr(argmodes, tokens[3][pos]))
					on_mode(&theUser, tokens[2], sign, tokens[3][pos], tokens[argpos++]);
				else
					on_mode(&theUser, tokens[2], sign, tokens[3][pos], NULL);
			}
			return;
		}

		if(strcmp(tokens[1], "NOTICE") == 0)
		{
			if(tokens[2][0] == '#') {
				message = line + (tokens[3] - tokens[0]) + 1;
				chan_msg(&theUser, tokens[2], message, tokens, &numtok, 1);
			}
			return;
		}

		if(strcmp(tokens[1], "INVITE") == 0) // :snazbaz!~winky@213.78.73.251 INVITE FireclawDev :#stats-console
		{
			if(!strcmp(tokens[2], cNick)) // sanity check
				on_invite(&theUser, &tokens[3][1]);
			return;
		}

		if(strcmp(tokens[1], "KICK") == 0)
		{
			message = line + (tokens[4] - tokens[0]) + 1;
//			message = strchr(&line[1], ':') + 1;
			on_kick(&theUser, tokens[2], tokens[3], message);
			return;
		}


		if(strcmp(tokens[1], "TOPIC") == 0)
		{
			message = ((strlen(tokens[3]) == 1) ? NULL : strchr(&line[1], ':') + 1);
			on_topic(&theUser, tokens[2], message);
			return;
		}


	}

	return;
}
/*
<-- [:qnps.tx.us.netgamers.org 005 Fireclaw WHOX WALLCHOPS WALLVOICES USERIP CPRIVMSG CNOTICE SILENCE=15 MODES=6 MAXCHANNELS=30 MAXBANS=45 NICKLEN=15 TOPICLEN=250 AWAYLEN=160 KICKLEN=250 :are supported by this server]
<-- [:qnps.tx.us.netgamers.org 005 Fireclaw CHANTYPES=#& PREFIX=(ov)@+ CHANMODES=b,k,l,imnpstrcCS CASEMAPPING=rfc1459 NETWORK=NetGamers :are supported by this server]
*/
void handleNumeric(char *tokens[], int *numeric, char *end)
{
	char c;
	char outbuf[BUFSIZE];
	struct schan *theChan;
	struct suser *theUser;

	if(!main_irc)
		return;

	switch(*numeric)
	{
		case 004:
		{
			if(tokens[7]) {
				mystrncpy(argmodes, tokens[7], 32);
				sprintf(outbuf, "[from server] Modes with arguments set to: %s.", argmodes);
				sRelay(outbuf);
			}
			break;
		}
		case 005:
		{
			char *pch;
			int i = 3;
			mystrncpy(cNick, tokens[2], NICKSIZE);
			while(tokens[i])
			{
				pch = strchr(tokens[i], '=');
				if(pch) {
					*pch++ = '\0';
					if(!strcmp(tokens[i], "MODES") && atoi(pch) > 0) {
						override_config("MODES", &config.maxmodes, atoi(pch));
					} else if(!strcmp(tokens[i], "MAXBANS") && atoi(pch) > 0) {
						override_config("MAXBANS", &config.banlim, atoi(pch));
					} else if(!strcmp(tokens[i], "NICKLEN") && atoi(pch) > NICKSIZE-1) {
						fprintf(out, "Server NICKLEN is longer than our allocated nicklen (cfg.h).\n");
						fprintf(out, "Aborting.\n");
						stop_running();
						irc_disconnect();
					} else if(!strcmp(tokens[i], "NETWORK")) {
						mystrncpy(myNetwork, pch, 32);
						sprintf(outbuf, "[from server] Network is: %s", myNetwork);
						sRelay(outbuf);
						sprintf(outbuf, "SET %s NETWORK %s", config.linkname, myNetwork);
						send_to_all_links(outbuf);
					}
				}
				i++;
			}
			break;
		}
		case 317:
		{
			struct suser *ur;
			int isecs = atoi(tokens[4]);

			theChan = ial.channel;
			while(theChan)
			{
				if(theChan->record && strchr(theChan->record->flags, 'I')) {
					if((ur = findUser(theChan, tokens[3]))) {
						if(isecs > config.idle_limit) {
							if(ur->isOp || (ur->isVoice && config.idlevoice == 1)) {
								sprintf(outbuf, "MODE %s -o%s %s %s", theChan->name,
									(ur->isVoice && config.idlevoice == 1) ? "v" : "", ur->nick,
									(ur->isVoice && config.idlevoice == 1) ? ur->nick : "");
								highMsg(outbuf);
								sprintf(outbuf, "[\002%s\002] %s deoped/devoiced (%d seconds idle).", theChan->name, ur->nick, isecs);
								sRelay(outbuf);
							}
						} else {
							ur->idleMsg = time(0) - isecs;
						}
					}
				}
				theChan = theChan->next;
			}
			break;
		}
		case 324: //<-- [:qnps.tx.us.netgamers.org 324 Fireclaw #liev +tnlk 1337 *]
		{
			theChan = findChan(tokens[3]);
			if(!theChan)
				return;

			mystrncpy(theChan->modes, tokens[4], MODELEN);

			if(strchr(theChan->modes, 'l'))
			{
				theChan->limit = atoi(tokens[5]);
				if(strchr(theChan->modes, 'k'))
					mystrncpy(theChan->key, tokens[6], KEYLEN);
			} else if(strchr(theChan->modes, 'k'))
				mystrncpy(theChan->key, tokens[5], KEYLEN);

//			printf("Chan(%s) Modes: %s, key: %s, limit: %d\n", theChan->name, theChan->modes, theChan->key, theChan->limit);

			break;
		}
		case 330: //<-- [:qnps.tx.us.netgamers.org 330 Fireclaw snazbaz snazbaz :is logged in as]
		{
			if(!userExists(tokens[3]))
				return;
			if(automagic_login(tokens[4], tokens[3]) && (redoAdmTs - time(0) < 10))
				parseLine(redoAdm);
			break;
		}
		case 332: //<-- [:qnps.tx.us.netgamers.org 332 Fireclaw #liev :jumpingWorm: CD3 in the same place etc for you to grab. (see P note if you forgot)| http://members.fortunecity.com/christinaconnection/contigolive.rm \o/ || http://frontpage.fok.nl/news.fok?id=28865 | http://www.tweakers.net/nieuws.dsp?ID=27061 |  | ]
		{
			theChan = findChan(tokens[3]);
			if(!theChan)
				return;
			mystrncpy(theChan->topic, end, TOPICLEN);
			break;
		}
		case 340: // :London.UK.Eu.Netgamers.org 340 rahhh :rahhh=+~rah@213.78.171.157
		{
			char *pch = strchr(tokens[3], '@');
			if(!pch)
				return;
			mystrncpy(myip, pch+1, 16);
		}
		case 352: // hybrid :irc.efnet.nl 352 FurryClaw #fireclaw ~claw 213.78.172.194 irc.efnet.nl FurryClaw H :0 dev model
		{
			if(end) {
				end = strchr(end, ' ');
			}
		}
		case 354: //354 : snazbaz #liev cservice netgamers.org P H@+di :Realname ++
		{
			int ti = 0;
			/* this is an optimisation that reduces work on successive who entries (n - 1) of all who's
			 but increases it on the first entry. basically we are replacing <up-to-totalchan> strcmp's with a single strcmp. */
			if(!strcmp(tokens[3], lastChan))
				theChan = lastPointer;
			else
			{
				theChan = findChan(tokens[3]);
				strcpy(lastChan, tokens[3]);
				lastPointer = theChan;
			}

			if(!theChan)
				return;

			/* if we get 9+ args, probably a hybrid ircd w/ server field. shift them. */
			if(tokens[9] && tokens[9][0] == ':') {
				for(ti = 6; tokens[ti+1]; ti++)
					tokens[ti] = tokens[ti+1];
			}

			if(tokens[8] && tokens[8][0] == ':')
			{
				theUser = findUser(theChan, tokens[6]);
				if(!theUser)
				{
					theUser = u_add(theChan, tokens[6], tokens[4], tokens[5]);
					if(strchr(tokens[7], '+'))
						theUser->isVoice = 1;
					if(strchr(tokens[7], '@'))
						theUser->isOp = 1;
				}

				if(!theUser->realname)
				{
					if(end)
					{
						theUser->realname = (char *)myalloc(strlen(end));
						end++;
						memcpy(theUser->realname, end, strlen(end) + 1);
						chkfor_rn_ban(theChan, theUser);
					}
				}
			} else if(tokens[6] && tokens[6][0] == ':') {
				//<-- [:qnps.tx.us.netgamers.org 354 Fireclaw #chan Deity 0 :I 0wn j00]
				//<-- [:qnps.tx.us.netgamers.org 354 Fireclaw #chan Stats Stats :stats.Chaos-realm.net]
				theUser = findUser(theChan, tokens[4]);
				if(tokens[5][0] != '0')
					automagic_login(tokens[5], tokens[4]);
				if(theUser && !theUser->realname)
				{
					if(end)
					{
						theUser->realname = (char *)myalloc(strlen(end));
						end++;
						memcpy(theUser->realname, end, strlen(end) + 1);
						chkfor_rn_ban(theChan, theUser);
					}
				}
			} else if(tokens[5] && tokens[5][0] == ':') {
				theUser = findUser(theChan, tokens[4]);
				if(theUser && !theUser->realname)
				{
					if(end)
					{
						theUser->realname = (char *)myalloc(strlen(end));
						end++;
						memcpy(theUser->realname, end, strlen(end) + 1);
						chkfor_rn_ban(theChan, theUser);
					}
				}
			}

			break;
		}
		case 367:	// <-- [:qnps.tx.us.netgamers.org 367 Fireclaw #liev *!*@*.nott.cable.ntl.com Monnik 1053884175]
		{
			theChan = findChan(tokens[3]);
			if(!theChan)
				return;

			if(tokens[6])
				bl_add(theChan, tokens[4], tokens[5], atoi(tokens[6]));
			else if(tokens[4]) /* ircnet (idiots :P) */
				bl_add(theChan, tokens[4], "Unknown", 0);

			break;
		}
		case 376:
		case 422:			/* on connect */
		{
			lastPointer = NULL;
			lastChan[0] = 0;
			on_connect();
			break;
		}
		case 433:			/* nick taken */
		{
			sRelay("My nickname is taken!");
			if(main_irc->con_ts < time(0) - 120)
			{
				sprintf(outbuf, "PRIVMSG %s :%s", config.servicePrefix, config.serviceGhost);
				highMsg(outbuf);
			} else {
				char mbuf[5];
				c = (char)time(NULL);
				strcpy(cNick, config.nick);
				cNick[10] = '\0';
				sprintf(mbuf, "%d", c);
				strcat(cNick, mbuf);
				sprintf(outbuf, "NICK %s", cNick);
				irc_write(outbuf);
			}
			break;
		}
		case 471:
		case 473:			/* invite */
		case 475:			/* key */
		{
			/* ask for invite over the botnet */
			sprintf(outbuf, "INV %s %s %s", config.linkname, cNick, tokens[3]);
			send_to_all_links(outbuf);
			break;
		}
		case 474:
		{

			break;
		}

	}

	return;
}

void make_user_struct(char *mask, sUser *usr)
{
	char *p, *c;
#ifdef GNU_X_HOST
	char hostbuf[100];
#endif

	mystrncpy(usr->mask, mask, MASKSIZE);
	
	c = strchr(mask + 1, '!');
	p = strchr(mask + 1, '@');

	if(p) {
		*p = '\0';
		mystrncpy(usr->host, p + 1, HOSTSIZE);
	} else
		strcpy(usr->host, "");

	if(c) {
		*c = '\0';
		mystrncpy(usr->ident, c + 1, IDENTSIZE);
	} else
		strcpy(usr->ident, "");

	mystrncpy(usr->nick, mask + 1, NICKSIZE);

#ifdef GNU_X_HOST
	mystrncpy(hostbuf, usr->host, 100);
	p = strchr(hostbuf, '.');
	if(p != NULL && strcmp(p, GNU_X_HOST) == 0)
	{
		*p = '\0';
		mystrncpy(usr->gnuUser, hostbuf, 32);
	} else
		strcpy(usr->gnuUser, "~None~");
#else
	strcpy(usr->gnuUser, "~None~");
#endif

	return;
}

