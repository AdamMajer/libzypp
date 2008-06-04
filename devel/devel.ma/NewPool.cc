#include "Tools.h"

#include "zypp/PoolQueryResult.h"

#include <zypp/base/PtrTypes.h>
#include <zypp/base/Exception.h>
#include <zypp/base/Gettext.h>
#include <zypp/base/LogTools.h>
#include <zypp/base/Debug.h>
#include <zypp/base/Functional.h>
#include <zypp/base/IOStream.h>
#include <zypp/base/InputStream.h>
#include <zypp/base/ProvideNumericId.h>
#include <zypp/AutoDispose.h>

#include "zypp/ResPoolProxy.h"

#include "zypp/ZYppCallbacks.h"
#include "zypp/NVRAD.h"
#include "zypp/ResPool.h"
#include "zypp/ResFilters.h"
#include "zypp/ResObjects.h"
#include "zypp/Digest.h"
#include "zypp/PackageKeyword.h"
#include "zypp/TmpPath.h"
#include "zypp/ManagedFile.h"
#include "zypp/NameKindProxy.h"
#include "zypp/pool/GetResolvablesToInsDel.h"

#include "zypp/RepoManager.h"
#include "zypp/Repository.h"
#include "zypp/RepoInfo.h"

#include "zypp/repo/PackageProvider.h"

#include "zypp/ui/PatchContents.h"
#include "zypp/ResPoolProxy.h"

#include "zypp/sat/Pool.h"
#include "zypp/sat/LocaleSupport.h"
#include "zypp/sat/LookupAttr.h"
#include "zypp/sat/SolvableSet.h"
#include "zypp/sat/SolvIterMixin.h"
#include "zypp/sat/detail/PoolImpl.h"
#include "zypp/PoolQuery.h"

#include "zypp/parser/ProductConfReader.h"

#include <boost/mpl/int.hpp>

using namespace std;
using namespace zypp;
using namespace zypp::functor;
using namespace zypp::ui;

///////////////////////////////////////////////////////////////////

static const Pathname sysRoot( getenv("SYSROOT") ? getenv("SYSROOT") : "/Local/ROOT" );

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

bool queryInstalledEditionHelper( const std::string & name_r,
                                  const Edition &     ed_r,
                                  const Arch &        arch_r )
{
  if ( ed_r == Edition::noedition )
    return true;
  if ( name_r == "kernel-default" && ed_r == Edition("2.6.22.5-10") )
    return true;
  if ( name_r == "update-test-affects-package-manager" && ed_r == Edition("1.1-6") )
    return true;

  return false;
}


ManagedFile repoProvidePackage( const PoolItem & pi )
{
  ResPool _pool( getZYpp()->pool() );
  repo::RepoMediaAccess _access;

  // Redirect PackageProvider queries for installed editions
  // (in case of patch/delta rpm processing) to rpmDb.
  repo::PackageProviderPolicy packageProviderPolicy;
  packageProviderPolicy.queryInstalledCB( queryInstalledEditionHelper );

  Package::constPtr p = asKind<Package>(pi.resolvable());

  // Build a repository list for repos
  // contributing to the pool
  repo::DeltaCandidates deltas( repo::makeDeltaCandidates( _pool.knownRepositoriesBegin(),
                                                           _pool.knownRepositoriesEnd() ) );
  repo::PackageProvider pkgProvider( _access, p, deltas, packageProviderPolicy );
  return pkgProvider.providePackage();
}

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

namespace zypp
{
  template <class _LIterator, class _RIterator, class _Function>
      inline int invokeOnEach( _LIterator lbegin_r, _LIterator lend_r,
                               _RIterator rbegin_r, _RIterator rend_r,
                               _Function fnc_r )
      {
        int cnt = 0;
        for ( _LIterator lit = lbegin_r; lit != lend_r; ++lit )
        {
          for ( _RIterator rit = rbegin_r; rit != rend_r; ++rit )
          {
            ++cnt;
            if ( ! fnc_r( *lit, *rit ) )
              return -cnt;
          }
        }
        return cnt;
      }
}


