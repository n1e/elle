#ifndef  ELLE_CRYPTOGRAPHY_DIGESTSERIALIZER_HXX
# define ELLE_CRYPTOGRAPHY_DIGESTSERIALIZER_HXX

# include <cassert>

# include <elle/serialize/ArchiveSerializer.hxx>
# include <elle/standalone/RegionSerializer.hxx>

# include <elle/cryptography/Digest.hh>

ELLE_SERIALIZE_SIMPLE(elle::cryptography::Digest,
                      archive,
                      value,
                      version)
{
  assert(version == 0);
  archive & value.region;
}

#endif
