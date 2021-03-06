/******************************************************************************
 * @author: waves(GenmZy_)
 * Source: https://viewsourcecode.org/snaptoken/kilo/
 *****************************************************************************/

/*** includes ***/
// for using getline
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

// general including
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
/*** includes end ***/

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define MSG_TIMEOUT 5
#define KILO_QUIT_TIMES 3

// When the first enum is set to int, the others will increased automatically
enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000, // 1000 stand for left
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DELETE,
  HOME,
  END,
  PAGE_UP,
  PAGE_DOWN
};
/*** defines end ***/

/*** data ***/
typedef struct erow {
  int size;
  int rsize; // row size
  char *chars;
  char *render; // row render
} Erow;

// Store editor state, for future, we will retain the arg like term-height
struct editorConfig {
  /*cursor position*/
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  /*buffer modified  flag*/
  int dirty;
  /*line viewer*/
  int numrows;
  Erow *row;
  /* status bar arguments */
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  /*terminal args*/
  struct termios orig_termios; // Just to set original mode to variable to store
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

int editorReadKey();

void editorProcessKeypress();

void editorRefreshScreen();

void editorDrawRows(struct abuf *buf);

int getWindowSize(int *rows, int *cols);

void initEditor();

int getCursorPosition(int *rows, int *cols);

void abAppend(struct abuf *ab, const char *s, int len);

void abFree(struct abuf *ab);

void editorMoveCursor(int key);

void editorOpen(char *filename);

void editorAppendRow(char *s, size_t len);

void editorScroll();

void editorUpdateRow(Erow *row);

int editorRowCxToRx(Erow *row, int cx);

void editorDrawStatusBar(struct abuf *ab);

void editorSetStatusMessage(const char *fmt, ...);

void editorDrawMessageBar(struct abuf *ab);

void editorRowInsertChar(Erow *row, int at, int c);

void editorInsertChar(int c);

char *editorRowsToString(int *buflen);

void editorSave();

void editorRowDelChar(Erow *row, int at);

void editorDelChar();
/* }}} Function headers */

/*** init ***/
int main(int argc, char *argv[])
{
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage("Kilo Help:<C-q> ~~> quit | <C-s> ~~> save.");

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}

void initEditor()
{
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.dirty = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1) {
    die("getWindowSize");
  }

  E.screenrows -= 2; // Leave one line for status bar
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
void editorMoveCursor(int key)
{
  // priority from high to low is:   .[]*&   so: &((E.row)[E.cy])
  // As we realloc this row, use multi, so it is a `E.row` can regard as array
  // now. Judge if cursor is in an actual line
  Erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key) {
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows) {
        E.cy++;
      }
      break;
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      } else if (row && E.cx == row->size) { // E.cy < E.numrows
        E.cy++;
        E.cx = 0;
      }
      break;
  }

  // If the cursor on the right edge of previous line, when move its next line,
  // if next line is shorter than the previous one, jump to the last character
  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void editorProcessKeypress()
{
  static int quit_times = KILO_QUIT_TIMES; // Should be static value (fucntion in loop)

  int c = editorReadKey();

  switch (c) {
    case '\r':
      /*TODO: Complete later*/
      break;

    // quit:
    case CTRL_KEY('q'):
      if (E.dirty && quit_times > 0) {
        editorSetStatusMessage("WARNING: File has unsaved changes. "
            "Press <C-q> %d more times for force quit.", quit_times);
        quit_times--;
        return;
      }
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case CTRL_KEY('s'):
      editorSave();
      break;

    // PAGE_UP/PAGE_DOWN:
    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else if(c == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenrows - 1;
          if (E.cy > E.numrows) {
            E.cy = E.numrows;
          }
        }

        int times = E.screenrows;
        while (--times) {
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
      }
      break;

    // normal move:
    case ARROW_UP:
    case ARROW_LEFT:
    case ARROW_RIGHT:
    case ARROW_DOWN:
      editorMoveCursor(c);
      break;

    // END/HOME:
    case HOME:
      E.cx = 0;
      break;
    case END:
      if (E.cy < E.numrows) {
        E.cx = E.row[E.cy].size;
      }
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DELETE:
      if (c == DELETE) {
        editorMoveCursor(ARROW_LEFT);
      }
      editorDelChar();
      break;

    case CTRL_KEY('l'):
    case '\x1b':
      break;

    default:
      editorInsertChar(c);
      break;
  }

  quit_times = KILO_QUIT_TIMES;
}
/*** input end ***/

/*** output ***/
void editorRefreshScreen()
{
  char buf[32];
  struct abuf ab = ABUF_INIT;

  editorScroll();
  abAppend(&ab, "\x1b[?25l", 6); // Hide the cursor when painting the `~`
  abAppend(&ab, "\x1b[H", 3);  // Put the cursor to first line first character.

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
                                            (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6); // Show the cursor as long as painting finished

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab); // Do not forget to free
}

