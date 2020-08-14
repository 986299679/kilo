/******************************************************************************
 * @author: waves(GenmZy_)
 * Source: https://viewsourcecode.org/snaptoken/kilo/
 *****************************************************************************/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// Just to keep original mode to variable
struct termios orig_termios;

/* Function headers {{{ */
void enableRawMode();

void disableRawMode();
/* }}} Function headers */

int main(void)
{
  enableRawMode();

  while (1) {
    char c = '\0';
    read(STDIN_FILENO, &c, 1);

    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d (%c)\r\n", c, c);
    }

    if ('q' == c) {
      break;
    }
  }

  return 0;
}

void enableRawMode()
{
  tcgetattr(STDIN_FILENO, &orig_termios);
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
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
