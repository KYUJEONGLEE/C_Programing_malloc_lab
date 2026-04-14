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

// size 요청을 header/footer, 정렬조건을 넣어서 새로운 asize를 반환하는 매크로
#define ADJUST_BLOCK_SIZE(size) (MAX(2 * DSIZE, ALIGN((size) + DSIZE)))

// free 블록용 매크로
#define PRED(bp) (*(char **)(bp))
#define SUCC(bp) (*(char **)((char *)(bp) + WSIZE))

// bp 블록의 pred 칸에 ptr 저장
#define SET_PRED(bp, ptr) (PRED(bp) = (ptr))
// bp 블록의 succ 칸에 ptr 저장
#define SET_SUCC(bp, ptr) (SUCC(bp) = (ptr))
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
static char *free_listp = NULL;

static void insert_free_block(char *bp)
{
    // 새 free 블록이 생기면 하는 일 : free list에 연결하기
    // 여기서는 LIFO 방식으로 구현
    // -> 새 free 블록을 free list 맨 앞에 넣는다.

    // free_listp 가 비어있다면, pred/succ 에 null 을 넣어주고 free 리스트 시작점을 bp로
    if (free_listp == NULL)
    {
        SET_PRED(bp, NULL);
        SET_SUCC(bp, NULL);
        free_listp = bp;
    }
    else
    {
        SET_PRED(bp, NULL);
        SET_SUCC(bp, free_listp);
        SET_PRED(free_listp, bp);
        free_listp = bp;
    }
}

static void remove_free_block(char *bp)
{
    if ((PRED(bp) == NULL) && (SUCC(bp) == NULL))
        free_listp = NULL;
    else if ((PRED(bp) == NULL) && (SUCC(bp) != NULL))
    {
        free_listp = SUCC(bp);
        SET_PRED(SUCC(bp), NULL);
    }
    else if ((PRED(bp) != NULL) && (SUCC(bp) == NULL))
    {
        SET_SUCC(PRED(bp), NULL);
    }
    else
    {
        SET_SUCC(PRED(bp), SUCC(bp));
        SET_PRED(SUCC(bp), PRED(bp));
    }
    SET_PRED(bp, NULL);
    SET_SUCC(bp, NULL);
}
static void *coalesce(char *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    {
        insert_free_block(bp);
        return bp;
    }

    // next만 free 인 경우
    else if (prev_alloc && !next_alloc)
    {
        char *next_bp = NEXT_BLKP(bp);
        remove_free_block(next_bp);
        size += GET_SIZE(HDRP(next_bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        insert_free_block(bp);
        return bp;
    }
    else if (!prev_alloc && next_alloc)
    {
        char *prev_bp = PREV_BLKP(bp);
        remove_free_block(prev_bp);
        size += GET_SIZE(HDRP(prev_bp));
        PUT(HDRP(prev_bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        bp = prev_bp;

        insert_free_block(bp);
        return bp;
    }
    else
    {
        char *next_bp = NEXT_BLKP(bp);
        char *prev_bp = PREV_BLKP(bp);

        remove_free_block(prev_bp);
        remove_free_block(next_bp);

        size += GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp));
        PUT(HDRP(prev_bp), PACK(size, 0));
        PUT(FTRP(next_bp), PACK(size, 0));

        bp = prev_bp;

        insert_free_block(bp);
        return bp;
    }
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
    free_listp = NULL;

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
    char *bp = free_listp;

    for (; bp != NULL; bp = SUCC(bp))
    {
        if (GET_SIZE(HDRP(bp)) >= asize)
            return bp;
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    char *next_bp;
    size_t fsize = GET_SIZE(HDRP(bp));
    remove_free_block(bp);
    if (fsize - asize >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        next_bp = NEXT_BLKP(bp);
        PUT(HDRP(next_bp), PACK(fsize - asize, 0));
        PUT(FTRP(next_bp), PACK(fsize - asize, 0));
        insert_free_block(next_bp);
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
    asize = ADJUST_BLOCK_SIZE(size);

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

    // 기존 포인터가 NULL이면 realloc이 아니라 malloc과 동일
    if (bp == NULL)
        return mm_malloc(size);

    // 요청 크기가 0이면 기존 블록을 해제하고 NULL 반환
    if (size == 0)
    {
        mm_free(bp);
        return NULL;
    }

    // 사용자가 요청한 payload 크기를
    // 정렬 + header/footer를 포함한 block 크기(asize)로 변환
    asize = ADJUST_BLOCK_SIZE(size);

    // 현재 블록의 전체 크기(header/footer 포함)
    oldSize = GET_SIZE(HDRP(bp));

    // 1. 현재 블록만으로 이미 충분히 큰 경우
    // 새로 할당하거나 복사할 필요 없이 그대로 반환
    if (oldSize >= asize)
    {
        if (oldSize - asize >= 2 * DSIZE)
        {
            // 앞부분은 realloc 결과 블록
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));

            // 뒤쪽 남는 공간은 free block
            void *next_bp = NEXT_BLKP(bp);
            PUT(HDRP(next_bp), PACK(oldSize - asize, 0));
            PUT(FTRP(next_bp), PACK(oldSize - asize, 0));
        }
        return bp;
    }

    // 다음 블록의 크기 확인
    nextSize = GET_SIZE(HDRP(NEXT_BLKP(bp)));

    // 2. 다음 블록이 free이고,
    // 현재 블록과 합치면 필요한 크기를 만족하는 경우
    // -> 제자리(in-place) 확장 시도
    if ((!GET_ALLOC(HDRP(NEXT_BLKP(bp))) && (oldSize + nextSize >= asize)))
    {
        size_t totalSize = oldSize + nextSize;

        // 합친 뒤 남는 공간이 최소 블록 크기 이상이면 분할
        if (totalSize - asize >= 2 * DSIZE)
        {
            // 앞부분은 realloc된 블록으로 사용
            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));

            // 뒷부분 남는 공간은 새로운 free block으로 생성
            void *next_bp = NEXT_BLKP(bp);
            PUT(HDRP(next_bp), PACK(totalSize - asize, 0));
            PUT(FTRP(next_bp), PACK(totalSize - asize, 0));
        }
        else
        {
            // 남는 공간이 너무 작으면 분할하지 않고 통째로 사용
            PUT(HDRP(bp), PACK(totalSize, 1));
            PUT(FTRP(bp), PACK(totalSize, 1));
        }

        // 복사 없이 현재 포인터 그대로 반환
        return bp;
    }

    if (!GET_SIZE(HDRP(NEXT_BLKP(bp))))
    {
        size_t extend_amount = MAX(asize - oldSize, CHUNKSIZE);

        if (extend_heap(extend_amount / WSIZE) != NULL)
        {
            if (GET_SIZE(HDRP(bp)) >= asize)
            {
                return bp;
            }
        }
    }

    // 3. 제자리 확장이 안 되면 새 블록을 할당
    if ((new_bp = mm_malloc(size)) == NULL)
        return NULL;

    // 기존 데이터는 새 블록으로 복사
    // 복사 크기는 "요청한 크기"와 "기존 payload 크기" 중 작은 값
    copySize = MIN(size, GET_SIZE(HDRP(bp)) - DSIZE);
    memcpy(new_bp, bp, copySize);

    // 기존 블록 해제
    mm_free(bp);

    return new_bp;
}
