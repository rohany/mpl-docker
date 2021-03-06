/* Copyright (C) 1999-2008 Henry Cejtin, Matthew Fluet, Suresh
 *    Jagannathan, and Stephen Weeks.
 * Copyright (C) 1997-2000 NEC Research Institute.
 *
 * MLton is released under a BSD-style license.
 * See the file MLton-LICENSE for details.
 */

#ifndef _C_CHUNK_H_
#define _C_CHUNK_H_

#include <pthread.h>
#include <stdio.h>

#include "ml-types.h"
#include "c-types.h"
#include "c-common.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef DEBUG_CCODEGEN
#define DEBUG_CCODEGEN FALSE
#endif

#define ExnStack *(size_t*)(GCState + ExnStackOffset)
#define FrontierMem *(Pointer*)(GCState + FrontierOffset)
#define Frontier frontier
#define StackBottom *(Pointer*)(GCState + StackBottomOffset)
#define StackTopMem *(Pointer*)(GCState + StackTopOffset)
#define StackTop stackTop

/* ------------------------------------------------- */
/*                      Memory                       */
/* ------------------------------------------------- */

#define C(ty, x) (*(ty*)(x))
#define G(ty, i) (global##ty [i])
#define GPNR(i) (((Pointer*)(GCState + GlobalObjptrNonRootOffset))[i])
#define O(ty, b, o) (*(ty*)((b) + (o)))
#define X(ty, b, i, s, o) (*(ty*)((b) + ((i) * (s)) + (o)))
#define S(ty, i) *(ty*)(StackTop + (i))

/* ------------------------------------------------- */
/*                       Tests                       */
/* ------------------------------------------------- */

#define IsInt(p) (0x3 & (int)(p))

