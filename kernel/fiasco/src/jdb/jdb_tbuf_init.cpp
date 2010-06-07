INTERFACE:

#include "jdb_tbuf.h"

class Jdb_tbuf_init : public Jdb_tbuf
{
public:
  static void init();
};


IMPLEMENTATION:

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <panic.h>

#include "boot_info.h"
#include "cmdline.h"
#include "config.h"
#include "cpu.h"
#include "jdb_ktrace.h"
#include "mem_layout.h"
#include "vmem_alloc.h"
#include "virq.h"

STATIC_INITIALIZE_P(Jdb_tbuf_init, JDB_MODULE_INIT_PRIO);

// init trace buffer
IMPLEMENT FIASCO_INIT
void Jdb_tbuf_init::init()
{
  static Virq tbuf_irq(Config::Tbuf_irq);

  static int init_done;

  if (!init_done)
    {
      init_done = 1;

      const char *c;
      unsigned n;
      unsigned want_entries = Config::tbuf_entries;

      if (  (c = strstr(Cmdline::cmdline(), " -tbuf_entries="))
	  ||(c = strstr(Cmdline::cmdline(), " -tbuf_entries ")))
	want_entries = strtol(c+15, 0, 0);
      
      // minimum: 8KB (  2 pages), maximum: 2MB (512 pages)
      // must be a power of 2 (for performance reasons)
      for (n = Config::PAGE_SIZE/sizeof(Tb_entry);
	   n < want_entries && n*sizeof(Tb_entry)<0x200000;
	   n<<=1)
	;

      if (n < want_entries)
	panic("Cannot allocate more than %d entries for tracebuffer\n", n);

      max_entries(n);
      unsigned size = n*sizeof(Tb_entry);

      if (! Vmem_alloc::page_alloc((void*) status(), Vmem_alloc::ZERO_FILL, Vmem_alloc::User))
	panic("jdb_tbuf: alloc status page at "L4_PTR_FMT" failed", 
	      (Address)Mem_layout::Tbuf_status_page);

      Address va = (Address) buffer();
      for (unsigned i=0; i<size/Config::PAGE_SIZE; i++)
	{
	  if (! Vmem_alloc::page_alloc((void*)va, Vmem_alloc::NO_ZERO_FILL, Vmem_alloc::User))
	    panic("jdb_tbuf: alloc buffer at "L4_PTR_FMT" failed", va);
	  
	  va += Config::PAGE_SIZE;
	}

      status()->tracebuffer0 = (Address)Mem_layout::Tbuf_ubuffer_area;
      status()->tracebuffer1 = (Address)Mem_layout::Tbuf_ubuffer_area + size/2;
      status()->size0        =
      status()->size1        = size / 2;
      status()->version0     =
      status()->version1     = 0;


      status()->scaler_tsc_to_ns = Cpu::boot_cpu()->get_scaler_tsc_to_ns();
      status()->scaler_tsc_to_us = Cpu::boot_cpu()->get_scaler_tsc_to_us();
      status()->scaler_ns_to_tsc = Cpu::boot_cpu()->get_scaler_ns_to_tsc();

      _tbuf_max    = buffer() + max_entries();
      _count_mask1 =  max_entries()    - 1;
      _count_mask2 = (max_entries())/2 - 1;
      _size        = size;

      clear_tbuf();
    }
}
