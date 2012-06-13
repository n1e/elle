#include <elle/log.hh>


#include <horizon/linux/Crux.hh>
#include <horizon/linux/Linux.hh>
#include <horizon/linux/Janitor.hh>
#include <horizon/linux/Handle.hh>
#include <horizon/linux/Crib.hh>

#include <agent/Agent.hh>
#include <etoile/Etoile.hh>

ELLE_LOG_TRACE_COMPONENT("Infinit.Horizon")

namespace horizon
{
  namespace linux
  {
    /// This macro-function makes it easier to return an error from an
    /// upcall, taking care to log the error but also to release the
    /// remaining identifiers.
#define error(_text_, _errno_, _identifiers_...)                        \
  do                                                                    \
    {                                                                   \
      log(_text_);                                                      \
                                                                        \
      Janitor::Clear(_identifiers_);                                    \
                                                                        \
      return ((_errno_));                                               \
    } while (false)

    /// The number of directory entries to fetch from etoile when
    /// performing a Readdir().
    const nucleus::Size                 Crux::Range = 128;

    /// General-purpose information on the file system object
    /// identified by _path_.
    int                 Crux::Getattr(const char*               path,
                                      struct ::stat*            stat)
    {
      etoile::gear::Identifier  identifier;
      etoile::path::Way         way(path);
      etoile::path::Chemin      chemin;
      struct ::fuse_file_info   info;
      int                       result;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, stat);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        {
          // Purge the error messages since it may be normal not to be
          // able to resolve the given way.
          purge();

          return (-ENOENT);
        }

      // Load the object.
      if (etoile::wall::Object::Load(chemin, identifier) == elle::Status::Error)
        error("unable to load the object",
              -ENOENT);

      // Create a local handle.
      Handle                    handle(Handle::OperationGetattr,
                                       identifier);

      // Set the handle in the fuse_file_info structure.  Be careful,
      // the address is local but it is alright since it is used in
      // Fgetattr() only.
      info.fh = reinterpret_cast<uint64_t>(&handle);

      // Call Fgetattr().
      if ((result = Crux::Fgetattr(path, stat, &info)) < 0)
        error("unable to get information on the given file descriptor",
              result,
              identifier);

      // Discard the object.
      if (etoile::wall::Object::Discard(identifier) == elle::Status::Error)
        error("unable to discard the object",
              -EPERM);

      // debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, stat);

      return (0);
    }

    /// General-purpose information on the file system object
    /// identified by _path_.
    int                 Crux::Fgetattr(const char*              path,
                                       struct ::stat*           stat,
                                       struct ::fuse_file_info* info)
    {
      Handle*                           handle;
      etoile::miscellaneous::Abstract   abstract;
      elle::String*                     name;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, stat);

      // Clear the stat structure.
      ::memset(stat, 0x0, sizeof (struct ::stat));

      // Retrieve the handle.
      handle = reinterpret_cast<Handle*>(info->fh);

      // Retrieve information on the object.
      if (etoile::wall::Object::Information(handle->identifier,
                                            abstract) == elle::Status::Error)
        error("unable to retrieve information on the object",
              -EPERM);

      // Set the uid by first looking into the users map. if no local
      // user is found, the 'somebody' user is used instead,
      // indicating that the file belongs to someone, with the given
      // permissions, but cannot be mapped to a local user name.
      if (Linux::Dictionary.users.Lookup(abstract.keys.owner,
                                        name) == elle::Status::True)
        {
          // In this case, the object's owner is known locally.
          struct ::passwd*      passwd;

          // Retrieve the passwd structure associated with this name.
          if ((passwd = ::getpwnam(name->c_str())) != NULL)
            {
              // Set the uid to the local user's.
              stat->st_uid = passwd->pw_uid;
            }
          else
            {
              // If an error occured, set the user to 'somebody'.
              stat->st_uid = Linux::Somebody::UID;
            }
        }
      else
        {
          // Otherwise, this user is unknown locally, so indicate the
          // system that this object belongs to the 'somebody' user.
          stat->st_uid = Linux::Somebody::UID;
        }

      // Since Infinit does not have the concept of current group, the
      // group of this object is set to 'somebody'.
      stat->st_gid = Linux::Somebody::GID;

      // Set the size.
      stat->st_size = static_cast<off_t>(abstract.size);

      // Set the disk usage by assuming the smallest disk unit is 512
      // bytes.  Note however the the optimised size of I/Os is set to
      // 4096.
      stat->st_blksize = 4096;
      stat->st_blocks =
        (stat->st_size / 512) +
        (stat->st_size % 512) > 0 ? 1 : 0;

      // Set the number of hard links to 1 since no hard link exist
      // but the original object.
      stat->st_nlink = 1;

      // Convert the times into time_t structures.
      stat->st_atime = time(NULL);

      if (abstract.stamps.creation.Get(stat->st_ctime) ==
          elle::Status::Error)
        error("unable to convert the time stamps",
              -EPERM);

      if (abstract.stamps.modification.Get(stat->st_mtime) ==
          elle::Status::Error)
        error("unable to convert the time stamps",
              -EPERM);

