//
// ---------- header ----------------------------------------------------------
//
// project       facade
//
// license       infinit
//
// author        julien quintard   [fri jul 31 22:11:24 2009]
//

#ifndef FACADE_UNIX_FUSE_HH
#define FACADE_UNIX_FUSE_HH

//
// ---------- macros ----------------------------------------------------------
//

#ifndef FUSE_USE_VERSION
# define FUSE_USE_VERSION               26
#endif

//
// ---------- includes --------------------------------------------------------
//

#include <Infinit.hh>
#include <elle/Elle.hh>
#include <nucleus/Nucleus.hh>
#include <lune/Lune.hh>

#include <facade/unix/FUker.hh>

#include <elle/idiom/Close.hh>
# include <fuse/fuse.h>
#include <elle/idiom/Open.hh>

namespace facade
{
  namespace unix
  {

//
// ---------- classes ---------------------------------------------------------
//

    ///
    /// this class contains everything related to FUSE.
    ///
    class FUSE
    {
    public:
      //
      // static methods
      //
      static elle::Status       Initialize(const struct ::fuse_operations&);
      static elle::Status       Setup(const elle::String&);
      static elle::Status       Clean();

      //
      // static attributes
      //
      static elle::String               Mountpoint;
      static FUker*                     Fuker;

      static struct ::fuse_operations   Operations;
    };

  }
}

//
// ---------- structures ------------------------------------------------------
//

///
/// this structure is theoretically kept internally within the FUSE source
/// code.
///
/// unfortunately, since facade is using FUSE's low-level functions, this
/// structure is needed to access the session (->se) field.
///
/// therefore this definition gives us just enough information in order
/// to access this field without having to define the rest of it.
///
struct fuse
{
  struct fuse_session *se;

  /* [...] */
};

#endif