// Put all the chars that should be print to a buf
void editorDrawRows(struct abuf *buf)
{
  int y;
  for (y = 0; y < E.screenrows; ++y) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "Kilo editor --version %s", KILO_VERSION);
        if (welcomelen > E.screenrows) {
          welcomelen = E.screenrows;
        }

        // Pur the buf in the editor messages into the center of the line
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(buf, "~", 1);
          padding--;
        }
        while (padding--) {
          abAppend(buf, " ", 1);
        }

        abAppend(buf, welcome, welcomelen);
      } else {
        abAppend(buf, "~", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0) {
        len = 0;
      }
      if (len > E.screencols) {
        len = E.screencols;
      }

      abAppend(buf, &E.row[filerow].render[E.coloff], len);
    }

    abAppend(buf, "\x1b[K", 3); // Erease part of current line
    abAppend(buf, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab)
{
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), " | %.20s - %d lines %s",
                     E.filename ? E.filename : "[NO NAME]", E.numrows,
                     E.dirty ? " " : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);

  if (len > E.screencols) {
    len = E.screencols;
  }

  // <esc>[7m can invert colors, <esc>[m siwtch back to normal formatting
  abAppend(ab, "\x1b[7m", 4);
  abAppend(ab, status, len);
  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

// scroll the editor output
void editorScroll()
{
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  // Check if the cursor above the window
  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  // Check if the cursor bottom the the screen
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.cx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.cx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorSetStatusMessage(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);

  E.statusmsg_time = time(NULL);
}

void editorDrawMessageBar(struct abuf *ab)
{
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);

  if (msglen > E.screencols) {
    msglen = E.screencols;
  }
  if (msglen && time(NULL) - E.statusmsg_time < MSG_TIMEOUT) {
    abAppend(ab, E.statusmsg, msglen);
  }
}
/*** output end ***/

/*** row operations ***/
void editorUpdateRow(Erow *row)
{
  int j;
  int tabs = 0;
  int idx = 0;

  for (j = 0; j < row->size; ++j) {
    if (row->chars[j] == '\t') {
      tabs++;
    }
  }
  free(row->render);
  row->render = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1);

  for (j = 0; j < row->size; ++j) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % KILO_TAB_STOP != 0) {
        row->render[idx++] = ' ';
      }
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
}

void editorAppendRow(char *s, size_t len)
{
  E.row = realloc(E.row, sizeof(Erow) * (E.numrows + 1));

  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
}

int editorRowCxToRx(Erow *row, int cx)
{
  int rx = 0;
  int j;

  for (j = 0; j < cx; ++j) {
    if (row->chars[j] == '\t') {
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    }
    rx++;
  }

  return rx;
}

// Insert a char to place that cursor hold.
void editorRowInsertChar(Erow *row, int at, int c)
{
  if (at < 0 || at > row->size) {
    at = row->size;
  }
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorInsertChar(int c)
{
  if (E.cy == E.numrows) {
    editorAppendRow("", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorRowDelChar(Erow *row, int at)
{
  if (at < 0 || at >= row->size) {
    return;
  }

  memmove(&row->chars[at], &row->chars[at + 1], row->size - 1); // Move current at to at+1;
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

void editorDelChar()
{
  if (E.cx == 0 && E.cy == 0) {
    return;
  }

  if (E.cx > 0) {
    editorRowDelChar(&E.row[E.cy], E.cx - 1);
    E.cx--;
  }
}
/*** row operations end ***/

/*** file I/O ***/
void editorOpen(char *filename)
{
  FILE *fp = fopen(filename, "r");
  char *line = NULL;
  size_t line_cap = 0;
  ssize_t linelen;

  if (!fp) {
    die("fopen");
  }

  free(E.filename);
  E.filename = strdup(filename);

  while ((linelen = getline(&line, &line_cap, fp)) != -1) {
    while (linelen > 0 &&
           ('\n' == line[linelen - 1] || '\r' == line[linelen - 1])) {
      linelen--;
    }
    editorAppendRow(line, linelen);
  }

  free(line);
  fclose(fp);
  E.dirty = 0;
}

// pointer buflen just get the lenth of buf
char *editorRowsToString(int *buflen)
{
  int j;
  int totlen = 0;
  char *p;
  char *buf;

  for (j = 0; j < E.numrows; j++) {
    totlen += E.row[j].size + 1; // Add 1 is to add '\r' or '\n'
  }

  *buflen = totlen;
  buf = malloc(totlen);
  p = buf;

  for (j = 0; j < E.numrows; ++j) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }

  return buf;
}

void editorSave()
{
  if (E.filename == NULL) {
    return;
  }

  int len;
  char *buf = editorRowsToString(&len);
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk.", len);
        return;
      }
    }

    close(fd);
  }

  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}
/*** file I/O end ***/

/*** terminal ***/
int editorReadKey()
{
  int nread;
  char c;
  // If want to return c normally, do not enter this loop
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("read");
    }
  }

  if (c == '\x1b') {
    char seq[3];

    /*
     * '\x1b[A' -> '\x1b[D' is arrow keys
     * If read a '\x1b'(escape sequence), read two more char and put them into
     * seq[2]. And if read() funtion cannot read other chars(may timeout), we
     * think just escape key
     */
    if (read(STDIN_FILENO, &seq[0], 1) != 1) {
      return '\x1b';
    }
    if (read(STDIN_FILENO, &seq[1], 1) != 1) {
      return '\x1b';
    }

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) {
          return '\x1b';
        }
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1':
              return HOME;
            case '3':
              return DELETE;
            case '4':
              return END;
            case '5':
              return PAGE_UP;   // PAGE_UP is "\x1b[5~"
            case '6':
              return PAGE_DOWN; // PAGE_DOWN is "\x1b[6~"
            case '7':
              return HOME;
            case '8':
              return END;
          }
        }
      } else {
        switch (seq[1]) {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'F':
          return END;
        case 'H':
          return HOME;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H':
          return HOME;
        case 'F':
          return END;
      }
    }

    return '\x1b';
  } else {
    return c;
  }
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
