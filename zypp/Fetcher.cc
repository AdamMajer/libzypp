/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/Fetcher.cc
 *
*/
#include <iostream>
#include <list>

#include "zypp/base/Logger.h"
#include "zypp/base/PtrTypes.h"
#include "zypp/base/DefaultIntegral.h"
#include "zypp/Fetcher.h"
#include "zypp/base/UserRequestException.h"

using namespace std;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  /**
   * Class to encapsulate the \ref OnMediaLocation object
   * and the \ref FileChecker together
   */
  struct FetcherJob
  {
    FetcherJob( const OnMediaLocation &loc )
      : location(loc)
    {
      //MIL << location << endl;
    }

    ~FetcherJob()
    {
      //MIL << location << " | * " << checkers.size() << endl;
    }

    OnMediaLocation location;
    //CompositeFileChecker checkers;
    list<FileChecker> checkers;
  };

  typedef shared_ptr<FetcherJob> FetcherJob_Ptr;

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : Fetcher::Impl
  //
  /** Fetcher implementation. */
  struct Fetcher::Impl
  {

  public:

    void enqueue( const OnMediaLocation &resource, const FileChecker &checker  );
    void enqueueDigested( const OnMediaLocation &resource, const FileChecker &checker );
    void addCachePath( const Pathname &cache_dir );
    void reset();
    void start( const Pathname &dest_dir,
                MediaSetAccess &media,
                const ProgressData::ReceiverFnc & progress_receiver );

    /** Offer default Impl. */
    static shared_ptr<Impl> nullimpl()
    {
      static shared_ptr<Impl> _nullimpl( new Impl );
      return _nullimpl;
    }

  private:
    friend Impl * rwcowClone<Impl>( const Impl * rhs );
    /** clone for RWCOW_pointer */
    Impl * clone() const
    { return new Impl( *this ); }

    std::list<FetcherJob_Ptr> _resources;
    std::list<Pathname> _caches;
  };
  ///////////////////////////////////////////////////////////////////


  void Fetcher::Impl::enqueueDigested( const OnMediaLocation &resource, const FileChecker &checker )
  {
    FetcherJob_Ptr job;
    job.reset(new FetcherJob(resource));
    ChecksumFileChecker digest_check(resource.checksum());
    job->checkers.push_back(digest_check);
    if ( checker )
      job->checkers.push_back(checker);
    _resources.push_back(job);
  }

  void Fetcher::Impl::enqueue( const OnMediaLocation &resource, const FileChecker &checker )
  {
    FetcherJob_Ptr job;
    job.reset(new FetcherJob(resource));
    if ( checker )
      job->checkers.push_back(checker);
    _resources.push_back(job);
  }

  void Fetcher::Impl::reset()
  {
    _resources.clear();
  }

  void Fetcher::Impl::addCachePath( const Pathname &cache_dir )
  {
    PathInfo info(cache_dir);
    if ( info.isDir() )
    {
      _caches.push_back(cache_dir);
    }
    else
    {
      // don't add bad cache directory, just log the error
      ERR << "Not adding cache: '" << cache_dir << "'. Not a direcotry." << endl;
    }
  }

  void Fetcher::Impl::start( const Pathname &dest_dir,
                             MediaSetAccess &media,
                             const ProgressData::ReceiverFnc & progress_receiver )
  {
    ProgressData progress(_resources.size());
    progress.sendTo(progress_receiver);

    for ( list<FetcherJob_Ptr>::const_iterator it_res = _resources.begin(); it_res != _resources.end(); ++it_res )
    {
      bool got_from_cache = false;
      for ( list<Pathname>::const_iterator it_cache = _caches.begin(); it_cache != _caches.end(); ++it_cache )
      {
        // does the current file exists in the current cache?
        Pathname cached_file = *it_cache + (*it_res)->location.filename();
        if ( PathInfo( cached_file ).isExist() )
        {
          // check the checksum
          if ( is_checksum( cached_file, (*it_res)->location.checksum() ) && (! (*it_res)->location.checksum().empty() ) )
          {
            // cached
            MIL << "file " << (*it_res)->location.filename() << " found in previous cache. Using cached copy." << endl;
            // checksum is already checked.
            // we could later implement double failover and try to download if file copy fails.

            // replicate the complete path in the target directory
            Pathname dest_full_path = dest_dir + (*it_res)->location.filename();
            if ( assert_dir( dest_full_path.dirname() ) != 0 )
              ZYPP_THROW( Exception("Can't create " + dest_full_path.dirname().asString()));

            if ( filesystem::copy(cached_file, dest_full_path ) != 0 )
            { //copy_file2dir
              //ZYPP_THROW(SourceIOException("Can't copy " + cached_file.asString() + " to " + destination.asString()));
              ERR << "Can't copy " << cached_file + " to " + dest_dir << endl;
              // try next cache
              continue;
            }

            got_from_cache = true;
            break;
          }
        }
      }

      if ( ! got_from_cache )
      {
        // try to get the file from the net
        try
        {
          Pathname tmp_file = media.provideFile((*it_res)->location);
          Pathname dest_full_path = dest_dir + (*it_res)->location.filename();
          if ( assert_dir( dest_full_path.dirname() ) != 0 )
                ZYPP_THROW( Exception("Can't create " + dest_full_path.dirname().asString()));
          if ( filesystem::copy(tmp_file, dest_full_path ) != 0 )
          {
            ZYPP_THROW( Exception("Can't copy " + tmp_file.asString() + " to " + dest_dir.asString()));
          }


        }
        catch (const Exception & excpt_r)
        {
          ZYPP_CAUGHT(excpt_r);
          Exception nexcpt("Can't provide " + (*it_res)->location.filename().asString() + " : " + excpt_r.msg());
          nexcpt.remember(excpt_r);
          ZYPP_THROW(nexcpt);
        }
      }
      else
      {
        // We got the file from cache
        // continue with next file
        continue;
      }

      // no matter where did we got the file, try to validate it:
      Pathname localfile = dest_dir + (*it_res)->location.filename();
      // call the checker function
      try {
        MIL << "Checking job [" << localfile << "] (" << (*it_res)->checkers.size() << " checkers )" << endl;
        for ( list<FileChecker>::const_iterator it = (*it_res)->checkers.begin();
              it != (*it_res)->checkers.end();
              ++it )
        {
          if (*it)
          {
            (*it)(localfile);
          }
          else
          {
            ERR << "Invalid checker for '" << localfile << "'" << endl;
          }
        }
        
      }
      catch ( const FileCheckException &e )
      {
        ZYPP_RETHROW(e);
      }
      catch ( const Exception &e )
      {
        ZYPP_RETHROW(e);
      }
      catch (...)
      {
        ZYPP_THROW(Exception("Unknown error while validating " + (*it_res)->location.filename().asString()));
      }

      if ( ! progress.incr() )
        ZYPP_THROW(AbortRequestException());
    } // for each job
  }

  /** \relates Fetcher::Impl Stream output */
  inline std::ostream & operator<<( std::ostream & str, const Fetcher::Impl & obj )
  {
    return str << "Fetcher::Impl";
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : Fetcher
  //
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Fetcher::Fetcher
  //	METHOD TYPE : Ctor
  //
  Fetcher::Fetcher()
  : _pimpl( Impl::nullimpl() )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Fetcher::~Fetcher
  //	METHOD TYPE : Dtor
  //
  Fetcher::~Fetcher()
  {}

  void Fetcher::enqueueDigested( const OnMediaLocation &resource, const FileChecker &checker )
  {
    _pimpl->enqueueDigested(resource, checker);
  }

  void Fetcher::enqueue( const OnMediaLocation &resource, const FileChecker &checker  )
  {
    _pimpl->enqueue(resource, checker);
  }

  void Fetcher::addCachePath( const Pathname &cache_dir )
  {
    _pimpl->addCachePath(cache_dir);
  }

  void Fetcher::reset()
  {
    _pimpl->reset();
  }

  void Fetcher::start( const Pathname &dest_dir,
                       MediaSetAccess &media,
                       const ProgressData::ReceiverFnc & progress_receiver )
  {
    _pimpl->start(dest_dir, media, progress_receiver);
  }


  /******************************************************************
  **
  **	FUNCTION NAME : operator<<
  **	FUNCTION TYPE : std::ostream &
  */
  std::ostream & operator<<( std::ostream & str, const Fetcher & obj )
  {
    return str << *obj._pimpl;
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////

