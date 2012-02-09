//
// ---------- header ----------------------------------------------------------
//
// project       elle
//
// license       infinit
//
// author        julien quintard   [wed jan 28 11:22:24 2009]
//

//
// ---------- includes --------------------------------------------------------
//

#include "Test.hh"

namespace elle
{
  namespace test
  {

//
// ---------- definitions -----------------------------------------------------
//

    const Natural32             Test::MinimumPackSize = 1234;
    const Natural32             Test::MaximumPackSize = 98765;

//
// ---------- functions -------------------------------------------------------
//

    Status              Main()
    {
      Natural32         size;
      Archive           archive;
      Archive           ar;
      Archive           a;

      ;

      // init the library.
      if (Elle::Initialize() == StatusError)
        escape("unable to initialize the Elle library");

      // compute the archive's size.
      size = Random::Generate(Test::MinimumPackSize,
                              Test::MaximumPackSize);

      // prepare the archive.
      if (archive.Create() == StatusError)
        escape("unable to prepare the serialization archive");

      // generate the archive's contents.
      if (Pack::Create(archive,
                       size,
                       true) == StatusError)
        escape("unable to create a pack");

      // test the assignment.
      ar = archive;

      // test the comparison.
      if (archive != ar)
        escape("the two archives should be detected as identical");

      // prepare the archive to be extracted.
      if (a.Wrap(Region(ar.contents, ar.size)) == StatusError)
        escape("unable to prepare the extraction archive");

      // verify the archive.
      if (Pack::Verify(a) == StatusError)
        escape("an error has been detected while verifying the archive");

      // clean the elle library.
      if (Elle::Clean() == StatusError)
        escape("unable to clean the Elle library");

      return elle::StatusOk;
    }

  }
}

//
// ---------- main ------------------------------------------------------------
//

int                     main()
{
  if (elle::test::Main() == elle::radix::StatusError)
    {
      show();

      return (1);
    }

  std::cout << "[success]" << std::endl;

  return (0);
}
