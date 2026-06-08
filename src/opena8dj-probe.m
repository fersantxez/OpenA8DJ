#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/usb/AppleUSBDefinitions.h>
#import <IOKit/usb/IOUSBHostFamilyDefinitions.h>
#import <IOUSBHost/AppleUSBDescriptorParsing.h>
#import <IOUSBHost/IOUSBHost.h>
#import <libkern/OSByteOrder.h>
#import <dispatch/dispatch.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef kUSBHostPropertyLinkSpeed
#define kUSBHostPropertyLinkSpeed "kUSBHostPropertyLinkSpeed"
#endif

static const uint16_t kNativeInstrumentsVendorID = 0x17cc;
static const uint16_t kAudio8DJProductID = 0x1978;

static const uint8_t kEndpointControlOut = 0x01;
static const uint8_t kEndpointControlIn = 0x81;
static const uint8_t kEndpointIsoCapture = 0x82;
static const uint8_t kEndpointIsoPlayback = 0x06;
static const uint8_t kCaiaqCommandGetDeviceInfo = 0x01;
static const uint8_t kCaiaqCommandAudioParams = 0x09;
static const uint8_t kInterfaceNumber = 0;
static const uint8_t kConfigurationValue = 1;
static const uint8_t kAlternateSetting = 1;
static const size_t kEp1BufferSize = 64;
static const int kDefaultSampleRate = 48000;
static const int kAudioBitDepth = 24;
static const int kChannelsPerStream = 2;
static const int kClockDriftTolerance = 5;

enum {
    kIsoFramesPerTransfer = 8,
    kIsoBytesPerFrame = 512,
    kDefaultIsoTransfers = 32
};

typedef struct Options {
    bool claim;
    bool seize;
    bool noCommand;
    bool isoTest;
    bool isoCaptureOnly;
    bool showHelp;
    NSTimeInterval timeout;
    int sampleRate;
    NSUInteger isoTransfers;
} Options;

typedef struct CaiaqDeviceSpec {
    uint16_t fwVersion;
    uint8_t hwSubtype;
    uint8_t numErp;
    uint8_t numAnalogIn;
    uint8_t numDigitalIn;
    uint8_t numDigitalOut;
    uint8_t numAnalogAudioOut;
    uint8_t numAnalogAudioIn;
    uint8_t numDigitalAudioOut;
    uint8_t numDigitalAudioIn;
    uint8_t numMidiOut;
    uint8_t numMidiIn;
    uint8_t dataAlignment;
} __attribute__((packed)) CaiaqDeviceSpec;

static uint16_t le16(uint16_t value)
{
    return OSSwapLittleToHostInt16(value);
}

static const char *StringOrEmpty(NSString *string)
{
    return string ? string.UTF8String : "";
}

static NSString *CopyServiceName(io_service_t service)
{
    io_name_t name = {0};
    if (IORegistryEntryGetName(service, name) == KERN_SUCCESS) {
        return [NSString stringWithUTF8String:name];
    }
    return @"<sin nombre>";
}

static NSString *CopyClassName(io_object_t object)
{
    CFStringRef className = IOObjectCopyClass(object);
    if (className == NULL) {
        return @"<clase desconocida>";
    }
    return CFBridgingRelease(className);
}

static id CopyRegistryProperty(io_registry_entry_t entry, const char *key)
{
    CFStringRef cfKey = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    if (cfKey == NULL) {
        return nil;
    }

    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, cfKey, kCFAllocatorDefault, 0);
    CFRelease(cfKey);
    if (value == NULL) {
        return nil;
    }

    return CFBridgingRelease(value);
}

static NSString *FormatData(NSData *data)
{
    const uint8_t *bytes = data.bytes;
    NSMutableString *result = [NSMutableString string];
    for (NSUInteger index = 0; index < data.length; index++) {
        if (index > 0) {
            [result appendString:@" "];
        }
        [result appendFormat:@"%02x", bytes[index]];
    }
    return result;
}

static NSString *FormatRegistryValue(id value)
{
    if ([value isKindOfClass:NSNumber.class]) {
        unsigned long long number = [value unsignedLongLongValue];
        return [NSString stringWithFormat:@"%llu (0x%llx)", number, number];
    }
    if ([value isKindOfClass:NSString.class]) {
        return value;
    }
    if ([value isKindOfClass:NSData.class]) {
        return FormatData(value);
    }
    if ([value isKindOfClass:NSArray.class] || [value isKindOfClass:NSDictionary.class]) {
        return [value description];
    }
    return [NSString stringWithFormat:@"%@", value];
}

static void PrintRegistryProperty(io_registry_entry_t entry, const char *label, const char *key)
{
    id value = CopyRegistryProperty(entry, key);
    if (value != nil) {
        printf("  %-22s %s\n", label, StringOrEmpty(FormatRegistryValue(value)));
    }
}

