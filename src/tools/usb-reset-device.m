#import <Foundation/Foundation.h>
#import <IOUSBHost/IOUSBHost.h>
#import <IOKit/IOKitLib.h>
#import <dispatch/dispatch.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        if (argc != 3) {
            fprintf(stderr, "usage: %s vendor_hex product_hex\n", argv[0]);
            return 64;
        }

        const uint32_t vendor = (uint32_t)strtoul(argv[1], NULL, 0);
        const uint32_t product = (uint32_t)strtoul(argv[2], NULL, 0);
        if (vendor == 0 || product == 0) {
            fprintf(stderr, "invalid vendor/product\n");
            return 64;
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
        kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault,
                                                        matching,
                                                        &iterator);
        CFRelease(matching);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "IOServiceGetMatchingServices failed: 0x%x\n", kr);
            return 1;
        }

        io_service_t service = IOIteratorNext(iterator);
        IOObjectRelease(iterator);
        if (service == IO_OBJECT_NULL) {
            fprintf(stderr, "no matching USB device vendor=0x%04x product=0x%04x\n", vendor, product);
            return 2;
        }

        fprintf(stderr, "phase=open_usb_host_device\n");
        NSError *error = nil;
        dispatch_queue_t queue = dispatch_queue_create("org.opena8dj.usb-reset-device", DISPATCH_QUEUE_SERIAL);
        IOUSBHostDevice *device = [[IOUSBHostDevice alloc] initWithIOService:service
                                                                     options:IOUSBHostObjectInitOptionsDeviceSeize
                                                                       queue:queue
                                                                       error:&error
                                                            interestHandler:nil];
        IOObjectRelease(service);
        if (device == nil) {
            const char *message = error.localizedDescription.UTF8String;
            fprintf(stderr, "failed to open USB device: %s\n", message != NULL ? message : "unknown");
            return 3;
        }

        printf("resetting_usb_device=vendor=0x%04x product=0x%04x\n", vendor, product);
        fflush(stdout);
        fprintf(stderr, "phase=reset\n");
        if (![device resetWithError:&error]) {
            const char *message = error.localizedDescription.UTF8String;
            fprintf(stderr, "reset failed: %s\n", message != NULL ? message : "unknown");
            [device destroy];
            return 4;
        }

        [device destroy];
        printf("reset=PASS\n");
        return 0;
    }
}
