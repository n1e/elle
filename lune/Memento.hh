//
// ---------- header ----------------------------------------------------------
//
// project       lune
//
// license       infinit
//
// file          /home/mycure/infinit/lune/Memento.hh
//
// created       julien quintard   [sat may  1 21:16:41 2010]
// updated       julien quintard   [fri may 28 17:29:19 2010]
//

#ifndef LUNE_MEMENTO_HH
#define LUNE_MEMENTO_HH

//
// ---------- includes --------------------------------------------------------
//

#include <elle/Elle.hh>

#include <lune/Authority.hh>

#include <etoile/hole/Address.hh>

namespace lune
{

//
// ---------- classes ---------------------------------------------------------
//

  ///
  /// this class represents a universe descriptor.
  ///
  /// note that the universe name is supposed to be unique as it plays the
  /// role of identifier.
  ///
  class Memento:
    public elle::Object
  {
  public:
    //
    // constants
    //
    static const elle::String		Extension;

    //
    // methods
    //
    elle::Status	Create(const elle::String&,
			       const etoile::hole::Address&,
			       const elle::Address&);

    elle::Status	Seal(const Authority&);
    elle::Status	Validate(const Authority&) const;

    elle::Status	Load(const elle::String&);
    elle::Status	Store(const elle::String&) const;

    //
    // interfaces
    //

    // object
    declare(Memento);

    // dumpable
    elle::Status	Dump(const elle::Natural32 = 0) const;

    // archivable
    elle::Status	Serialize(elle::Archive&) const;
    elle::Status	Extract(elle::Archive&);

    //
    // attributes
    //
    elle::String		name;
    etoile::hole::Address	address;
    elle::Address		network;
    elle::Signature		signature;
  };

}

#endif
