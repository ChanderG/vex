#define _XOPEN_SOURCE 600
#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <pty.h>
#include <X11/Xft/Xft.h>
#include <vterm.h>

/* Launching /bin/sh may launch a GNU Bash and that can have nasty side
 * effects. On my system, it clobbers ~/.bash_history because it doesn't
 * respect $HISTSIZE from my ~/.bashrc. That's very annoying. So, launch
 * /bin/dash which does nothing of the sort. */
#define SHELL "/bin/sh"

struct PTY
{
    int master, slave;
};

struct X11
{
    int fd;
    Display *dpy;
    int screen;
    Window root;

    Window termwin;
    GC termgc;
    unsigned long col_fg, col_bg;
    int w, h;

    XftFont* font;
    XftDraw* fdraw;
    int font_width, font_height;
    XftColor fcol_fg, fcol_bg;

    int buf_w, buf_h;
};

struct X11 x11;
struct PTY pty;
VTerm *vt;
VTermScreen *vts;
VTermState *vtstate;

bool
term_set_size(struct PTY *pty, struct X11 *x11)
{
    struct winsize ws = {
        .ws_col = x11->buf_w,
        .ws_row = x11->buf_h,
    };

    /* This is the very same ioctl that normal programs use to query the
     * window size. Normal programs are actually able to do this, too,
     * but it makes little sense: Setting the size has no effect on the
     * PTY driver in the kernel (it just keeps a record of it) or the
     * terminal emulator. IIUC, all that's happening is that subsequent
     * ioctls will report the new size -- until another ioctl sets a new
     * size.
     *
     * I didn't see any response to ioctls of normal programs in any of
     * the popular terminals (XTerm, VTE, st). They are not informed by
     * the kernel when a normal program issues an ioctl like that.
     *
     * On the other hand, if we were to issue this ioctl during runtime
     * and the size actually changed, child programs would get a
     * SIGWINCH. */
    if (ioctl(pty->master, TIOCSWINSZ, &ws) == -1)
    {
        perror("ioctl(TIOCSWINSZ)");
        return false;
    }

    return true;
}

bool
pt_pair(struct PTY *pty)
{
   int err = openpty(&(pty->master), &(pty->slave), NULL, NULL, NULL);
   if (err == -1) {
     return false;
   }

   return true;
}

void
vt_output_callback(const char* s, size_t len, void *user)
{
    (void)user;
    for (size_t i = 0; i < len; i++)
        write(pty.master, &s[i], 1);
}

int
x11_get_selection(struct X11* x11, char** s){
    Atom PRIMARY = XInternAtom(x11->dpy, "PRIMARY", 0);
    Atom XSEL_DATA = XInternAtom(x11->dpy, "XSEL_DATA", 0);
    Atom UTF8_STRING = XInternAtom(x11->dpy, "UTF8_STRING", True);
    XEvent event;
    Atom target;
    int format;
    unsigned long N, size;
    char *data;

    XConvertSelection(x11->dpy, PRIMARY, UTF8_STRING, XSEL_DATA, x11->termwin, CurrentTime);
    XSync(x11->dpy, False);

    XNextEvent(x11->dpy, &event);
    if (event.type != SelectionNotify) {
        printf("Incorrect event received!");
        return 0;
    }

    if (event.xselection.selection != PRIMARY) {
        printf("Wrong type of selection received!");
        return 0;
    }

    if (!event.xselection.property) {
        printf("Selection property not received!");
        return 0;
    }

    XGetWindowProperty(x11->dpy, x11->termwin, event.xselection.property,
                       0L, (~0L), 0, AnyPropertyType, &target,
                       &format, &size, &N, (unsigned char**) &data);
    if (target != UTF8_STRING) {
        printf("Selection target incorrect!");
        return 0;
    }

    *s = (char *)malloc(size+1);
    memcpy(*s, data, size);
    (*s)[size] = '\0';

    XFree(data);
    XDeleteProperty(x11->dpy, x11->termwin, event.xselection.property);

    return size;
}

void
x11_button(XButtonEvent *ev)
{
    // if middle click - paste
    if (ev->button == Button2) {
        char *data;
        int size = x11_get_selection(&x11, &data);
        if (size == 0) {
            printf("No data found when trying to paste.\n");
            return;
        }

        vterm_keyboard_start_paste(vt);
        // send in the characters
        for(int i = 0; i < size; i++) {
            vterm_keyboard_unichar(vt, data[i], VTERM_MOD_NONE);
        }
        vterm_keyboard_end_paste(vt);
        free(data);
    }
}