static void PrintChildServices(io_registry_entry_t entry)
{
    io_iterator_t iterator = IO_OBJECT_NULL;
    kern_return_t kr = IORegistryEntryGetChildIterator(entry, kIOServicePlane, &iterator);
    if (kr != KERN_SUCCESS || iterator == IO_OBJECT_NULL) {
        return;
    }

    bool printedHeader = false;
    io_object_t child = IO_OBJECT_NULL;
    while ((child = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        if (!printedHeader) {
            printf("  Servicios asociados:\n");
            printedHeader = true;
        }

        NSString *name = CopyServiceName(child);
        NSString *className = CopyClassName(child);
        id owner = CopyRegistryProperty(child, kUSBHostPropertyExclusiveOwner);
        id creator = CopyRegistryProperty(child, "IOUserClientCreator");

        printf("    - %s (%s)", StringOrEmpty(name), StringOrEmpty(className));
        if (owner != nil) {
            printf(" owner=%s", StringOrEmpty(FormatRegistryValue(owner)));
        }
        if (creator != nil) {
            printf(" creator=%s", StringOrEmpty(FormatRegistryValue(creator)));
        }
        printf("\n");

        IOObjectRelease(child);
    }

    IOObjectRelease(iterator);
}

static void PrintRegistrySummary(io_service_t service)
{
    printf("Audio 8 DJ encontrada en IORegistry: %s\n", StringOrEmpty(CopyServiceName(service)));
    PrintRegistryProperty(service, "Vendor", kUSBHostDevicePropertyVendorString);
    PrintRegistryProperty(service, "Product", kUSBHostDevicePropertyProductString);
    PrintRegistryProperty(service, "Serial", kUSBHostDevicePropertySerialNumberString);
    PrintRegistryProperty(service, "idVendor", kUSBHostMatchingPropertyVendorID);
    PrintRegistryProperty(service, "idProduct", kUSBHostMatchingPropertyProductID);
    PrintRegistryProperty(service, "bcdDevice", kUSBHostMatchingPropertyDeviceReleaseNumber);
    PrintRegistryProperty(service, "bDeviceClass", kUSBHostMatchingPropertyDeviceClass);
    PrintRegistryProperty(service, "bDeviceSubClass", kUSBHostMatchingPropertyDeviceSubClass);
    PrintRegistryProperty(service, "bDeviceProtocol", kUSBHostMatchingPropertyDeviceProtocol);
    PrintRegistryProperty(service, "USBSpeed", kUSBHostMatchingPropertySpeed);
    PrintRegistryProperty(service, "UsbLinkSpeed", kUSBHostPropertyLinkSpeed);
    PrintRegistryProperty(service, "locationID", kUSBHostPropertyLocationID);
    PrintRegistryProperty(service, "Current config", kUSBHostDevicePropertyCurrentConfiguration);
    PrintRegistryProperty(service, "Exclusive owner", kUSBHostPropertyExclusiveOwner);
    PrintChildServices(service);
}

static CFMutableDictionaryRef CreateDeviceMatchingDictionary(void)
{
    return [IOUSBHostDevice createMatchingDictionaryWithVendorID:@(kNativeInstrumentsVendorID)
                                                       productID:@(kAudio8DJProductID)
                                                       bcdDevice:nil
                                                     deviceClass:nil
                                                  deviceSubclass:nil
                                                  deviceProtocol:nil
                                                           speed:nil
                                                  productIDArray:nil];
}

static CFMutableDictionaryRef CreateInterfaceMatchingDictionary(uint8_t interfaceNumber)
{
    return [IOUSBHostInterface createMatchingDictionaryWithVendorID:@(kNativeInstrumentsVendorID)
                                                          productID:@(kAudio8DJProductID)
                                                          bcdDevice:nil
                                                    interfaceNumber:@(interfaceNumber)
                                                 configurationValue:@(kConfigurationValue)
                                                     interfaceClass:nil
                                                  interfaceSubclass:nil
                                                  interfaceProtocol:nil
                                                              speed:nil
                                                     productIDArray:nil];
}

static void PrintUsage(const char *program)
{
    printf("Uso: %s [opciones]\n", program);
    printf("\n");
    printf("Opciones:\n");
    printf("  --claim              Abre el dispositivo, selecciona config 1 / alt 1 y consulta EP1.\n");
    printf("  --seize              Pide al cliente USB actual que cierre antes de abrirlo.\n");
    printf("  --iso-test           Programa audio y prueba captura/reproduccion isocrona con silencio.\n");
    printf("  --iso-capture-only   Con --iso-test, no envia paquetes de salida por 0x06.\n");
    printf("  --sample-rate hz     Tasa para AUDIO_PARAMS: 44100, 48000, 88200 o 96000. Default: 48000.\n");
    printf("  --iso-transfers n    Numero de transferencias de 8 frames. Default: 32.\n");
    printf("  --no-command         Con --claim, no envia GET_DEVICE_INFO.\n");
    printf("  --timeout segundos   Timeout para transferencias bulk EP1. Default: 1.0.\n");
    printf("  --help               Muestra esta ayuda.\n");
    printf("\n");
    printf("Sin --claim solo enumera la tarjeta, sin tocar el estado USB.\n");
}

static bool ParseOptions(int argc, char **argv, Options *options)
{
    options->claim = false;
    options->seize = false;
    options->noCommand = false;
    options->isoTest = false;
    options->isoCaptureOnly = false;
    options->showHelp = false;
    options->timeout = 1.0;
    options->sampleRate = kDefaultSampleRate;
    options->isoTransfers = kDefaultIsoTransfers;

    for (int index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--claim") == 0) {
            options->claim = true;
        } else if (strcmp(argv[index], "--seize") == 0) {
            options->claim = true;
            options->seize = true;
        } else if (strcmp(argv[index], "--iso-test") == 0) {
            options->claim = true;
            options->isoTest = true;
        } else if (strcmp(argv[index], "--iso-capture-only") == 0) {
            options->isoCaptureOnly = true;
        } else if (strcmp(argv[index], "--sample-rate") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "--sample-rate necesita un valor.\n");
                return false;
            }
            char *end = NULL;
            long parsed = strtol(argv[++index], &end, 10);
            if (end == argv[index] ||
                (parsed != 44100 && parsed != 48000 && parsed != 88200 && parsed != 96000)) {
                fprintf(stderr, "--sample-rate debe ser 44100, 48000, 88200 o 96000.\n");
                return false;
            }
            options->sampleRate = (int)parsed;
        } else if (strcmp(argv[index], "--iso-transfers") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "--iso-transfers necesita un valor.\n");
                return false;
            }
            char *end = NULL;
            long parsed = strtol(argv[++index], &end, 10);
            if (end == argv[index] || parsed < 1 || parsed > 4096) {
                fprintf(stderr, "--iso-transfers debe estar entre 1 y 4096.\n");
                return false;
            }
            options->isoTransfers = (NSUInteger)parsed;
        } else if (strcmp(argv[index], "--no-command") == 0) {
            options->noCommand = true;
        } else if (strcmp(argv[index], "--timeout") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "--timeout necesita un valor en segundos.\n");
                return false;
            }
            char *end = NULL;
            double parsed = strtod(argv[++index], &end);
            if (end == argv[index] || parsed <= 0.0) {
                fprintf(stderr, "--timeout debe ser un numero positivo.\n");
                return false;
            }
            options->timeout = parsed;
        } else if (strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0) {
            options->showHelp = true;
        } else {
            fprintf(stderr, "Opcion desconocida: %s\n", argv[index]);
            return false;
        }
    }

    if (options->isoCaptureOnly && !options->isoTest) {
        fprintf(stderr, "--iso-capture-only solo tiene sentido con --iso-test.\n");
        return false;
    }

    if (options->isoTest && options->noCommand) {
        fprintf(stderr, "--iso-test necesita comandos EP1; no es compatible con --no-command.\n");
        return false;
    }

    return true;
}

