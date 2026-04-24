
/*
 *  POOLMEM.H - Amiga mem pools
 */

APTR __asm AllocVecPooled(register __a0 APTR poolHeader, register __d0 ULONG memSize, register __a6 struct ExecBase *sysBase);
void __asm FreeVecPooled(register __a0 APTR poolHeader, register __a1 APTR memSize, register __a6 struct ExecBase *sysBase);

void * __asm AsmAllocPooled(register __a0 void *poolHeader,
			    register __d0 ULONG memSize,
			    register __a6 struct ExecBase *sysBase);

void __asm AsmFreePooled(register __a0 void *poolHeader,
			 register __a1 void *memory,
			 register __d0 ULONG memSize,
			 register __a6 struct ExecBase *sysBase);

void * __asm AsmCreatePool(register __d0 ULONG memFlags,
			   register __d1 ULONG puddleSize,
			   register __d2 ULONG threshSize,
			   register __a6 struct ExecBase *sysBase);

void __asm AsmDeletePool(register __a0 void *poolHeader,
			 register __a6 struct ExecBase *sysBase);

#define PAllocVec(memSize) AllocVecPooled(MemPool, memSize, SysBase)
#define PFreeVec(memory) FreeVecPooled(MemPool, memory, SysBase)

#if OSVERMIN >= 39
#define PAlloc(memSize) AllocPooled(MemPool, memSize)
#define PFree(memory,memSize) FreePooled(MemPool, memory, memSize)
#define	PCreate(puddleSize,threshSize) CreatePool(MEMF_ANY, puddleSize, threshSize)
#define PDelete() DeletePool(MemPool)
#else
#define PAlloc(memSize) AsmAllocPooled(MemPool, memSize, SysBase)
#define PFree(memory,memSize) AsmFreePooled(MemPool, memory, memSize, SysBase)
#define	PCreate(puddleSize,threshSize) AsmCreatePool(MEMF_ANY, puddleSize, threshSize, SysBase)
#define PDelete() AsmDeletePool(MemPool, SysBase)
#endif