void dbgDu( Selectable::Ptr sel )
{
  if ( sel->installedObj() )
  {
    DBG << "i: " << sel->installedObj() << endl
        << sel->installedObj()->diskusage() << endl;
  }
  if ( sel->candidateObj() )
  {
    DBG << "c: " << sel->candidateObj() << endl
        << sel->candidateObj()->diskusage() << endl;
  }
  INT << sel << endl
      << getZYpp()->diskUsage() << endl;
}

///////////////////////////////////////////////////////////////////

std::ostream & testDump( std::ostream & str, const PoolItem & pi )
{
  str << pi << endl;
  Package::constPtr p( asKind<Package>(pi) );
  if ( p )
  {
#define OUTS(V) str << str::form("%-25s: ",#V) << p->V() << endl
    Locale l( "de" );
    str << str::form("%-25s: ",l.code().c_str()) << p->summary(l) << endl;
    l = Locale( "fr" );
    str << str::form("%-25s: ",l.code().c_str()) << p->summary(l) << endl;
    l = Locale( "dsdf" );
    str << str::form("%-25s: ",l.code().c_str()) << p->summary(l) << endl;
    OUTS( summary );
    OUTS( installsize );
    OUTS( downloadSize );
    OUTS( sourcePkgName );
    OUTS( sourcePkgEdition );
    OUTS( checksum );
    OUTS( location );
#undef OUTS


  }
  return str;
}

struct Xprint
{
  bool operator()( const PoolItem & obj_r )
  {
    //MIL << obj_r << endl;
    //DBG << " -> " << obj_r->satSolvable() << endl;

    return true;
  }

  bool operator()( const sat::Solvable & obj_r )
  {
    //dumpOn( MIL, obj_r ) << endl;
    return true;
  }
};

///////////////////////////////////////////////////////////////////
struct SetTransactValue
{
  SetTransactValue( ResStatus::TransactValue newVal_r, ResStatus::TransactByValue causer_r )
  : _newVal( newVal_r )
  , _causer( causer_r )
  {}

  ResStatus::TransactValue   _newVal;
  ResStatus::TransactByValue _causer;

  bool operator()( const PoolItem & pi ) const
  {
    bool ret = pi.status().setTransactValue( _newVal, _causer );
    if ( ! ret )
      ERR << _newVal <<  _causer << " " << pi << endl;
    return ret;
  }
};

struct StatusReset : public SetTransactValue
{
  StatusReset()
  : SetTransactValue( ResStatus::KEEP_STATE, ResStatus::USER )
  {}
};

struct StatusInstall : public SetTransactValue
{
  StatusInstall()
  : SetTransactValue( ResStatus::TRANSACT, ResStatus::USER )
  {}
};

inline bool g( const NameKindProxy & nkp, Arch arch = Arch() )
{
  if ( nkp.availableEmpty() )
  {
    ERR << "No Item to select: " << nkp << endl;
    return false;
    ZYPP_THROW( Exception("No Item to select") );
  }

  if ( arch != Arch() )
  {
    typeof( nkp.availableBegin() ) it =  nkp.availableBegin();
    for ( ; it != nkp.availableEnd(); ++it )
    {
      if ( (*it)->arch() == arch )
	return (*it).status().setTransact( true, ResStatus::USER );
    }
  }

  return nkp.availableBegin()->status().setTransact( true, ResStatus::USER );
}

///////////////////////////////////////////////////////////////////

bool solve()
{
  bool rres = false;
  {
    //zypp::base::LogControl::TmpLineWriter shutUp;
    getZYpp()->resolver()->setIgnoreAlreadyRecommended( true );
    rres = getZYpp()->resolver()->resolvePool();
  }
  if ( ! rres )
  {
    ERR << "resolve " << rres << endl;
    return false;
  }
  MIL << "resolve " << rres << endl;
  return true;
}

