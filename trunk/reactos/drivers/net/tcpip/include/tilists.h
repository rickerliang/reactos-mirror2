#ifndef _TILISTS_H
#define _TILISTS_H

#define TIPASTE(x,y) x ## y

#define IF_LIST_ITER(n) \
    PLIST_ENTRY TIPASTE(n,Entry); \
    PIP_INTERFACE n;

#define ADE_LIST_ITER(n) \
    PLIST_ENTRY TIPASTE(n,Entry); \
    PADDRESS_ENTRY n;

#define ForEachInterface(n) \
    TIPASTE(n,Entry) = InterfaceListHead.Flink; \
    while (TIPASTE(n,Entry) != &InterfaceListHead) { \
              TI_DbgPrint \
                  (MAX_TRACE,( # n ": %x\n", \
                               TIPASTE(n,Entry))); \
              ASSERT(TIPASTE(n,Entry)); \
	      n = CONTAINING_RECORD(TIPASTE(n,Entry), IP_INTERFACE, \
				    ListEntry); \
	      ASSERT(n);

#define EndFor(n) \
     TI_DbgPrint(MAX_TRACE,("Next " # n " %x\n",  \
			    TIPASTE(n,Entry->Flink))); \
     TIPASTE(n,Entry) = TIPASTE(n,Entry)->Flink; \
}

#define ForEachADE(ADEList,n) \
            TIPASTE(n,Entry) = ADEList.Flink; \
            ASSERT(TIPASTE(n,Entry)); \
            while (TIPASTE(n,Entry) != &ADEList) { \
                    ASSERT(TIPASTE(n,Entry)); \
	            n = CONTAINING_RECORD(TIPASTE(n,Entry), \
                                          ADDRESS_ENTRY, ListEntry); \
                    ASSERT(n);

#endif/*_TILISTS_H*/
