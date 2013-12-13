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
// $Id: main.c,v 1.29 2003/07/16 15:35:02 snazbaz Exp $
//

#include <unistd.h>
#include <malloc.h>

#define NOEXTERN
#include "main.h"

static char sendbuf[1024];
extern int signal_catch;

#ifdef REPORT_UPTIME
int init_uptime(void);
#endif

void init_badurls();
void reset_badurls();

static void forkme()
{
	int pid;
	FILE *fp;

	printf("Forking...\n");

	pid = fork();

	if(pid < 0)
	{
		printf("Unable to fork.\n");
		exit(0);
	}

	if(pid == 0)
	{
#ifndef SETVBUF_REVERSED
		setvbuf( stdout, NULL, _IONBF, 0 );
#else
		setvbuf( stdout, _IONBF, NULL, 0 );
#endif
	} else {

		fp = fopen(DEFAULT_PID, "w");
		if(fp == NULL)
			return;
		fprintf(fp, "%d\n", pid );
		fclose(fp);
		exit(0);
	}
	return;
}

int main(int argc, char **argv)
{
	int dofork;
	FILE *targetout;
#ifndef SOLARIS
	struct mallinfo mem;
#endif

	out = stdout;

	if(argc > 1 && strlen(argv[1]) == 2 && argv[1][1] == 'f')
	{
		targetout = 0;
		dofork = 0;
	} else {
		targetout = fopen("/dev/null", "w");
		dofork = 1;
		if(!targetout)
		{
			printf("Unable to open /dev/null\n");
			exit(1);
		}
	}

	timeSkewWait = 0;
	count_main_in = 0;
	count_main_out = 0;
	count_dcc_in = 0;
	count_dcc_out = 0;
	count_oj_in = 0;
	count_oj_out = 0;
	lastConnect = 0;
	signal_catch = 1;
	gotworkingcfg = 0;
	doHop = 0;
	tStart = (int)time(NULL);
	lastMsg = 0;
	ial.channel = NULL;
	admins.prev = NULL;
	admins.next = NULL;
	admins.level = -1;
	admins.nick[0] = 0;
	chans.prev = NULL;
	chans.next = NULL;
	chans.name = NULL;
	chans.physical = NULL;
	chans.flags[0] = 0;
	redoAdmTs = 0;
	redoAdm[0] = 0;
	myip[0] = 0;
	strcpy(myNetwork, "unknown");
	strcpy(argmodes, MODES_ARGS);

	printf("Fireclaw %s\n", VERSION_STR);

	setup_signals();
	setup_socks();
	setup_queue();
//	zero_dcclist();
	init_servers();
	init_badurls();
	firelink_init();
	load_config(DEFAULT_CONFIG);
	check_config();
	strcpy(cNick, config.nick);
	load_admins(DEFAULT_ADMIN);
	printf("Admins: loaded %d record(s)\n", count_admins());
	load_chans(DEFAULT_CHANS);
	printf("Channels: loaded %d record(s)\n", count_chans());
	expireBans();
	init_regex();
	init_timers();
#ifdef REPORT_UPTIME
	init_uptime();
#endif

	write_pid(DEFAULT_PID);

	if(config.dcc_listenport > 0) {
		if(add_listen('D', 3, config.dcc_listenport, config.dcc_listenip))
			fprintf(out, "Listening for dcc/telnet connections on %s:%d\n", config.dcc_listenip, config.dcc_listenport);
	}
	if(config.linkport > 0) {
		if(add_listen('F', 4, config.linkport, config.vhost))
			fprintf(out, "Listening for firelink connections on %s:%d\n", config.vhost, config.linkport);
	}

	if(dofork)
		forkme();

	if(targetout)
		out = targetout;

	initiate_main_connect();
	start_links();

	while(signal_catch) {
		signal_catch = 0;
		if(io_loop() == -1)
			break;
	}

	dcc_closeall("Program terminated.");
	disconnect_all_links("ERROR Program terminated.");
	fprintf(out, "Cleaning up...\n");
	clear_ial();
	fprintf(out, "Chans...");
	clear_chans();
	fprintf(out, "Admins...");
	clear_admins();
	fprintf(out, "Queue...\n");
	reset_queue();
	fprintf(out, "Servers/badurls/words/vhosts...\n");
	kill_servers();
	reset_badurls();
	wipe_ojdata();
	fprintf(out, "regex...\n");
	free_regex();
	fprintf(out, "sockets...\n");
	clean_all_socks();

#ifndef SOLARIS
	mem = mallinfo();

	fprintf(out, "Remaining heap: %d bytes. Used: %.2fs of cputime.\n", (int)mem.uordblks, my_cputime());
#endif
	if(out && out != stdout)
		fclose(out);

	return 0;
}

void service_auth()
{
	snprintf(sendbuf, 510, "PRIVMSG %s :%s", config.servicePrefix, config.serviceAuth);
	highMsg(sendbuf);
	return;
}

void sRelay_butone(char *message, struct socket *tso)
{
	if(main_irc && config.relaychan[0] == '#')
	{
		snprintf(sendbuf, 510, "PRIVMSG %s :%s", config.relaychan, message);
		lowMsg(sendbuf);
	}
	snprintf(sendbuf, 510, "# %s", message);
	send_to_dcc_butonesock(sendbuf, tso, 0);
	return;
}

void sRelay(char *message)
{
	if(main_irc && config.relaychan[0] == '#')
	{
		snprintf(sendbuf, 510, "PRIVMSG %s :%s", config.relaychan, message);
		lowMsg(sendbuf);
	}
	snprintf(sendbuf, 510, "# %s", message);
	send_to_all_dcc(sendbuf, 0);
	return;
}

void *myalloc(size_t size)
{
	void *ret = malloc(size);
	if(!ret)
	{
#ifndef SOLARIS
		struct mallinfo mem = mallinfo();
		fprintf(out, "Heap: %d, Used: %d, Spare: %d\n", (int)mem.arena, (int)mem.uordblks, (int)mem.fordblks);
#endif
		fprintf(out, "*ERROR* Out of memory trying to allocate %d bytes!\n", (int)size);
		exit(0);
	}
	return ret;
}

#ifndef SOLARIS
void printmem()
{
	struct mallinfo mem = mallinfo();
	fprintf(out, "**********: Heap size: %d bytes.\n", (int)mem.uordblks);
	return;
}
#endif