void
x11_key(XKeyEvent *ev)
{
    char buf[32];
    int i, num;
    KeySym ksym;

    num = XLookupString(ev, buf, sizeof buf, &ksym, 0);
    if (ksym == XK_Left)
        vterm_keyboard_key(vt, VTERM_KEY_LEFT, VTERM_MOD_NONE);
    else if (ksym == XK_Right)
        vterm_keyboard_key(vt, VTERM_KEY_RIGHT, VTERM_MOD_NONE);
    else if (ksym == XK_Up)
        vterm_keyboard_key(vt, VTERM_KEY_UP, VTERM_MOD_NONE);
    else if (ksym == XK_Down)
        vterm_keyboard_key(vt, VTERM_KEY_DOWN, VTERM_MOD_NONE);
    else if (ksym == XK_Prior)
        vterm_keyboard_key(vt, VTERM_KEY_PAGEUP, VTERM_MOD_NONE);
    else if (ksym == XK_Next)
        vterm_keyboard_key(vt, VTERM_KEY_PAGEDOWN, VTERM_MOD_NONE);
    else
        for (i = 0; i < num; i++)
            vterm_keyboard_unichar(vt, buf[i], VTERM_MOD_NONE);
}

void
x11_redraw(struct X11 *x11)
{
    int x, y;
    VTermScreenCell cell;
    XftColor *fg;

    XSetForeground(x11->dpy, x11->termgc, x11->col_bg);
    XFillRectangle(x11->dpy, x11->termwin, x11->termgc, 0, 0, x11->w, x11->h);

    for (y = 0; y < x11->buf_h; y++)
    {
        for (x = 0; x < x11->buf_w; x++)
        {
            vterm_screen_get_cell(vts, (VTermPos){.row = y, .col = x}, &cell);

            // default color
            fg = &x11->fcol_fg;

            if (cell.attrs.reverse) {
                // draw background of cell
                XSetForeground(x11->dpy, x11->termgc, x11->col_fg);
                XFillRectangle(x11->dpy, x11->termwin, x11->termgc,
                            x * x11->font_width,
                            y * x11->font_height,
                            x11->font_width, x11->font_height);
                fg = &x11->fcol_bg;
            }

            XftDrawString32(x11->fdraw, fg, x11->font,
                        x * x11->font_width,
                        y * x11->font_height + x11->font->ascent,
                        (XftChar32 *) cell.chars, 1);
            // note that we only use the first char instead of cell.width
            // we we don't have the tech (read: harfbuzz) for combining chars
        }
    }

    XSetForeground(x11->dpy, x11->termgc, x11->col_fg);

    VTermPos cursor;
    vterm_state_get_cursorpos(vtstate, &cursor);
    XFillRectangle(x11->dpy, x11->termwin, x11->termgc,
                   cursor.col * x11->font_width,
                   cursor.row * x11->font_height,
                   x11->font_width, x11->font_height);

    XSync(x11->dpy, False);
}

bool
x11_setup(struct X11 *x11)
{
    Colormap cmap;
    XColor color;

    x11->dpy = XOpenDisplay(NULL);
    if (x11->dpy == NULL)
    {
        fprintf(stderr, "Cannot open display\n");
        return false;
    }

    x11->screen = DefaultScreen(x11->dpy);
    x11->root = RootWindow(x11->dpy, x11->screen);
    x11->fd = ConnectionNumber(x11->dpy);

    x11->font = XftFontOpenName(x11->dpy, x11->screen,
                                "Monospace:size=22");

    if (x11->font == NULL)
    {
        fprintf(stderr, "Could not load font\n");
        return false;
    }

    x11->font_height = x11->font->height;
    XGlyphInfo ext;
    XftTextExtents8(x11->dpy, x11->font, (FcChar8 *)"m", 1, &ext);
    x11->font_width = ext.width + 2;

    cmap = DefaultColormap(x11->dpy, x11->screen);

    if (!XAllocNamedColor(x11->dpy, cmap, "white", &color, &color))
    {
        fprintf(stderr, "Could not load bg color\n");
        return false;
    }
    x11->col_bg = color.pixel;

    if (!XAllocNamedColor(x11->dpy, cmap, "black", &color, &color))
    {
        fprintf(stderr, "Could not load fg color\n");
        return false;
    }
    x11->col_fg = color.pixel;

    // init XftColor for use with text
    if (XftColorAllocName(x11->dpy,
                           DefaultVisual(x11->dpy, x11->screen),
                           cmap,
                          "black", &x11->fcol_fg) == False)
    {
        fprintf(stderr, "Could not load font fg color\n");
        return false;
    }

    if (XftColorAllocName(x11->dpy,
                           DefaultVisual(x11->dpy, x11->screen),
                           cmap,
                          "white", &x11->fcol_bg) == False)
    {
        fprintf(stderr, "Could not load font bg color\n");
        return false;
    }

    /* The terminal will have a fixed size of 80x25 cells. This is an
     * arbitrary number. No resizing has been implemented and child
     * processes can't even ask us for the current size (for now).
     */
    x11->buf_w = 80;
    x11->buf_h = 25;
    x11->w = x11->buf_w * x11->font_width;
    x11->h = x11->buf_h * x11->font_height;

    x11->termwin = XCreateSimpleWindow(x11->dpy, x11->root,
                                       0, 0,
                                       x11->w, x11->h,
                                       2, x11->col_fg, x11->col_bg);

    /* allow receiving mouse events */
    XSelectInput(x11->dpy, x11->termwin,
                KeyReleaseMask|KeyPressMask|ExposureMask|ButtonPressMask);

    XStoreName(x11->dpy, x11->termwin, "vex");
    XMapWindow(x11->dpy, x11->termwin);
    x11->termgc = XCreateGC(x11->dpy, x11->termwin, 0, NULL);

    // init draw for xft drawing
    x11->fdraw = XftDrawCreate(x11->dpy, x11->termwin,
                               DefaultVisual(x11->dpy, x11->screen), cmap);
    if (x11->fdraw == NULL)
    {
        fprintf(stderr, "Could not create xft draw \n");
        return false;
    }

    XSync(x11->dpy, False);

    return true;
}

