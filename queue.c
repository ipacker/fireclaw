// Fireclaw Copyright (C) 2003 Ian Packer <snazbaz at onetel.net.uk>
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
// $Id: queue.c,v 1.6 2003/06/17 00:32:48 snazbaz Exp $
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <malloc.h>

#include "main.h"

#define Q_LOWSIZE 256
#define Q_HIGHSIZE 128

struct nodeQ {
	char *message;
	int ts;
};

struct _sendQ {
	struct nodeQ high[Q_HIGHSIZE];
	struct nodeQ low[Q_LOWSIZE];
	int highSize;
	int lowSize;
};

static int bytets;
static struct _sendQ sendQ;

void setup_queue()
{
	sendQ.highSize = sendQ.lowSize = 0;
	bytets = bper = 0;
	return;
}

void reset_queue()
{
	int i;
	for(i = 0; i < sendQ.highSize; i++)
		free(sendQ.high[i].message);
	for(i = 0; i < sendQ.lowSize; i++)
		free(sendQ.low[i].message);
	setup_queue();
}

void highMsg(char *message)
{
	int size;
	int pos = sendQ.highSize++;

	if(pos >= Q_HIGHSIZE)
	{
		/* max sendQ exceeded. */
		sendQ.highSize--;
		fprintf(out, "Max sendQ exceeded (high).\n");
		return;
	}

	size = strlen(message) + 1;
	
	sendQ.high[pos].ts = (int)time(0);
	sendQ.high[pos].message = myalloc(size * sizeof(char));
	memcpy(sendQ.high[pos].message, message, size);

	return;
}

void lowMsg(char *message)
{
	int size;
	int pos = sendQ.lowSize++;

	if(pos >= Q_LOWSIZE)
	{
		/* max sendQ exceeded. */
		sendQ.lowSize--;
		fprintf(out, "Max sendQ exceeded (low).\n");
		return;
	}

	size = strlen(message) + 1;
	
	sendQ.low[pos].ts = (int)time(0);
	sendQ.low[pos].message = myalloc(size * sizeof(char));
	memcpy(sendQ.low[pos].message, message, size);

	return;
}

void popMsg()
{
	if(bytets < time(NULL) - 1)
	{
		bytets = time(NULL);
		if(bper >= 75) 
			bper -= 75;
	}
//	printf("Debug: %d bytes\n", bper);
	
	if(bper > CLIENT_FLOOD_AUX)
		return;

	if(sendQ.highSize == 0)
	{
		if((sendQ.lowSize > 0) && (lastMsg < time(0) - LOWMSG_DELAY))
		{
			bper += strlen(sendQ.low[0].message);
			irc_write(sendQ.low[0].message);
			free(sendQ.low[0].message);
			memcpy(sendQ.low, &sendQ.low[1], --sendQ.lowSize * sizeof(struct nodeQ) );
		}
	} else if( (lastMsg < time(0) - HIGHMSG_DELAY) || (bper < CLIENT_FLOOD_LOW) ) {
		bper += strlen(sendQ.high[0].message);
		irc_write(sendQ.high[0].message);
		free(sendQ.high[0].message);
		memcpy(sendQ.high, &sendQ.high[1], --sendQ.highSize * sizeof(struct nodeQ) );
	}
	return;
}

void quickMsg(char *message)
{
	if(bper > CLIENT_FLOOD)
		highMsg(message);
	else
		irc_write(message);
	return;
}
