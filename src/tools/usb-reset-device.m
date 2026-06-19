#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <IOUSBHost/IOUSBHost.h>

static void Usage(const char *argv0)
{
    fprintf(stderr, "usage: %s vendor_hex product_hex\n", argv0);
    fprintf(stderr, "examples:\n");
    fprintf(stderr, "  %s 0x17cc 0x1978  # Native Instruments Audio 8 DJ\n", argv0);
    fprintf(stderr, "  %s 0x1963 0x0059  # IK Multimedia iRig Stream\n", argv0);
    fprintf(stderr, "no default target is allowed; resetting the wrong USB audio device can leave Core Audio capture unavailable.\n");
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        uint32_t vendor = 0x1963;
        uint32_t product = 0x0059;
        if (argc == 3) {
            vendor = (uint32_t)strtoul(argv[1], NULL, 0);
            product = (uint32_t)strtoul(argv[2], NULL, 0);
        } else {
            Usage(argv[0]);
            return 2;
        }

        CFMutableDictionaryRef matching = [IOUSBHostDevice createMatchingDictionaryWithVendorID:@(vendor)
                                                                                      productID:@(product)
                                                                                      bcdDevice:nil
                                                                                    deviceClass:nil
                                                                                 deviceSubclass:nil
                                                                                 deviceProtocol:nil
                                                                                          speed:nil
                                                                                 productIDArray:nil];
        io_iterator_t iterator = IO_OBJECT_NULL;
        kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
        if (kr != KERN_SUCCESS || iterator == IO_OBJECT_NULL) {
            fprintf(stderr, "matching failed: 0x%08x\n", kr);
            return 3;
        }

        io_service_t service = IOIteratorNext(iterator);
        IOObjectRelease(iterator);
        if (service == IO_OBJECT_NULL) {
            fprintf(stderr, "device not found: vendor=0x%04x product=0x%04x\n", vendor, product);
            return 4;
        }

        dispatch_queue_t queue = dispatch_queue_create("org.opena8dj.usb-reset-device", DISPATCH_QUEUE_SERIAL);
        NSError *error = nil;
        CFTypeRef productNameRef = IORegistryEntryCreateCFProperty(service,
                                                                   CFSTR(kUSBHostDevicePropertyProductString),
                                                                   kCFAllocatorDefault,
                                                                   0);
        CFTypeRef vendorNameRef = IORegistryEntryCreateCFProperty(service,
                                                                  CFSTR(kUSBHostDevicePropertyVendorString),
                                                                  kCFAllocatorDefault,
                                                                  0);

        IOUSBHostDevice *device = [[IOUSBHostDevice alloc] initWithIOService:service
                                                                     options:IOUSBHostObjectInitOptionsDeviceSeize
                                                                       queue:queue
                                                                       error:&error
                                                             interestHandler:nil];
        IOObjectRelease(service);
        if (device == nil) {
            const char *message = error.localizedDescription.UTF8String;
            fprintf(stderr, "open failed: %s\n", message ? message : "unknown");
            if (productNameRef) {
                CFRelease(productNameRef);
            }
            if (vendorNameRef) {
                CFRelease(vendorNameRef);
            }
            return 5;
        }

        NSString *productName = CFBridgingRelease(productNameRef);
        NSString *vendorName = CFBridgingRelease(vendorNameRef);
        printf("resetting_usb_device=%s %s vendor=0x%04x product=0x%04x\n",
               vendorName.UTF8String ? vendorName.UTF8String : "",
               productName.UTF8String ? productName.UTF8String : "",
               vendor,
               product);

        error = nil;
        BOOL ok = [device resetWithError:&error];
        [device destroy];
        if (!ok) {
            const char *message = error.localizedDescription.UTF8String;
            fprintf(stderr, "reset failed: %s\n", message ? message : "unknown");
            return 6;
        }
        printf("reset_usb_device=PASS\n");
        return 0;
    }
}
