//
// ---------- header ----------------------------------------------------------
//
// project       hole
//
// license       infinit
//
// author        julien quintard   [wed aug 31 01:02:08 2011]
//

//
// ---------- includes --------------------------------------------------------
//

#include <hole/implementations/remote/Machine.hh>

#include <hole/Hole.hh>

namespace hole
{
  namespace implementations
  {
    namespace remote
    {

//
// ---------- constructors & destructors --------------------------------------
//

      ///
      /// default constructor.
      ///
      Machine::Machine():
        role(Machine::RoleUnknown)
      {
      }

      ///
      /// destructor.
      ///
      Machine::~Machine()
      {
        // delete the entity depending on the role.
        switch (this->role)
          {
          case Machine::RoleServer:
            {
              delete this->server;

              break;
            }
          case Machine::RoleClient:
            {
              delete this->client;

              break;
            }
          case Machine::RoleUnknown:
            {
              // nothing to do.

              break;
            }
          }
      }

//
// ---------- methods ---------------------------------------------------------
//

      ///
      /// this method launches the machine which tries to act first as a
      /// client and then as a server if no server seems to exist.
      ///
      elle::Status      Machine::Launch()
      {
        elle::String    string;
        elle::Locus     locus;

        // retrieve the server's locus.
        if (Hole::Descriptor.Get("remote", "server",
                                 string) == elle::StatusError)
          escape("unable to retrieve the server's locus from the "
                 "network descriptor");

        // build the host locus.
        if (locus.Create(string) == elle::StatusError)
          escape("unable to create the host locus");

        // try to connect to the server's host.
        {
          // allocate a client.
          auto client = std::unique_ptr<Client>(new Client(locus));

          // launch the client.
          if (client->Launch() == elle::StatusOk)
            {
              // set the client as the host.
              this->client = client.release();

              // set the role.
              this->role = Machine::RoleClient;

              return elle::StatusOk;
            }
        }

        // purge the error messages.
        purge();

        // if the client did not succeed, create a server a wait for a client.
        {
          // allocate a server.
          auto server = std::unique_ptr<Server>(new Server(locus));

          // launch the server.
          if (server->Launch() == elle::StatusOk)
            {
              // set the server as the host.
              this->server = server.release();

              // set the role.
              this->role = Machine::RoleServer;

              return elle::StatusOk;
            }
        }

        escape("unable to create a client or a server");
      }

//
// ---------- dumpable --------------------------------------------------------
//

      ///
      /// this method dumps the machine.
      ///
      elle::Status      Machine::Dump(const elle::Natural32     margin) const
      {
        elle::String    alignment(margin, ' ');

        std::cout << alignment << "[Machine]" << std::endl;

        // dump the role.
        std::cout << alignment << elle::Dumpable::Shift
                  << "[Role] " << this->role << std::endl;

        // dump the entity.
        switch (this->role)
          {
          case Machine::RoleServer:
            {
              // dump the server.
              if (this->server->Dump(margin + 2) == elle::StatusError)
                escape("unable to dump the server");

              break;
            }
          case Machine::RoleClient:
            {
              // dump the client.
              if (this->client->Dump(margin + 2) == elle::StatusError)
                escape("unable to dump the client");

              break;
            }
          default:
            {
              // nothing to do.

              break;
            }
          }

        return elle::StatusOk;
      }

    }
  }
}
