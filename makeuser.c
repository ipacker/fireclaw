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
// $Id: makeuser.c,v 1.2 2003/06/17 18:55:46 snazbaz Exp $
//

#include <stdio.h>
#include <crypt.h>
#include "cfg.h"

int main(void)
{
	char buffer[NICKSIZE + PASSLEN];
	char *pass;
	FILE *fp;
	unsigned long seed[2];
	char salt[] = "$1$........";
	const char *const seedchars = 
		"./0123456789ABCDEFGHIJKLMNOPQRST"
		"UVWXYZabcdefghijklmnopqrstuvwxyz";
	int i;

	seed[0] = time(NULL);
	seed[1] = getpid() ^ (seed[0] >> 14 & 0x30000);
  	for (i = 0; i < 8; i++)
		salt[3+i] = seedchars[(seed[i/5] >> (i%5)*6) & 0x3f];

	printf("This util will create a brand new admins.txt for you with your chosen\n");
	printf("user and password.\n\n");
	printf("Username: ");

	fgets(buffer, NICKSIZE, stdin);
	buffer[strlen(buffer)-1] = 0;

	pass = crypt((char*)getpass("Password: "), salt);

	fp = fopen("admins.txt", "w");

	if(!fp) {
		printf("\n*** ERROR: Unable to open admins.txt!\n");
		exit(13);
	}

	fprintf(fp, "%s %s 500\n", buffer, pass);

	fclose(fp);

	printf("\nUser %s written to admins.txt!\n", buffer);
	printf("Now lets choose a channel to start up on, you will need to be in at\n");
	printf("least one common channel with the bot in order to login, hence why\n");
	printf("we need to set this here.\n\n");
	printf("Channel name: ");

	fgets(buffer, NICKSIZE, stdin);
	buffer[strlen(buffer)-1] = 0;

	fp = fopen("chans.txt", "w");

	if(!fp) {
		printf("\n*** ERROR: Unable to open chans.txt!\n");
		exit(13);
	}

	fprintf(fp, "%s aj 0 + -\n", buffer);

	fclose(fp);

	printf("\n%s written to chans.txt!\n", buffer);
	printf("All done, now set up your claw.conf and you are ready to go.\n");

	return 0;
}

