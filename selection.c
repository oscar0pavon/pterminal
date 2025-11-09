#include "selection.h"
#include "terminal.h"
#include "utf8.h"

Selection selection;

void selinit(void) {
  selection.mode = SEL_IDLE;
  selection.snap = 0;
  selection.original_beginning.x = -1;
}

void selstart(int col, int row, int snap) {
  selclear();
  selection.mode = SEL_EMPTY;
  selection.type = SEL_REGULAR;
  selection.alt = IS_SET(MODE_ALTSCREEN);
  selection.snap = snap;
  selection.original_end.x = selection.original_beginning.x = col;
  selection.original_end.y = selection.original_beginning.y = row;
  selnormalize();

  if (selection.snap != 0)
    selection.mode = SEL_READY;
  tsetdirt(selection.beginning_normalized.y, selection.end_normalized.y);
}

void selextend(int col, int row, int type, int done) {
  int oldey, oldex, oldsby, oldsey, oldtype;

  if (selection.mode == SEL_IDLE)
    return;
  if (done && selection.mode == SEL_EMPTY) {
    selclear();
    return;
  }

  oldey = selection.original_end.y;
  oldex = selection.original_end.x;
  oldsby = selection.beginning_normalized.y;
  oldsey = selection.end_normalized.y;
  oldtype = selection.type;

  selection.original_end.x = col;
  selection.original_end.y = row;
  selnormalize();
  selection.type = type;

  if (oldey != selection.original_end.y || oldex != selection.original_end.x ||
      oldtype != selection.type || selection.mode == SEL_EMPTY)
    tsetdirt(MIN(selection.beginning_normalized.y, oldsby),
             MAX(selection.end_normalized.y, oldsey));

  selection.mode = done ? SEL_IDLE : SEL_READY;
}

void selnormalize(void) {
  int i;

  if (selection.type == SEL_REGULAR &&
      selection.original_beginning.y != selection.original_end.y) {
    selection.beginning_normalized.x =
        selection.original_beginning.y < selection.original_end.y
            ? selection.original_beginning.x
            : selection.original_end.x;
    selection.end_normalized.x =
        selection.original_beginning.y < selection.original_end.y
            ? selection.original_end.x
            : selection.original_beginning.x;
  } else {
    selection.beginning_normalized.x =
        MIN(selection.original_beginning.x, selection.original_end.x);
    selection.end_normalized.x =
        MAX(selection.original_beginning.x, selection.original_end.x);
  }
  selection.beginning_normalized.y =
      MIN(selection.original_beginning.y, selection.original_end.y);
  selection.end_normalized.y =
      MAX(selection.original_beginning.y, selection.original_end.y);

  selsnap(&selection.beginning_normalized.x, &selection.beginning_normalized.y,
          -1);
  selsnap(&selection.end_normalized.x, &selection.end_normalized.y, +1);

  /* expand selection over line breaks */
  if (selection.type == SEL_RECTANGULAR)
    return;
  i = tlinelen(selection.beginning_normalized.y);
  if (i < selection.beginning_normalized.x)
    selection.beginning_normalized.x = i;
  if (tlinelen(selection.end_normalized.y) <= selection.end_normalized.x)
    selection.end_normalized.x = term.col - 1;
}

int selected(int x, int y) {
  if (selection.mode == SEL_EMPTY || selection.original_beginning.x == -1 ||
      selection.alt != IS_SET(MODE_ALTSCREEN))
    return 0;

  if (selection.type == SEL_RECTANGULAR)
    return BETWEEN(y, selection.beginning_normalized.y,
                   selection.end_normalized.y) &&
           BETWEEN(x, selection.beginning_normalized.x,
                   selection.end_normalized.x);

  return BETWEEN(y, selection.beginning_normalized.y,
                 selection.end_normalized.y) &&
         (y != selection.beginning_normalized.y ||
          x >= selection.beginning_normalized.x) &&
         (y != selection.end_normalized.y || x <= selection.end_normalized.x);
}

