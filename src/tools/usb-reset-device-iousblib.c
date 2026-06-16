#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void release_plugin(IOCFPlugInInterface **plugin) {
    if (plugin != NULL) {
        IODestroyPlugInInterface(plugin);
    }
}

static int get_int_property(io_service_t service, CFStringRef key, int *out) {
    CFTypeRef value = IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, 0);
    if (value == NULL) {
        return 0;
    }
    int ok = 0;
    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        ok = CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, out);
    }
    CFRelease(value);
    return ok;
}

static io_service_t find_usb_plane_device(int vendor, int product) {
    io_iterator_t iterator = IO_OBJECT_NULL;
    kern_return_t kr = IORegistryCreateIterator(kIOMainPortDefault, "IOUSB",
                                                kIORegistryIterateRecursively,
                                                &iterator);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "IORegistryCreateIterator(IOUSB) failed: 0x%x\n", kr);
        return IO_OBJECT_NULL;
    }

    io_service_t service = IO_OBJECT_NULL;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        int service_vendor = 0;
        int service_product = 0;
        if (get_int_property(service, CFSTR("idVendor"), &service_vendor) &&
            get_int_property(service, CFSTR("idProduct"), &service_product) &&
            service_vendor == vendor && service_product == product) {
            IOObjectRelease(iterator);
            return service;
        }
        IOObjectRelease(service);
    }

    IOObjectRelease(iterator);
    return IO_OBJECT_NULL;
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "usage: %s vendor_hex product_hex [ioreg_path]\n", argv[0]);
        return 64;
    }

    const int vendor = (int)strtol(argv[1], NULL, 0);
    const int product = (int)strtol(argv[2], NULL, 0);
    if (vendor <= 0 || product <= 0 || vendor > 0xffff || product > 0xffff) {
        fprintf(stderr, "invalid vendor/product\n");
        return 64;
    }

    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBHostDevice");
    if (matching == NULL) {
        fprintf(stderr, "IOServiceMatching failed\n");
        return 1;
    }

    io_iterator_t iterator = IO_OBJECT_NULL;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices failed: 0x%x\n", kr);
        return 1;
    }

    io_service_t service = IO_OBJECT_NULL;
    io_service_t candidate = IO_OBJECT_NULL;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        int service_vendor = 0;
        int service_product = 0;
        if (get_int_property(service, CFSTR("idVendor"), &service_vendor) &&
            get_int_property(service, CFSTR("idProduct"), &service_product) &&
            service_vendor == vendor && service_product == product) {
            candidate = service;
            break;
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);
    if (candidate == IO_OBJECT_NULL) {
        fprintf(stderr, "no matching registered USB device vendor=0x%04x product=0x%04x\n", vendor, product);
        candidate = find_usb_plane_device(vendor, product);
    }
    if (candidate == IO_OBJECT_NULL) {
        fprintf(stderr, "no matching IOUSB-plane device vendor=0x%04x product=0x%04x\n", vendor, product);
        if (argc == 4) {
            candidate = IORegistryEntryFromPath(kIOMainPortDefault, argv[3]);
            if (candidate == IO_OBJECT_NULL) {
                fprintf(stderr, "IORegistryEntryFromPath failed: %s\n", argv[3]);
                return 2;
            }
            fprintf(stderr, "using_ioreg_path=%s\n", argv[3]);
        } else {
            return 2;
        }
    }

    IOCFPlugInInterface **plugin = NULL;
    SInt32 score = 0;
    kr = IOCreatePlugInInterfaceForService(candidate, kIOUSBDeviceUserClientTypeID,
                                           kIOCFPlugInInterfaceID, &plugin, &score);
    IOObjectRelease(candidate);
    if (kr != KERN_SUCCESS || plugin == NULL) {
        fprintf(stderr, "IOCreatePlugInInterfaceForService failed: 0x%x\n", kr);
        return 3;
    }

    IOUSBDeviceInterface **device = NULL;
    HRESULT hr = (*plugin)->QueryInterface(plugin,
                                           CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                           (LPVOID *)&device);
    release_plugin(plugin);
    if (hr != S_OK || device == NULL) {
        fprintf(stderr, "QueryInterface(IOUSBDeviceInterface) failed: 0x%x\n", (unsigned)hr);
        return 4;
    }

    kr = (*device)->USBDeviceOpen(device);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "USBDeviceOpen failed: 0x%x\n", kr);
        (*device)->Release(device);
        return 5;
    }

    const char *action = getenv("USB_RESET_ACTION");
    const int reenumerate = action != NULL && strcmp(action, "reenumerate") == 0;
    printf("%s_usb_device=vendor=0x%04x product=0x%04x\n",
           reenumerate ? "reenumerating" : "resetting", vendor, product);
    fflush(stdout);
    kr = reenumerate ? (*device)->USBDeviceReEnumerate(device, 0) : (*device)->ResetDevice(device);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "%s failed: 0x%x\n", reenumerate ? "USBDeviceReEnumerate" : "ResetDevice", kr);
        (void)(*device)->USBDeviceClose(device);
        (*device)->Release(device);
        return 6;
    }

    (void)(*device)->USBDeviceClose(device);
    (*device)->Release(device);
    printf("reset=PASS\n");
    return 0;
}