bool
spawn(struct PTY *pty)
{
    pid_t p;
    setenv("TERM", "xterm", 1);

    p = fork();
    if (p == 0)
    {
        close(pty->master);

        /* Create a new session and make our terminal this process'
         * controlling terminal. The shell that we'll spawn in a second
         * will inherit the status of session leader. */
        setsid();
        if (ioctl(pty->slave, TIOCSCTTY, NULL) == -1)
        {
            perror("ioctl(TIOCSCTTY)");
            return false;
        }

        dup2(pty->slave, 0);
        dup2(pty->slave, 1);
        dup2(pty->slave, 2);
        close(pty->slave);

        execl(SHELL, "-" SHELL, (char *)NULL);
        return false;
    }
    else if (p > 0)
    {
        close(pty->slave);
        return true;
    }

    perror("fork");
    return false;
}

int
run(struct PTY *pty, struct X11 *x11)
{
    int maxfd;
    fd_set readable;
    XEvent ev;
    char* buf = (char *)malloc(100*sizeof(char));
    size_t bread;

    maxfd = pty->master > x11->fd ? pty->master : x11->fd;

    for (;;)
    {
        FD_ZERO(&readable);
        FD_SET(pty->master, &readable);
        FD_SET(x11->fd, &readable);

        if (select(maxfd + 1, &readable, NULL, NULL, NULL) == -1)
        {
            perror("select");
            return 1;
        }

        if (FD_ISSET(pty->master, &readable))
        {
            bread = read(pty->master, buf, 100);
            if (bread <= 0)
            {
                /* This is not necessarily an error but also happens
                 * when the child exits normally. */
                fprintf(stderr, "Nothing to read from child: ");
                perror(NULL);
                return 1;
            }

            vterm_input_write(vt, buf, bread);

            x11_redraw(x11);
        }

        if (FD_ISSET(x11->fd, &readable))
        {
            while (XPending(x11->dpy))
            {
                XNextEvent(x11->dpy, &ev);
                switch (ev.type)
                {
                    case Expose:
                        x11_redraw(x11);
                        break;
                    case KeyPress:
                        x11_key(&ev.xkey);
                        break;
                    case ButtonPress:
                        x11_button(&ev.xbutton);
                        break;
                }
            }
        }
    }

    return 0;
}

int
main()
{

    int rows, cols;
    rows = 25, cols = 80;

    if (!x11_setup(&x11))
        return 1;

    if (!pt_pair(&pty))
        return 1;

    vt = vterm_new(rows, cols);
    if (vt == NULL)
        return 1;

    vterm_set_utf8(vt, 1);
    vtstate = vterm_obtain_state(vt);
    vterm_state_reset(vtstate, 1);
    vts = vterm_obtain_screen(vt);
    vterm_output_set_callback(vt, &vt_output_callback, NULL);
    vterm_screen_enable_altscreen(vts, 1);

    if (!term_set_size(&pty, &x11))
        return 1;

    if (!spawn(&pty))
        return 1;

    return run(&pty, &x11);
}