#define BZ(x, l)                                                        \
        do {                                                            \
                if (DEBUG_CCODEGEN)                                     \
                        fprintf (stderr, "%s:%d: BZ(%d, %s)\n", \
                                        __FILE__, __LINE__, (x), #l);   \
                if (0 == (x)) goto l;                                   \
        } while (0)

#define BNZ(x, l)                                                       \
        do {                                                            \
                if (DEBUG_CCODEGEN)                                     \
                        fprintf (stderr, "%s:%d: BNZ(%d, %s)\n",        \
                                        __FILE__, __LINE__, (x), #l);   \
                if (x) goto l;                                          \
        } while (0)

#define FlushFrontier()                         \
        do {                                    \
                FrontierMem = Frontier;         \
        } while (0)

#define FlushStackTop()                         \
        do {                                    \
                StackTopMem = StackTop;         \
        } while (0)

#define CacheFrontier()                         \
        do {                                    \
                Frontier = FrontierMem;         \
        } while (0)

#define CacheStackTop()                         \
        do {                                    \
                StackTop = StackTopMem;         \
        } while (0)

/* ------------------------------------------------- */
/*                       Chunk                       */
/* ------------------------------------------------- */

/*
 * RAM_NOTE: The 'Pointer GCState...' is how he passes in the correct GC_state
 * regardless of which thread is running the code
 */
#if (defined (__sun__) && defined (REGISTER_FRONTIER_STACKTOP))
#define Chunk(n)                                                \
        DeclareChunk(n) {                                       \
                struct cont cont;                               \
                Pointer GCState = (Pointer)(pthread_getspecific (gcstate_key)); \
                register unsigned int frontier asm("g5");       \
                register unsigned int stackTop asm("g6");
#else
#define Chunk(n)                                \
        DeclareChunk(n) {                       \
                struct cont cont;               \
                Pointer GCState = (Pointer)(pthread_getspecific (gcstate_key)); \
                Pointer frontier;               \
                Pointer stackTop;
#endif

#define ChunkSwitch(n)                                                  \
                if (DEBUG_CCODEGEN)                                     \
                        fprintf (stderr, "%s:%d: entering chunk %d  l_nextFun = %d\n", \
                                        __FILE__, __LINE__, n, (int)l_nextFun); \
                CacheFrontier();                                        \
                CacheStackTop();                                        \
                while (1) {                                             \
                top:                                                    \
                switch (l_nextFun) {

#define EndChunk                                                        \
                default:                                                \
                        /* interchunk return */                         \
                        cont.nextFun = l_nextFun;                       \
                        cont.nextChunk = (void*)nextChunks[l_nextFun];  \
                        leaveChunk:                                     \
                                FlushFrontier();                        \
                                FlushStackTop();                        \
                                return cont;                            \
                } /* end switch (l_nextFun) */                          \
                } /* end while (1) */                                   \
        } /* end chunk */

/* ------------------------------------------------- */
/*                Calling SML from C                 */
/* ------------------------------------------------- */

#define Thread_returnToC()                                              \
        do {                                                            \
                if (DEBUG_CCODEGEN)                                     \
                        fprintf (stderr, "%s:%d: Thread_returnToC()\n", \
                                        __FILE__, __LINE__);            \
                (*(uint32_t*)(GCState + ReturnToCOffset)) = TRUE;       \
                return cont;                                            \
        } while (0)

/* ------------------------------------------------- */
/*                      farJump                      */
/* ------------------------------------------------- */

/* XXX spoons should take cont as arg? */
#define FarJump(n, l)                           \
        do {                                    \
                PrepFarJump(cont, n, l);        \
                goto leaveChunk;                \
        } while (0)

/* ------------------------------------------------- */
/*                       Stack                       */
/* ------------------------------------------------- */

#define Push(bytes)                                                     \
        do {                                                            \
                if (DEBUG_CCODEGEN)                                     \
                        fprintf (stderr, "%s:%d: Push (%d)\n",          \
                                        __FILE__, __LINE__, bytes);     \
                StackTop += (bytes);                                    \
        } while (0)

#define Return()                                                                \
        do {                                                                    \
                l_nextFun = *(uintptr_t*)(StackTop - sizeof(void*));            \
                if (DEBUG_CCODEGEN)                                             \
                        fprintf (stderr, "%s:%d: Return()  l_nextFun = %d\n",   \
                                        __FILE__, __LINE__, (int)l_nextFun);    \
                goto top;                                                       \
        } while (0)

#define Raise()                                                                 \
        do {                                                                    \
                if (DEBUG_CCODEGEN)                                             \
                        fprintf (stderr, "%s:%d: Raise\n",                      \
                                        __FILE__, __LINE__);                    \
                StackTop = StackBottom + ExnStack;                              \
        /* SAM_NOTE: could do a fence here to make the exn bug more likely? */   \
                Return();                                                       \
        } while (0)                                                             \

/* ------------------------------------------------- */
/*                       Primitives                  */
/* ------------------------------------------------- */

#ifndef MLTON_CODEGEN_STATIC_INLINE
#define MLTON_CODEGEN_STATIC_INLINE static inline
#endif
/* Declare inlined math functions, since <math.h> isn't included.
 */
#ifndef MLTON_CODEGEN_MATHFN
#define MLTON_CODEGEN_MATHFN(decl) decl
#endif
/* WordS<N>_quot and WordS<N>_rem can't be inlined with the C-codegen,
 * because the gcc optimizer sometimes produces incorrect results when
 * one of the arguments is a constant.
 */
#ifndef MLTON_CODEGEN_WORDSQUOTREM
#define MLTON_CODEGEN_WORDSQUOTREM(func) PRIVATE
#endif
#ifndef MLTON_CODEGEN_WORDSQUOTREM_IMPL
#define MLTON_CODEGEN_WORDSQUOTREM_IMPL(func)
#endif
/* Declare memcpy, since <string.h> isn't included.
 */
#ifndef MLTON_CODEGEN_MEMCPY
#define MLTON_CODEGEN_MEMCPY(decl)
#endif
MLTON_CODEGEN_MEMCPY(void * memcpy(void *, const void*, size_t);)
#include "basis-ffi.h"
#include "basis/coerce.h"
#include "basis/cpointer.h"
#include "basis/Real/Real-ops.h"
#include "basis/Real/Math-fns.h"
#include "basis/Word/Word-ops.h"
#include "basis/Word/Word-consts.h"
#include "basis/Word/Word-check.h"

/* ------------------------------------------------- */
/*                        Word                       */
/* ------------------------------------------------- */

#define WordS_addCheckCX(size, dst, cW, xW, l)                  \
  do {                                                          \
    WordS##size c = cW;                                         \
    WordS##size x = xW;                                         \
    WordS_addCheckBodyCX(size, c, x, goto l, dst = c + x);      \
  } while (0)
#define WordS8_addCheckCX(dst, c, x, l) WordS_addCheckCX(8, dst, c, x, l)
#define WordS16_addCheckCX(dst, c, x, l) WordS_addCheckCX(16, dst, c, x, l)
#define WordS32_addCheckCX(dst, c, x, l) WordS_addCheckCX(32, dst, c, x, l)
#define WordS64_addCheckCX(dst, c, x, l) WordS_addCheckCX(64, dst, c, x, l)

#define WordS8_addCheckXC(dst, x, c, l) WordS8_addCheckCX(dst, c, x, l)
#define WordS16_addCheckXC(dst, x, c, l) WordS16_addCheckCX(dst, c, x, l)
#define WordS32_addCheckXC(dst, x, c, l) WordS32_addCheckCX(dst, c, x, l)
#define WordS64_addCheckXC(dst, x, c, l) WordS64_addCheckCX(dst, c, x, l)

#define WordS8_addCheck WordS8_addCheckXC
#define WordS16_addCheck WordS16_addCheckXC
#define WordS32_addCheck WordS32_addCheckXC
#define WordS64_addCheck WordS64_addCheckXC


#define WordU_addCheckCX(size, dst, cW, xW, l)                  \
  do {                                                          \
    WordU##size c = cW;                                         \
    WordU##size x = xW;                                         \
    WordU_addCheckBodyCX(size, c, x, goto l, dst = c + x);      \
  } while (0)
#define WordU8_addCheckCX(dst, c, x, l) WordU_addCheckCX(8, dst, c, x, l)
#define WordU16_addCheckCX(dst, c, x, l) WordU_addCheckCX(16, dst, c, x, l)
#define WordU32_addCheckCX(dst, c, x, l) WordU_addCheckCX(32, dst, c, x, l)
#define WordU64_addCheckCX(dst, c, x, l) WordU_addCheckCX(64, dst, c, x, l)

#define WordU8_addCheckXC(dst, x, c, l) WordU8_addCheckCX(dst, c, x, l)
#define WordU16_addCheckXC(dst, x, c, l) WordU16_addCheckCX(dst, c, x, l)
#define WordU32_addCheckXC(dst, x, c, l) WordU32_addCheckCX(dst, c, x, l)
#define WordU64_addCheckXC(dst, x, c, l) WordU64_addCheckCX(dst, c, x, l)

#define WordU8_addCheck WordU8_addCheckXC
#define WordU16_addCheck WordU16_addCheckXC
#define WordU32_addCheck WordU32_addCheckXC
#define WordU64_addCheck WordU64_addCheckXC


#define WordS_negCheck(size, dst, xW, l)                \
  do {                                                  \
    WordS##size x = xW;                                 \
    WordS_negCheckBody(size, x, goto l, dst = -x);      \
  } while (0)
#define Word8_negCheck(dst, x, l) WordS_negCheck(8, dst, x, l)
#define Word16_negCheck(dst, x, l) WordS_negCheck(16, dst, x, l)
#define Word32_negCheck(dst, x, l) WordS_negCheck(32, dst, x, l)
#define Word64_negCheck(dst, x, l) WordS_negCheck(64, dst, x, l)


#define WordS_subCheckCX(size, dst, cW, xW, l)                  \
  do {                                                          \
    WordS##size c = cW;                                         \
    WordS##size x = xW;                                         \
    WordS_subCheckBodyCX(size, c, x, goto l, dst = c - x);      \
  } while (0)
#define WordS8_subCheckCX(dst, c, x, l) WordS_subCheckCX(8, dst, c, x, l)
#define WordS16_subCheckCX(dst, c, x, l) WordS_subCheckCX(16, dst, c, x, l)
#define WordS32_subCheckCX(dst, c, x, l) WordS_subCheckCX(32, dst, c, x, l)
#define WordS64_subCheckCX(dst, c, x, l) WordS_subCheckCX(64, dst, c, x, l)

#define WordS_subCheckXC(size, dst, xW, cW, l)                  \
  do {                                                          \
    WordS##size x = xW;                                         \
    WordS##size c = cW;                                         \
    WordS_subCheckBodyXC(size, x, c, goto l, dst = x - c);      \
  } while (0)
#define WordS8_subCheckXC(dst, x, c, l) WordS_subCheckXC(8, dst, x, c, l)
#define WordS16_subCheckXC(dst, x, c, l) WordS_subCheckXC(16, dst, x, c, l)
#define WordS32_subCheckXC(dst, x, c, l) WordS_subCheckXC(32, dst, x, c, l)
#define WordS64_subCheckXC(dst, x, c, l) WordS_subCheckXC(64, dst, x, c, l)

#define WordS8_subCheck WordS8_subCheckXC
#define WordS16_subCheck WordS16_subCheckXC
#define WordS32_subCheck WordS32_subCheckXC
#define WordS64_subCheck WordS64_subCheckXC


#define WordS_mulCheck(size, dst, xW, yW, l)                    \
  do {                                                          \
    WordS##size x = xW;                                         \
    WordS##size y = yW;                                         \
    WordS_mulCheckBody(size, x, y, goto l, dst = x * y);        \
  } while (0)
#define WordS8_mulCheck(dst, x, y, l) WordS_mulCheck(8, dst, x, y, l)
#define WordS16_mulCheck(dst, x, y, l) WordS_mulCheck(16, dst, x, y, l)
#define WordS32_mulCheck(dst, x, y, l) WordS_mulCheck(32, dst, x, y, l)
#define WordS64_mulCheck(dst, x, y, l) WordS_mulCheck(64, dst, x, y, l)

#define WordU_mulCheck(size, dst, xW, yW, l)                    \
  do {                                                          \
    WordU##size x = xW;                                         \
    WordU##size y = yW;                                         \
    WordU_mulCheckBody(size, x, y, goto l, dst = x * y);        \
  } while (0)
#define WordU8_mulCheck(dst, x, y, l) WordU_mulCheck(8, dst, x, y, l)
#define WordU16_mulCheck(dst, x, y, l) WordU_mulCheck(16, dst, x, y, l)
#define WordU32_mulCheck(dst, x, y, l) WordU_mulCheck(32, dst, x, y, l)
#define WordU64_mulCheck(dst, x, y, l) WordU_mulCheck(64, dst, x, y, l)

/* ------------------------------------------------- */
/*                       Processor                   */
/* ------------------------------------------------- */

extern volatile int32_t Proc_syncCount;
static inline Int32 Proc_threadInSection (void) {
  return Proc_syncCount > 0;
}

/* ------------------------------------------------- */
/*                 References                        */
/* ------------------------------------------------- */

#define IDX(ty, b, i) ((ty*)(b) + (i))

extern CPointer Assignable_findLockedTrueReplicaReader(
  CPointer, Objptr, CPointer *
  );
extern CPointer Assignable_findLockedTrueReplicaWriter(
  CPointer, Objptr, CPointer *
  );

extern int Assignable_isMaster(
    CPointer, Objptr
    );

extern void Assignable_unlockReplicaReader(
  CPointer, CPointer
  );

extern void Assignable_unlockReplicaWriter(
  CPointer, CPointer
  );

extern Objptr Assignable_get(CPointer, Objptr, Int64);
extern void Assignable_set(CPointer, Objptr, Int64, Objptr);

static inline Objptr Ref_deref_P(CPointer s, Objptr src) {
    Objptr res;

    res = *IDX(Objptr, src, 0);
    // assert(Assignable_isMaster(s, src));
    return res;
    // if (Assignable_isMaster(s, src)) {
    //     return res;
    // }

    // return Assignable_get(s, src, 0);
}

static inline void Ref_assign_P(CPointer s, Objptr dst, Objptr src) {
  Assignable_set(s, dst, 0, src);
}

#define RefAccessFunctionsForOpaqueData(suffix, type)                   \
    static inline type Ref_deref_##suffix (CPointer s, Objptr src) {    \
        /*CPointer hh;*/                                                    \
        /*CPointer src_repl;*/                                              \
        type res;                                                       \
                                                                        \
        res = *IDX(type, src, 0);                                       \
        return res;                                                     \
        /*if (Assignable_isMaster(s, src)) {                              \
            return res;                                                 \
        }                                                               \
                                                                        \
        src_repl = Assignable_findLockedTrueReplicaReader(s, src, &hh); \
        res = *IDX(type, src_repl, 0);                                  \
        Assignable_unlockReplicaReader(s, hh);                          \
                                                                        \
        return res;*/                                                     \
    }                                                                   \
                                                                        \
    static inline void Ref_assign_##suffix (CPointer s,                 \
                                            Objptr dst, type src) {     \
        /*CPointer hh;*/                                                    \
        /*CPointer dst_repl;*/                                              \
                                                                        \
        *IDX(type, dst, 0) = src;                                       \
        return;                                                         \
        /*if (Assignable_isMaster(s, dst)) {                              \
            return;                                                     \
        }                                                               \
                                                                        \
        dst_repl = Assignable_findLockedTrueReplicaWriter(s, dst, &hh); \
        *IDX(type, dst_repl, 0) = src;                                  \
        Assignable_unlockReplicaWriter(s, hh);*/                          \
    }

RefAccessFunctionsForOpaqueData(Q,   Pointer)
RefAccessFunctionsForOpaqueData(I8,  Int8_t)
RefAccessFunctionsForOpaqueData(I16, Int16_t)
RefAccessFunctionsForOpaqueData(I32, Int32_t)
RefAccessFunctionsForOpaqueData(I64, Int64_t)
RefAccessFunctionsForOpaqueData(W8,  Word8_t)
RefAccessFunctionsForOpaqueData(W16, Word16_t)
RefAccessFunctionsForOpaqueData(W32, Word32_t)
RefAccessFunctionsForOpaqueData(W64, Word64_t)
RefAccessFunctionsForOpaqueData(R32, Real32_t)
RefAccessFunctionsForOpaqueData(R64, Real64_t)

/* ------------------------------------------------- */
/*                 Arrays                            */
/* ------------------------------------------------- */

static inline Objptr Array_sub_P(CPointer s, Objptr src, Int64 index) {
    Objptr res;

    res = *IDX(Objptr, src, index);
    // assert(Assignable_isMaster(s, src));
    return res;
    // if (Assignable_isMaster(s, src)) {
    //     return res;
    // }

    // return Assignable_get(s, src, index);
}

static inline void Array_update_P(
  CPointer s, Objptr dst, Int64 index, Objptr src) {
  Assignable_set(s, dst, index, src);
}

#define ArrayAccessFunctionsForOpaqueData(suffix, type)                 \
  static inline type Array_sub_##suffix (                               \
    CPointer s, Objptr src, Int64 index) {                              \
        /*CPointer hh;*/                                                    \
        /*CPointer src_repl;*/                                              \
        type res;                                                       \
                                                                        \
        res = *IDX(type, src, index);                                   \
        return res;                                                     \
        /*if (Assignable_isMaster(s, src)) {                              \
            return res;                                                 \
        }                                                               \
                                                                        \
        src_repl = Assignable_findLockedTrueReplicaReader(s, src, &hh); \
        res = *IDX(type, src_repl, index);                              \
        Assignable_unlockReplicaReader(s, hh);                          \
                                                                        \
        return res;*/                                                     \
    }                                                                   \
                                                                        \
  static inline void Array_update_##suffix (CPointer s,                 \
                                            Objptr dst,                 \
                                            Int64 index,                \
                                            type src) {                 \
        /*CPointer hh;*/                                                    \
        /*CPointer dst_repl;*/                                              \
                                                                        \
        *IDX(type, dst, index) = src;                                   \
        return;                                                         \
        /*if (Assignable_isMaster(s, dst)) {                              \
            return;                                                     \
        }                                                               \
                                                                        \
        dst_repl = Assignable_findLockedTrueReplicaWriter(s, dst, &hh); \
        *IDX(type, dst_repl, index) = src;                              \
        Assignable_unlockReplicaWriter(s, hh);*/                          \
    }

ArrayAccessFunctionsForOpaqueData(Q,   Pointer)
ArrayAccessFunctionsForOpaqueData(I8,  Int8_t)
ArrayAccessFunctionsForOpaqueData(I16, Int16_t)
ArrayAccessFunctionsForOpaqueData(I32, Int32_t)
ArrayAccessFunctionsForOpaqueData(I64, Int64_t)
ArrayAccessFunctionsForOpaqueData(W8,  Word8_t)
ArrayAccessFunctionsForOpaqueData(W16, Word16_t)
ArrayAccessFunctionsForOpaqueData(W32, Word32_t)
ArrayAccessFunctionsForOpaqueData(W64, Word64_t)
ArrayAccessFunctionsForOpaqueData(R32, Real32_t)
ArrayAccessFunctionsForOpaqueData(R64, Real64_t)

#endif /* #ifndef _C_CHUNK_H_ */
