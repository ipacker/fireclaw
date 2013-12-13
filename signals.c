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
// $Id: signals.c,v 1.8 2003/06/21 12:43:11 snazbaz Exp $
//

#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "main.h"

int signal_catch;

void _sigint()
{
	stop_running();
	fprintf(out, "Caught signal, please wait...\n");
	if(main_irc) {
		sendQUIT("caught sigint");
	}
	irc_disconnect();
}

void _sighup()
{
	attempt_reload_conf(DEFAULT_CONFIG);
	sRelay("[\002sighup\002] Got SIGHUP, rehashing config...");	
	signal_catch = 1;
}

void setup_signals()
{
    struct sigaction sv;

    sigemptyset( &sv.sa_mask );
    sv.sa_flags = 0;
	sv.sa_handler = _sigint;
    sigaction( SIGINT, &sv, NULL );

    sv.sa_handler = _sighup;
    sigaction( SIGHUP, &sv, NULL );

    sv.sa_handler = SIG_IGN;
    sigaction( SIGUSR1, &sv, NULL );
    sigaction( SIGPIPE, &sv, NULL );

	return;
}

void write_pid(char *file)
{
	FILE *fp;
	fp = fopen(file, "w");
	if(fp == NULL)
		return;
	fprintf(fp, "%d\n", (int)getpid() );
	fclose(fp);
	return;
}