static const char *EndpointTypeName(uint8_t attributes)
{
    switch (attributes & kIOUSBEndpointDescriptorTransferType) {
        case kIOUSBEndpointDescriptorTransferTypeControl:
            return "control";
        case kIOUSBEndpointDescriptorTransferTypeIsochronous:
            return "isochronous";
        case kIOUSBEndpointDescriptorTransferTypeBulk:
            return "bulk";
        case kIOUSBEndpointDescriptorTransferTypeInterrupt:
            return "interrupt";
        default:
            return "unknown";
    }
}

static void PrintDeviceDescriptor(const IOUSBDeviceDescriptor *descriptor)
{
    if (descriptor == NULL) {
        printf("Descriptor de dispositivo: no disponible\n");
        return;
    }

    printf("Descriptor de dispositivo:\n");
    printf("  USB                 0x%04x\n", le16(descriptor->bcdUSB));
    printf("  idVendor            0x%04x\n", le16(descriptor->idVendor));
    printf("  idProduct           0x%04x\n", le16(descriptor->idProduct));
    printf("  bcdDevice           0x%04x\n", le16(descriptor->bcdDevice));
    printf("  clase/sub/proto     %u/%u/%u\n",
           descriptor->bDeviceClass,
           descriptor->bDeviceSubClass,
           descriptor->bDeviceProtocol);
    printf("  configuraciones     %u\n", descriptor->bNumConfigurations);
}

static void PrintConfigurationDescriptor(const IOUSBConfigurationDescriptor *configuration)
{
    if (configuration == NULL) {
        printf("Descriptor de configuracion: no disponible\n");
        return;
    }

    printf("Descriptor de configuracion:\n");
    printf("  value               %u\n", configuration->bConfigurationValue);
    printf("  interfaces          %u\n", configuration->bNumInterfaces);
    printf("  totalLength         %u\n", le16(configuration->wTotalLength));
    printf("  attributes          0x%02x\n", configuration->bmAttributes);
    printf("  maxPower            %u mA\n", configuration->MaxPower * 2);

    const IOUSBDescriptorHeader *current = NULL;
    while ((current = IOUSBGetNextDescriptor(configuration, current)) != NULL) {
        if (current->bDescriptorType == kIOUSBDescriptorTypeInterface &&
            current->bLength >= sizeof(IOUSBInterfaceDescriptor)) {
            const IOUSBInterfaceDescriptor *interface = (const IOUSBInterfaceDescriptor *)current;
            printf("  Interface %u alt %u endpoints=%u class=%u/%u/%u\n",
                   interface->bInterfaceNumber,
                   interface->bAlternateSetting,
                   interface->bNumEndpoints,
                   interface->bInterfaceClass,
                   interface->bInterfaceSubClass,
                   interface->bInterfaceProtocol);
        } else if (current->bDescriptorType == kIOUSBDescriptorTypeEndpoint &&
                   current->bLength >= sizeof(IOUSBEndpointDescriptor)) {
            const IOUSBEndpointDescriptor *endpoint = (const IOUSBEndpointDescriptor *)current;
            printf("    Endpoint 0x%02x %-11s maxPacket=%u interval=%u attr=0x%02x\n",
                   endpoint->bEndpointAddress,
                   EndpointTypeName(endpoint->bmAttributes),
                   le16(endpoint->wMaxPacketSize),
                   endpoint->bInterval,
                   endpoint->bmAttributes);
        } else {
            printf("  Descriptor type=%u length=%u\n", current->bDescriptorType, current->bLength);
        }
    }
}

static bool FindBulkEndpointsForInterface(const IOUSBConfigurationDescriptor *configuration,
                                          uint8_t interfaceNumber,
                                          uint8_t alternateSetting,
                                          uint8_t *bulkIn,
                                          uint8_t *bulkOut)
{
    if (configuration == NULL) {
        return false;
    }

    const IOUSBInterfaceDescriptor *interface = NULL;
    while ((interface = IOUSBGetNextInterfaceDescriptor(configuration, (const IOUSBDescriptorHeader *)interface)) != NULL) {
        if (interface->bInterfaceNumber != interfaceNumber ||
            interface->bAlternateSetting != alternateSetting) {
            continue;
        }

        uint8_t foundBulkIn = 0;
        uint8_t foundBulkOut = 0;
        const IOUSBEndpointDescriptor *endpoint = NULL;
        while ((endpoint = IOUSBGetNextEndpointDescriptor(configuration,
                                                          interface,
                                                          (const IOUSBDescriptorHeader *)endpoint)) != NULL) {
            uint8_t type = endpoint->bmAttributes & kIOUSBEndpointDescriptorTransferType;
            if (type != kIOUSBEndpointDescriptorTransferTypeBulk) {
                continue;
            }

            if ((endpoint->bEndpointAddress & kIOUSBEndpointDescriptorDirection) == kIOUSBEndpointDescriptorDirectionIn) {
                foundBulkIn = endpoint->bEndpointAddress;
            } else {
                foundBulkOut = endpoint->bEndpointAddress;
            }
        }

        if (foundBulkIn != 0 && foundBulkOut != 0) {
            *bulkIn = foundBulkIn;
            *bulkOut = foundBulkOut;
            return true;
        }

        return false;
    }

    return false;
}