bool install()
{
  SEC << getZYpp()->commit( ZYppCommitPolicy().dryRun() ) << endl;
  return true;
}

///////////////////////////////////////////////////////////////////

struct ConvertDbReceive : public callback::ReceiveReport<target::ScriptResolvableReport>
{
  virtual void start( const Resolvable::constPtr & script_r,
                      const Pathname & path_r,
                      Task task_r )
  {
    SEC << __FUNCTION__ << endl
    << "  " << script_r << endl
    << "  " << path_r   << endl
    << "  " << task_r   << endl;
  }

  virtual bool progress( Notify notify_r, const std::string & text_r )
  {
    SEC << __FUNCTION__ << endl
    << "  " << notify_r << endl
    << "  " << text_r   << endl;
    return true;
  }

  virtual void problem( const std::string & description_r )
  {
    SEC << __FUNCTION__ << endl
    << "  " << description_r << endl;
  }

  virtual void finish()
  {
    SEC << __FUNCTION__ << endl;
  }

};
///////////////////////////////////////////////////////////////////

struct DigestReceive : public callback::ReceiveReport<DigestReport>
{
  DigestReceive()
  {
    connect();
  }

  virtual bool askUserToAcceptNoDigest( const zypp::Pathname &file )
  {
    USR << endl;
    return false;
  }
  virtual bool askUserToAccepUnknownDigest( const Pathname &file, const std::string &name )
  {
    USR << endl;
    return false;
  }
  virtual bool askUserToAcceptWrongDigest( const Pathname &file, const std::string &requested, const std::string &found )
  {
    USR << "fle " << PathInfo(file) << endl;
    USR << "req " << requested << endl;
    USR << "fnd " << found << endl;
    return false;
  }
};

struct KeyRingSignalsReceive : public callback::ReceiveReport<KeyRingSignals>
{
  KeyRingSignalsReceive()
  {
    connect();
  }
  virtual void trustedKeyAdded( const PublicKey &/*key*/ )
  {
    USR << endl;
  }
  virtual void trustedKeyRemoved( const PublicKey &/*key*/ )
  {
    USR << endl;
  }
};

///////////////////////////////////////////////////////////////////

struct MediaChangeReceive : public callback::ReceiveReport<media::MediaChangeReport>
{
  virtual Action requestMedia( Url & source
                               , unsigned mediumNr
                               , const std::string & label
                               , Error error
                               , const std::string & description
                               , const std::vector<std::string> & devices
                               , unsigned int & dev_current )
  {
    SEC << __FUNCTION__ << endl
    << "  " << source << endl
    << "  " << mediumNr << endl
    << "  " << label << endl
    << "  " << error << endl
    << "  " << description << endl
    << "  " << devices << endl
    << "  " << dev_current << endl;
    return IGNORE;
  }
};

///////////////////////////////////////////////////////////////////

namespace container
{
  template<class _Tp>
    bool isIn( const std::set<_Tp> & cont, const typename std::set<_Tp>::value_type & val )
    { return cont.find( val ) != cont.end(); }
}
///////////////////////////////////////////////////////////////////

void itCmp( const sat::Pool::SolvableIterator & l, const sat::Pool::SolvableIterator & r )
{
  SEC << *l << " - " << *r << endl;
  INT << "== " << (l==r) << endl;
  INT << "!= " << (l!=r) << endl;
}

bool isTrue()  { return true; }
bool isFalse() { return false; }

void dumpIdStr()
{
  for ( int i = -3; i < 30; ++i )
  {
    DBG << i << '\t' << IdString( i ) << endl;
  }
}

void ttt( const char * lhs, const char * rhs )
{
  DBG << lhs << " <=> " << rhs << " --> " << ::strcmp( lhs, rhs ) << endl;
}

