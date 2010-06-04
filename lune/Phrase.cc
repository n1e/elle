//
// ---------- header ----------------------------------------------------------
//
// project       lune
//
// license       infinit
//
// file          /home/mycure/infinit/lune/Phrase.cc
//
// created       julien quintard   [tue may  4 20:49:38 2010]
// updated       julien quintard   [fri may 28 18:15:51 2010]
//

//
// ---------- includes --------------------------------------------------------
//

#include <lune/Phrase.hh>
#include <lune/Lune.hh>

namespace lune
{

//
// ---------- definitions -----------------------------------------------------
//

  ///
  /// this string defines the phrase files extension.
  ///
  const elle::String		Phrase::Extension = ".phr";

//
// ---------- methods ---------------------------------------------------------
//

  ///
  /// this method creates a phrase.
  ///
  elle::Status		Phrase::Create(const elle::String&	string)
  {
    enter();

    // assign the string.
    this->string = string;

    leave();
  }

  ///
  /// this method loads the user's local map.
  ///
  elle::Status		Phrase::Load()
  {
    elle::String	path =
      Lune::User::Home + elle::System::Path::Separator +
      Lune::User::Name + Phrase::Extension;
    elle::Region	region;

    enter();

    // check the mode.
    if (Lune::Environment != Lune::ModeUser)
      escape("unable to manipulate phrase files in this mode");

    // read the file's content.
    if (elle::File::Read(path, region) == elle::StatusError)
      escape("unable to read the file's content");

    // decode and extract the object.
    if (elle::Hexadecimal::Decode(elle::String((char*)region.contents,
					       region.size),
				  *this) == elle::StatusError)
      escape("unable to decode the object");

    leave();
  }

  ///
  /// this method stores the user's local map.
  ///
  elle::Status		Phrase::Store() const
  {
    elle::String	path =
      Lune::User::Home + elle::System::Path::Separator +
      Lune::User::Name + Phrase::Extension;
    elle::Region	region;
    elle::String	string;

    enter();

    // check the mode.
    if (Lune::Environment != Lune::ModeUser)
      escape("unable to manipulate phrase files in this mode");

    // encode in hexadecimal.
    if (elle::Hexadecimal::Encode(*this, string) == elle::StatusError)
      escape("unable to encode the object in hexadecimal");

    // wrap the string.
    if (region.Wrap((elle::Byte*)string.c_str(),
		    string.length()) == elle::StatusError)
      escape("unable to wrap the string in a region");

    // write the file's content.
    if (elle::File::Write(path, region) == elle::StatusError)
      escape("unable to write the file's content");

    leave();
  }

//
// ---------- object ----------------------------------------------------------
//

  ///
  /// this macro-function call generates the object.
  ///
  embed(Phrase, _());

//
// ---------- dumpable --------------------------------------------------------
//

  ///
  /// this method dumps the object.
  ///
  elle::Status		Phrase::Dump(const elle::Natural32	margin) const
  {
    elle::String	alignment(margin, ' ');

    enter();

    std::cout << alignment << "[Phrase] " << this->string << std::endl;

    leave();
  }

//
// ---------- archivable ------------------------------------------------------
//

  ///
  /// this method serializes the object.
  ///
  elle::Status		Phrase::Serialize(elle::Archive&	archive) const
  {
    enter();

    // serialize the attributes.
    if (archive.Serialize(this->string) == elle::StatusError)
      escape("unable to serialize the attributes");

    leave();
  }

  ///
  /// this method extracts the object.
  ///
  elle::Status		Phrase::Extract(elle::Archive&		archive)
  {
    enter();

    // extract the attributes.
    if (archive.Extract(this->string) == elle::StatusError)
      escape("unable to extract the attributes");

    leave();
  }

}
