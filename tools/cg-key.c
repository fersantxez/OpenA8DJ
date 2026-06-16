#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s keycode\n", argv[0]);
    return 2;
  }
  const CGKeyCode key = (CGKeyCode)strtoul(argv[1], NULL, 0);
  CGEventRef down = CGEventCreateKeyboardEvent(NULL, key, true);
  CGEventRef up = CGEventCreateKeyboardEvent(NULL, key, false);
  if (down == NULL || up == NULL) {
    fprintf(stderr, "failed to create key events\n");
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