static io_service_t FindInterfaceServiceWithRetry(uint8_t interfaceNumber, NSTimeInterval timeout)
{
    const useconds_t stepUsec = 100000;
    int attempts = (int)((timeout * 1000000.0) / (double)stepUsec);
    if (attempts < 1) {
        attempts = 1;
    }

    for (int attempt = 0; attempt < attempts; attempt++) {
        CFMutableDictionaryRef matching = CreateInterfaceMatchingDictionary(interfaceNumber);
        if (matching == NULL) {
            return IO_OBJECT_NULL;
        }

        io_iterator_t iterator = IO_OBJECT_NULL;
        kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
        if (kr == KERN_SUCCESS && iterator != IO_OBJECT_NULL) {
            io_service_t service = IOIteratorNext(iterator);
            IOObjectRelease(iterator);
            if (service != IO_OBJECT_NULL) {
                return service;
            }
        } else if (iterator != IO_OBJECT_NULL) {
            IOObjectRelease(iterator);
        }

        usleep(stepUsec);
    }

    return IO_OBJECT_NULL;
}

static void PrintNSError(NSString *prefix, NSError *error)
{
    if (error == nil) {
        printf("%s\n", StringOrEmpty(prefix));
        return;
    }

    printf("%s: %s (domain=%s code=%ld)\n",
           StringOrEmpty(prefix),
           StringOrEmpty(error.localizedDescription),
           StringOrEmpty(error.domain),
           (long)error.code);
}

static const char *IOReturnName(IOReturn status)
{
    switch (status) {
        case kIOReturnSuccess:
            return "success";
        case kIOReturnTimeout:
            return "timeout";
        case kIOReturnNoDevice:
            return "no device";
        case kIOReturnNotOpen:
            return "not open";
        case kIOReturnAborted:
            return "aborted";
        case kIOReturnNotResponding:
            return "not responding";
        case kIOReturnOverrun:
            return "overrun";
        default:
            return "unknown";
    }
}

static void PrintIOReturn(NSString *prefix, IOReturn status)
{
    printf("%s: %s (0x%08x)\n",
           StringOrEmpty(prefix),
           IOReturnName(status),
           status);
}

static void PrintHexBuffer(const uint8_t *bytes, NSUInteger length)
{
    for (NSUInteger index = 0; index < length; index++) {
        if (index > 0) {
            printf(" ");
        }
        printf("%02x", bytes[index]);
    }
    printf("\n");
}

static bool ParseCaiaqSpec(const uint8_t *reply, NSUInteger length, CaiaqDeviceSpec *specOut)
{
    if (length < 1) {
        printf("Respuesta EP1 vacia.\n");
        return false;
    }

    if (reply[0] != kCaiaqCommandGetDeviceInfo) {
        printf("Respuesta inesperada: comando=0x%02x, esperado=0x%02x\n",
               reply[0],
               kCaiaqCommandGetDeviceInfo);
        return false;
    }

    if (length < 1 + sizeof(CaiaqDeviceSpec)) {
        printf("Respuesta demasiado corta para caiaq_device_spec: %lu bytes\n", (unsigned long)length);
        return false;
    }

    memcpy(specOut, reply + 1, sizeof(*specOut));
    specOut->fwVersion = le16(specOut->fwVersion);
    return true;
}

static void PrintCaiaqSpec(const CaiaqDeviceSpec *spec)
{
    printf("caiaq_device_spec:\n");
    printf("  firmware            %u\n", spec->fwVersion);
    printf("  hwSubtype           %u\n", spec->hwSubtype);
    printf("  ERP controls        %u\n", spec->numErp);
    printf("  analog inputs       %u\n", spec->numAnalogIn);
    printf("  digital inputs      %u\n", spec->numDigitalIn);
    printf("  digital outputs     %u\n", spec->numDigitalOut);
    printf("  analog audio out    %u\n", spec->numAnalogAudioOut);
    printf("  analog audio in     %u\n", spec->numAnalogAudioIn);
    printf("  digital audio out   %u\n", spec->numDigitalAudioOut);
    printf("  digital audio in    %u\n", spec->numDigitalAudioIn);
    printf("  MIDI out            %u\n", spec->numMidiOut);
    printf("  MIDI in             %u\n", spec->numMidiIn);
    printf("  dataAlignment       %u\n", spec->dataAlignment);
}

