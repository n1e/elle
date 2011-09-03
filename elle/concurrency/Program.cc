//
// ---------- header ----------------------------------------------------------
//
// project       elle
//
// license       infinit
//
// file          /home/mycure/infinit/elle/concurrency/Program.cc
//
// created       julien quintard   [mon mar 15 20:40:02 2010]
// updated       julien quintard   [sat sep  3 13:16:56 2011]
//

//
// ---------- includes --------------------------------------------------------
//

#include <elle/concurrency/Program.hh>
#include <elle/concurrency/Callback.hh>

#include <elle/standalone/Maid.hh>
#include <elle/standalone/Report.hh>

namespace elle
{
  namespace concurrency
  {

//
// ---------- globals ---------------------------------------------------------
//

    ///
    /// this variable represents the program.
    ///
    Program*			program = NULL;

//
// ---------- static methods --------------------------------------------------
//

    ///
    /// this method initializes the program.
    ///
    Status		Program::Initialize()
    {
      enter();

      // allocate a new program.
      program = new Program;

      leave();
    }

    ///
    /// this method cleans the program.
    ///
    Status		Program::Clean()
    {
      enter();

      // delete the program.
      if (program != NULL)
	delete program;

      leave();
    }

    ///
    /// this method sets up the program for startup.
    ///
    Status		Program::Setup()
    {
      int		n;

      enter();

      // set the number of arguments as zero since we do not want
      // QCoreApplication to bother parsing arguments.
      n = 0;

      // allocate the QT program.
      program->core = new ::QCoreApplication(n, NULL);

      // set the signal handlers.
      ::signal(SIGINT, &Program::Exception);
      ::signal(SIGQUIT, &Program::Exception);
      ::signal(SIGABRT, &Program::Exception);
      ::signal(SIGTERM, &Program::Exception);

      ::signal(SIGKILL, &Program::Exception);

      leave();
    }

    ///
    /// this method stops the program.
    ///
    Status		Program::Exit()
    {
      enter();

      // exit the core application.
      program->core->exit();

      leave();
    }

    ///
    /// this method processes events.
    ///
    Status		Program::Launch()
    {
      enter();

      // check the program.
      if ((program == NULL) || (program->core == NULL))
	escape("unable to process events since the program has not "
	       "been set up");

      // process the events.
      program->core->exec();

      leave();
    }

    ///
    /// this method is triggered whenever a POSIX signal is received.
    ///
    Void		Program::Exception(int			signal)
    {
      // stop the program depending on the signal.
      switch (signal)
	{
	case SIGINT:
	case SIGQUIT:
	case SIGABRT:
	case SIGTERM:
	  {
	    // exit properly by finishing processing the last events.
	    Program::Exit();

	    break;
	  }
	case SIGKILL:
	  {
	    // exit brutally!!!
	    ::exit(EXIT_FAILURE);
	  }
	}
    }

    ///
    /// this method attaches a broker to the program's event loop.
    ///
    Status		Program::Attach(Broker&			broker)
    {
      ::QAbstractEventDispatcher*	dispatcher;

      enter();

#include <elle/idiom/Close.hh>

      // retrieve the event dispatcher instance.
      dispatcher = ::QAbstractEventDispatcher::instance();

#include <elle/idiom/Open.hh>

      // register the socket notifier.
      dispatcher->registerSocketNotifier(&broker.notifier);

      leave();
    }

    ///
    /// this method detaches a broker from the program's event loop.
    ///
    Status		Program::Detach(Broker&			broker)
    {
      ::QAbstractEventDispatcher*	dispatcher;

      enter();

#include <elle/idiom/Close.hh>

      // retrieve the event dispatcher instance.
      dispatcher = ::QAbstractEventDispatcher::instance();

#include <elle/idiom/Open.hh>

      // unregister the socket notifier.
      dispatcher->unregisterSocketNotifier(&broker.notifier);

      leave();
    }

//
// ---------- constructors & destructors --------------------------------------
//

    ///
    /// default constructor.
    ///
    Program::Program():
      core(NULL)
    {
    }

    ///
    /// destructor.
    ///
    Program::~Program()
    {
      // releae the core.
      if (this->core != NULL)
	delete this->core;
    }

  }
}
