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
// $Id: help.c,v 1.15 2003/06/25 23:37:41 snazbaz Exp $
//

#include <ctype.h>
#include "main.h"

static char prefix[128];

/*
 * decides if reply should be via irc or via dcc and sends it 
 */
void send_hreply(char *reply, struct socket *dcc)
{
	if(dcc == NULL) {
		highMsg(reply);
	} else {
		sockwrite(dcc, reply);
	}
}

/*
 * print out a hard coded list of flags.
 */
void showFlags(sUser *theUser, struct socket *dcc)
{
	char bh[513];
	if(!dcc)
		snprintf(prefix, 127, "NOTICE %s :", theUser->nick);
	else
		prefix[0] = 0;

	sprintf(bh, "%s-- Channel flags --", prefix); 
	send_hreply(bh, dcc);
	sprintf(bh, "%sj = join, e = enforcebans, a = scan channel for automatic logins, o = kick ops, k = kickclones, I = idledeop", prefix); 
	send_hreply(bh, dcc);
	sprintf(bh, "%sA = op admins, B = bitchmode, l = dynamic limiting, b = no bad urls, c = no colours, m = no offensive masks", prefix); 
	send_hreply(bh, dcc);
	sprintf(bh, "%sf = flood protection, i = no chanctcp's, h = no lamehosts, n = no chan notices, p = no CAPS, y = joinflood", prefix); 
	send_hreply(bh, dcc);
	sprintf(bh, "%sr = no repeating, s = no swearing, t = no excessive ctrl chars, x = no repeatchars, z = nickflood, d = onjoin", prefix); 
	send_hreply(bh, dcc);
}

/*
 * print out hard coded main help response.
 */
void mainHelp(sUser *theUser, char *tok2, struct socket *dcc)
{
	char bh[513];
	int i;

	if(tok2) {
		for(i = 0; i < strlen(tok2); i++)
			tok2[i] = tolower(tok2[i]);
		syntaxHelp(theUser, tok2, dcc);
		return;
	}


	if(!dcc)
		snprintf(prefix, 127, "NOTICE %s :", theUser->nick);
	else
		prefix[0] = 0;

	sprintf(bh, "%sFor a list of commands type 'commands'.", prefix); send_hreply(bh, dcc);
	sprintf(bh, "%sFor a list of the different channel flags, type 'showflags'.", prefix); send_hreply(bh, dcc);
	sprintf(bh, "%sSee the docs for more detail.", prefix); send_hreply(bh, dcc);
	return;
}

/*
 * print out a hard coded list of cmds.
 */
void cmdList(sUser *theUser, struct socket *dcc)
{
	char bh[513];
	if(!dcc)
		snprintf(prefix, 127, "NOTICE %s :", theUser->nick);
	else
		prefix[0] = 0;

	sprintf(bh, "%s500: deladmin newadmin raw rehash shutdown", prefix); send_hreply(bh, dcc);
	sprintf(bh, "%s400: delchan hop me newchan say setflags", prefix); send_hreply(bh, dcc);
	sprintf(bh, "%s100: kick ban connect realnameban unban mode setkey setgoodmodes setbadmodes invite op deop", prefix); send_hreply(bh, dcc);
	sprintf(bh, "%s*: boot chaninfo commands criminalstats devoice namescan newpass linktree listbans listchans listlinks sockets" \
					"status transfer who whoami voice", prefix); send_hreply(bh, dcc);
	return;
}

/*
 * called a) when not enough args are supplied to a command.
 *        b) when you do help <command>
 */
