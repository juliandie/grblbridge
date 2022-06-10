#ifndef LIB_TTY_H_
#define LIB_TTY_H_

#if 0
struct termios {
    tcflag_t c_iflag; /* input mode flags */
    tcflag_t c_oflag; /* output mode flags */
    tcflag_t c_cflag; /* control mode flags */
    tcflag_t c_lflag; /* local mode flags */
    cc_t c_line;	  /* line discipline */
    cc_t c_cc[NCCS];  /* control characters */
};
tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
tty.c_cflag |= PARENB;  // Set parity bit, enabling parity

tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
tty.c_cflag |= CSTOPB;  // Set stop field, two stop bits used in communication

tty.c_cflag &= ~CSIZE; // Clear all the size bits, then use one of the statements below
tty.c_cflag |= CS5; // 5 bits per byte
tty.c_cflag |= CS6; // 6 bits per byte
tty.c_cflag |= CS7; // 7 bits per byte
tty.c_cflag |= CS8; // 8 bits per byte (most common)

tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
tty.c_cflag |= CRTSCTS;  // Enable RTS/CTS hardware flow control

tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

tty.c_lflag &= ~ICANON;

tty.c_lflag &= ~ECHO; // Disable echo
tty.c_lflag &= ~ECHOE; // Disable erasure
tty.c_lflag &= ~ECHONL; // Disable new-line echo

tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP

tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl

tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes

VMIN = 0, VTIME = 0: No blocking, return immediately with what is available
VMIN > 0, VTIME = 0: This will make read() always wait for bytes(exactly how many is determined by VMIN), so read() could block indefinitely.
VMIN = 0, VTIME > 0: This is a blocking read of any number of chars with a maximum timeout(given by VTIME).read() will block until either any amount of data is available, or the timeout occurs.This happens to be my favourite mode(and the one I use the most).
VMIN > 0, VTIME > 0: Block until either VMIN characters have been received, or VTIME after first character has elapsed.Note that the timeout for VTIME does not begin until the first character is received.
#endif


int tty_open(const char *ttyif);

int tty_select(int fd, int timeout);
int tty_read(int fd, char *buf, size_t size);
int tty_write(int fd, const char *buf, size_t size);

#endif