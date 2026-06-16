#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOLLEvent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static io_connect_t open_hid_system(void) {
  io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault,
                                                     IOServiceMatching("IOHIDSystem"));
  if (service == IO_OBJECT_NULL) {
    return IO_OBJECT_NULL;
  }

  io_connect_t connect = IO_OBJECT_NULL;
  kern_return_t kr = IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &connect);
  IOObjectRelease(service);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "IOServiceOpen(IOHIDSystem) failed: 0x%x\n", kr);
    return IO_OBJECT_NULL;
  }
  return connect;
}

static int post_mouse(io_connect_t connect, int event_type, int x, int y) {
  NXEventData data;
  memset(&data, 0, sizeof(data));
  data.mouse.buttonNumber = 0;
  data.mouse.click = 1;
  data.mouse.pressure = 255;
  IOGPoint point = {x, y};
  kern_return_t kr = IOHIDPostEvent(connect,
                                    (UInt32)event_type,
                                    point,
                                    &data,
                                    kNXEventDataVersion,
                                    0,
                                    kIOHIDSetCursorPosition);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "IOHIDPostEvent(%d) failed: 0x%x\n", event_type, kr);
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s x y\n", argv[0]);
    return 2;
  }
  const int x = (int)strtol(argv[1], NULL, 0);
  const int y = (int)strtol(argv[2], NULL, 0);

  IOHIDAccessType before = IOHIDCheckAccess(kIOHIDRequestTypePostEvent);
  printf("hid_post_event_access_before=%d\n", before);
  if (before != kIOHIDAccessTypeGranted) {
    bool granted = IOHIDRequestAccess(kIOHIDRequestTypePostEvent);
    printf("hid_post_event_request_granted=%d\n", granted ? 1 : 0);
    sleep(1);
  }
  IOHIDAccessType after = IOHIDCheckAccess(kIOHIDRequestTypePostEvent);
  printf("hid_post_event_access_after=%d\n", after);

  io_connect_t connect = open_hid_system();
  if (connect == IO_OBJECT_NULL) {
    return 1;
  }

  int status = 0;
  status |= post_mouse(connect, NX_MOUSEMOVED, x, y);
  usleep(100000);
  status |= post_mouse(connect, NX_LMOUSEDOWN, x, y);
  usleep(100000);
  status |= post_mouse(connect, NX_LMOUSEUP, x, y);
  IOServiceClose(connect);
  return status;
}
