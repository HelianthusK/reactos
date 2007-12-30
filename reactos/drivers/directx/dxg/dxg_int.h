
/* DDK/NDK/SDK Headers */
#include <ddk/ntddk.h>
#include <ddk/ntddmou.h>
#include <ddk/ntifs.h>
#include <ddk/tvout.h>
#include <ndk/ntndk.h>

#define WINBASEAPI
#define STARTF_USESIZE 2
#define STARTF_USEPOSITION 4
#define INTERNAL_CALL NTAPI

#include <stdarg.h>
#include <windef.h>
#include <winerror.h>
#include <wingdi.h>
#include <winddi.h>
#include <winuser.h>
#include <prntfont.h>
#include <dde.h>
#include <wincon.h>

#include <reactos/drivers/directx/directxint.h>

#include <reactos/win32k/ntgdityp.h>
#include <reactos/win32k/ntgdihdl.h>

#include <reactos/drivers/directx/dxg.h>
#include <reactos/drivers/directx/dxeng.h>

#include "tags.h"

typedef struct _DD_BASEOBJECT
{
    HANDLE hHmgr;
    ULONG ulShareCount;
    LONG cExclusiveLock;
    PVOID Tid;
} DD_BASEOBJECT, *PDD_BASEOBJECT, *POBJ;

 typedef struct _DD_ENTRY
{
    union
    {
        POBJ pobj;
        HANDLE hFree;
    };
    union
    {
         ULONG ulObj;
         struct
         {
                USHORT Count;
                USHORT Lock;
                HANDLE Pid;
         };
    } ObjectOwner;
    USHORT FullUnique;
    UCHAR Objt;
    UCHAR Flags;
    PVOID pUser;
} DD_ENTRY, *PDD_ENTRY;




/* exported functions */
NTSTATUS DriverEntry(IN PVOID Context1, IN PVOID Context2);
NTSTATUS GsDriverEntry(IN PVOID Context1, IN PVOID Context2);
NTSTATUS DxDdCleanupDxGraphics();

/* Global pointers */
extern ULONG gcSizeDdHmgr;
extern PDD_ENTRY gpentDdHmgr;
extern ULONG gcMaxDdHmgr;
extern PDD_ENTRY gpentDdHmgrLast;
extern HANDLE ghFreeDdHmgr;
extern HSEMAPHORE ghsemHmgr;
extern LONG gcDummyPageRefCnt;
extern HSEMAPHORE ghsemDummyPage;
extern VOID *gpDummyPage;
extern PEPROCESS gpepSession;
extern PLARGE_INTEGER gpLockShortDelay;



/* Driver list export functions */
DWORD STDCALL DxDxgGenericThunk(ULONG_PTR ulIndex, ULONG_PTR ulHandle, SIZE_T *pdwSizeOfPtr1, PVOID pvPtr1, SIZE_T *pdwSizeOfPtr2, PVOID pvPtr2);
DWORD STDCALL DxDdIoctl(ULONG ulIoctl, PVOID pBuffer, ULONG ulBufferSize);
PDD_SURFACE_LOCAL STDCALL DxDdLockDirectDrawSurface(HANDLE hDdSurface);
BOOL STDCALL DxDdUnlockDirectDrawSurface(PDD_SURFACE_LOCAL pSurface);

/* Internel functions */
BOOL FASTCALL VerifyObjectOwner(PDD_ENTRY pEntry);
BOOL FASTCALL DdHmgCreate();
BOOL FASTCALL DdHmgDestroy();
PVOID FASTCALL DdHmgLock( HANDLE DdHandle, UCHAR ObjectType,  BOOLEAN LockOwned);



/* define stuff */
#define drvDxEngLockDC          gpEngFuncs[DXENG_INDEX_DxEngLockDC]
#define drvDxEngGetDCState      gpEngFuncs[DXENG_INDEX_DxEngGetDCState]
#define drvDxEngGetHdevData     gpEngFuncs[DXENG_INDEX_DxEngGetHdevData]
#define drvDxEngUnlockDC        gpEngFuncs[DXENG_INDEX_DxEngUnlockDC]
#define drvDxEngUnlockHdev      gpEngFuncs[DXENG_INDEX_DxEngUnlockHdev]
#define drvDxEngLockHdev        gpEngFuncs[DXENG_INDEX_DxEngLockHdev]

