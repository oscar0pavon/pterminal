#include "selection.h"
#include "terminal.h"
#include "utf8.h"

void selstart(int col, int row, int snap) {
  selclear();
  sel.mode = SEL_EMPTY;
  sel.type = SEL_REGULAR;
  sel.alt = IS_SET(MODE_ALTSCREEN);
  sel.snap = snap;
  sel.original_end.x = sel.original_beginning.x = col;
  sel.original_end.y = sel.original_beginning.y = row;
  selnormalize();

  if (sel.snap != 0)
    sel.mode = SEL_READY;
  tsetdirt(sel.beginning_normalized.y, sel.end_normalized.y);
}

void selextend(int col, int row, int type, int done) {
  int oldey, oldex, oldsby, oldsey, oldtype;

  if (sel.mode == SEL_IDLE)
    return;
  if (done && sel.mode == SEL_EMPTY) {
    selclear();
    return;
  }

  oldey = sel.original_end.y;
  oldex = sel.original_end.x;
  oldsby = sel.beginning_normalized.y;
  oldsey = sel.end_normalized.y;
  oldtype = sel.type;

  sel.original_end.x = col;
  sel.original_end.y = row;
  selnormalize();
  sel.type = type;

  if (oldey != sel.original_end.y || oldex != sel.original_end.x || oldtype != sel.type ||
      sel.mode == SEL_EMPTY)
    tsetdirt(MIN(sel.beginning_normalized.y, oldsby), MAX(sel.end_normalized.y, oldsey));

  sel.mode = done ? SEL_IDLE : SEL_READY;
}

void selnormalize(void) {
  int i;

  if (sel.type == SEL_REGULAR &&
      sel.original_beginning.y != sel.original_end.y) {
    sel.beginning_normalized.x = sel.original_beginning.y < sel.original_end.y
                                     ? sel.original_beginning.x
                                     : sel.original_end.x;
    sel.end_normalized.x = sel.original_beginning.y < sel.original_end.y
                               ? sel.original_end.x
                               : sel.original_beginning.x;
  } else {
    sel.beginning_normalized.x =
        MIN(sel.original_beginning.x, sel.original_end.x);
    sel.end_normalized.x = MAX(sel.original_beginning.x, sel.original_end.x);
  }
  sel.beginning_normalized.y =
      MIN(sel.original_beginning.y, sel.original_end.y);
  sel.end_normalized.y = MAX(sel.original_beginning.y, sel.original_end.y);

  selsnap(&sel.beginning_normalized.x, &sel.beginning_normalized.y, -1);
  selsnap(&sel.end_normalized.x, &sel.end_normalized.y, +1);

  /* expand selection over line breaks */
  if (sel.type == SEL_RECTANGULAR)
    return;
  i = tlinelen(sel.beginning_normalized.y);
  if (i < sel.beginning_normalized.x)
    sel.beginning_normalized.x = i;
  if (tlinelen(sel.end_normalized.y) <= sel.end_normalized.x)
    sel.end_normalized.x = term.col - 1;
}

int selected(int x, int y) {
  if (sel.mode == SEL_EMPTY || sel.original_beginning.x == -1 ||
      sel.alt != IS_SET(MODE_ALTSCREEN))
    return 0;

  if (sel.type == SEL_RECTANGULAR)
    return BETWEEN(y, sel.beginning_normalized.y, sel.end_normalized.y) &&
           BETWEEN(x, sel.beginning_normalized.x, sel.end_normalized.x);

  return BETWEEN(y, sel.beginning_normalized.y, sel.end_normalized.y) &&
         (y != sel.beginning_normalized.y || x >= sel.beginning_normalized.x) &&
         (y != sel.end_normalized.y || x <= sel.end_normalized.x);
}

void selsnap(int *x, int *y, int direction) {
  int newx, newy, xt, yt;
  int delim, prevdelim;
  const PGlyph *gp, *prevgp;

  switch (sel.snap) {
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

  if (sel.original_beginning.x == -1)
    return NULL;

  bufsize = (term.col + 1) * (sel.end_normalized.y - sel.beginning_normalized.y + 1) * UTF_SIZ;
  ptr = str = xmalloc(bufsize);

  /* append every set & selected glyph to the selection */
  for (y = sel.beginning_normalized.y; y <= sel.end_normalized.y; y++) {
    if ((linelen = tlinelen(y)) == 0) {
      *ptr++ = '\n';
      continue;
    }

    if (sel.type == SEL_RECTANGULAR) {
      gp = &TLINE(y)[sel.beginning_normalized.x];
      lastx = sel.end_normalized.x;
    } else {
      gp = &TLINE(y)[sel.beginning_normalized.y == y ? sel.beginning_normalized.x : 0];
      lastx = (sel.end_normalized.y == y) ? sel.end_normalized.x : term.col - 1;
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
    if ((y < sel.end_normalized.y || lastx >= linelen) &&
        (!(last->mode & ATTR_WRAP) || sel.type == SEL_RECTANGULAR))
      *ptr++ = '\n';
  }
  *ptr = 0;
  return str;
}