      // Set the mode and permissions.
      switch (abstract.genre)
        {
        case nucleus::GenreDirectory:
          {
            // Set the object as being a directory.
            stat->st_mode = S_IFDIR;

            // If the user has the read permission, allow her to
            // access and read the directory.
            if ((abstract.permissions.owner & nucleus::PermissionRead) ==
                nucleus::PermissionRead)
              stat->st_mode |= S_IRUSR | S_IXUSR;

            // If the user has the write permission, allow her to
            // modify the directory content.
            if ((abstract.permissions.owner & nucleus::PermissionWrite) ==
                nucleus::PermissionWrite)
              stat->st_mode |= S_IWUSR;

            break;
          }
        case nucleus::GenreFile:
          {
            nucleus::Trait*     trait;

            stat->st_mode = S_IFREG;

            // If the user has the read permission, allow her to read
            // the file.
            if ((abstract.permissions.owner & nucleus::PermissionRead) ==
                nucleus::PermissionRead)
              stat->st_mode |= S_IRUSR;

            // If the user has the write permission, allow her to
            // modify the file content.
            if ((abstract.permissions.owner & nucleus::PermissionWrite) ==
                nucleus::PermissionWrite)
              stat->st_mode |= S_IWUSR;

            // Retrieve the attribute.
            if (etoile::wall::Attributes::Get(handle->identifier,
                                              "perm::exec",
                                              trait) == elle::Status::Error)
              error("unable to retrieve an attribute",
                    -EPERM);

            // Check the trait.
            if ((trait != NULL) &&
                (trait->value == "true"))
              {
                // Active the exec bit.
                stat->st_mode |= S_IXUSR;
              }

            break;
          }
        case nucleus::GenreLink:
          {
            stat->st_mode = S_IFLNK;

            // If the user has the read permission, allow her to read
            // and search the linked object.
            if ((abstract.permissions.owner & nucleus::PermissionRead) ==
                nucleus::PermissionRead)
              stat->st_mode |= S_IRUSR | S_IXUSR;

            // If the user has the write permission, allow her to
            // modify the link.
            if ((abstract.permissions.owner & nucleus::PermissionWrite) ==
                nucleus::PermissionWrite)
              stat->st_mode |= S_IWUSR;

            break;
          }
        default:
          {
            error("unknown genre",
                  -EPERM);
          }
        }

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, stat);

      return (0);
    }

    /// This method changes the access and modification time of the
    /// object.
    int                 Crux::Utimens(const char*               path,
                                      const struct ::timespec[2])
    {
      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, ...)\n",
               __FUNCTION__,
               path);

      // Xxx not supported: do something about it

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, ...)\n",
               __FUNCTION__,
               path);

      return (0);
    }

    /// This method opens the directory _path_.
    int                 Crux::Opendir(const char*               path,
                                      struct ::fuse_file_info*  info)
    {
      etoile::gear::Identifier  identifier;
      etoile::path::Way         way(path);
      etoile::path::Chemin      chemin;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, info);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the directory.
      if (etoile::wall::Directory::Load(chemin,
                                        identifier) == elle::Status::Error)
        error("unable to load the directory",
              -ENOENT);

      // Duplicate the identifier and save it in the info structure's
      // file handle.
      info->fh =
        reinterpret_cast<uint64_t>(new Handle(Handle::OperationOpendir,
                                              identifier));

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, info);

      return (0);
    }

    /// This method reads the directory entries.
    int                 Crux::Readdir(const char*               path,
                                      void*                     buffer,
                                      ::fuse_fill_dir_t         filler,
                                      off_t                     offset,
                                      struct ::fuse_file_info*  info)
    {
      Handle*           handle;
      off_t             next;
      nucleus::Record*  record;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %p, %p, %qu, %p)\n",
               __FUNCTION__,
               path, buffer, filler,
               static_cast<elle::Natural64>(offset), info);

      // Set the handle pointer to the file handle that has been
      // filled by Opendir().
      handle = reinterpret_cast<Handle*>(info->fh);

      // Retrieve the subject's permissions on the object.
      if (etoile::wall::Access::Lookup(handle->identifier,
                                       agent::Agent::Subject,
                                       record) == elle::Status::Error)
        error("unable to retrieve the access record",
              -EPERM);

      // Check the record.
      if (!((record != NULL) &&
            ((record->permissions & nucleus::PermissionRead) ==
             nucleus::PermissionRead)))
        error("the subject does not have the right to read the "
              "directory entries",
              -EACCES);

      // Fill the . and .. entries.
      if (offset == 0)
        filler(buffer, ".", NULL, 1);
      if (offset <= 1)
        filler(buffer, "..", NULL, 2);

      // Compute the offset of the next entry.
      if (offset < 2)
        next = 3;
      else
        next = offset + 1;

      // Adjust the offset since Etoile starts with zero while in
      // POSIX terms, zero and one are used for '.' and '..'.
      if (offset > 2)
        offset -= 2;

      while (true)
        {
          nucleus::Range<nucleus::Entry>                range;
          nucleus::Range<nucleus::Entry>::Scoutor       scoutor;

          // Read the directory entries.
          if (etoile::wall::Directory::Consult(
                handle->identifier,
                static_cast<nucleus::Index>(offset),
                Crux::Range,
                range) == elle::Status::Error)
            error("unable to retrieve some directory entries",
                  -EPERM);

          // Add the entries by using the filler() function.
          for (scoutor = range.container.begin();
               scoutor != range.container.end();
               scoutor++)
            {
              nucleus::Entry*   entry = *scoutor;

              // Fill the buffer with filler().
              if (filler(buffer, entry->name.c_str(), NULL, next) == 1)
                {
                  // Debug.
                  if (Infinit::Configuration.horizon.debug == true)
                    printf("[horizon] /Crux::%s(%s, %p, %p, %qu, %p)\n",
                           __FUNCTION__,
                           path, buffer, filler,
                           static_cast<elle::Natural64>(offset), info);

                  return (0);
                }

              // Compute the offset of the next entry.
              next++;

              // Increment the offset as well.
              offset++;
            }

          if (range.container.size() < Crux::Range)
            break;
        }

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %p, %p, %qu, %p)\n",
               __FUNCTION__,
               path, buffer, filler,
               static_cast<elle::Natural64>(offset), info);

      return (0);
    }

    /// This method closes the directory _path_.
    int                 Crux::Releasedir(const char*            path,
                                         struct ::fuse_file_info* info)
    {
      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, info);

      // Set the handle pointer to the file handle that has been
      // filled by Opendir().
      std::unique_ptr<Handle> handle(reinterpret_cast<Handle*>(info->fh));

      // Discard the object.
      if (etoile::wall::Directory::Discard(
            handle->identifier) == elle::Status::Error)
        error("unable to discard the directory",
              -EPERM);

      // Reset the file handle, just to make sure it is not used
      // anymore.
      info->fh = 0;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, info);

      return (0);
    }

    /// This method creates a directory.
    int                 Crux::Mkdir(const char*                 path,
                                    mode_t                      mode)
    {
      nucleus::Permissions      permissions = nucleus::PermissionNone;
      etoile::path::Slab        name;
      etoile::path::Way         way(etoile::path::Way(path), name);
      etoile::path::Chemin      chemin;
      etoile::gear::Identifier  directory;
      etoile::gear::Identifier  subdirectory;
      nucleus::Record*          record;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, 0%o)\n",
               __FUNCTION__,
               path, mode);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the directory.
      if (etoile::wall::Directory::Load(chemin,
                                        directory) == elle::Status::Error)
        error("unable to load the directory",
              -ENOENT);

      // Retrieve the subject's permissions on the object.
      if (etoile::wall::Access::Lookup(directory,
                                       agent::Agent::Subject,
                                       record) == elle::Status::Error)
        error("unable to retrieve the access record",
              -EPERM,
              directory);

      // Check the record.
      if (!((record != NULL) &&
            ((record->permissions & nucleus::PermissionWrite) ==
             nucleus::PermissionWrite)))
        error("the subject does not have the right to create a "
              "subdirectory in this directory",
              -EACCES,
              directory);

      // Create the subdirectory.
      if (etoile::wall::Directory::Create(subdirectory) == elle::Status::Error)
        error("unable to create the directory",
              -EPERM,
              directory);

      // Compute the permissions.
      if (mode & S_IRUSR)
        permissions |= nucleus::PermissionRead;

      if (mode & S_IWUSR)
        permissions |= nucleus::PermissionWrite;

      // Set the owner permissions.
      if (etoile::wall::Access::Grant(subdirectory,
                                      agent::Agent::Subject,
                                      permissions) == elle::Status::Error)
        error("unable to update the access record",
              -EPERM,
              subdirectory, directory);

      // Add the subdirectory.
      if (etoile::wall::Directory::Add(directory,
                                       name,
                                       subdirectory) == elle::Status::Error)
        error("unable to add an entry to the parent directory",
              -EPERM,
              subdirectory, directory);

      // Store the subdirectory.
      if (etoile::wall::Directory::Store(subdirectory) == elle::Status::Error)
        error("unable to store the directory",
              -EPERM,
              directory);

      // Store the directory.
      if (etoile::wall::Directory::Store(directory) == elle::Status::Error)
        error("unable to store the directory",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, 0%o)\n",
               __FUNCTION__,
               path, mode);

      return (0);
    }

    /// This method removes a directory.
    int                 Crux::Rmdir(const char*                 path)
    {
      etoile::path::Slab                name;
      etoile::path::Way                 child(path);
      etoile::path::Way                 parent(child, name);
      etoile::path::Chemin              chemin;
      etoile::gear::Identifier          directory;
      etoile::gear::Identifier          subdirectory;
      etoile::miscellaneous::Abstract   abstract;
      nucleus::Record*                  record;
      nucleus::Subject                  subject;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s)\n",
               __FUNCTION__,
               path);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(parent, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the directory.
      if (etoile::wall::Directory::Load(chemin,
                                        directory) == elle::Status::Error)
        error("unable to load the directory",
              -ENOENT);

      // Retrieve the subject's permissions on the object.
      if (etoile::wall::Access::Lookup(directory,
                                       agent::Agent::Subject,
                                       record) == elle::Status::Error)
        error("unable to retrieve the access record",
              -EPERM,
              directory);

      // Check the record.
      if (!((record != NULL) &&
            ((record->permissions & nucleus::PermissionWrite) ==
             nucleus::PermissionWrite)))
        error("the subject does not have the right to remove "
              "a subdirectory from this directory",
              -EACCES,
              directory);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(child, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT,
              directory);

      // Load the subdirectory.
      if (etoile::wall::Directory::Load(chemin,
                                        subdirectory) == elle::Status::Error)
        error("unable to load the directory",
              -ENOENT,
              directory);

      // Retrieve information on the object.
      if (etoile::wall::Object::Information(subdirectory,
                                            abstract) == elle::Status::Error)
        error("unable to retrieve information on the object",
              -EPERM,
              subdirectory, directory);

      // Create a temporary subject based on the object owner's key.
      if (subject.Create(abstract.keys.owner) == elle::Status::Error)
        error("unable to create a temporary subject",
              -EPERM,
              subdirectory, directory);

      // Check that the subject is the owner of the object.
      if (agent::Agent::Subject != subject)
        error("the subject does not have the right to destroy "
              "this directory",
              -EACCES,
              subdirectory, directory);

      // Remove the entry.
      if (etoile::wall::Directory::Remove(directory,
                                          name) == elle::Status::Error)
        error("unable to remove a directory entry",
              -EPERM,
              subdirectory, directory);

      // Store the directory.
      if (etoile::wall::Directory::Store(directory) == elle::Status::Error)
        error("unable to store the directory",
              -EPERM,
              subdirectory);

      // Destroy the subdirectory.
      if (etoile::wall::Directory::Destroy(subdirectory) == elle::Status::Error)
        error("unable to destroy the directory",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s)\n",
               __FUNCTION__,
               path);

      return (0);
    }

    /// This method checks if the current user has the permission to
    /// access the object _path_ for the operations _mask_.
    int                 Crux::Access(const char*                path,
                                     int                        mask)
    {
      etoile::gear::Identifier          identifier;
      etoile::miscellaneous::Abstract   abstract;
      etoile::path::Way                 way(path);
      etoile::path::Chemin              chemin;
      nucleus::Record*                  record;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, 0%o)\n",
               __FUNCTION__,
               path, mask);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the object.
      if (etoile::wall::Object::Load(chemin, identifier) == elle::Status::Error)
        error("unable to load the object",
              -ENOENT);

      // Retrieve information on the object.
      if (etoile::wall::Object::Information(identifier,
                                            abstract) == elle::Status::Error)
        error("unable to retrieve information on the object",
              -EPERM,
              identifier);

      // Retrieve the user's permissions on the object.
      if (etoile::wall::Access::Lookup(identifier,
                                       agent::Agent::Subject,
                                       record) == elle::Status::Error)
        error("unable to retrieve the access record",
              -EPERM,
              identifier);

      // Check the record.
      if (record == NULL)
        goto _access;

      // Check if the permissions match the mask for execution.
      if (mask & X_OK)
        {
          switch (abstract.genre)
            {
            case nucleus::GenreDirectory:
              {
                // Check if the user has the read permission meaning
                // the exec bit
                if ((record->permissions & nucleus::PermissionRead) !=
                    nucleus::PermissionRead)
                  goto _access;

                break;
              }
            case nucleus::GenreFile:
              {
                nucleus::Trait* trait;

                // Get the perm::exec attribute
                if (etoile::wall::Attributes::Get(identifier,
                                                  "perm::exec",
                                                  trait) == elle::Status::Error)
                  error("unable to retrieve the attribute",
                        -EPERM,
                        identifier);

                // Check the trait.
                if (!((trait != NULL) &&
                      (trait->value == "true")))
                  goto _access;

                break;
              }
            case nucleus::GenreLink:
              {
                nucleus::Trait* trait;

                // Get the perm::exec attribute
                if (etoile::wall::Attributes::Get(identifier,
                                                  "perm::exec",
                                                  trait) == elle::Status::Error)
                  error("unable ti retrive the attribute",
                        -EPERM,
                        identifier);

                // Check the trait.
                if (!((trait != NULL) &&
                      (trait->value == "true")))
                  goto _access;

                break;
              }
            }
        }

      // Check if the permissions match the mask for reading.
      if (mask & R_OK)
        {
          if ((record->permissions & nucleus::PermissionRead) !=
              nucleus::PermissionRead)
            goto _access;
        }

      // Check if the permissions match the mask for writing.
      if (mask & W_OK)
        {
          if ((record->permissions & nucleus::PermissionWrite) !=
              nucleus::PermissionWrite)
            goto _access;
        }

      // Discard the object.
      if (etoile::wall::Object::Discard(identifier) == elle::Status::Error)
        error("unable to discard the object",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, 0%o)\n",
               __FUNCTION__,
               path, mask);

      return (0);

    _access:
      // At this point, the access has been refused.  Therefore, the
      // identifier must be discarded while EACCES must be returned.

      // Discard the identifier.
      etoile::wall::Object::Discard(identifier);

      // Purge the errors.
      purge();

      return (-EACCES);
    }

    /// This method modifies the permissions on the object.
    int                 Crux::Chmod(const char*                 path,
                                    mode_t                      mode)
    {
      nucleus::Permissions              permissions = nucleus::PermissionNone;
      etoile::gear::Identifier          identifier;
      etoile::path::Way                 way(path);
      etoile::path::Chemin              chemin;
      etoile::miscellaneous::Abstract   abstract;
      nucleus::Subject                  subject;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, 0%o)\n",
               __FUNCTION__,
               path, mode);

      // Note that this method ignores both the group and other
      // permissions.
      //
      // In order not to ignore them, the system would have to
      // create/update the group entries in the object's access
      // list. although this is completely feasible, it has been
      // decided not to do so because it would incur too much cost.
      //
      // Indeed, on most Linux systems, the umask is set to 022 or is
      // somewhat equivalent, granting permissions, by default, to the
      // default group and the others.
      //
      // Although this is, from our point of view, a very bad idea, it
      // would be catastrophic to create such access records in
      // Infinit, especially because Infinit has been designed and
      // optimised for objects accessed by their sole owners.

      // Compute the permissions.
      if (mode & S_IRUSR)
        permissions |= nucleus::PermissionRead;

      if (mode & S_IWUSR)
        permissions |= nucleus::PermissionWrite;

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the object.
      if (etoile::wall::Object::Load(chemin, identifier) == elle::Status::Error)
        error("unable to load the object",
              -ENOENT);

      // Retrieve information on the object.
      if (etoile::wall::Object::Information(identifier,
                                            abstract) == elle::Status::Error)
        error("unable to retrieve information on the object",
              -EPERM,
              identifier);

      // Create a temporary subject based on the object owner's key.
      if (subject.Create(abstract.keys.owner) == elle::Status::Error)
        error("unable to create a temporary subject",
              -EPERM,
              identifier);

      // Check that the subject is the owner of the object.
      if (agent::Agent::Subject != subject)
        error("the subject does not have the right to modify the "
              "access permissions on this object",
              -EACCES,
              identifier);

      // The permission modification must be performed according to
      // the object state.
      //
      // Indeed, if the object has just been created, the permissions
      // assigned at creation will actually be granted when closed.
      //
      // Therefore, should a chmod() be requested between a create()
      // and a close(), the change of permissions should be delayed as
      // it is the case for any file being created.
      //
      // The following therefore checks if the path corresponds to a
      // file in creation. if so, the permissions are recorded for
      // future application.
      if (Crib::Exist(elle::String(path)) == elle::Status::True)
        {
          Handle*       handle;

          // Retrieve the handle, representing the created file, from
          // the crib.  Then update the future permissions in the
          // handle so that, when the file gets closed, these
          // permissions get applied.

          if (Crib::Retrieve(elle::String(path), handle) == elle::Status::Error)
            error("unable to retrieve the handle from the crib",
                  -EBADF,
                  identifier);

          handle->permissions = permissions;
        }
      else
        {
          // Update the accesses.  Note that the method assumes that
          // the caller is the object's owner! if not, an error will
          // occur anyway, so why bother checking.
          if (etoile::wall::Access::Grant(identifier,
                                          agent::Agent::Subject,
                                          permissions) == elle::Status::Error)
            error("unable to update the access records",
                  -EPERM,
                  identifier);
        }

      // If the execution bit is to be set...
      if (mode & S_IXUSR)
        {
          // Set the perm::exec attribute if necessary i.e depending
          // on the file genre.
          switch (abstract.genre)
            {
            case nucleus::GenreFile:
              {
                // Set the perm::exec attribute
                if (etoile::wall::Attributes::Set(identifier,
                                                  "perm::exec",
                                                  "true") == elle::Status::Error)
                  error("unable to set the attribute",
                        -EPERM,
                        identifier);

                break;
              }
            case nucleus::GenreDirectory:
            case nucleus::GenreLink:
              {
                // Nothing to do for the other genres.

                break;
              }
            }
        }

      // Store the object.
      if (etoile::wall::Object::Store(identifier) == elle::Status::Error)
        error("unable to store the object",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, 0%o)\n",
               __FUNCTION__,
               path, mode);

      return (0);
    }

    /// This method modifies the owner of a given object.
    int                 Crux::Chown(const char*                 path,
                                    uid_t                       uid,
                                    gid_t                       gid)
    {
      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %u, %u)\n",
               __FUNCTION__,
               path, uid, gid);

      // Xxx to implement.

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %u, %u)\n",
               __FUNCTION__,
               path, uid, gid);

      return (0);
    }

