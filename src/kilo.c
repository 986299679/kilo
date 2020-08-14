/******************************************************************************
 * @author: waves(GenmZy_)
 * Source: https://viewsourcecode.org/snaptoken/kilo/
 *****************************************************************************/

/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/
struct termios orig_termios; // Just to set original mode to variable to store

/* Function headers {{{ */
void enableRawMode();

void disableRawMode();

void die(const char *s);
/* }}} Function headers */

/*** init ***/
int main(void)
{
  enableRawMode();

  while (1) {
    char c = '\0';
    if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) {
      die("read");
    }

    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d (%c)\r\n", c, c);
    }

    if (c == CTRL_KEY('q')) {
      break;
    }
  }

  return 0;
}

/*** terminal ***/
void enableRawMode()
{
  if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
    die("tcsetattr");
  }
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP); // Disable <C-s> and <C-q> so that can display it | Fix <C-m>
  raw.c_oflag &= ~(OPOST); // Make a \r\n
  raw.c_cflag |= ~(CS8);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); // Disable echo | Enable runtime refect | Disable <C-c> |
                                                   // Disable <C-v>
  raw.c_cc[VMIN] = 0;  // Set the mininal number of bytes of input needed for read(), 0 for return as soon when input.
  raw.c_cc[VTIME] = 1; // Set the max time to wait for for read(), it is in tenth of a second, so 1 is 1/10 second.

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
    die("tcsetattr");
  }
}

/*Add an die function to print the error messages*/
void die(const char *s)
{
  perror(s);
  exit(EXIT_FAILURE);
}