namespace zypp
{
namespace filter
{
  template <class _MemFun, class _Value>
  class HasValue
  {
    public:
      HasValue( _MemFun fun_r, _Value val_r )
      : _fun( fun_r ), _val( val_r )
      {}
      template <class _Tp>
      bool operator()( const _Tp & obj_r ) const
      { return( _fun && (obj_r.*_fun)() == _val ); }
    private:
      _MemFun _fun;
      _Value  _val;
  };

  template <class _MemFun, class _Value>
  HasValue<_MemFun, _Value> byValue( _MemFun fun_r, _Value val_r )
  { return HasValue<_MemFun, _Value>( fun_r, val_r ); }
}

}

template <class L>
struct _TestO { _TestO( const L & lhs ) : _lhs( lhs ) {} const L & _lhs; };

template <class L>
std::ostream & operator<<( std::ostream & str, const _TestO<L> & obj )
{ const L & lhs( obj._lhs); return str << (lhs?'_':'*') << (lhs.empty()?'e':'_') << "'" << lhs << "'"; }

template <class L>
_TestO<L> testO( const L & lhs )
{ return _TestO<L>( lhs ); }

template <class L, class R>
void testCMP( const L & lhs, const R & rhs )
{
  MIL << "LHS " << testO(lhs) << endl;
  MIL << "RHS " << rhs << endl;

#define OUTS(S) DBG << #S << ": " << (S) << endl
  OUTS( lhs.compare(rhs) );
  OUTS( lhs != rhs );
  OUTS( lhs <  rhs );
  OUTS( lhs <= rhs );
  OUTS( lhs == rhs );
  OUTS( lhs >= rhs );
  OUTS( lhs >  rhs );
#undef OUTS
}

namespace zypp
{
}

