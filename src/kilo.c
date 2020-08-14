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
/*** includes end ***/

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
/*** defines end ***/

/*** data ***/
struct termios orig_termios; // Just to set original mode to variable to store
/*** data end ***/

/* Function headers {{{ */
void enableRawMode();

void disableRawMode();

void die(const char *s);

char editorReadKey();

void editorProcessKeypress();
/* }}} Function headers */

/*** init ***/
int main(void)
{
  enableRawMode();

  while (1) {
    editorProcessKeypress();
  }

  return 0;
}
/*** init end ***/

/*** input ***/
void editorProcessKeypress()
{
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      exit(0);
      break;
  }
}
/*** input end ***/

/*** terminal ***/
char editorReadKey()
{
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1))) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }

  return c;
}

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
/*** terminal end ***/
