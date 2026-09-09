/**
  ******************************************************************************
  * @file    Core/Src/sysmem.c
  * @brief   Heap 实现（_sbrk），与 STM32H743VITX_FLASH.ld 的 end/_end 符号配套
  ******************************************************************************
  */

#include <errno.h>
#include <stddef.h>

void *_sbrk(ptrdiff_t incr)
{
  extern char end asm("end");
  static char *heap_end;
  char *prev_heap_end;

  if (heap_end == 0)
  {
    heap_end = &end;
  }
  prev_heap_end = heap_end;

  heap_end += incr;

  return (void *)prev_heap_end;
}
