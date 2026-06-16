#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s x y\n", argv[0]);
    return 2;
  }

  const double x = strtod(argv[1], NULL);
  const double y = strtod(argv[2], NULL);
  const CGPoint point = CGPointMake(x, y);

  CGEventRef down = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, point, kCGMouseButtonLeft);
  CGEventRef up = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, point, kCGMouseButtonLeft);
  if (down == NULL || up == NULL) {
    fprintf(stderr, "failed to create mouse events\n");
    if (down != NULL) {
      CFRelease(down);
    }
    if (up != NULL) {
      CFRelease(up);
    }
    return 1;
  }

  CGEventPost(kCGHIDEventTap, down);
  CGEventPost(kCGHIDEventTap, up);
  CFRelease(down);
  CFRelease(up);
  return 0;
}
