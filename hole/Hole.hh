//
// ---------- header ----------------------------------------------------------
//
// project       hole
//
// license       infinit
//
// author        julien quintard   [tue apr 13 15:25:04 2010]
//

#ifndef HOLE_HOLE_HH
#define HOLE_HOLE_HH

//
// ---------- includes --------------------------------------------------------
//

#include <elle/Elle.hh>
#include <nucleus/Nucleus.hh>
#include <lune/Lune.hh>

#include <hole/Holeable.hh>

///
/// this namespace is used by the hole storage abstraction.
///
namespace hole
{

//
// ---------- classes ---------------------------------------------------------
//

  ///
  /// this class represents the abstract Hole component.
  ///
  class Hole
  {
  public:
    //
    // static methods
    //
    static elle::Status		Initialize();
    static elle::Status		Clean();

    static elle::Status		Options();

    static elle::Status		Origin(nucleus::Address&);

    static elle::Status		Push(const nucleus::Address&,
				     const nucleus::Block&);
    static elle::Status		Pull(const nucleus::Address&,
				     const nucleus::Version&,
				     nucleus::Block&);
    static elle::Status		Wipe(const nucleus::Address&);

    //
    // static attributes
    //
    static lune::Descriptor	Descriptor;
    static lune::Passport	Passport;

    static Holeable*		Implementation;
  };

}

//
// ---------- includes --------------------------------------------------------
//

#include <hole/Label.hh>
#include <hole/Model.hh>
#include <hole/Manifest.hh>

#include <hole/implementations/Implementations.hh>

#endif
