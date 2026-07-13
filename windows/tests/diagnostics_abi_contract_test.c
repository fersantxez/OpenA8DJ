#include <windows.h>
#include <stddef.h>
#include <stdio.h>

#include "../include/OpenA8DJShared.h"

C_ASSERT(OPENA8DJ_DRIVER_API_VERSION == 44);
C_ASSERT(sizeof(OPENA8DJ_DIAGNOSTICS) == 816);
C_ASSERT(offsetof(OPENA8DJ_DIAGNOSTICS, IsoCaptureFrameQueries) == 672);
C_ASSERT(offsetof(OPENA8DJ_DIAGNOSTICS, IsoCaptureErrorSnapshotSequence) == 700);
C_ASSERT(offsetof(OPENA8DJ_DIAGNOSTICS, IsoLastCaptureErrorSlot) == 704);
C_ASSERT(offsetof(OPENA8DJ_DIAGNOSTICS, IsoLastCaptureErrorCompletionQpc) == 744);

int main(void)
{
    puts("PASS: OpenA8DJ diagnostics API 44 ABI");
    return 0;
}