/******************************************************************
**
**      FUNCTION NAME : main
**      FUNCTION TYPE : int
*/
int main( int argc, char * argv[] )
try {
  --argc;
  ++argv;
  zypp::base::LogControl::instance().logToStdErr();
  INT << "===[START]==========================================" << endl;
  ZConfig::instance();

  ResPool   pool( ResPool::instance() );
  sat::Pool satpool( sat::Pool::instance() );

  if ( 1 )
  {
    RepoManager repoManager( makeRepoManager( sysRoot ) );
    RepoInfoList repos = repoManager.knownRepositories();

    // launch repos
    for ( RepoInfoList::iterator it = repos.begin(); it != repos.end(); ++it )
    {
      RepoInfo & nrepo( *it );
      SEC << nrepo << endl;

      if ( ! nrepo.enabled() )
        continue;

      if ( ! repoManager.isCached( nrepo ) || /*force*/false )
      {
        if ( repoManager.isCached( nrepo ) )
        {
          SEC << "cleanCache" << endl;
          repoManager.cleanCache( nrepo );
        }
        //SEC << "refreshMetadata" << endl;
        //repoManager.refreshMetadata( nrepo, RepoManager::RefreshForced );
        SEC << "buildCache" << endl;
        repoManager.buildCache( nrepo );
      }
    }

    // create from cache:
    {
      Measure x( "CREATE FROM CACHE" );
      for ( RepoInfoList::iterator it = repos.begin(); it != repos.end(); ++it )
      {
        RepoInfo & nrepo( *it );
        if ( ! nrepo.enabled() )
          continue;

        Measure x( "CREATE FROM CACHE "+nrepo.alias() );
        try
        {
          repoManager.loadFromCache( nrepo );
        }
        catch ( const Exception & exp )
        {
          MIL << "Try to rebuild cache..." << endl;
          SEC << "cleanCache" << endl;
          repoManager.cleanCache( nrepo );
          SEC << "buildCache" << endl;
          repoManager.buildCache( nrepo );
          SEC << "Create from cache" << endl;
          repoManager.loadFromCache( nrepo );
        }

        USR << "pool: " << pool << endl;
      }
    }
  }

  if ( 0 )
  {
    Measure x( "INIT TARGET" );
    {
      {
        zypp::base::LogControl::TmpLineWriter shutUp;
        getZYpp()->initializeTarget( sysRoot );
      }
      getZYpp()->target()->load();
    }
  }

  dumpRange( USR, satpool.reposBegin(), satpool.reposEnd() );
  USR << "pool: " << pool << endl;

  ///////////////////////////////////////////////////////////////////

  if ( 0 )
  {
    Measure x( "Upgrade" );
    UpgradeStatistics u;
    getZYpp()->resolver()->doUpgrade( u );
  }
  ///////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////

  for_( it, pool.byKindBegin<Package>(), pool.byKindEnd<Package>() )
  {
    it->status().setTransact( true, ResStatus::USER );
  }

  SEC << zypp::getZYpp()->diskUsage() << endl;


  MIL << (*pool.byKindBegin<SrcPackage>())->provides() << endl;

  MIL << (Capability( "srcpackage:foo == 1.0" ).detail()) << endl;
  MIL << (Capability( "foo == 1.0", ResKind::srcpackage ).detail()) << endl;
  MIL << (Capability( "foo.src == 1.0" ).detail()) << endl;
  MIL << (Capability( "foo.i386 == 1.0" ).detail()) << endl;

  MIL << (Capability( "srcpackage:foo == 1.0" )) << endl;
  MIL << (Capability( "foo == 1.0", ResKind::srcpackage )) << endl;
  MIL << (Capability( "foo.src == 1.0" )) << endl;
  MIL << (Capability( "foo.i386 == 1.0" )) << endl;

  SEC << sat::WhatProvides( Capability( "srcpackage:zypper" ) ) << endl;;
  SEC << sat::WhatProvides( Capability( "zypper", ResKind::srcpackage  ) ) << endl;;
  SEC << sat::WhatProvides( Capability( "zypper.src" ) ) << endl;;

  ///////////////////////////////////////////////////////////////////
  INT << "===[END]============================================" << endl << endl;
  zypp::base::LogControl::instance().logNothing();
  return 0;
  SEC << zypp::getZYpp()->diskUsage() << endl;

//   for_( it, pool.byKindBegin<SrcPackage>(), pool.byKindEnd<SrcPackage>() )
  {
//     MIL << *it << endl;
  }

//   for_( it, pool.byIdentBegin( ResKind::srcpackage, "zypper" ), pool.byIdentEnd( ResKind::srcpackage, "zypper" ) )
  {
//     WAR << *it << endl;
  }


  ui::Selectable::Ptr srcp( zypp::getZYpp()->poolProxy().lookup( ResKind::srcpackage, "zypper" ) );
  MIL << srcp << endl;
  MIL << srcp->setStatus( S_Install ) << endl;
  MIL << srcp << endl;
  solve();
  vdumpPoolStats( USR << "Transacting:"<< endl,
                  make_filter_begin<resfilter::ByTransact>(pool),
                  make_filter_end<resfilter::ByTransact>(pool) ) << endl;
  ///////////////////////////////////////////////////////////////////
  INT << "===[END]============================================" << endl << endl;
  zypp::base::LogControl::instance().logNothing();
  return 0;

  if ( 0 )
  {
    PoolItem pi ( getPi<Package>("amarok") );
    MIL << pi << endl;
    if ( pi )
    {
      pi.status().setTransact( true, ResStatus::USER );
      solve();
      vdumpPoolStats( USR << "Transacting:"<< endl,
                      make_filter_begin<resfilter::ByTransact>(pool),
                      make_filter_end<resfilter::ByTransact>(pool) ) << endl;
    }
  }

  SEC << zypp::getZYpp()->diskUsage() << endl;

  //vdumpPoolStats( USR << "Pool:"<< endl, pool.begin(), pool.end() ) << endl;
  //waitForInput();

  //std::for_each( pool.begin(), pool.end(), Xprint() );

  ///////////////////////////////////////////////////////////////////
  INT << "===[END]============================================" << endl << endl;
  zypp::base::LogControl::instance().logNothing();
  return 0;
}
catch ( const Exception & exp )
{
  INT << exp << endl << exp.historyAsString();
}
catch (...)
{}


