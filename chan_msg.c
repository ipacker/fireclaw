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
// $Id: chan_msg.c,v 1.17 2003/06/17 00:32:48 snazbaz Exp $
//

#include <ctype.h>

#include "main.h"

/* various sub/helper functions */
static int make_msg_id(char *in);
static int count_coloured_chars(char *text);
static int caps_percent(char *text);
static int ctrlchar_percent(char *text);
static void perform_basic_strip(char *output, char *input);
static int check_repeat_char(char *text);
static void strip_for_sweartest(char *output, char *input);
void set_lower_message(char *output, char *input);
int match_badurl(char *text);

regex_t swear_norm, swear_mild;

void init_regex()
{
	/* compile frequently used regexs perm */
	regcomp(&swear_norm, SWEAR_MATCH_MAIN, REG_EXTENDED|REG_NOSUB);
	regcomp(&swear_mild, SWEAR_MATCH_MILD, REG_EXTENDED|REG_NOSUB);
	regcomp(&lamehost_5, LAMEHOST_5, REG_EXTENDED|REG_NOSUB|REG_ICASE);
	regcomp(&lamehost_10, LAMEHOST_10, REG_EXTENDED|REG_NOSUB|REG_ICASE);
	regcomp(&lamehost_15, LAMEHOST_15, REG_EXTENDED|REG_NOSUB|REG_ICASE);
	regcomp(&safehosts, EXCEPTIONS, REG_EXTENDED|REG_NOSUB);
	return;
}

void free_regex()
{
	regfree(&swear_norm);
	regfree(&swear_mild);
	regfree(&lamehost_5);
	regfree(&lamehost_10);
	regfree(&lamehost_15);
	regfree(&safehosts);
	return;
}

int regmatch(regex_t *re, char *string)
{
	return regexec(re, string, (size_t) 0, NULL, 0) ? 0 : 1;
}