#if defined(HAVE_SETXATTR)
    /// This method sets an extended attribute value.
    ///
    /// Note that the flags are ignored!
    int                 Crux::Setxattr(const char*              path,
                                       const char*              name,
                                       const char*              value,
                                       size_t                   size,
                                       int                      flags)
    {
      etoile::gear::Identifier          identifier;
      etoile::path::Way                 way(path);
      etoile::path::Chemin              chemin;
      etoile::miscellaneous::Abstract   abstract;
      nucleus::Subject                  subject;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %s, %p, %u, 0x%x)\n",
               __FUNCTION__,
               path, name, value, size, flags);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the object.
      if (etoile::wall::Object::Load(chemin, identifier) == elle::Status::Error)
        error("unable to load the object",
              -ENOENT);

      // Retrieve information on the object.
      if (etoile::wall::Object::Information(identifier,
                                            abstract) == elle::Status::Error)
        error("unable to retrieve information on the object",
              -EPERM,
              identifier);

      // Create a temporary subject based on the object owner's key.
      if (subject.Create(abstract.keys.owner) == elle::Status::Error)
        error("unable to create a temporary subject",
              -EPERM,
              identifier);

      // Check that the subject is the owner of the object.
      if (agent::Agent::Subject != subject)
        error("the subject does not have the right to modify the attributes "
              "associated with this object",
              -EACCES,
              identifier);

      // Set the attribute.
      if (etoile::wall::Attributes::Set(identifier,
                                        elle::String(name),
                                        elle::String(value, size)) ==
          elle::Status::Error)
        error("unable to set the attribute",
              -EPERM,
              identifier);

      // Store the object.
      if (etoile::wall::Object::Store(identifier) == elle::Status::Error)
        error("unable to store the object",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %s, %p, %u, 0x%x)\n",
               __FUNCTION__,
               path, name, value, size, flags);

      return (0);
    }

    /// This method returns the attribute associated with the given
    /// object.
    int                 Crux::Getxattr(const char*              path,
                                       const char*              name,
                                       char*                    value,
                                       size_t                   size)
    {
      etoile::gear::Identifier  identifier;
      etoile::path::Way         way(path);
      etoile::path::Chemin      chemin;
      nucleus::Trait*           trait;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %s, %p, %u)\n",
               __FUNCTION__,
               path, name, value, size);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the object.
      if (etoile::wall::Object::Load(chemin, identifier) == elle::Status::Error)
        error("unable to load the object",
              -ENOENT);

      // Get the attribute.
      if (etoile::wall::Attributes::Get(identifier,
                                        elle::String(name),
                                        trait) == elle::Status::Error)
        error("unable to retrieve an attribute",
              -EPERM,
              identifier);

      // Discard the object.
      if (etoile::wall::Object::Discard(identifier) == elle::Status::Error)
        error("unable to discard the object",
              -EPERM);

      // Test if a trait has been found.
      if (trait == NULL)
        return (-ENOATTR);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %s, %p, %u)\n",
               __FUNCTION__,
               path, name, value, size);

      // If the size is null, it means that this call must be
      // considered as a request for the size required to store the
      // value.
      if (size == 0)
        {
          return (trait->value.length());
        }
      else
        {
          // Otherwise, copy the trait value in the value buffer.
          ::memcpy(value, trait->value.data(), trait->value.length());

          // Return the length of the value.
          return (trait->value.length());
        }
    }

    /// This method returns the list of attribute names.
    int                 Crux::Listxattr(const char*             path,
                                        char*                   list,
                                        size_t                  size)
    {
      etoile::gear::Identifier                  identifier;
      etoile::path::Way                         way(path);
      etoile::path::Chemin                      chemin;
      nucleus::Range<nucleus::Trait>            range;
      nucleus::Range<nucleus::Trait>::Scoutor   scoutor;
      size_t                                    offset;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %p, %u)\n",
               __FUNCTION__,
               path, list, size);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the object.
      if (etoile::wall::Object::Load(chemin, identifier) == elle::Status::Error)
        error("unable to load the object",
              -ENOENT);

      // Fetch the attributes.
      if (etoile::wall::Attributes::Fetch(identifier,
                                          range) == elle::Status::Error)
        error("unable to fetch the attributes",
              -EPERM,
              identifier);

      // Discard the object.
      if (etoile::wall::Object::Discard(identifier) == elle::Status::Error)
        error("unable to discard the object",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %p, %u)\n",
               __FUNCTION__,
               path, list, size);

      // If the size is zero, this call must return the size required
      // to store the list.
      if (size == 0)
        {
          for (scoutor = range.container.begin();
               scoutor != range.container.end();
               scoutor++)
            {
              nucleus::Trait*   trait = *scoutor;

              // Compute the size.
              size = size + trait->name.length() + 1;
            }

          return (size);
        }
      else
        {
          // Otherwise, go through the attributes and concatenate
          // their names.
          for (scoutor = range.container.begin(), offset = 0;
               scoutor != range.container.end();
               scoutor++)
            {
              nucleus::Trait*   trait = *scoutor;

              // Concatenate the name.
              ::strcpy(list + offset,
                       trait->name.c_str());

              // Adjust the offset.
              offset = offset + trait->name.length() + 1;
            }

          return (offset);
        }
    }

    /// This method removes an attribute.
    int                 Crux::Removexattr(const char*           path,
                                          const char*           name)
    {
      etoile::gear::Identifier          identifier;
      etoile::path::Way                 way(path);
      etoile::path::Chemin              chemin;
      etoile::miscellaneous::Abstract   abstract;
      nucleus::Subject                  subject;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %s)\n",
               __FUNCTION__,
               path, name);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the object.
      if (etoile::wall::Object::Load(chemin, identifier) == elle::Status::Error)
        error("unable to load the object",
              -ENOENT);

      // Retrieve information on the object.
      if (etoile::wall::Object::Information(identifier,
                                            abstract) == elle::Status::Error)
        error("unable to retrieve information on the object",
              -EPERM,
              identifier);

      // Create a temporary subject based on the object owner's key.
      if (subject.Create(abstract.keys.owner) == elle::Status::Error)
        error("unable to create a temporary subject",
              -EPERM,
              identifier);

      // Check that the subject is the owner of the object.
      if (agent::Agent::Subject != subject)
        error("the subject does not have the right to modify the attributes "
              "associated with this object",
              -EACCES,
              identifier);

      // Omit the attribute.
      if (etoile::wall::Attributes::Omit(identifier,
                                         elle::String(name)) ==
          elle::Status::Error)
        error("unable to omit the attributes",
              -EPERM,
              identifier);

      // Store the object.
      if (etoile::wall::Object::Store(identifier) == elle::Status::Error)
        error("unable to store the object",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %s)\n",
               __FUNCTION__,
               path, name);

      return (0);
    }