static bool SendCaiaqCommand(IOUSBHostPipe *bulkOutPipe,
                             IOUSBHostPipe *bulkInPipe,
                             uint8_t command,
                             const uint8_t *payload,
                             NSUInteger payloadLength,
                             const char *commandName,
                             NSMutableData **replyOut,
                             NSUInteger *replyLengthOut,
                             NSTimeInterval timeout)
{
    const int maxAttempts = 3;

    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        NSMutableData *outData = [NSMutableData dataWithLength:1 + payloadLength];
        uint8_t *outBytes = outData.mutableBytes;
        outBytes[0] = command;
        if (payload != NULL && payloadLength > 0) {
            memcpy(outBytes + 1, payload, payloadLength);
        }

        NSMutableData *inData = [NSMutableData dataWithLength:kEp1BufferSize];
        dispatch_semaphore_t readDone = dispatch_semaphore_create(0);
        __block IOReturn readStatus = kIOReturnInvalid;
        __block NSUInteger readBytes = 0;

        NSError *error = nil;
        printf("Preparando lectura EP1 IN para %s (intento %d/%d)...\n",
               commandName,
               attempt,
               maxAttempts);
        BOOL queued = [bulkInPipe enqueueIORequestWithData:inData
                                         completionTimeout:timeout
                                                     error:&error
                                         completionHandler:^(IOReturn status, NSUInteger bytesTransferred) {
                                             readStatus = status;
                                             readBytes = bytesTransferred;
                                             dispatch_semaphore_signal(readDone);
                                         }];
        if (!queued) {
            PrintNSError(@"Fallo preparando lectura EP1", error);
            return false;
        }

        NSUInteger bytesTransferred = 0;
        error = nil;
        printf("Enviando %s por EP1 OUT...\n", commandName);
        BOOL sent = [bulkOutPipe sendIORequestWithData:outData
                                      bytesTransferred:&bytesTransferred
                                     completionTimeout:timeout
                                                 error:&error];
        if (!sent) {
            PrintNSError([NSString stringWithFormat:@"Fallo enviando %s", commandName], error);
            [bulkInPipe abortWithError:nil];
            dispatch_semaphore_wait(readDone, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)));
            return false;
        }
        printf("  bytes enviados: %lu\n", (unsigned long)bytesTransferred);

        dispatch_time_t deadline = dispatch_time(DISPATCH_TIME_NOW, (int64_t)((timeout + 0.5) * NSEC_PER_SEC));
        long waitResult = dispatch_semaphore_wait(readDone, deadline);
        if (waitResult != 0) {
            printf("Fallo leyendo respuesta EP1: timeout local esperando completion handler.\n");
            [bulkInPipe abortWithError:nil];
            dispatch_semaphore_wait(readDone, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)));
            continue;
        }

        if (readStatus != kIOReturnSuccess) {
            PrintIOReturn(@"Fallo leyendo respuesta EP1", readStatus);
            [bulkInPipe abortWithError:nil];
            usleep(100000);
            continue;
        }

        printf("  bytes recibidos: %lu\n", (unsigned long)readBytes);
        printf("  raw: ");
        PrintHexBuffer(inData.bytes, readBytes);
        if (replyOut != NULL) {
            *replyOut = inData;
        }
        if (replyLengthOut != NULL) {
            *replyLengthOut = readBytes;
        }
        return true;
    }

    return false;
}

static bool SendGetDeviceInfo(IOUSBHostPipe *bulkOutPipe,
                              IOUSBHostPipe *bulkInPipe,
                              CaiaqDeviceSpec *specOut,
                              NSTimeInterval timeout)
{
    NSMutableData *reply = nil;
    NSUInteger replyLength = 0;
    if (!SendCaiaqCommand(bulkOutPipe,
                          bulkInPipe,
                          kCaiaqCommandGetDeviceInfo,
                          NULL,
                          0,
                          "GET_DEVICE_INFO",
                          &reply,
                          &replyLength,
                          timeout)) {
        return false;
    }

    CaiaqDeviceSpec parsed = {0};
    if (!ParseCaiaqSpec(reply.bytes, replyLength, &parsed)) {
        return false;
    }

    PrintCaiaqSpec(&parsed);
    if (specOut != NULL) {
        *specOut = parsed;
    }
    return true;
}

static int SampleRateCode(int sampleRate)
{
    switch (sampleRate) {
        case 44100:
            return 0;
        case 48000:
            return 1;
        case 96000:
            return 2;
        case 88200:
            return 4;
        default:
            return -1;
    }
}

static uint8_t MaxUInt8(uint8_t a, uint8_t b)
{
    return a > b ? a : b;
}

static int AudioStreamCount(const CaiaqDeviceSpec *spec)
{
    uint8_t inputChannels = MaxUInt8(spec->numAnalogAudioIn, spec->numDigitalAudioIn);
    uint8_t outputChannels = MaxUInt8(spec->numAnalogAudioOut, spec->numDigitalAudioOut);
    int inputStreams = inputChannels / kChannelsPerStream;
    int outputStreams = outputChannels / kChannelsPerStream;
    int streams = inputStreams > outputStreams ? inputStreams : outputStreams;
    return streams > 0 ? streams : 1;
}

static uint16_t CalculateBytesPerPacket(const CaiaqDeviceSpec *spec, int sampleRate)
{
    int bytesPerSample = 3;
    if (spec->dataAlignment >= 2) {
        bytesPerSample++;
    }

    int bpp = ((sampleRate / 8000) + kClockDriftTolerance) *
        bytesPerSample *
        kChannelsPerStream *
        AudioStreamCount(spec);

    if (bpp > (int)kIsoBytesPerFrame) {
        bpp = (int)kIsoBytesPerFrame;
    }
    return (uint16_t)bpp;
}