void chan_msg(sUser *theUser, char *channel, char *message, char *tokens[], int *ctok, char isNotice)
{
	struct chanRec *theChan;
	struct criminalRecord *userRec;
	char stripped_message[512];
	struct suser *u;
	char isAction = 0;

	theChan = fetchChanRec(channel);
	userRec = NULL;

	if(!theChan || !theChan->physical || !tokens[0])
		return;

	if(!(u = findUser(theChan->physical, theUser->nick)))
		return;

	u->idleMsg = time(0);

	if(!strcmp(tokens[3], ":\001ACTION")) {
		isAction = 1;
		message += 8;
	} else if((message[0] == '\001') && (message[strlen(message)-1] == '\001') && strchr(theChan->flags, 'i')) {
		punish_user(theChan, findUser(theChan->physical, theUser->nick), 'i', 0, 0);
		return;
	}

	if(!(*message))
		return;

	perform_basic_strip(stripped_message, message);

	if(strchr(theChan->flags, 'b')) {
		char lower_message[512];
		set_lower_message(lower_message, stripped_message);
		if(match_badurl(lower_message)) {
			punish_user(theChan, findUser(theChan->physical, theUser->nick), 'b', 0, 0);
			return;
		}
	}

	/* FLOOD */
	if(strchr(theChan->flags, 'f')) {
		// Chans global flood (i.e floodbot detect)
		if(theChan->physical->flood_last < (time(0) - config.flood_global_period)) {
			theChan->physical->flood_last = time(0);
			theChan->physical->flood_lines = 1;
		} else {
			if(++theChan->physical->flood_lines >= config.flood_global_limit) {
				theChan->physical->flood_lines = theChan->physical->flood_last = 0;
				sprintf(stripped_message, "MODE %s +m", theChan->name);
				irc_write(stripped_message);
				sprintf(stripped_message, "NOTICE %s :Suspected flood attempt, locking channel.", theChan->name);
				highMsg(stripped_message);
				return;
			}
		}

		// User flood detection.
		if(!u) 
			return;
		if(u->lastMsg < (time(0) - config.flood_period)) { 
			u->lastMsg = time(0); 
			u->flines = 1;
		} else {
			if(++u->flines >= config.flood_limit) {
				sprintf(stripped_message, "%d lines in %d secs", u->flines, (int)(time(0) - u->lastMsg));
				punish_user(theChan, u, 'f', 0, stripped_message);
				u->flines = 0;
				u->lastMsg = 0;
				return;
			}
		}
	}

	/* REPEAT */
	if(strchr(theChan->flags, 'r')) {
		struct criminalRecord *c;
		int repeat_period, addid, i;
		int msgid = make_msg_id(stripped_message);
		c = findCriminal(theChan->physical, theUser->nick);
		if(!c) {
			c = nu_add(theChan->physical, theUser->nick, theUser->ident, theUser->host);
			if(!c) return;
		}
		repeat_period = (strlen(stripped_message) < 5) ? 6 : REPEAT_HISTORY_SIZE;
		addid = -1;
		for(i = 0; i < REPEAT_HISTORY_SIZE; i++)
		{
			if(time(0) - c->trackrepeat[i].ts > repeat_period) {
				c->trackrepeat[i].id = 0;
				addid = i;
			}
			else if(c->trackrepeat[i].id == 0)
				addid = i;
			else if(c->trackrepeat[i].id == msgid) {
				c->trackrepeat[i].ts = time(0);
				if(++c->trackrepeat[i].count > config.repeat_limit) {
					punish_user(theChan, findUser(theChan->physical, theUser->nick), 'r', 1, 0);
					return;
				} else if(c->trackrepeat[i].count == config.repeat_limit) {
					punish_user(theChan, findUser(theChan->physical, theUser->nick), 'r', 0, 0);
					return;
				}
				addid = -1;
			}
		}
		// No match, we must add this new msg type to the list.

		if(addid >= 0) {	/* there's a free slot! yay! (should always be one unless flood protect is off and we get LOADS of msgs) */
			c->trackrepeat[addid].id = msgid;
			c->trackrepeat[addid].ts = time(0);
			c->trackrepeat[addid].count = 1;
		}
	}

	/* NOTICE */
	if(strchr(theChan->flags, 'n') && isNotice) {
		punish_user(theChan, findUser(theChan->physical, theUser->nick), 'n', 0, 0);
		return;
	}

	/* SWEAR */
	if(strchr(theChan->flags, 's'))	{
		char swear_match_message[512];
		strip_for_sweartest(swear_match_message, stripped_message);
		if(regmatch(&swear_norm, swear_match_message)) {
			punish_user(theChan, findUser(theChan->physical, theUser->nick), 's', 1, 0);
			return;
		} else if(regmatch(&swear_mild, swear_match_message)) {
			punish_user(theChan, findUser(theChan->physical, theUser->nick), 's', 0, 0);
			return;
		}
	}

	/* COLOUR */
	if(strchr(theChan->flags, 'c') && (count_coloured_chars(message))) {
		punish_user(theChan, findUser(theChan->physical, theUser->nick), 'c', 0, 0);
		return;
	}

	/* CTRLCHARS */
	if(strchr(theChan->flags, 't') && (strlen(message) > 7)) {
		int percent = ctrlchar_percent(message);
		if(percent > config.ctrlchar_max) {
			char numbuf[8];
			sprintf(numbuf, "%d%%", percent);
			punish_user(theChan, findUser(theChan->physical, theUser->nick), 't', 0, numbuf);
			return;
		}
	}

	/* CAPS */
	if(strchr(theChan->flags, 'p') && (strlen(stripped_message) > 7)) {
		int percent = caps_percent(stripped_message);
		if(percent > config.caps_max) {
			char numbuf[8];
			sprintf(numbuf, "%d%%", percent);
			punish_user(theChan, findUser(theChan->physical, theUser->nick), 'p', 0, numbuf);
			return;
		}
	}

	/* REPEATCHAR */
	if(strchr(theChan->flags, 'x') && check_repeat_char(stripped_message)) {
		punish_user(theChan, findUser(theChan->physical, theUser->nick), 'x', 0, 0);
		return;	
	}

	return;
}

#define STRIP_CHARS "\001\002\017\037\026"
static void perform_basic_strip(char *output, char *input)
{
	char sinceK = 0, isCom = 0;
	for(; *input; input++)
	{
		if(strchr(STRIP_CHARS, *input))
			continue;
		else if(*input == '\003')
			{ sinceK = 1; isCom = 0; }
		else if(*input >= '0' && *input <= '9')
		{
			if(sinceK == 0) 
				*output++ = *input;
			else if(++sinceK > 2 + isCom) 
				sinceK = isCom = 0;
		} else if(*input == ',' && sinceK != 0)
			isCom = 1;
		else
			*output++ = *input;
	}
	*output = '\0';
	return;
}
#undef STRIP_CHARS

static int count_coloured_chars(char *text)
{
	int ret;
	char c = 0;

	for(ret = 0; *text; text++)
	{
		switch(*text)
		{
			case '\003': { c = (c) ? 0 : 1; break; }
			case '\017': { c = 0; break; }
			default: { if(c) ret++; break; }
		}
	}
	return ret;
}

