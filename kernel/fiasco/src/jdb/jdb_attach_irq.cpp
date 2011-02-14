IMPLEMENTATION:

#include <cstdio>

#include "irq_chip.h"
#include "irq.h"
#include "jdb_module.h"
#include "kernel_console.h"
#include "static_init.h"
#include "thread.h"
#include "types.h"


//===================
// Std JDB modules
//===================

/**
 * 'IRQ' module.
 * 
 * This module handles the 'R' command that
 * provides IRQ attachment and listing functions.
 */
class Jdb_attach_irq : public Jdb_module
{
public:
  Jdb_attach_irq() FIASCO_INIT;
private:
  static char     subcmd;
};

char     Jdb_attach_irq::subcmd;
static Jdb_attach_irq jdb_attach_irq INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

IMPLEMENT
Jdb_attach_irq::Jdb_attach_irq()
  : Jdb_module("INFO")
{}

PUBLIC
Jdb_module::Action_code
Jdb_attach_irq::action( int cmd, void *&args, char const *&, int & )
{
  if (cmd!=0)
    return NOTHING;

  if ((char*)args == &subcmd)
    {
      switch(subcmd) 
	{
	case 'l': // list
  	    {
  	      Irq *r;
	      putchar('\n');
              for (unsigned i = 0; i < Config::Max_num_dirqs; ++i)
		{
		  r = static_cast<Irq*>(Irq_chip::hw_chip->irq(i));
		  if (!r)
		    continue;
  		  printf("IRQ %02x/%02d\n", i, i);
		}
	    }
	  return NOTHING;
	}
    }
  return NOTHING;
}

PUBLIC
int
Jdb_attach_irq::num_cmds() const
{
  return 1;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_attach_irq::cmds() const
{
  static Cmd cs[] =
    {   { 0, "R", "irq", " [l]ist/[a]ttach: %c",
	   "R{l}\tlist IRQ threads", &subcmd }
    };

  return cs;
}


IMPLEMENTATION:

#include "jdb_kobject.h"
#include "irq.h"

class Jdb_kobject_irq : public Jdb_kobject_handler
{
};

#define FIASCO_JDB_CMP_VTABLE(n, o) \
  extern char n[]; \
  char const *const *z = reinterpret_cast<char const* const*>(o); \
  return *z == n + 12 ? (o) : 0


PUBLIC static
Irq_sender *
Jdb_kobject_irq::dcast_h(Irq_sender *i)
{
  FIASCO_JDB_CMP_VTABLE(_ZTV10Irq_sender, i);
}

PUBLIC static
Irq_muxer *
Jdb_kobject_irq::dcast_h(Irq_muxer *i)
{
  FIASCO_JDB_CMP_VTABLE(_ZTV9Irq_muxer, i);
}

PUBLIC template<typename T>
static
T
Jdb_kobject_irq::dcast(Kobject *o)
{
  Irq *i = Kobject::dcast<Irq*>(o);
  if (!i)
    return 0;

  return dcast_h(static_cast<T>(i));
}

PUBLIC inline
Jdb_kobject_irq::Jdb_kobject_irq()
  : Jdb_kobject_handler(Irq::static_kobj_type)
{
  Jdb_kobject::module()->register_handler(this);
}

PUBLIC
char const *
Jdb_kobject_irq::kobject_type() const
{
  return JDB_ANSI_COLOR(white) "IRQ" JDB_ANSI_COLOR(default);
}


PUBLIC
bool
Jdb_kobject_irq::handle_key(Kobject_common *o, int key)
{
  if (key != 'x')
    return false;

  Irq *t = Kobject::dcast<Irq*>(o);

  if (!t)
    return true;

  Irq_sender *i = static_cast<Irq_sender*>(t);

  printf("IRQ: owner = %p\n     cnt = %d\n", i->owner(), i->queued());

  Jdb::getchar();

  return true;
}


PUBLIC
Kobject_common *
Jdb_kobject_irq::follow_link(Kobject_common *o)
{
  Irq_sender *t = Kobject::dcast<Irq_sender*>(o);
  return t ? Kobject::pointer_to_obj(t->owner()) : 0;
}

PUBLIC
bool
Jdb_kobject_irq::show_kobject(Kobject_common *, int)
{ return true; }

PUBLIC
int
Jdb_kobject_irq::show_kobject_short(char *buf, int max, Kobject_common *o)
{
  Irq_sender *t = Kobject::dcast<Irq_sender*>(o);
  Kobject_common *w = follow_link(o);
  return snprintf(buf, max, " I=%3lx %s L=%lx T=%lx F=%x",
                  t->irq(), t->pin()->pin_type(), t->obj_id(),
                  w ? w->dbg_info()->dbg_id() : 0,
		  (unsigned)t->pin()->flags());
}

static
bool
filter_irqs(Kobject_common const *o)
{ return Kobject::dcast<Irq const *>(o); }

static Jdb_kobject_list::Mode INIT_PRIORITY(JDB_MODULE_INIT_PRIO) tnt("[IRQs]", filter_irqs);

static Jdb_kobject_irq jdb_kobject_irq INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
