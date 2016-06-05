#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define TEMPER_MAGIC 'T' 
#define TEMPER_IOR_TIN  _IOR(TEMPER_MAGIC, 'i', int)
#define TEMPER_IOR_TOUT _IOR(TEMPER_MAGIC, 'o', int)

void usage()
{
	fprintf(stderr, "\
    This short program aims to test ioctl functions of the temper\n\
    device driver. It takes one argument:\n\
      - 'i' for in temperature\n\
      - 'o' for out temperature\n\
    The result is printed.\n");
}

int main(int argc, char *argv[])
{
	char cmd;
	int fd, rc = 0;
	unsigned long value = 0;

	if ((argc != 2) || (argv[1][0] != 'i' && argv[1][0] != 'o')) {
		usage();
		return -EINVAL;
	}
	cmd = argv[1][0];
	
	fd = open("/dev/temper", O_RDWR);
	if (fd < 0) {
		return errno;
	}

	switch (cmd) {
	case 'i':
		rc = ioctl(fd, TEMPER_IOR_TIN, &value);
		fprintf(stdout, "Inner temperature = %3lu.%03lu°C\n",
			value / 1000, value % 1000);
		break;
	case 'o':
		rc = ioctl(fd, TEMPER_IOR_TOUT, &value);
		fprintf(stdout, "Outer temperature = %3lu.%03lu°C\n",
			value / 1000, value % 1000);
		break;
	default:
		fprintf(stderr, "Command not known '%c'.\n", cmd);
		rc = -EINVAL;
	}

	close(fd);

	return rc;
}
