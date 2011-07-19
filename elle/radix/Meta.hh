//
// ---------- header ----------------------------------------------------------
//
// project       elle
//
// license       infinit
//
// file          /home/mycure/infinit/elle/radix/Meta.hh
//
// created       julien quintard   [sun nov 29 19:31:55 2009]
// updated       julien quintard   [thu jul 14 21:45:15 2011]
//

#ifndef ELLE_RADIX_META_HH
#define ELLE_RADIX_META_HH

//
// ---------- includes --------------------------------------------------------
//

#include <elle/core/Void.hh>
#include <elle/core/Natural.hh>
#include <elle/core/Boolean.hh>

#include <elle/radix/Status.hh>
#include <elle/radix/Trace.hh>

#include <elle/idiom/Close.hh>
# include <list>
# include <stdlib.h>
# include <execinfo.h>
#include <elle/idiom/Open.hh>

namespace elle
{
  using namespace core;

  namespace radix
  {

//
// ---------- classes ---------------------------------------------------------
//

    ///
    /// this class represents the root of the hierarchy. every class
    /// should directly or indirectly derive this class.
    ///
    /// note that the traces are stored in files in order to avoid
    /// maintaining a data structure which would require allocating memory
    ///
    class Meta
    {
    public:
      //
      // constants
      //
      struct				Debug
      {
	static Boolean			Status;
	static Boolean			State;
      };

      //
      // static methods
      //
      static Status	Initialize();
      static Status	Clean();

      static Status	Enable();
      static Status	Disable();

      static Status	Show(const Natural32 = 0);

      //
      // operators
      //
      Void*		operator new(Natural32);
      Void*		operator new(Natural32,
				     Void*);

      Void*		operator new[](Natural32);
      Void*		operator new[](Natural32,
				       Void*);

      Void		operator delete(Void*);
    };

  }
}

#endif
