/* not part of fsh; here to help with part 8 of the assignment */

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc != 2) {  /* simplified as compared to the real cd */
	fprintf(stderr, "usage: %s dir\n", argv[0]);  /* (reflects actual usage requirements of this lousy little program; different from your built-in cd in fsh) */
	return(1);
    }
    if (chdir(argv[1])) {
	perror(argv[1]);
	return(1);
    }
    return(0);
}