void selsnap(int *x, int *y, int direction) {
  int newx, newy, xt, yt;
  int delim, prevdelim;
  const PGlyph *gp, *prevgp;

  switch (selection.snap) {
  case SNAP_WORD:
    /*
     * Snap around if the word wraps around at the end or
     * beginning of a line.
     */
    prevgp = &TLINE(*y)[*x];
    prevdelim = ISDELIM(prevgp->u);
    for (;;) {
      newx = *x + direction;
      newy = *y;
      if (!BETWEEN(newx, 0, term.col - 1)) {
        newy += direction;
        newx = (newx + term.col) % term.col;
        if (!BETWEEN(newy, 0, term.row - 1))
          break;

        if (direction > 0)
          yt = *y, xt = *x;
        else
          yt = newy, xt = newx;
        if (!(TLINE(yt)[xt].mode & ATTR_WRAP))
          break;
      }

      if (newx >= tlinelen(newy))
        break;

      gp = &TLINE(newy)[newx];
      delim = ISDELIM(gp->u);
      if (!(gp->mode & ATTR_WDUMMY) &&
          (delim != prevdelim || (delim && gp->u != prevgp->u)))
        break;

      *x = newx;
      *y = newy;
      prevgp = gp;
      prevdelim = delim;
    }
    break;
  case SNAP_LINE:
    /*
     * Snap around if the the previous line or the current one
     * has set ATTR_WRAP at its end. Then the whole next or
     * previous line will be selected.
     */
    *x = (direction < 0) ? 0 : term.col - 1;
    if (direction < 0) {
      for (; *y > 0; *y += direction) {
        if (!(TLINE(*y - 1)[term.col - 1].mode & ATTR_WRAP)) {
          break;
        }
      }
    } else if (direction > 0) {
      for (; *y < term.row - 1; *y += direction) {
        if (!(TLINE(*y)[term.col - 1].mode & ATTR_WRAP)) {
          break;
        }
      }
    }
    break;
  }
}

char *getsel(void) {
  char *str, *ptr;
  int y, bufsize, lastx, linelen;
  const PGlyph *gp, *last;

  if (selection.original_beginning.x == -1)
    return NULL;

  bufsize =
      (term.col + 1) *
      (selection.end_normalized.y - selection.beginning_normalized.y + 1) *
      UTF_SIZ;
  ptr = str = xmalloc(bufsize);

  /* append every set & selected glyph to the selection */
  for (y = selection.beginning_normalized.y; y <= selection.end_normalized.y;
       y++) {
    if ((linelen = tlinelen(y)) == 0) {
      *ptr++ = '\n';
      continue;
    }

    if (selection.type == SEL_RECTANGULAR) {
      gp = &TLINE(y)[selection.beginning_normalized.x];
      lastx = selection.end_normalized.x;
    } else {
      gp = &TLINE(y)[selection.beginning_normalized.y == y
                         ? selection.beginning_normalized.x
                         : 0];
      lastx = (selection.end_normalized.y == y) ? selection.end_normalized.x
                                                : term.col - 1;
    }
    last = &TLINE(y)[MIN(lastx, linelen - 1)];
    while (last >= gp && last->u == ' ')
      --last;

    for (; gp <= last; ++gp) {
      if (gp->mode & ATTR_WDUMMY)
        continue;

      ptr += utf8encode(gp->u, ptr);
    }

    /*
     * Copy and pasting of line endings is inconsistent
     * in the inconsistent terminal and GUI world.
     * The best solution seems like to produce '\n' when
     * something is copied from st and convert '\n' to
     * '\r', when something to be pasted is received by
     * st.
     * FIXME: Fix the computer world.
     */
    if ((y < selection.end_normalized.y || lastx >= linelen) &&
        (!(last->mode & ATTR_WRAP) || selection.type == SEL_RECTANGULAR))
      *ptr++ = '\n';
  }
  *ptr = 0;
  return str;
}
