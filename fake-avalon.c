#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <signal.h>


#define BAUDRATE B115200
#define MODEMDEVICE "/dev/ttyUSB1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;

#define hex_print(p) printf("%s\n", p)

static char nibble[] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

#define BYTES_PER_LINE 0x10

void hexdump(const uint8_t *p, unsigned int len)
{
	unsigned int i, addr;
	unsigned int wordlen = sizeof(void*);
	unsigned char v, line[BYTES_PER_LINE * 5];

	for (addr = 0; addr < len; addr += BYTES_PER_LINE) {
		/* clear line */
		for (i = 0; i < sizeof(line); i++) {
			if (i == wordlen * 2 + 52 ||
			    i == wordlen * 2 + 69) {
			    	line[i] = '|';
				continue;
			}

			if (i == wordlen * 2 + 70) {
				line[i] = '\0';
				continue;
			}

			line[i] = ' ';
		}

		/* print address */
		for (i = 0; i < wordlen * 2; i++) {
			v = addr >> ((wordlen * 2 - i - 1) * 4);
			line[i] = nibble[v & 0xf];
		}

		/* dump content */
		for (i = 0; i < BYTES_PER_LINE; i++) {
			int pos = (wordlen * 2) + 3 + (i / 8);

			if (addr + i >= len)
				break;
		
			v = p[addr + i];
			line[pos + (i * 3) + 0] = nibble[v >> 4];
			line[pos + (i * 3) + 1] = nibble[v & 0xf];

			/* character printable? */
			line[(wordlen * 2) + 53 + i] =
				(v >= ' ' && v <= '~') ? v : '.';
		}

		hex_print(line);
	}
}


/* Does the reverse of bin2hex but does not allocate any ram */
int hex2bin(unsigned char *p, const char *hexstr, size_t len)
{
	int ret = FALSE;

	while (*hexstr && len) {
		char hex_byte[4];
		unsigned int v;

		if (!hexstr[1]) {
			return ret;
		}

		memset(hex_byte, 0, 4);
		hex_byte[0] = hexstr[0];
		hex_byte[1] = hexstr[1];

		if (sscanf(hex_byte, "%x", &v) != 1) {
			printf("hex2bin sscanf '%s' failed", hex_byte);
			return ret;
		}

		*p = (unsigned char) v;

		p++;
		hexstr += 2;
		len--;
	}

	if (len == 0 && *hexstr == 0)
		ret = TRUE;
	return ret;
}

int rts(int fd, int rtsEnable)
{
	int flags;

	ioctl(fd, TIOCMGET, &flags);
	fprintf(stderr, "Flags are %x.\n", flags);

	if(rtsEnable!=0)
		flags |= TIOCM_RTS;
	else
		flags &= ~TIOCM_RTS;

	ioctl(fd, TIOCMSET, &flags);
	fprintf(stderr, "After set: %x.\n", flags);
}

main()
{
	struct termios oldtio,newtio;
	char buf[255];
	int fd, c, res;
	int read_count = 56;

	fd = open(MODEMDEVICE, O_RDWR | O_CLOEXEC | O_NOCTTY ); 
	if (fd <0) {perror(MODEMDEVICE); exit(-1); }
        
	tcgetattr(fd,&oldtio); /* save current serial port settings */
	bzero(&newtio, sizeof(newtio)); /* clear struct for new port settings */
        
	newtio.c_cflag |= BAUDRATE;
	newtio.c_cflag |= CS8;
	newtio.c_cflag |= CREAD;
	newtio.c_cflag |= CRTSCTS;
	newtio.c_cflag |= CLOCAL;
	newtio.c_cflag &= ~(CSIZE | PARENB);
	newtio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK |
			    ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	newtio.c_oflag &= ~OPOST;
	newtio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

        
	newtio.c_cc[VTIME] = 60;
	newtio.c_cc[VMIN] = 0;

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	while (read_count) {
		if (read_count == 0) {
			rts(fd, 0);
			hexdump(buf, 56);
			write(fd, buf, 56);
			read_count = 56;
			continue;
		}

		rts(fd, 1);
		res = read(fd, buf, 1);
		if (res != 56) {
			read_count -= res;
			continue;
		}
	}

	tcsetattr(fd,TCSANOW,&oldtio);
	close(fd);
}
