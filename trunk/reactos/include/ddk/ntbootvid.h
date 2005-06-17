#define FILE_DEVICE_BOOTVID 53335

#define IOCTL_BOOTVID_INITIALIZE \
  CTL_CODE(FILE_DEVICE_BOOTVID, 1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BOOTVID_CLEANUP \
  CTL_CODE(FILE_DEVICE_BOOTVID, 2, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct
{
  BOOL (STDCALL *ResetDisplay)(VOID);
} NTBOOTVID_FUNCTION_TABLE;
