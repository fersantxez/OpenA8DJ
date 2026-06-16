#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hidsystem/IOHIDUserDevice.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const uint8_t kMouseDescriptor[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x02,  // Usage (Mouse)
    0xA1, 0x01,  // Collection (Application)
    0x09, 0x01,  //   Usage (Pointer)
    0xA1, 0x00,  //   Collection (Physical)
    0x05, 0x09,  //     Usage Page (Buttons)
    0x19, 0x01,  //     Usage Minimum (1)
    0x29, 0x03,  //     Usage Maximum (3)
    0x15, 0x00,  //     Logical Minimum (0)
    0x25, 0x01,  //     Logical Maximum (1)
    0x95, 0x03,  //     Report Count (3)
    0x75, 0x01,  //     Report Size (1)
    0x81, 0x02,  //     Input (Data, Variable, Absolute)
    0x95, 0x01,  //     Report Count (1)
    0x75, 0x05,  //     Report Size (5)
    0x81, 0x03,  //     Input (Constant)
    0x05, 0x01,  //     Usage Page (Generic Desktop)
    0x09, 0x30,  //     Usage (X)
    0x09, 0x31,  //     Usage (Y)
    0x15, 0x81,  //     Logical Minimum (-127)
    0x25, 0x7F,  //     Logical Maximum (127)
    0x75, 0x08,  //     Report Size (8)
    0x95, 0x02,  //     Report Count (2)
    0x81, 0x06,  //     Input (Data, Variable, Relative)
    0xC0,        //   End Collection
    0xC0         // End Collection
};

static CFNumberRef number(int value) {
  return CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
}

static void set_number(CFMutableDictionaryRef dict, CFStringRef key, int value) {
  CFNumberRef n = number(value);
  CFDictionarySetValue(dict, key, n);
  CFRelease(n);
}

static IOHIDUserDeviceRef create_mouse(void) {
  CFMutableDictionaryRef props =
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);
  CFDataRef descriptor =
      CFDataCreate(kCFAllocatorDefault, kMouseDescriptor, sizeof(kMouseDescriptor));
  CFDictionarySetValue(props, CFSTR(kIOHIDReportDescriptorKey), descriptor);
  set_number(props, CFSTR(kIOHIDVendorIDKey), 0x1209);
  set_number(props, CFSTR(kIOHIDProductIDKey), 0xA8D0);
  set_number(props, CFSTR(kIOHIDVersionNumberKey), 1);
  set_number(props, CFSTR(kIOHIDPrimaryUsagePageKey), 0x01);
  set_number(props, CFSTR(kIOHIDPrimaryUsageKey), 0x02);
  CFDictionarySetValue(props, CFSTR(kIOHIDManufacturerKey), CFSTR("OpenA8DJ"));
  CFDictionarySetValue(props, CFSTR(kIOHIDProductKey), CFSTR("OpenA8DJ Virtual Mouse"));

  IOHIDUserDeviceRef device =
      IOHIDUserDeviceCreateWithProperties(kCFAllocatorDefault, props, 0);
  CFRelease(descriptor);
  CFRelease(props);
  return device;
}

static void send_report(IOHIDUserDeviceRef device, uint8_t buttons, int8_t dx, int8_t dy) {
  uint8_t report[3] = {buttons, (uint8_t)dx, (uint8_t)dy};
  IOReturn ret = IOHIDUserDeviceHandleReportWithTimeStamp(device, 0, report, sizeof(report));
  if (ret != kIOReturnSuccess) {
    fprintf(stderr, "IOHIDUserDeviceHandleReportWithTimeStamp failed: 0x%x\n", ret);
  }
  usleep(6000);
}

static void move_relative(IOHIDUserDeviceRef device, int dx, int dy) {
  while (dx != 0 || dy != 0) {
    int step_x = dx > 127 ? 127 : (dx < -127 ? -127 : dx);
    int step_y = dy > 127 ? 127 : (dy < -127 ? -127 : dy);
    send_report(device, 0, (int8_t)step_x, (int8_t)step_y);
    dx -= step_x;
    dy -= step_y;
  }
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s x y\n", argv[0]);
    return 2;
  }
  const int x = (int)strtol(argv[1], NULL, 0);
  const int y = (int)strtol(argv[2], NULL, 0);

  IOHIDUserDeviceRef device = create_mouse();
  if (device == NULL) {
    fprintf(stderr, "failed to create virtual mouse\n");
    return 1;
  }

  CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);

  move_relative(device, -4000, -4000);
  usleep(100000);
  move_relative(device, x, y);
  usleep(100000);
  send_report(device, 1, 0, 0);
  usleep(120000);
  send_report(device, 0, 0, 0);

  CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.5, false);
  CFRelease(device);
  return 0;
}
