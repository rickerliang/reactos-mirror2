
/* FIXME: Bootvid will eventually become a DLL and this will be deprecated */
#define FILE_DEVICE_BOOTVID 53335

#define IOCTL_BOOTVID_INITIALIZE \
  CTL_CODE(FILE_DEVICE_BOOTVID, 1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BOOTVID_CLEANUP \
  CTL_CODE(FILE_DEVICE_BOOTVID, 2, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct
{
  BOOLEAN (NTAPI *ResetDisplay)(VOID);
} NTBOOTVID_FUNCTION_TABLE;