#endif

    /// This method creates a symbolic link.
    int                 Crux::Symlink(const char*               target,
                                      const char*               source)
    {
      etoile::gear::Identifier  directory;
      etoile::gear::Identifier  link;
      etoile::path::Slab        name;
      etoile::path::Way         from(etoile::path::Way(source), name);
      etoile::path::Way         to(target);
      etoile::path::Chemin      chemin;
      nucleus::Record*          record;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %s)\n",
               __FUNCTION__,
               target, source);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(from, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the directory.
      if (etoile::wall::Directory::Load(chemin,
                                        directory) == elle::Status::Error)
        error("unable to load the directory",
              -ENOENT);

      // Retrieve the subject's permissions on the object.
      if (etoile::wall::Access::Lookup(directory,
                                       agent::Agent::Subject,
                                       record) == elle::Status::Error)
        error("unable to retrieve the access record",
              -EPERM,
              directory);

      // Check the record.
      if (!((record != NULL) &&
            ((record->permissions & nucleus::PermissionWrite) ==
             nucleus::PermissionWrite)))
        error("the subject does not have the right to create a link in "
              "this directory",
              -EACCES,
              directory);

      // Create a link
      if (etoile::wall::Link::Create(link) == elle::Status::Error)
        error("unable to create a link",
              -EPERM,
              directory);

      // Bind the link.
      if (etoile::wall::Link::Bind(link, to) == elle::Status::Error)
        error("unable to bind the link",
              -EPERM,
              link, directory);

      // Add an entry for the link.
      if (etoile::wall::Directory::Add(directory,
                                       name,
                                       link) == elle::Status::Error)
        error("unable to add an entry to the directory",
              -EPERM,
              link, directory);

      // Store the link.
      if (etoile::wall::Link::Store(link) == elle::Status::Error)
        error("unable to store the link",
              -EPERM,
              directory);

      // Store the modified directory.
      if (etoile::wall::Directory::Store(directory) == elle::Status::Error)
        error("unable to store the directory",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %s)\n",
               __FUNCTION__,
               target, source);

      return (0);
    }

    /// This method returns the target path pointed by the symbolic
    /// link.
    int                 Crux::Readlink(const char*              path,
                                       char*                    buffer,
                                       size_t                   size)
    {
      etoile::gear::Identifier  identifier;
      etoile::path::Way         way(path);
      etoile::path::Chemin      chemin;
      etoile::path::Way         target;
      nucleus::Record*          record;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %p, %qu)\n",
               __FUNCTION__,
               path, buffer, static_cast<elle::Natural64>(size));

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the link.
      if (etoile::wall::Link::Load(chemin, identifier) == elle::Status::Error)
        error("unable to load the link",
              -ENOENT);

      // Retrieve the subject's permissions on the object.
      if (etoile::wall::Access::Lookup(identifier,
                                       agent::Agent::Subject,
                                       record) == elle::Status::Error)
        error("unable to retrieve the access record",
              -EPERM,
              identifier);

      // Check the record.
      if (!((record != NULL) &&
            ((record->permissions & nucleus::PermissionRead) ==
             nucleus::PermissionRead)))
        error("the subject does not have the right to read this link",
              -EACCES,
              identifier);

      // Resolve the link.
      if (etoile::wall::Link::Resolve(identifier, target) == elle::Status::Error)
        error("unable to resolve the link",
              -EPERM,
              identifier);

      // Discard the link.
      if (etoile::wall::Link::Discard(identifier) == elle::Status::Error)
        error("unable to discard the link",
              -EPERM);

      // Copy as much as possible of the target into the output
      // buffer.
      ::strncpy(buffer,
                target.path.c_str(),
                (target.path.length() + 1) < size ?
                target.path.length() + 1 :
                size);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %p, %qu)\n",
               __FUNCTION__,
               path, buffer, static_cast<elle::Natural64>(size));

      return (0);
    }

    /// This method creates a new file and opens it.
    int                 Crux::Create(const char*                path,
                                     mode_t                     mode,
                                     struct ::fuse_file_info*   info)
    {
      nucleus::Permissions      permissions = nucleus::PermissionNone;
      etoile::path::Slab        name;
      etoile::path::Way         way(etoile::path::Way(path), name);
      etoile::path::Chemin      chemin;
      etoile::gear::Identifier  directory;
      etoile::gear::Identifier  file;
      nucleus::Record*          record;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, 0%o, %p)\n",
               __FUNCTION__,
               path, mode, info);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the directory.
      if (etoile::wall::Directory::Load(chemin,
                                        directory) == elle::Status::Error)
        error("unable to load the directory",
              -ENOENT);

      // Retrieve the subject's permissions on the object.
      if (etoile::wall::Access::Lookup(directory,
                                       agent::Agent::Subject,
                                       record) == elle::Status::Error)
        error("unable to retrieve the access record",
              -EPERM,
              directory);

      // Check the record.
      if (!((record != NULL) &&
            ((record->permissions & nucleus::PermissionWrite) ==
             nucleus::PermissionWrite)))
        error("the subject does not have the right to create a file in "
              "this directory",
              -EACCES,
              directory);

      // Create the file.
      if (etoile::wall::File::Create(file) == elle::Status::Error)
        error("unable to create a file",
              -EPERM,
              directory);

      // Set default permissions: read and write.
      permissions = nucleus::PermissionRead | nucleus::PermissionWrite;

      // Set the owner permissions.
      if (etoile::wall::Access::Grant(file,
                                      agent::Agent::Subject,
                                      permissions) == elle::Status::Error)
        error("unable to update the access records",
              -EPERM,
              file, directory);

      // If the file has the exec bit, add the perm::exec attribute.
      if (mode & S_IXUSR)
        {
          // Set the perm::exec attribute
          if (etoile::wall::Attributes::Set(file,
                                            "perm::exec",
                                            "true") == elle::Status::Error)
            error("unable to set the attributes",
                  -EPERM,
                  file, directory);
        }

      // Add the file to the directory.
      if (etoile::wall::Directory::Add(directory,
                                       name,
                                       file) == elle::Status::Error)
        error("unable to add an entry to the directory",
              -EPERM,
              file, directory);

      // Store the file, ensuring the file system consistency.
      //
      // Indeed, if the file is kept opened and the directory is
      // stored someone could see an incorrect entry. although errors
      // will occur extremely rarely and such errors do not cause
      // harm, especially considering the Infinit consistency
      // guaranties, we still prefer to do things right, at least for
      // now.
      if (etoile::wall::File::Store(file) == elle::Status::Error)
        error("unable to store the file",
              -EPERM,
              directory);

      // Store the directory.
      if (etoile::wall::Directory::Store(directory) == elle::Status::Error)
        error("unable to store the directory",
              -EPERM);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(etoile::path::Way(path),
                                      chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Finally, the file is reopened.
      if (etoile::wall::File::Load(chemin, file) == elle::Status::Error)
        error("unable to load the file",
              -ENOENT);

      // Compute the future permissions as the current ones are
      // temporary.
      permissions = nucleus::PermissionNone;

      if (mode & S_IRUSR)
        permissions |= nucleus::PermissionRead;

      if (mode & S_IWUSR)
        permissions |= nucleus::PermissionWrite;

      // Store the identifier in the file handle.
      info->fh =
        reinterpret_cast<uint64_t>(new Handle(Handle::OperationCreate,
                                              file,
                                              permissions));

      // Add the created and opened file in the crib.
      if (Crib::Add(elle::String(path),
                    reinterpret_cast<Handle*>(info->fh)) == elle::Status::Error)
        error("unable to add the created file to the crib",
              -EBADF);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, 0%o, %p)\n",
               __FUNCTION__,
               path, mode, info);

      return (0);
    }

    /// This method opens a file.
    int                 Crux::Open(const char*                  path,
                                   struct ::fuse_file_info*     info)
    {
      etoile::path::Way         way(path);
      etoile::path::Chemin      chemin;
      etoile::gear::Identifier  identifier;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, info);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the file.
      if (etoile::wall::File::Load(chemin, identifier) == elle::Status::Error)
        error("unable to load the file",
              -ENOENT);

      // Store the identifier in the file handle.
      info->fh =
        reinterpret_cast<uint64_t>(new Handle(Handle::OperationOpen,
                                              identifier));

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, info);

      return (0);
    }

    /// This method writes data to a file.
    int                 Crux::Write(const char*                 path,
                                    const char*                 buffer,
                                    size_t                      size,
                                    off_t                       offset,
                                    struct ::fuse_file_info*    info)
    {
      Handle*           handle;
      elle::standalone::Region      region;
      nucleus::Record*  record;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %p, %qu, %qu, %p)\n",
               __FUNCTION__,
               path, buffer, static_cast<elle::Natural64>(size),
               static_cast<elle::Natural64>(offset), info);

      // Retrieve the handle;
      handle = reinterpret_cast<Handle*>(info->fh);

      // Retrieve the subject's permissions on the object.
      if (etoile::wall::Access::Lookup(handle->identifier,
                                       agent::Agent::Subject,
                                       record) == elle::Status::Error)
        error("unable to retrieve the access record",
              -EPERM);

      // Check the record.
      if (!((record != NULL) &&
            ((record->permissions & nucleus::PermissionWrite) ==
             nucleus::PermissionWrite)))
        error("the subject does not have the right to update this file",
              -EACCES);

      // Wrap the buffer.
      if (region.Wrap(reinterpret_cast<const elle::Byte*>(buffer),
                      size) == elle::Status::Error)
        error("unable to wrap the buffer",
              -EPERM);

      // Write the file.
      if (etoile::wall::File::Write(handle->identifier,
                                    static_cast<nucleus::Offset>(offset),
                                    region) == elle::Status::Error)
        error("unable to write the file",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %p, %qu, %qu, %p)\n",
               __FUNCTION__,
               path, buffer, static_cast<elle::Natural64>(size),
               static_cast<elle::Natural64>(offset), info);

      return (size);
    }

    /// This method reads data from a file.
    int                 Crux::Read(const char*                  path,
                                   char*                        buffer,
                                   size_t                       size,
                                   off_t                        offset,
                                   struct ::fuse_file_info*     info)
    {
      Handle*           handle;
      elle::standalone::Region      region;
      nucleus::Record*  record;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %p, %qu, %qu, %p)\n",
               __FUNCTION__,
               path, buffer, static_cast<elle::Natural64>(size),
               static_cast<elle::Natural64>(offset), info);

      // Retrieve the handle.
      handle = reinterpret_cast<Handle*>(info->fh);

      // Retrieve the subject's permissions on the object.
      if (etoile::wall::Access::Lookup(handle->identifier,
                                       agent::Agent::Subject,
                                       record) == elle::Status::Error)
        error("unable to retrieve the access record",
              -EPERM);

      // Check the record.
      if (!((record != NULL) &&
            ((record->permissions & nucleus::PermissionRead) ==
             nucleus::PermissionRead)))
        error("the subject does not have the right to read this file",
              -EACCES);

      // Read the file.
      if (etoile::wall::File::Read(handle->identifier,
                                   static_cast<nucleus::Offset>(offset),
                                   static_cast<nucleus::Size>(size),
                                   region) == elle::Status::Error)
        error("unable to read the file",
              -EPERM);

      // Copy the data to the output buffer.
      ::memcpy(buffer, region.contents, region.size);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %p, %qu, %qu, %p)\n",
               __FUNCTION__,
               path, buffer, static_cast<elle::Natural64>(size),
               static_cast<elle::Natural64>(offset), info);

      return (region.size);
    }

    /// This method modifies the size of a file.
    int                 Crux::Truncate(const char*              path,
                                       off_t                    size)
    {
      etoile::gear::Identifier  identifier;
      etoile::path::Way         way(path);
      etoile::path::Chemin      chemin;
      struct ::fuse_file_info   info;
      int                       result;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %qu)\n",
               __FUNCTION__,
               path, static_cast<elle::Natural64>(size));

      // Resolve the path.
      if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the file.
      if (etoile::wall::File::Load(chemin, identifier) == elle::Status::Error)
        error("unable to load the file",
              -ENOENT);

      // Create a local handle.
      Handle                    handle(Handle::OperationTruncate,
                                       identifier);

      // Set the handle in the fuse_file_info structure.
      info.fh = reinterpret_cast<uint64_t>(&handle);

      // Call the Ftruncate() method.
      if ((result = Crux::Ftruncate(path, size, &info)) < 0)
        error("unable to truncate the given file descriptpr",
              result,
              identifier);

      // Store the file.
      if (etoile::wall::File::Store(identifier) == elle::Status::Error)
        error("unable to store the file",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %qu)\n",
               __FUNCTION__,
               path, static_cast<elle::Natural64>(size));

      return (result);
    }

    /// This method modifies the size of an opened file.
    int                 Crux::Ftruncate(const char*             path,
                                        off_t                   size,
                                        struct ::fuse_file_info* info)
    {
      Handle*           handle;
      nucleus::Record*  record;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %qu, %p)\n",
               __FUNCTION__,
               path, static_cast<elle::Natural64>(size), info);

      // Retrieve the handle.
      handle = reinterpret_cast<Handle*>(info->fh);

      // Retrieve the subject's permissions on the object.
      if (etoile::wall::Access::Lookup(handle->identifier,
                                       agent::Agent::Subject,
                                       record) == elle::Status::Error)
        error("unable to retrieve the access record",
              -EPERM);

      // Check the record.
      if (!((record != NULL) &&
            ((record->permissions & nucleus::PermissionWrite) ==
             nucleus::PermissionWrite)))
        error("the subject does not have the right to modify the size of "
              "this file",
              -EACCES);

      // Adjust the file's size.
      if (etoile::wall::File::Adjust(handle->identifier,
                                     size) == elle::Status::Error)
        error("unable to adjust the size of the file",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %qu, %p)\n",
               __FUNCTION__,
               path, static_cast<elle::Natural64>(size), info);

      return (0);
    }

    /// This method closes a file.
    int                 Crux::Release(const char*               path,
                                      struct ::fuse_file_info*  info)
    {
      etoile::path::Way way(path);
      Handle*           handle;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, info);

      // Retrieve the handle.
      handle = reinterpret_cast<Handle*>(info->fh);

      // Perform final actions depending on the initial operation.
      switch (handle->operation)
        {
        case Handle::OperationCreate:
          {
            // Remove the created and opened file in the crib.
            if (Crib::Remove(elle::String(path)) == elle::Status::Error)
              error("unable to remove the created file from the crib",
                    -EBADF,
                    handle->identifier);

            // The permissions settings have been delayed in order to
            // support a read-only file being copied in which case a
            // file is created with read-only permissions before being
            // written.
            //
            // Such a normal behaviour would result in runtime
            // permission errors. therefore, the permissions are set
            // once the created file is released in order to overcome
            // this issue.

            // Set the owner permissions.
            if (etoile::wall::Access::Grant(
                  handle->identifier,
                  agent::Agent::Subject,
                  handle->permissions) == elle::Status::Error)
              error("unable to update the access records",
                    -EPERM,
                    handle->identifier);

            break;
          }
        default:
          {
            // Nothing special to do.

            break;
          }
        }

      // Store the file.
      if (etoile::wall::File::Store(handle->identifier) == elle::Status::Error)
        error("unable to store the file",
              -EPERM);

      // Delete the handle.
      delete handle;

      // Reset the file handle.
      info->fh = 0;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %p)\n",
               __FUNCTION__,
               path, info);

      return (0);
    }

    /// This method renames a file.
    int                 Crux::Rename(const char*                source,
                                     const char*                target)
    {
      etoile::path::Slab        f;
      etoile::path::Way         from(etoile::path::Way(source), f);
      etoile::path::Slab        t;
      etoile::path::Way         to(etoile::path::Way(target), t);
      etoile::gear::Identifier  object;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s, %s)\n",
               __FUNCTION__,
               source, target);

      // If the source and target directories are identical.
      if (from == to)
        {
          // In this case, the object to move can simply be renamed
          // since the source and target directory are identical.
          etoile::path::Chemin          chemin;
          etoile::gear::Identifier      directory;
          nucleus::Entry*               entry;
          nucleus::Record*              record;

          // Resolve the path.
          if (etoile::wall::Path::Resolve(from, chemin) == elle::Status::Error)
            error("unable to resolve the path",
                  -ENOENT);

          // Load the directory.
          if (etoile::wall::Directory::Load(chemin,
                                            directory) == elle::Status::Error)
            error("unable to load the directory",
                  -ENOENT);

          // Retrieve the subject's permissions on the object.
          if (etoile::wall::Access::Lookup(directory,
                                           agent::Agent::Subject,
                                           record) == elle::Status::Error)
            error("unable to retrieve the access record",
                  -EPERM,
                  directory);

          // Check the record.
          if (!((record != NULL) &&
                ((record->permissions & nucleus::PermissionWrite) ==
                 nucleus::PermissionWrite)))
            error("the subject does not have the right to rename this "
                  "directory entry",
                  -EACCES,
                  directory);

          // Lookup for the target name.
          if (etoile::wall::Directory::Lookup(directory,
                                              t,
                                              entry) == elle::Status::Error)
            error("unable to lookup the target name",
                  -EPERM,
                  directory);

          // Check if an entry actually exist for the target name
          // meaning that an object is about to get overwritten.
          if (entry != NULL)
            {
              // In this case, the target object must be destroyed.
              int               result;

              // Unlink the object, assuming it is either a file or a
              // link.  Note that the Crux's method is called in order
              // not to have to deal with the target's genre.
              if ((result = Crux::Unlink(target)) < 0)
                error("unable to unlink the target object which is "
                      "about to get overwritte",
                      result,
                      directory);
            }

          // Rename the entry from _f_ to _t_.
          if (etoile::wall::Directory::Rename(directory,
                                              f,
                                              t) == elle::Status::Error)
            error("unable to rename a directory entry",
                  -EPERM,
                  directory);

          // Store the directory.
          if (etoile::wall::Directory::Store(directory) == elle::Status::Error)
            error("unable to store the directory",
                  -EPERM);
        }
      else
        {
          // Otherwise, the object must be moved from _from_ to _to_.
          //
          // The process goes as follows: the object is loaded, an
          // entry in the _to_ directory is added while the entry in
          // the _from_ directory is removed.
          etoile::path::Way             way(source);
          etoile::path::Chemin          chemin;
          struct
          {
            etoile::gear::Identifier    object;
            etoile::gear::Identifier    from;
            etoile::gear::Identifier    to;
          }                             identifier;
          nucleus::Entry*               entry;
          nucleus::Record*              record;

          // Resolve the path.
          if (etoile::wall::Path::Resolve(way, chemin) == elle::Status::Error)
            error("unable to resolve the path",
                  -ENOENT);

          // Load the object even though we don't know its genre as we
          // do not need to know to perform this operation.
          if (etoile::wall::Object::Load(
                chemin,
                identifier.object) == elle::Status::Error)
            error("unable to load the object",
                  -ENOENT);

          // Resolve the path.
          if (etoile::wall::Path::Resolve(to, chemin) == elle::Status::Error)
            error("unable to resolve the path",
                  -ENOENT,
                  identifier.object);

          // Load the _to_ directory.
          if (etoile::wall::Directory::Load(
                chemin,
                identifier.to) == elle::Status::Error)
            error("unable to load the directory",
                  -ENOENT,
                  identifier.object);

          // Retrieve the subject's permissions on the object.
          if (etoile::wall::Access::Lookup(identifier.to,
                                           agent::Agent::Subject,
                                           record) == elle::Status::Error)
            error("unable to retrieve the access record",
                  -EPERM,
                  identifier.object, identifier.to);

          // Check the record.
          if (!((record != NULL) &&
                ((record->permissions & nucleus::PermissionWrite) ==
                 nucleus::PermissionWrite)))
            error("the subject does not have the right to rename this "
                  "directory entry",
                  -EACCES,
                  identifier.object, identifier.to);

          // Resolve the path.
          if (etoile::wall::Path::Resolve(from, chemin) == elle::Status::Error)
            error("unable to resolve the path",
                  -ENOENT,
                  identifier.object, identifier.to);

          // Load the _from_ directory.
          if (etoile::wall::Directory::Load(
                chemin,
                identifier.from) == elle::Status::Error)
            error("unable to load the directory",
                  -ENOENT,
                  identifier.object, identifier.to);

          // Retrieve the subject's permissions on the object.
          if (etoile::wall::Access::Lookup(identifier.from,
                                           agent::Agent::Subject,
                                           record) == elle::Status::Error)
            error("unable to retrieve the access record",
                  -EPERM,
                  identifier.object, identifier.to, identifier.from);

          // Check the record.
          if (!((record != NULL) &&
                ((record->permissions & nucleus::PermissionWrite) ==
                 nucleus::PermissionWrite)))
            error("the subject does not have the right to rename this "
                  "directory entry",
                  -EACCES,
                  identifier.object, identifier.to, identifier.from);

          // Lookup for the target name.
          if (etoile::wall::Directory::Lookup(identifier.to,
                                              t,
                                              entry) == elle::Status::Error)
            error("unable to lookup the target name",
                  -EPERM,
                  identifier.object, identifier.to, identifier.from);

          // Check if an entry actually exist for the target name
          // meaning that an object is about to get overwritten.
          if (entry != NULL)
            {
              // In this case, the target object must be destroyed.
              int               result;

              // Unlink the object, assuming it is either a file or a
              // link.  Note that the Crux's method is called in order
              // not to have to deal with the target's genre.
              if ((result = Crux::Unlink(target)) < 0)
                error("unable to unlink the target object which is "
                      "about to get overwritte",
                      result,
                      identifier.object, identifier.to, identifier.from);
            }

          // Add an entry.
          if (etoile::wall::Directory::Add(
                identifier.to,
                t,
                identifier.object) == elle::Status::Error)
            error("unable to add an entry to the directory",
                  -EPERM,
                  identifier.object, identifier.to, identifier.from);

          // Remove the entry.
          if (etoile::wall::Directory::Remove(
                identifier.from,
                f) == elle::Status::Error)
            error("unable to remove a directory entry",
                  -EPERM,
                  identifier.object, identifier.to, identifier.from);

          // Store the _to_ directory.
          if (etoile::wall::Directory::Store(
                identifier.to) == elle::Status::Error)
            error("unable to store the directory",
                  -EPERM,
                  identifier.object, identifier.from);

          // Store the _from_ directory.
          if (etoile::wall::Directory::Store(
                identifier.from) == elle::Status::Error)
            error("unable to store the directory",
                  -EPERM,
                  identifier.object);

          // Store the object.
          if (etoile::wall::Object::Store(
                identifier.object) == elle::Status::Error)
            error("unable to store the object",
                  -EPERM);
        }

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s, %s)\n",
               __FUNCTION__,
               source, target);

      return (0);
    }

    /// This method removes an existing file.
    int                 Crux::Unlink(const char*                path)
    {
      etoile::path::Slab                name;
      etoile::path::Way                 child(path);
      etoile::path::Way                 parent(child, name);
      struct
      {
        etoile::path::Chemin            child;
        etoile::path::Chemin            parent;
      }                                 chemin;
      etoile::gear::Identifier          directory;
      etoile::gear::Identifier          identifier;
      etoile::miscellaneous::Abstract   abstract;
      nucleus::Record*                  record;
      nucleus::Subject                  subject;

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] Crux::%s(%s)\n",
               __FUNCTION__,
               path);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(child,
                                      chemin.child) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT);

      // Load the object.
      if (etoile::wall::Object::Load(chemin.child,
                                     identifier) == elle::Status::Error)
        error("unable to load the object",
              -ENOENT);

      // Retrieve information on the object.
      if (etoile::wall::Object::Information(identifier,
                                            abstract) == elle::Status::Error)
        error("unable to retrieve information on the object",
              -EPERM,
              identifier);

      // Create a temporary subject based on the object owner's key.
      if (subject.Create(abstract.keys.owner) == elle::Status::Error)
        error("unable to create a temporary subject",
              -EPERM,
              identifier);

      // Check that the subject is the owner of the object.
      if (agent::Agent::Subject != subject)
        error("the subject does not have the right to destroy this object",
              -EACCES,
              identifier);

      // Resolve the path.
      if (etoile::wall::Path::Resolve(parent,
                                      chemin.parent) == elle::Status::Error)
        error("unable to resolve the path",
              -ENOENT,
              identifier);

      // Load the directory.
      if (etoile::wall::Directory::Load(chemin.parent,
                                        directory) == elle::Status::Error)
        error("unable to load the directory",
              -ENOENT,
              identifier);

      // Retrieve the subject's permissions on the object.
      if (etoile::wall::Access::Lookup(directory,
                                       agent::Agent::Subject,
                                       record) == elle::Status::Error)
        error("unable to retrieve the access record",
              -EPERM,
              identifier, directory);

      // Check the record.
      if (!((record != NULL) &&
            ((record->permissions & nucleus::PermissionWrite) ==
             nucleus::PermissionWrite)))
        error("the subject does not have the right to remove an entry from "
              "this directory",
              -EACCES,
              identifier, directory);

      // Remove the object according to its type: file or link.
      switch (abstract.genre)
        {
        case nucleus::GenreFile:
          {
            // Destroy the file.
            if (etoile::wall::File::Destroy(identifier) == elle::Status::Error)
              error("unable to destroy the file",
                    -EPERM,
                    directory);

            break;
          }
        case nucleus::GenreLink:
          {
            // Destroy the link.
            if (etoile::wall::Link::Destroy(identifier) == elle::Status::Error)
              error("unable to destroy the link",
                    -EPERM,
                    directory);

            break;
          }
        case nucleus::GenreDirectory:
          {
            error("meaningless operation: unlink on a directory object",
                  -EPERM);
          }
        };

      // Remove the entry.
      if (etoile::wall::Directory::Remove(directory,
                                          name) == elle::Status::Error)
        error("unable to remove a directory entry",
              -EPERM,
              directory);

      // Store the directory.
      if (etoile::wall::Directory::Store(directory) == elle::Status::Error)
        error("unable to store the directory",
              -EPERM);

      // Debug.
      if (Infinit::Configuration.horizon.debug == true)
        printf("[horizon] /Crux::%s(%s)\n",
               __FUNCTION__,
               path);

      return (0);
    }

  }
}
