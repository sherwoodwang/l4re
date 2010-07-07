/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/sys/kip>
#include <l4/re/env>

#include <cstdio>
#include <cstdlib>

#include "phys_space.h"
#include "cfg.h"

void *operator new (size_t sz, cxx::Nothrow const &) throw()
{ return malloc(sz); }

Phys_space Phys_space::space;

Phys_space::Phys_space()
{
  _set.insert(Phys_region(4 << 22, Phys_region::Addr(~0)));
  //_set.insert(Phys_region(0xd0000000, Phys_region::Addr(~0)));
 // reserve(Phys_region(0xe0000000, 0xf0000000-1));

  L4::Kip::Mem_desc *md = L4::Kip::Mem_desc::first(l4re_kip());
  L4::Kip::Mem_desc *mde = md + L4::Kip::Mem_desc::count(l4re_kip());

  for (; md < mde; ++md)
    {
      if (md->is_virtual())
	continue;

      switch (md->type())
	{
	case L4::Kip::Mem_desc::Arch:
	case L4::Kip::Mem_desc::Conventional:
	case L4::Kip::Mem_desc::Reserved:
	case L4::Kip::Mem_desc::Dedicated:
	case L4::Kip::Mem_desc::Shared:
	case L4::Kip::Mem_desc::Bootloader:
	    {
	      Phys_region re(l4_trunc_page(md->start()),
	                     l4_round_page(md->end())-1);
	      bool r = reserve(re);
              if (Io_config::cfg->verbose())
                {
                  printf("  reserve phys memory space %014lx-%014lx (%s)\n",
                         md->start(), md->end(), r ? "ok" : "failed");
                }
              else
                (void)r;
	    }
	  break;
	default:
	  break;
	}
    }

}

bool
Phys_space::alloc_from(Set::Iterator const &o, Phys_region const &r)
{
  if (r.contains(*o))
    {
      _set.remove(*o);
      return true;
    }

  if (o->start() >= r.start())
    {
      o->set_start(r.end() + 1);
      return true;
    }

  if (o->end() <= r.end())
    {
      o->set_end(r.start() - 1);
      return true;
    }

  Phys_region nr(r.end() + 1, o->end());
  o->set_end(r.start() - 1);
  _set.insert(nr);
  return true;
}

bool
Phys_space::reserve(Phys_region const &r)
{

  Set::Iterator n;
  bool res = false;
  do
    {
      n = _set.find(r);
      if (n == _set.end())
	return res;

      res = true;
    }
  while (alloc_from(n, r));

  return true;
}

bool
Phys_space::alloc(Phys_region const &r)
{

  Set::Iterator n;
  n = _set.find(r);
  if (n == _set.end())
    return false;

  if (n->contains(r))
    {
      reserve(r);
      return true;
    }

  return false;
}

Phys_space::Phys_region
Phys_space::alloc(Phys_region::Addr sz, Phys_region::Addr align)
{
  for (Set::Iterator i = _set.begin(); i != _set.end(); ++i)
    {
      if (i->start() > Phys_region::Addr(0) - sz)
	continue;

      Phys_region r((i->start() + align) & ~align, 0);
      r.set_end(r.start() + sz - 1);

      if (i->contains(r))
	{
	  reserve(r);
	  return r;
	}
    }

  return Phys_region();
}

void
Phys_space::dump()
{
  printf("unused physical memory space:\n");
  for (Set::Iterator i = _set.begin(); i != _set.end(); ++i)
    printf("  [%014lx-%014lx]\n", i->start(), i->end());
}
