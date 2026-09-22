/* Exercise accelerated override-redirect windows like recording notifications. */
#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <errno.h>
#include <time.h>

int main(void) {
  Display *display = XOpenDisplay(NULL);
  if (!display) return 1;
  int attributes[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8,
                     GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, None};
  XVisualInfo *visual = glXChooseVisual(display, DefaultScreen(display), attributes);
  if (!visual) return 2;
  Window root = RootWindow(display, visual->screen);
  Colormap colormap = XCreateColormap(display, root, visual->visual, AllocNone);
  XSetWindowAttributes settings = {0};
  settings.colormap = colormap;
  settings.override_redirect = True;
  settings.event_mask = StructureNotifyMask;
  Window window = XCreateWindow(display, root, 10, 10, 300, 100, 0,
      visual->depth, InputOutput, visual->visual,
      CWColormap | CWOverrideRedirect | CWEventMask, &settings);
  XStoreName(display, window, "Recorder notification lifecycle test");
  GLXContext context = glXCreateContext(display, visual, NULL, True);
  if (!context || !glXMakeCurrent(display, window, context)) return 3;
  XMapRaised(display, window);
  XSync(display, False);
  for (int frame = 0; frame < 15; ++frame) {
    glClearColor(0.1f, 0.6f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glXSwapBuffers(display, window);
    struct timespec remaining = {.tv_nsec = 20000000};
    while (nanosleep(&remaining, &remaining) == -1 && errno == EINTR) {}
  }
  glXMakeCurrent(display, None, NULL);
  glXDestroyContext(display, context);
  XDestroyWindow(display, window);
  XFreeColormap(display, colormap);
  XFree(visual);
  XCloseDisplay(display);
  return 0;
}
