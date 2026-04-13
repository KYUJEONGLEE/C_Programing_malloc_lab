/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "_7team",
    /* First member's full name */
    "Kyu Jeong Lee",
    /* First member's email address */
    "veracr97@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// 상수 매크로
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

// 블록의 header/footer에 값(크기 및 할당 상태)을 다루는 매크로
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// 블록 포인터(bp)를 이용해 블록의 헤더와 푸터의 주소를 계산하는 매크로
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 다음과 이전 블록의 블록 포인터(bp)를 계산하는 매크로
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/*
 * mm_init - initialize the malloc package.
 */

/*
할당기를 처음 켤 때, 딱 1번만 실행되는 초기화 함수
1. 기본 Heap의 틀을 만들기
   - 처음에 4워드(16 byte)를 확보한다.
       - 왜? 정렬용 패딩, 프롤로그 header, footer, 에필로그 header
   - 즉 아주 작은 "초기 Heap 뼈대" 를 만든다.
2. 실제 가용 공간 만들기
   - extend_heap(CHUNKSIZE)를 호출해서 실제로 사용 할 수 있는 큰 free 블록을 만든다.
   - extend_heap(CHUNKSIZE)?
       - Heap 이 부족할 때 Heap의 크기를 늘리는 함수
       - 1단계 : mem_sbrk()로 Heap을 키운다
       - 2단계 : 새로 얻는 공간을 하나의 큰 free 블록으로 설정한다.
       - 3단계 : 맨 끝에 새 에필로그 header 를 설치한다.
       - 4단계 : 바로 앞 블록도 free 상태하면 coalesce() (병합) 한다.
*/
static char *heap_listp = NULL;

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    {
        return bp;
    }

    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        // 호출 순서가 중요하다.

        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    bp = mem_sbrk(size);

    if (bp == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    heap_listp += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

static void *find_fit(size_t asize)
{
    char *bp = heap_listp;

    while (GET_SIZE(HDRP(bp)) > 0)
    {
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= asize))
            return bp;

        bp = NEXT_BLKP(bp);
    }

    return NULL;
}

static void place(void *bp, size_t asize)
{
    char *next_bp;
    size_t fsize = GET_SIZE(HDRP(bp));

    if (fsize - asize >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        next_bp = NEXT_BLKP(bp);
        PUT(HDRP(next_bp), PACK(fsize - asize, 0));
        PUT(FTRP(next_bp), PACK(fsize - asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(fsize, 1));
        PUT(FTRP(bp), PACK(fsize, 1));
    }

    return;
}
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    // 예외 처리
    if (size == 0)
        return NULL;

    // 새로운 블록 asize 계산 완료
    if (size < DSIZE)
    {
        asize = (2 * DSIZE);
    }
    else
    {
        asize = DSIZE * ((DSIZE + (DSIZE - 1) + size) / DSIZE);
    }

    // free 블록 탐색, find_fit(asize) 사용
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    // 맞는 free 공간을 찾지 못했다면 Heap을 늘려준다.
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    // 새롭게 늘린 Heap 공간에 place 해준다.
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    char *new_bp;
    size_t asize;
    size_t oldSize;
    size_t copySize;
    size_t nextSize;

    if (bp == NULL)
        return mm_malloc(size);

    if (size == 0)
    {
        mm_free(bp);
        return NULL;
    }

    if (size <= DSIZE)
    {
        asize = (2 * DSIZE);
    }
    else
    {
        asize = DSIZE * ((DSIZE + (DSIZE - 1) + size) / DSIZE);
    }

    oldSize = GET_SIZE(HDRP(bp));
    if (oldSize >= asize)
        return bp;

    nextSize = GET_SIZE(HDRP(NEXT_BLKP(bp)));

    if ((!GET_ALLOC(HDRP(NEXT_BLKP(bp))) && (oldSize + nextSize >= asize)))
    {
        size_t totalSize = oldSize + nextSize;

        if (totalSize - asize >= 2 * DSIZE)
        {
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));

            void *next_bp = NEXT_BLKP(bp);
            PUT(HDRP(next_bp), PACK(totalSize - asize, 0));
            PUT(FTRP(next_bp), PACK(totalSize - asize, 0));
        }
        else
        {
            PUT(HDRP(bp), PACK(totalSize, 1));
            PUT(FTRP(bp), PACK(totalSize, 1));
        }

        return bp;
    }

    if ((new_bp = mm_malloc(size)) == NULL)
        return NULL;

    // 이 다음 코드부터는 new_bp 에는 size로 설정된, 새로운 블록의 bp가 저장된다.
    // 이제 기존의 데이터를 옮겨주는 작업이 필요하다.

    copySize = MIN(size, GET_SIZE(HDRP(bp)) - DSIZE);
    memcpy(new_bp, bp, copySize);
    mm_free(bp);

    return new_bp;
}