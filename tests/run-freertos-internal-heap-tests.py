"""Exercise actual heap_3 + MM + TLSF on the host, with only IRQ/RTOS stubs.

This validates internal-heap selection and failure handling, not target timing
or a full hardware boot. --fallback=0 also reproduces SDK 2.3.38's restriction.
"""
import argparse
import subprocess
import tempfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--cc', required=True)
p.add_argument('--fallback', type=int, choices=(0, 1), required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
test = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "mm.h"
void *pvPortMalloc(size_t);
void vPortFree(void *);
static unsigned nesting, failures, allocations, releases;
static uintptr_t irq_on=1;
uintptr_t bflb_irq_save(void){uintptr_t old=irq_on;irq_on=0;return old;}
void bflb_irq_restore(uintptr_t old){assert(!irq_on);irq_on=old;}
void vTaskSuspendAll(void){++nesting;}
int xTaskResumeAll(void){assert(nesting);--nesting;return 0;}
void vApplicationMallocFailedHook(void){assert(!nesting);++failures;}
void trace_alloc(void *p,size_t n){(void)p;(void)n;assert(nesting==1);++allocations;}
void trace_free(void *p,size_t n){(void)p;(void)n;assert(nesting==1);++releases;}
static _Alignas(64) unsigned char ocram[117616],wram[32768],psram[32768];
static int inside(void *p,void *base,size_t n){return (uintptr_t)p>=(uintptr_t)base && (uintptr_t)p-(uintptr_t)base<n;}
static void setup(int have_wram){
 mm_heap_unregister_all();assert(mem_manager_init()==0);
 assert(mm_register_heap(MM_HEAP_OCRAM_0,"OCRAM",MM_ALLOCATOR_TLSF,ocram,sizeof(ocram)));
 if(have_wram)assert(mm_register_heap(MM_HEAP_WRAM_0,"WRAM",MM_ALLOCATOR_TLSF,wram,sizeof(wram)));
 assert(mm_register_heap(MM_HEAP_PSRAM_0,"PSRAM",MM_ALLOCATOR_TLSF,psram,sizeof(psram)));
 failures=allocations=releases=0;
}
int main(void){
 setup(1);
 size_t external=kfree_size(MM_HEAP_PSRAM_0);
 void *p=pvPortMalloc(256);assert(p && inside(p,ocram,sizeof(ocram)));
 vPortFree(p);assert(releases==1 && !failures);
 // Occupied/fragmented OCRAM while the second internal pool is still usable.
 void *blocks[200];unsigned n=0;
 while(n<200 && (blocks[n]=kmalloc(1024,MM_FLAG_HEAP_OCRAM_0)))++n;
 assert(n>80 && n<200);
 for(unsigned i=0;i<n;i+=2)kfree(blocks[i]);
 assert(kfree_size(MM_HEAP_OCRAM_0)>8192 && kmax_free_size(MM_HEAP_OCRAM_0)<2048);
 size_t wfree=kfree_size(MM_HEAP_WRAM_0);
 p=pvPortMalloc(2048);
#if EXPECT_FALLBACK
 assert(p && inside(p,wram,sizeof(wram)) && !failures);
 assert(kfree_size(MM_HEAP_WRAM_0)<wfree);
 vPortFree(p);assert(kfree_size(MM_HEAP_WRAM_0)==wfree);
#else
 assert(!p && failures==1 && kfree_size(MM_HEAP_WRAM_0)==wfree);
 p=kmalloc(2048,MM_FLAG_HEAP_WRAM_0);assert(p);kfree(p);
#endif
 assert(kfree_size(MM_HEAP_PSRAM_0)==external);
 for(unsigned i=1;i<n;i+=2)kfree(blocks[i]);
 // Success-path churn and free routing must return both internal heaps intact.
 setup(1);size_t before=kfree_size(0);
 for(unsigned round=0;round<10000;round++){
  p=pvPortMalloc(80+(round%37)*4);assert(p && inside(p,ocram,sizeof(ocram)));
  vPortFree(p);
 }
 assert(kfree_size(0)==before && !failures && allocations==10000 && releases==10000);
 vPortFree(NULL);assert(releases==10000);
 // Missing fallback heap and complete internal exhaustion still fail normally,
 // even with external RAM available; the fatal hook runs once after resume.
 for(unsigned have_wram=0;have_wram<=1;have_wram++){
  setup(have_wram);external=kfree_size(MM_HEAP_PSRAM_0);
  while(kmalloc(512,MM_FLAG_HEAP_OCRAM_0)){}
  while(kmalloc(512,MM_FLAG_HEAP_WRAM_0)){}
  p=pvPortMalloc(2048);assert(!p && failures==1 && !nesting && irq_on);
  assert(kfree_size(MM_HEAP_PSRAM_0)==external);
 }
 printf("PASS actual FreeRTOS/MM/TLSF: fallback=%d, OCRAM fragmentation with usable WRAM, OCRAM priority, 10000 reuse cycles, free routing, no PSRAM fallback, absent/exhausted internal heaps and unchanged fatal-hook ordering\n",EXPECT_FALLBACK);
}
'''
with tempfile.TemporaryDirectory(prefix='freertos-internal-heap-') as directory:
    d = Path(directory)
    (d/'FreeRTOS.h').write_text('''#include <stddef.h>
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configUSE_MALLOC_FAILED_HOOK 1
void vApplicationMallocFailedHook(void);
void trace_alloc(void *,size_t); void trace_free(void *,size_t);
#define traceMALLOC(p,n) trace_alloc(p,n)
#define traceFREE(p,n) trace_free(p,n)
''')
    (d/'task.h').write_text('void vTaskSuspendAll(void); int xTaskResumeAll(void);\n')
    (d/'bflb_core.h').write_text('#include <stdint.h>\nuintptr_t bflb_irq_save(void); void bflb_irq_restore(uintptr_t);\n')
    (d/'bflb_l1c.h').write_text('#define BFLB_CACHE_LINE_SIZE 32\n')
    (d/'log.h').write_text('#define LOG_E(...) ((void)0)\n#define LOG_W(...) ((void)0)\n#define LOG_D(...) ((void)0)\n#define LOG_I(...) ((void)0)\n')
    (d/'config.h').write_text('#define IS_ENABLED(x) (x)\n#define __riscv_xlen 64\n')
    (d/'test.c').write_text(test)
    command=[a.cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
             '-DCONFIG_MM_TLSF_FL_INDEX_MAX=30','-DCONFIG_MM_TLSF_SL_INDEX_COUNT_LOG2=5',
             f'-DCONFIG_FREERTOS_INTERNAL_WRAM_FALLBACK={a.fallback}',f'-DEXPECT_FALLBACK={a.fallback}',
             '-I'+str(d),'-I'+str(root/'components/mm'),'-include',str(d/'config.h'),
             str(d/'test.c'),str(root/'components/os/freertos/portable/MemMang/heap_3.c'),
             str(root/'components/mm/mm.c'),str(root/'components/mm/tlsf/tlsf_allocator.c'),
             str(root/'components/mm/tlsf/tlsf.c'),'-o',str(d/'test.exe')]
    subprocess.run(command,check=True)
    subprocess.run([str(d/'test.exe')],check=True)
