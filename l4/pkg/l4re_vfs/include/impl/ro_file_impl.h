/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */

#include "ds_util.h"
#include "ro_file.h"

#include <sys/ioctl.h>

#include <l4/re/env>

namespace L4Re { namespace Core {


Simple_store<Ro_file> Ro_file::store;


void *
Ro_file::operator new(size_t s) throw()
{
  if (s != sizeof(Ro_file))
    return 0;

  return store.alloc();
}

void
Ro_file::operator delete(void *b) throw()
{
  store.free(b);
}


Ro_file::~Ro_file() throw()
{
  if (_addr)
    L4Re::Env::env()->rm()->detach(l4_addr_t(_addr), 0);

  release_ds(_ds);
}

int
Ro_file::fstat64(struct stat64 *buf) const throw()
{
  static int fake = 0;

  memset(buf, 0, sizeof(*buf));
  buf->st_size = _size;
  buf->st_mode = S_IFREG | 0644;
  buf->st_dev = _ds.cap();
  buf->st_ino = ++fake;
  buf->st_blksize = L4_PAGESIZE;
  buf->st_blocks = l4_round_page(_size);
  return 0;
}

ssize_t
Ro_file::read(const struct iovec *vec) throw()
{
  off64_t l = vec->iov_len;
  if (_size - _f_pos < l)
    l = _size - _f_pos;

  if (l > 0)
    {
      Vfs_config::memcpy(vec->iov_base, _addr + _f_pos, l);
      _f_pos += l;
      return l;
    }

  return 0;
}

ssize_t
Ro_file::readv(const struct iovec *vec, int cnt) throw()
{
  if (!_addr)
    {
      void const *file = (void*)L4_PAGESIZE;
      long err = L4Re::Env::env()->rm()->attach(&file, _size,
	  Rm::Search_addr | Rm::Read_only, _ds, 0);

      if (err < 0)
	return err;

      _addr = (char const *)file;
    }

  ssize_t l = 0;

  while (cnt > 0)
    {
      ssize_t r = read(vec);
      l += r;

      if ((size_t)r < vec->iov_len)
	return l;

      ++vec;
      --cnt;
    }
  return l;
}

ssize_t
Ro_file::writev(const struct iovec*, int iovcnt) throw()
{
  (void)iovcnt;
  return -EBADF;
}

off64_t
Ro_file::lseek64(off64_t offset, int whence) throw()
{
  off64_t r;
  switch (whence)
    {
    case SEEK_SET: r = offset; break;
    case SEEK_CUR: r = _f_pos + offset; break;
    case SEEK_END: r = _size + offset; break;
    default: return -EINVAL;
    };

  if (r < 0)
    return -EINVAL;

  _f_pos = r;

  return _f_pos;
}

int
Ro_file::ioctl(unsigned long v, va_list args) throw()
{
  switch (v)
    {
    case FIONREAD: // return amount of data still available
      int *available = va_arg(args, int *);
      *available = _size - _f_pos;
      return 0;
    };
  return -EINVAL;
}

}}