void syntaxHelp(sUser *theUser, char *cmd, struct socket *dcc)
{
	char bh[513];

	if(!dcc)
		snprintf(prefix, 127, "NOTICE %s :", theUser->nick);
	else
		prefix[0] = 0;

	if(!strcmp(cmd, "newpass"))
		sprintf(bh, "%sSyntax: newpass <password>", prefix);
	else
	if(!strcmp(cmd, "deladmin"))
		sprintf(bh, "%sSyntax: deladmin <username>", prefix);
	else
	if(!strcmp(cmd, "newadmin"))
		sprintf(bh, "%sSyntax: newadmin <username> <password> <level(1-500)>", prefix);
	else
	if(!strcmp(cmd, "say"))
		sprintf(bh, "%sSyntax: say <#channel> <message>", prefix);
	else
	if(!strcmp(cmd, "me"))
		sprintf(bh, "%sSyntax: me <#channel> <message>", prefix);
	else
	if(!strcmp(cmd, "action"))
		sprintf(bh, "%sSyntax: action <#channel> <message>", prefix);
	else
	if(!strcmp(cmd, "mode"))
		sprintf(bh, "%sSyntax: mode <#channel> <modes> [arguments...]", prefix);
	else
	if(!strcmp(cmd, "unban"))
		sprintf(bh, "%sSyntax: unban <#channel> <mask>", prefix);
	else
	if(!strcmp(cmd, "ban"))
		sprintf(bh, "%sSyntax: ban <#channel> <mask|nickname> <time>[w|d|h|m] [reason]", prefix);
	else
	if(!strcmp(cmd, "realnameban"))
		sprintf(bh, "%sSyntax: realnameban <#channel> <mask> <time>[w|d|h|m] [reason]", prefix);
	else
	if(!strcmp(cmd, "setkey"))
		sprintf(bh, "%sSyntax: setkey <#channel> <key>", prefix);
	else
	if(!strcmp(cmd, "setbadmodes"))
		sprintf(bh, "%sSyntax: setbadmodes <#channel> -[modes]", prefix);
	else
	if(!strcmp(cmd, "setgoodmodes"))
		sprintf(bh, "%sSyntax: setgoodmodes <#channel> +[modes]", prefix);
	else
	if(!strcmp(cmd, "newchan"))
		sprintf(bh, "%sSyntax: newchan <#channel>", prefix);
	else
	if(!strcmp(cmd, "delchan"))
		sprintf(bh, "%sSyntax: delchan <#channel>", prefix);
	else
	if(!strcmp(cmd, "listbans"))
		sprintf(bh, "%sSyntax: listbans <#channel> <mask>", prefix);
	else
	if(!strcmp(cmd, "setflags")) {
		sprintf(bh, "%sSyntax: setflags <#channel> <+/-flags>", prefix); send_hreply(bh, dcc);
		sprintf(bh, "%sExample: setflags #foo +fo-rlmn", prefix);
	}
	else
	if(!strcmp(cmd, "kick"))
		sprintf(bh, "%sSyntax: kick <#channel> <nickname> [reason]", prefix);
	else
	if(!strcmp(cmd, "namescan"))
		sprintf(bh, "%sSyntax: namescan <#channel> <mask>", prefix);
	else
	if(!strcmp(cmd, "invite"))
		sprintf(bh, "%sSyntax: invite <#channel>", prefix);
	else
	if(!strcmp(cmd, "chaninfo"))
		sprintf(bh, "%sSyntax: chaninfo <#channel>", prefix);
	else
	if(!strcmp(cmd, "boot")) {
		sprintf(bh, "%sSyntax: boot <id>", prefix); send_hreply(bh, dcc);
		sprintf(bh, "%sUse 'who' to find the 'id' of the user you want to boot.", prefix);
	}
	else
	if(!strcmp(cmd, "connect")) {
		sprintf(bh, "%sSyntax: connect <mask>", prefix); 
	}
	else
	if(!strcmp(cmd, "op") || !strcmp(cmd, "deop") || !strcmp(cmd, "voice") || !strcmp(cmd, "devoice")) {
		sprintf(bh, "%sSyntax: %s <#channel> [users]", prefix, cmd);
	} else {
		bh[0] = 0;
	}

	if(bh[0] != 0)
		send_hreply(bh, dcc);	

	return;
}