static bool SendAudioParams(IOUSBHostPipe *bulkOutPipe,
                            IOUSBHostPipe *bulkInPipe,
                            const CaiaqDeviceSpec *spec,
                            int sampleRate,
                            NSTimeInterval timeout)
{
    int sampleRateCode = SampleRateCode(sampleRate);
    if (sampleRateCode < 0) {
        printf("Sample rate no soportada por este prototipo: %d\n", sampleRate);
        return false;
    }

    uint16_t bpp = CalculateBytesPerPacket(spec, sampleRate);
    uint8_t payload[5] = {
        (uint8_t)sampleRateCode,
        2, /* DEPTH_24 */
        (uint8_t)(bpp & 0xff),
        (uint8_t)(bpp >> 8),
        1  /* packets per microframe */
    };

    printf("Programando AUDIO_PARAMS: rate=%d depth=%d bpp=%u streams=%d alignment=%u\n",
           sampleRate,
           kAudioBitDepth,
           bpp,
           AudioStreamCount(spec),
           spec->dataAlignment);

    NSMutableData *reply = nil;
    NSUInteger replyLength = 0;
    if (!SendCaiaqCommand(bulkOutPipe,
                          bulkInPipe,
                          kCaiaqCommandAudioParams,
                          payload,
                          sizeof(payload),
                          "AUDIO_PARAMS",
                          &reply,
                          &replyLength,
                          timeout)) {
        return false;
    }

    const uint8_t *bytes = reply.bytes;
    if (replyLength < 2 || bytes[0] != kCaiaqCommandAudioParams) {
        printf("Respuesta AUDIO_PARAMS inesperada.\n");
        return false;
    }

    printf("AUDIO_PARAMS answer=%u\n", bytes[1]);
    return bytes[1] == 1;
}

static void InitializeIsoTransactions(IOUSBHostIsochronousTransaction *transactions,
                                      NSUInteger count,
                                      const uint32_t *requestCounts)
{
    uint32_t offset = 0;
    for (NSUInteger index = 0; index < count; index++) {
        transactions[index].status = kIOReturnInvalid;
        transactions[index].requestCount = requestCounts[index];
        transactions[index].offset = offset;
        transactions[index].completeCount = 0;
        transactions[index].timeStamp = 0;
        transactions[index].options = IOUSBHostIsochronousTransactionOptionsNone;
        offset += requestCounts[index];
    }
}

static NSUInteger CountNonZeroBytes(NSData *data, NSUInteger length)
{
    const uint8_t *bytes = data.bytes;
    NSUInteger count = 0;
    for (NSUInteger index = 0; index < length && index < data.length; index++) {
        if (bytes[index] != 0) {
            count++;
        }
    }
    return count;
}

static bool RunIsochronousAudioTest(IOUSBHostPipe *capturePipe,
                                    IOUSBHostPipe *playbackPipe,
                                    const Options *options)
{
    uint32_t fullFrameRequests[kIsoFramesPerTransfer];
    for (NSUInteger frame = 0; frame < kIsoFramesPerTransfer; frame++) {
        fullFrameRequests[frame] = (uint32_t)kIsoBytesPerFrame;
    }

    NSUInteger captureTransfersOk = 0;
    NSUInteger captureTransfersFailed = 0;
    NSUInteger playbackTransfersOk = 0;
    NSUInteger playbackTransfersFailed = 0;
    NSUInteger captureFramesOk = 0;
    NSUInteger captureFramesFailed = 0;
    NSUInteger playbackFramesOk = 0;
    NSUInteger playbackFramesFailed = 0;
    NSUInteger captureBytes = 0;
    NSUInteger playbackBytes = 0;
    NSUInteger nonZeroBytes = 0;
    uint32_t minFrameBytes = UINT32_MAX;
    uint32_t maxFrameBytes = 0;

    printf("Arrancando prueba isocrona: transfers=%lu frames/transfer=%lu frameBytes=%lu playback=%s\n",
           (unsigned long)options->isoTransfers,
           (unsigned long)kIsoFramesPerTransfer,
           (unsigned long)kIsoBytesPerFrame,
           options->isoCaptureOnly ? "no" : "silencio");

    for (NSUInteger transfer = 0; transfer < options->isoTransfers; transfer++) {
        NSMutableData *captureData = [NSMutableData dataWithLength:kIsoFramesPerTransfer * kIsoBytesPerFrame];
        IOUSBHostIsochronousTransaction captureTransactions[kIsoFramesPerTransfer];
        InitializeIsoTransactions(captureTransactions, kIsoFramesPerTransfer, fullFrameRequests);

        NSError *error = nil;
        BOOL captureOk = [capturePipe sendIORequestWithData:captureData
                                            transactionList:captureTransactions
                                       transactionListCount:kIsoFramesPerTransfer
                                           firstFrameNumber:0
                                                    options:IOUSBHostIsochronousTransferOptionsNone
                                                      error:&error];
        if (!captureOk) {
            captureTransfersFailed++;
            PrintNSError(@"Fallo en transferencia isocrona IN", error);
            usleep(1000);
            continue;
        }

        captureTransfersOk++;
        uint32_t playbackRequests[kIsoFramesPerTransfer] = {0};
        NSUInteger playbackRequestCount = 0;
        NSUInteger transferBytes = 0;
        NSUInteger failedThisTransfer = 0;
        for (NSUInteger frame = 0; frame < kIsoFramesPerTransfer; frame++) {
            IOReturn status = captureTransactions[frame].status;
            uint32_t completeCount = captureTransactions[frame].completeCount;
            if (status == kIOReturnSuccess) {
                captureFramesOk++;
                if (completeCount > 0) {
                    playbackRequests[playbackRequestCount++] = completeCount;
                }
                transferBytes += completeCount;
                if (completeCount < minFrameBytes) {
                    minFrameBytes = completeCount;
                }
                if (completeCount > maxFrameBytes) {
                    maxFrameBytes = completeCount;
                }
            } else {
                captureFramesFailed++;
                failedThisTransfer++;
                if (failedThisTransfer <= 2) {
                    PrintIOReturn(@"Frame IN con error", status);
                }
            }
        }

        captureBytes += transferBytes;
        nonZeroBytes += CountNonZeroBytes(captureData, transferBytes);

        if (transfer < 6 || ((transfer + 1) % 16) == 0 || transfer + 1 == options->isoTransfers) {
            printf("  IN transfer %lu: bytes=%lu failedFrames=%lu\n",
                   (unsigned long)(transfer + 1),
                   (unsigned long)transferBytes,
                   (unsigned long)failedThisTransfer);
        }

        if (!options->isoCaptureOnly && playbackPipe != nil && transferBytes > 0) {
            NSMutableData *playbackData = [NSMutableData dataWithLength:transferBytes];
            IOUSBHostIsochronousTransaction playbackTransactions[kIsoFramesPerTransfer];
            InitializeIsoTransactions(playbackTransactions, playbackRequestCount, playbackRequests);

            error = nil;
            BOOL playbackOk = [playbackPipe sendIORequestWithData:playbackData
                                                  transactionList:playbackTransactions
                                             transactionListCount:playbackRequestCount
                                                 firstFrameNumber:0
                                                          options:IOUSBHostIsochronousTransferOptionsNone
                                                            error:&error];
            if (!playbackOk) {
                playbackTransfersFailed++;
                PrintNSError(@"Fallo en transferencia isocrona OUT", error);
            } else {
                playbackTransfersOk++;
                for (NSUInteger frame = 0; frame < playbackRequestCount; frame++) {
                    IOReturn status = playbackTransactions[frame].status;
                    if (status == kIOReturnSuccess) {
                        playbackFramesOk++;
                        playbackBytes += playbackTransactions[frame].completeCount;
                    } else {
                        playbackFramesFailed++;
                    }
                }
            }
        }
    }

    printf("Resumen isocrono:\n");
    printf("  IN transfers ok/fail      %lu/%lu\n",
           (unsigned long)captureTransfersOk,
           (unsigned long)captureTransfersFailed);
    printf("  IN frames ok/fail         %lu/%lu\n",
           (unsigned long)captureFramesOk,
           (unsigned long)captureFramesFailed);
    printf("  IN bytes                  %lu\n", (unsigned long)captureBytes);
    if (minFrameBytes != UINT32_MAX) {
        printf("  IN frame bytes min/max    %u/%u\n", minFrameBytes, maxFrameBytes);
    }
    printf("  IN non-zero bytes         %lu\n", (unsigned long)nonZeroBytes);

    if (!options->isoCaptureOnly) {
        printf("  OUT transfers ok/fail     %lu/%lu\n",
               (unsigned long)playbackTransfersOk,
               (unsigned long)playbackTransfersFailed);
        printf("  OUT frames ok/fail        %lu/%lu\n",
               (unsigned long)playbackFramesOk,
               (unsigned long)playbackFramesFailed);
        printf("  OUT bytes                 %lu\n", (unsigned long)playbackBytes);
    }

    return captureTransfersOk > 0 && captureBytes > 0 && captureFramesFailed == 0 &&
        (options->isoCaptureOnly || playbackTransfersFailed == 0);
}