static int caps_percent(char *text)
{
	float upper, lower;
	upper = lower = 0.01;
	for(; *text; text++) {
		if(*text >= 'A' && *text <= 'Z')
			upper++;
		else if(*text >= 'a' && *text <= 'z')
			lower++;
	}
	return (int)(100*(upper/(upper+lower)));
}

static int ctrlchar_percent(char *text)
{
	char cb, cu, cr;
	float yes, no;
	yes = no = 0.001;
	cb = cu = cr = 0;
	for(; *text; text++) {
		switch(*text) {
			case '\002': { cb = (cb) ? 0 : 1; break; }
			case '\037': { cu = (cu) ? 0 : 1; break; }
			case '\026': { cr = (cu) ? 0 : 1; break; }
			case '\017': { cr = cu = cb = 0; break; }
			default: {
				if(cb || cu || cr)
					yes++;
				else
					no++;
				break;
			}
		}
	}
	return (int)(100*(yes/(yes+no)));
}

static int check_repeat_char(char *text)
{
	int count = 0, i = 0;

	for(; i < strlen(text) - 1; i++)
	{		
		if(text[i] == text[i+1]) {
			if(++count >= config.repeatchar_lim)
				return 1;
		}
		else
			count = 0;
	}
	return 0;
}

static void strip_for_sweartest(char *output, char *input)
{
	char *anchor = output;
	str_remove(input, "h..");
	for(; *input; input++)
	{
		if(*input >= 'a' && *input <= 'z')
			*output++ = *input;
		else if(*input >= 'A' && *input <= 'Z')
			*output++ = tolower(*input);
		else if(*input == '*' || *input == ' ')
			*output++ = *input;
		else if(*input == 'µ') *output++ = 'u'; // µ
		else if(*input == '@') *output++ = 'a'; // @
		else if(*input == '!') *output++ = 'i'; // !
		else if(*input == '$') *output++ = 's'; // $
		else if(*input == '0') *output++ = 'o'; // 0
		else if(*input == '1') *output++ = 'i'; // 1
		else if(*input == '3') *output++ = 'e'; // 3
		else if(*input == '4') *output++ = 'a'; // 4
		else if(*input == '5') *output++ = 's'; // 5
		else if((*input == 'Ç') || (*input == 'ç')) *output++ = 'c'; // Ç, ç
		else if(*input == 'ü') *output++ = 'u'; // ü
		else if(*input == 'é') *output++ = 'e'; // é
		else if((*input >= 'â') && (*input <= 'å')) *output++ = 'a'; // ( 131 ) â ( 132 ) ä ( 133 ) à ( 134 ) å
		else if((*input >= 'ê') && (*input <= 'è')) *output++ = 'e'; // ( 136 ) ê ( 137 ) ë ( 138 ) è 
		else if((*input >= 'ï') && (*input <= 'ì')) *output++ = 'i'; // ( 139 ) ï ( 140 ) î ( 141 ) ì 
		else if((*input >= 'ô') && (*input <= 'ò')) *output++ = 'o'; //( 147 ) ô ( 148 ) ö ( 149 ) ò 
		else if((*input == 'û') || (*input == 'ù')) *output++ = 'u'; // ( 150 ) û ( 151 ) ù
		else if((*input == '£')) *output++ = 'e'; // £
		else if(*input == 'ƒ') *output++ = 'f'; // ƒ
		else if(*input == 'á') *output++ = 'a'; // á
		else if(*input == 'í') *output++ = 'i'; // í
		else if(*input == 'ó') *output++ = 'o'; // ó
		else if(*input == 'ú') *output++ = 'u'; // ú

	}
	*output = '\0';
	output = anchor;
	str_remove(output, "cocktail");
	str_remove(output, "cocke");
	str_remove(output, "cockp");
	str_remove(output, "cocky");
	str_remove(output, "cockney");
	str_remove(output, "swanky");
	str_remove(output, "twater");
	str_remove(output, "scuntho");
	str_remove(output, "twatch");
	return;
}

int regMatch(char *string, char *pattern)
{
    int    status;
    regex_t    re;

    if (regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB|REG_ICASE) != 0) {
        return 0;      /* report error */
    }
    status = regexec(&re, string, (size_t) 0, NULL, 0);
    regfree(&re);
    if (status != 0) {
        return 0;      /* report error */
    }
    return 1;
}

void set_lower_message(char *output, char *input)
{
	for(; *input; input++)
		*output++ = tolower(*input);
	*output = '\0';
	return;
}

static int make_msg_id(char *in)
{
	int i;
	int ID = 0;
	for(i = 0; *in; in++, i++) {
		ID += (int)( (i+255) * (unsigned char)*in );
	}
	return ID;
}
