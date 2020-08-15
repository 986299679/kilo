/******************************************************************************
 * @author: waves(GenmZy_)
 * Source: https://viewsourcecode.org/snaptoken/kilo/
 *****************************************************************************/

/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
/*** includes end ***/

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
/*** defines end ***/

/*** data ***/
// Save editor state, for future, we will retain the arg like term-height
struct editorConfig {
  int screenrows;
  int screencols;
  // Just to set original mode to variable to store
  struct termios orig_termios;
};

struct editorConfig E;
/*** data end ***/

/*** append buffer data ***/
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}
/*** append buffer end data ***/

/* Function headers {{{ */
void enableRawMode();

void disableRawMode();

void die(const char *s);

char editorReadKey();

void editorProcessKeypress();

void editorRefreshScreen();

void editorDrawRows(struct abuf *buf);

int getWindowSize(int *rows, int *cols);

void initEditor();

int getCursorPosition(int *rows, int *cols);

void abAppend(struct abuf *ab, const char *s, int len);

void abFree(struct abuf *ab);
/* }}} Function headers */

/*** init ***/
int main(void)
{
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}

void initEditor()
{
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) {
    die("getWindowSize");
  }
}
/*** init end ***/

/*** append buffer ***/
void abAppend(struct abuf *ab, const char *s, int len) 
{
  char *new = realloc(ab->b, ab->len + len);
  if (!new) {
    return;
  }

  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab)
{
  free(ab->b);
}
/*** append buffer end ***/

/*** input ***/
void editorProcessKeypress()
{
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}
/*** input end ***/

/*** output ***/
void editorRefreshScreen()
{
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[2J", 4); // Clean the screen
  abAppend(&ab, "\x1b[H", 3);  // Put the cursor to first line first character.

  editorDrawRows(&ab);

  abAppend(&ab, "\x1b[H", 3);
  write(STDOUT_FILENO, ab.b, ab.len);
}

void editorDrawRows(struct abuf *buf)
{
  int y;
  for (y = 0; y < E.screenrows; ++y) {
    abAppend(buf, "~", 1);

    if (y < E.screenrows - 1) {
      abAppend(buf, "\r\n", 2);
    }
  }
}
/*** output end ***/

/*** terminal ***/
char editorReadKey()
{
  int nread;
  char c;
  // If want to return c normally, do not enter this loop
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }

  return c;
}

void enableRawMode()
{
  if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  // Disable <C-s> and <C-q> so that can display it | Fix <C-m>
  raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST); // Make a \r\n
  raw.c_cflag |= ~(CS8);
  // Disable echo | Enable runtime refect | Disable <C-c> | Disable <C-v>
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

  raw.c_cc[VMIN] = 0; // Set the mininal number of bytes of input needed for
                      // read(), 0 for return as soon when input.
  raw.c_cc[VTIME] = 1; // Set the max time to wait for for read(),
                       // it is in tenth of a second, so 1 is 1/10 second.

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  }
}

// Add an die function to print the error messages
void die(const char *s)
{
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(EXIT_FAILURE);
}

int getWindowSize(int *rows, int *cols)
{
  struct winsize ws;

  // Easy way to get terminal window size, so need a backfall project
  // Function ioctl put the windows arguments to the struct ws, if fail, return -1
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
      return -1;
    }

    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;

    return 0;
  }
}

int getCursorPosition(int *rows, int *cols)
{
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
    return -1;
  }

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) {
      break;
    }

    if (buf[i] == 'R') {
      break;
    }

    i++;
  }

  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') {
    return -1;
  }

  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
    return -1;
  }

  return 0;
}
/*** terminal end ***/