static int ClaimAndProbe(io_service_t service, const Options *options)
{
    dispatch_queue_t queue = dispatch_queue_create("org.opena8dj.probe.usb", DISPATCH_QUEUE_SERIAL);
    IOUSBHostObjectInitOptions initOptions = IOUSBHostObjectInitOptionsNone;
    if (options->seize) {
        initOptions |= IOUSBHostObjectInitOptionsDeviceSeize;
    }

    NSError *error = nil;
    IOUSBHostDevice *device = [[IOUSBHostDevice alloc] initWithIOService:service
                                                                 options:initOptions
                                                                   queue:queue
                                                                   error:&error
                                                         interestHandler:nil];
    if (device == nil) {
        PrintNSError(@"No se pudo abrir IOUSBHostDevice", error);
        printf("Pista: cierra Traktor, NIHardwareAgent y cualquier app que use la Audio 8 DJ. "
               "Luego vuelve a ejecutar con --claim.\n");
        return 10;
    }

    @try {
        PrintDeviceDescriptor(device.deviceDescriptor);

        const IOUSBConfigurationDescriptor *configuration = [device configurationDescriptorWithIndex:0 error:&error];
        if (configuration == NULL) {
            PrintNSError(@"No se pudo leer el descriptor de configuracion 0", error);
        } else {
            PrintConfigurationDescriptor(configuration);
        }

        error = nil;
        printf("Seleccionando configuracion %u...\n", kConfigurationValue);
        if (![device configureWithValue:kConfigurationValue matchInterfaces:YES error:&error]) {
            PrintNSError(@"No se pudo seleccionar configuracion", error);
            [device destroy];
            return 11;
        }

        io_service_t interfaceService = FindInterfaceServiceWithRetry(kInterfaceNumber, 2.0);
        if (interfaceService == IO_OBJECT_NULL) {
            printf("No se encontro IOUSBHostInterface %u tras configurar el dispositivo.\n", kInterfaceNumber);
            [device destroy];
            return 12;
        }

        error = nil;
        IOUSBHostInterface *interface = [[IOUSBHostInterface alloc] initWithIOService:interfaceService
                                                                             options:IOUSBHostObjectInitOptionsNone
                                                                               queue:queue
                                                                               error:&error
                                                                     interestHandler:nil];
        IOObjectRelease(interfaceService);
        if (interface == nil) {
            PrintNSError(@"No se pudo abrir IOUSBHostInterface 0", error);
            [device destroy];
            return 13;
        }

        @try {
            printf("Seleccionando interface %u alternate setting %u...\n",
                   kInterfaceNumber,
                   kAlternateSetting);
            error = nil;
            if (![interface selectAlternateSetting:kAlternateSetting error:&error]) {
                PrintNSError(@"No se pudo seleccionar alternate setting", error);
                [interface destroy];
                [device destroy];
                return 14;
            }

            const IOUSBConfigurationDescriptor *activeConfiguration = interface.configurationDescriptor;
            if (activeConfiguration != NULL) {
                printf("Descriptor activo tras selectAlternateSetting:\n");
                PrintConfigurationDescriptor(activeConfiguration);
            }

            if (options->noCommand) {
                printf("Saltando comandos EP1 por --no-command.\n");
                [interface destroy];
                [device destroy];
                return 0;
            }

            uint8_t bulkInAddress = kEndpointControlIn;
            uint8_t bulkOutAddress = kEndpointControlOut;
            if (FindBulkEndpointsForInterface(activeConfiguration,
                                              kInterfaceNumber,
                                              kAlternateSetting,
                                              &bulkInAddress,
                                              &bulkOutAddress)) {
                printf("Endpoints bulk detectados: OUT=0x%02x IN=0x%02x\n",
                       bulkOutAddress,
                       bulkInAddress);
            } else {
                printf("No se detectaron endpoints bulk en descriptores; usando EP1 esperado OUT=0x%02x IN=0x%02x.\n",
                       bulkOutAddress,
                       bulkInAddress);
            }

            error = nil;
            IOUSBHostPipe *bulkOutPipe = [interface copyPipeWithAddress:bulkOutAddress error:&error];
            if (bulkOutPipe == nil) {
                PrintNSError(@"No se pudo abrir pipe bulk OUT", error);
                [interface destroy];
                [device destroy];
                return 15;
            }

            error = nil;
            IOUSBHostPipe *bulkInPipe = [interface copyPipeWithAddress:bulkInAddress error:&error];
            if (bulkInPipe == nil) {
                PrintNSError(@"No se pudo abrir pipe bulk IN", error);
                [bulkOutPipe abortWithError:nil];
                [interface destroy];
                [device destroy];
                return 16;
            }

            CaiaqDeviceSpec spec = {0};
            bool ok = SendGetDeviceInfo(bulkOutPipe, bulkInPipe, &spec, options->timeout);
            if (ok && options->isoTest) {
                if (!SendAudioParams(bulkOutPipe,
                                     bulkInPipe,
                                     &spec,
                                     options->sampleRate,
                                     options->timeout)) {
                    [bulkOutPipe abortWithError:nil];
                    [bulkInPipe abortWithError:nil];
                    [interface destroy];
                    [device destroy];
                    return 20;
                }

                error = nil;
                IOUSBHostPipe *capturePipe = [interface copyPipeWithAddress:kEndpointIsoCapture error:&error];
                if (capturePipe == nil) {
                    PrintNSError(@"No se pudo abrir pipe isocrono IN 0x82", error);
                    [bulkOutPipe abortWithError:nil];
                    [bulkInPipe abortWithError:nil];
                    [interface destroy];
                    [device destroy];
                    return 21;
                }

                IOUSBHostPipe *playbackPipe = nil;
                if (!options->isoCaptureOnly) {
                    error = nil;
                    playbackPipe = [interface copyPipeWithAddress:kEndpointIsoPlayback error:&error];
                    if (playbackPipe == nil) {
                        PrintNSError(@"No se pudo abrir pipe isocrono OUT 0x06", error);
                        [capturePipe abortWithError:nil];
                        [bulkOutPipe abortWithError:nil];
                        [bulkInPipe abortWithError:nil];
                        [interface destroy];
                        [device destroy];
                        return 22;
                    }
                }

                ok = RunIsochronousAudioTest(capturePipe, playbackPipe, options);
                [capturePipe abortWithError:nil];
                [playbackPipe abortWithError:nil];
            }

            [bulkOutPipe abortWithError:nil];
            [bulkInPipe abortWithError:nil];
            [interface destroy];
            [device destroy];
            return ok ? 0 : (options->isoTest ? 23 : 17);
        } @catch (NSException *exception) {
            printf("Excepcion durante la prueba de interface: %s\n", StringOrEmpty(exception.reason));
            [interface destroy];
            [device destroy];
            return 18;
        }
    } @catch (NSException *exception) {
        printf("Excepcion durante la prueba de dispositivo: %s\n", StringOrEmpty(exception.reason));
        [device destroy];
        return 19;
    }
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        Options options;
        if (!ParseOptions(argc, argv, &options)) {
            PrintUsage(argv[0]);
            return 64;
        }

        if (options.showHelp) {
            PrintUsage(argv[0]);
            return 0;
        }

        CFMutableDictionaryRef matching = CreateDeviceMatchingDictionary();
        if (matching == NULL) {
            fprintf(stderr, "No se pudo crear el matching dictionary para Audio 8 DJ.\n");
            return 70;
        }

        io_iterator_t iterator = IO_OBJECT_NULL;
        kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
        if (kr != KERN_SUCCESS || iterator == IO_OBJECT_NULL) {
            fprintf(stderr, "IOServiceGetMatchingServices fallo: 0x%08x\n", kr);
            return 71;
        }

        io_service_t service = IOIteratorNext(iterator);
        IOObjectRelease(iterator);
        if (service == IO_OBJECT_NULL) {
            printf("No se encontro Audio 8 DJ (%04x:%04x).\n",
                   kNativeInstrumentsVendorID,
                   kAudio8DJProductID);
            return 2;
        }

        PrintRegistrySummary(service);

        int result = 0;
        if (options.claim) {
            if (options.isoTest) {
                printf("\nModo audio: se intentara reclamar la tarjeta, programar AUDIO_PARAMS y probar isocrono.\n");
            } else {
                printf("\nModo activo: se intentara reclamar la tarjeta y consultar el canal EP1.\n");
            }
            result = ClaimAndProbe(service, &options);
        } else {
            printf("\nModo no invasivo completado. Para probar el canal de control:\n");
            printf("  %s --claim\n", argv[0]);
            printf("Para probar transporte isocrono:\n");
            printf("  %s --iso-test\n", argv[0]);
        }

        IOObjectRelease(service);
        return result;
    }
}
