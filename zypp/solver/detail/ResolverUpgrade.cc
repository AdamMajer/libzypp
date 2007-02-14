/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/* ResolverUpgrade.cc
 *
 * Implements the distribution upgrade algorithm.
 *
 * Copyright (C) 2005 SUSE Linux Products GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/*
  stolen from PMPackageManager_update.cc
  original author Michael Andres <ma@suse.de>
  zypp port by Klaus Kaempf <kkaempf@suse.de>

/-*/

#include "zypp/CapSet.h"
#include "zypp/capability/SplitCap.h"

#include "zypp/base/Logger.h"
#include "zypp/base/String.h"
#include "zypp/base/Gettext.h"
#include "zypp/base/Exception.h"

#include "zypp/base/Algorithm.h"
#include "zypp/ResPool.h"
#include "zypp/ResStatus.h"
#include "zypp/ResFilters.h"
#include "zypp/CapFilters.h"
#include "zypp/Capability.h"
#include "zypp/CapFactory.h"
#include "zypp/VendorAttr.h"
#include "zypp/Package.h"

#include "zypp/capability/CapabilityImpl.h"
#include "zypp/ZYppFactory.h"

#include "zypp/solver/detail/Types.h"
#include "zypp/solver/detail/Helper.h"
#include "zypp/solver/detail/Resolver.h"

#include "zypp/Target.h"

/////////////////////////////////////////////////////////////////////////
namespace zypp
{ ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  namespace solver
  { /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
    namespace detail
    { ///////////////////////////////////////////////////////////////////

using namespace std;
using namespace zypp;
using zypp::capability::SplitCap;


/** Order on AvialableItemSet.
 * \li best Arch
 * \li best Edition
 * \li ResObject::constPtr as fallback.
*/
struct AVOrder : public std::binary_function<PoolItem_Ref,PoolItem_Ref,bool>
{
    // NOTE: operator() provides LESS semantics to order the set.
    // So LESS means 'prior in set'. We want 'better' archs and
    // 'better' editions at the beginning of the set. So we return
    // TRUE if (lhs > rhs)!
    //
    bool operator()( const PoolItem_Ref lhs, const PoolItem_Ref rhs ) const
        {
	    int res = lhs->arch().compare( rhs->arch() );
	    if ( res )
		return res > 0;
	    res = lhs->edition().compare( rhs->edition() );
	    if ( res )
		return res > 0;

	    // no more criteria, still equal:
	    // use the ResObject::constPtr (the poiner value)
	    // (here it's arbitrary whether < or > )
	    return lhs.resolvable() < rhs.resolvable();
        }
};

typedef std::set<PoolItem_Ref, AVOrder> PoolItemOrderSet;



// check if downgrade is allowed
// (Invariant on entry: installed.edition >= candidate.edition)
//
// candidate must have allowed vendor (e.g. 'SuSE', 'Novell', ...) and candidates buildtime must be
// newer.

static bool
downgrade_allowed( PoolItem_Ref installed, PoolItem_Ref candidate, bool silent_downgrades )
{
    if (installed.status().isLocked()) {
	MIL << "Installed " << installed << " is locked, not upgrading" << endl;
	return false;
    }

    Resolvable::constPtr ires = installed.resolvable();
    Package::constPtr ipkg = asKind<Package>(ires);
    Resolvable::constPtr cres = candidate.resolvable();
    Package::constPtr cpkg = asKind<Package>(cres);

    if (ipkg)
      DBG << "Installed vendor '" << ipkg->vendor() << "'" << endl;
    if (cpkg)
      DBG << "Candidate vendor '" << cpkg->vendor() << "'" << endl;

    if (cpkg
	&& VendorAttr::instance().isKnown( cpkg->vendor() ) )
    {
	if ( silent_downgrades )
	    return true;
	if ( ipkg->buildtime() < cpkg->buildtime() ) {			// installed has older buildtime
	    MIL << "allowed downgrade " << installed << " to " << candidate << endl;
	    return true;						// see bug #152760
	}
    }
    return false;
}



struct FindObsoletes
{
    bool obsoletes;

    FindObsoletes ()
	: obsoletes (false)
    { }

    bool operator()( const CapAndItem & cai )
    {
	obsoletes = true;				// we have a match
	return false;					// stop looping here
    }
};


// does the candidate obsolete the capability ?

bool
Resolver::doesObsoleteCapability (PoolItem_Ref candidate, const Capability & cap)
{
    _DEBUG("doesObsoleteCapability " << candidate << ", " << cap);

    Dep dep (Dep::OBSOLETES);
    FindObsoletes info;
    invokeOnEach( _pool.byCapabilityIndexBegin( cap.index(), dep ),
		  _pool.byCapabilityIndexEnd( cap.index(), dep ),
		  resfilter::ByCapMatch( cap ),
		  functor::functorRef<bool,CapAndItem>(info) );

    _DEBUG((info.obsoletes ? "YES" : "NO"));
    return info.obsoletes;
}


bool
Resolver::doesObsoleteItem (PoolItem_Ref candidate, PoolItem_Ref installed)
{
    CapFactory factory;
    Capability installedCap =  factory.parse ( installed->kind(), installed->name(), Rel::EQ, installed->edition());

    return doesObsoleteCapability (candidate, installedCap);
}


//-----------------------------------------------------------------------------


// find best available providers for installed name

typedef map<string, PoolItem_Ref> FindMap;

struct FindProviders
{
    FindMap providers;		// the best providers which matched

    FindProviders ()
    { }

    bool operator()( const CapAndItem & cai )
    {
	PoolItem provider( cai.item );
	if ( provider.status().isToBeUninstalled() ) {
	    MIL << "  IGNORE relation match (package is tagged to delete): " << cai.cap << " ==> " << provider << endl;
	}
	else {
	    FindMap::iterator it = providers.find( provider->name() );

	    if (it != providers.end()) {				// provider with same name found
		int cmp = it->second->arch().compare( provider->arch() );
		if (cmp < 0) {						// new provider has better arch
		    it->second = provider;
		}
		else if (cmp == 0) {					// new provider has equal arch
		    if (it->second->edition().compare( provider->edition() ) < 0) {
			it->second = provider;				// new provider has better edition
		    }
		}
	    }
	    else {
		providers[provider->name()] = provider;
	    }
	}
	return true;
    }
};


//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : Resolver::doUpgrade
//	METHOD TYPE : int
//
//	DESCRIPTION : go through all installed (but not yet touched by user)
//		packages and look for update candidates
//		handle splitprovides and replaced and dropped
//
void
Resolver::doUpgrade( UpgradeStatistics & opt_stats_r )
{
  typedef map<PoolItem_Ref,PoolItem_Ref> CandidateMap;
  typedef intrusive_ptr<const SplitCap> SplitCapPtr;
  typedef map<PoolItem_Ref,PoolItemOrderSet> SplitMap;
  typedef map<PoolItem_Ref,PoolItemOrderSet> TodoMap;

  CandidateMap candidatemap;

  SplitMap    splitmap;
  TodoMap     applyingSplits;
  TodoMap     addSplitted;
  TodoMap     addProvided;
  TodoMap     addMultiProvided;

  Target_Ptr target;
  try {
	target = getZYpp()->target();
  }
  catch( const Exception & excpt_r) {
	ERR << "Huh, no target ?";
	ZYPP_CAUGHT(excpt_r);
	if (!_testing) return;		// can't continue without target
	MIL << "Running in test mode, continuing without target" << endl;
  }
  MIL << "target at " << target << endl;

  MIL << "doUpgrade start... "
    << "(delete_unmaintained:" << (opt_stats_r.delete_unmaintained?"yes":"no") << ")"
    << "(silent_downgrades:" << (opt_stats_r.silent_downgrades?"yes":"no") << ")"
    << "(keep_installed_patches:" << (opt_stats_r.keep_installed_patches?"yes":"no") << ")"
    << endl;

  _update_items.clear();
  {
    UpgradeOptions opts( opt_stats_r );
    opt_stats_r = UpgradeStatistics();
    (UpgradeOptions&)opt_stats_r = opts;
  }

  ///////////////////////////////////////////////////////////////////
  // Reset all auto states and build PoolItemOrderSet of available candidates
  // (those that do not belong to PoolItems set to delete).
  //
  // On the fly remember splitprovides and afterwards check, which
  // of them do apply.
  ///////////////////////////////////////////////////////////////////
  PoolItemOrderSet available; // candidates available for install (no matter if selected for install or not)

  for ( ResPool::const_iterator it = _pool.begin(); it != _pool.end(); ++it ) {
    PoolItem_Ref item = *it;
    PoolItem_Ref candidate;
    PoolItem_Ref installed;

    if ( item.status().isToBeUninstalled() ) {
      MIL << "doUpgrade available: SKIP to delete " << item << endl;
      ++opt_stats_r.pre_todel;
      continue;
    }
    if ( item.status().isLocked() ) {
      MIL << "doUpgrade available: SKIP locked " << item << endl;
      if ( item.status().staysInstalled() ) {
	++opt_stats_r.pre_nocand;
      }
      continue;
    }

    if ( item.status().staysInstalled() ) {	// installed item
      installed = item;
      CandidateMap::const_iterator cand_it = candidatemap.find( installed );
      if (cand_it != candidatemap.end()) {
	candidate = cand_it->second;				// found candidate already
      }
      else {
	candidate = Helper::findUpdateItem( _pool, installed );	// find 'best' upgrade candidate
      }
      if (!candidate) {
	MIL << "doUpgrade available: SKIP no candidate for " << installed << endl;
	++opt_stats_r.pre_nocand;
	continue;
      }
      MIL << "item " << item << " is installed, candidate is " << candidate << endl;
      if (candidate.status().isSeen()) {			// seen already
	candidate.status().setSeen(true);
	continue;
      }
      candidate.status().setSeen(true);				// mark as seen
      candidatemap[installed] = candidate;
    }
    else {					// assume Uninstalled
      if (item.status().isSeen()) {				// seen already
	item.status().setSeen(true);
	continue;
      }
      candidate = item;
      candidate.status().setSeen(true);				// mark as seen
      installed = Helper::findInstalledItem( _pool, candidate );
      if (installed) {						// check if we already have an installed
	if ( installed.status().isLocked() ) {
	  MIL << "doUpgrade available: SKIP candidate " << candidate << ", locked " << installed << endl;
	  continue;
	}

        MIL << "found installed " << installed << " for item " << candidate << endl;
	CandidateMap::const_iterator cand_it = candidatemap.find( installed );
	if (cand_it == candidatemap.end()						// not in map yet
	    || (cand_it->second->arch().compare( candidate->arch() ) < 0)		// or the new has better architecture
	    || ((cand_it->second->arch().compare( candidate->arch() ) == 0)		// or the new has the same architecture
		&& (cand_it->second->edition().compare( candidate->edition() ) < 0) ) )	//   and a better edition (-> 157501)
	{
	    candidatemap[installed] = candidate;				// put it in !
	}
      }
    }

    ++opt_stats_r.pre_avcand;
    available.insert( candidate );

    MIL << "installed " << installed << ", candidate " << candidate << endl;

    // remember any splitprovides to packages actually installed.
    CapSet caps = candidate->dep (Dep::PROVIDES);
    for (CapSet::iterator cit = caps.begin(); cit != caps.end(); ++cit ) {
	if (isKind<capability::SplitCap>( *cit ) ) {

	    capability::CapabilityImpl::SplitInfo splitinfo = capability::CapabilityImpl::getSplitInfo( *cit );

	    PoolItem splititem = Helper::findInstalledByNameAndKind (_pool, splitinfo.name, ResTraits<zypp::Package>::kind);
            MIL << "has split cap " << splitinfo.name << ":" << splitinfo.path << ", splititem:" << splititem << endl;
	    if (splititem) {
		if (target) {
		    ResObject::constPtr robj = target->whoOwnsFile( splitinfo.path );
                    if (robj)
                      MIL << "whoOwnsFile(): " << *robj << endl;
		    if (robj
			&& robj->name() == splitinfo.name)
		    {
                      MIL << "split matched !" << endl;
			splitmap[splititem].insert( candidate );
		    }
		}
	    }
	}
    }

  } // iterate over the complete pool

  // reset all seen (for next run)
  for ( ResPool::const_iterator it = _pool.begin(); it != _pool.end(); ++it ) {
	it->status().setSeen( false );
  }

  MIL << "doUpgrade: " << opt_stats_r.pre_todel  << " packages tagged to delete" << endl;
  MIL << "doUpgrade: " << opt_stats_r.pre_nocand << " packages without candidate (foreign, replaced or dropped)" << endl;
  MIL << "doUpgrade: " << opt_stats_r.pre_avcand << " packages available for update" << endl;

  MIL << "doUpgrade: going to check " << splitmap.size() << " probably splitted packages" << endl;
  {
    ///////////////////////////////////////////////////////////////////
    // splitmap entries are gouped by PoolItems (we know this). So get the
    // filelist as a new PoolItem occures, and use it for consecutive entries.
    //
    // On the fly build SplitPkgMap from splits that do apply (i.e. file is
    // in PoolItems's filelist). The way splitmap was created, candidates added
    // are not initially tagged to delete!
    ///////////////////////////////////////////////////////////////////

    PoolItem_Ref citem;

    for ( SplitMap::iterator it = splitmap.begin(); it != splitmap.end(); ++it ) {
	applyingSplits[it->first].insert( it->second.begin(), it->second.end() );
	_DEBUG("  split count for " << it->first->name() << " now " << applyingSplits[it->first].size());
    }
    splitmap.clear();
  }

  ///////////////////////////////////////////////////////////////////
  // Now iterate installed packages, not selected to delete, and
  // figure out what might be an appropriate replacement. Current
  // packages state is changed immediately. Additional packages are
  // reported but set to install later.
  ///////////////////////////////////////////////////////////////////
  MIL << "doUpgrade pass 1..." << endl;

  for ( ResPool::const_iterator it = _pool.begin(); it != _pool.end(); ++it ) {

    PoolItem_Ref installed(*it);
    ResStatus status (installed.status());

    if ( ! status.staysInstalled() ) {
      continue;
    }
    ++opt_stats_r.chk_installed_total;

    if ( status.transacts() ) {					// we know its installed, if it transacts also
      MIL << "SKIP to delete: " << installed.resolvable() << endl;	// it'll be deleted
      ++opt_stats_r.chk_already_todel;
      continue;
    }

    if ( installed.status().isLocked() ) {			// skip locked
      MIL << "SKIP taboo: " << installed << endl;
      ++opt_stats_r.chk_is_taboo;
      _update_items.push_back( installed );			// remember in problem list
      continue;
    }

    if ( isKind<Patch>(installed.resolvable())
         || isKind<Atom>(installed.resolvable())
         || isKind<Script>(installed.resolvable())
         || isKind<Message>(installed.resolvable()) )
      {
        if ( ! opt_stats_r.keep_installed_patches )
          {
            if ( isKind<Patch>(installed.resolvable()) )
              MIL << "OUTDATED Patch: " << installed << endl;
            installed.status().setToBeUninstalled( ResStatus::APPL_HIGH );
          }
        else
          {
            if ( isKind<Patch>(installed.resolvable()) )
              MIL << "SKIP Patch: " << installed << endl;
          }
        continue;
      }

    CandidateMap::iterator cand_it = candidatemap.find( installed );

    bool probably_dropped = false;

    MIL << "REPLACEMENT FOR " << installed << endl;
    ///////////////////////////////////////////////////////////////////
    // figure out replacement
    ///////////////////////////////////////////////////////////////////
    if ( cand_it != candidatemap.end() ) {

      PoolItem_Ref candidate (cand_it->second);

      if ( ! candidate.status().isToBeInstalled() ) {
	int cmp = installed->edition().compare( candidate->edition() );
	if ( cmp < 0 ) {						// new edition
	  candidate.status().setToBeInstalled( ResStatus::APPL_HIGH );
	  MIL << " ==> INSTALL (new version): " << candidate << endl;
	  ++opt_stats_r.chk_to_update;
	} else {							// older or equal edition
	  // check whether to downgrade:

	  if (cmp == 0							// equal
	      || !downgrade_allowed( installed, candidate,
                                     opt_stats_r.silent_downgrades) )	//  or downgrade not allowed
	  {
	    MIL << " ==> (keep installed)" << candidate << endl;	// keep installed
	    ++opt_stats_r.chk_to_keep_installed;
	  } else {							// older and downgrade allowed
	    candidate.status().setToBeInstalled( ResStatus::APPL_HIGH );
	    MIL << " ==> INSTALL (SuSE version downgrade): " << candidate << endl;
	    ++opt_stats_r.chk_to_downgrade;
	  }
	}
      } else {
	MIL << " ==> INSTALL (preselected): " << candidate << endl;
	++opt_stats_r.chk_already_toins;
      }

    }
    else {		// no candidate

      // replaced or dropped (anyway there's no candidate for this!)
      // If unique provides exists check if obsoleted (replaced).
      // Remember new package for 2nd pass.

      Dep dep (Dep::PROVIDES);
      CapFactory factory;
      Capability installedCap = factory.parse( installed->kind(), installed->name(), Rel::EQ, installed->edition() );

      FindProviders info;

      invokeOnEach( _pool.byCapabilityIndexBegin( installed->name(), dep ),
		    _pool.byCapabilityIndexEnd( installed->name(), dep ),
		    functor::chain( resfilter::ByCaIUninstalled(),
				    resfilter::ByCapMatch( installedCap ) ) ,
		    functor::functorRef<bool,CapAndItem>(info) );

      int num_providers = info.providers.size();

      _DEBUG("lookup " << num_providers << " provides for installed " << installedCap);

      // copy from map to set
      PoolItemOrderSet providers;
      for (FindMap::const_iterator mapit = info.providers.begin(); mapit != info.providers.end(); ++mapit) {
	providers.insert( mapit->second );
      }

      switch ( info.providers.size() ) {
      case 0:
	MIL << " ==> (dropped)" << endl;
	// wait untill splits are processed. Might be a split obsoletes
	// this one (i.e. package replaced but not provided by new one).
	// otherwise it's finaly dropped.
	probably_dropped = true;
	break;
      case 1:
        addProvided[installed] = providers;
	MIL << " ==> REPLACED by: " << (*providers.begin()) << endl;
	// count stats later
	// check obsoletes later
	break;
      default:
	addMultiProvided[installed] = providers;
	MIL << " ==> pass 2 (" << providers.size() << " times provided)" << endl;
	// count stats later
	// check obsoletes later
	break;
      }

    }	// no candidate

    ///////////////////////////////////////////////////////////////////
    // anyway check for packages split off
    ///////////////////////////////////////////////////////////////////

    TodoMap::iterator sit = applyingSplits.find( installed );
    if ( sit != applyingSplits.end() ) {
      PoolItemOrderSet & toadd( sit->second );
      if ( !toadd.size() ) {
	INT << "Empty SplitPkgMap entry for " << installed << endl;
      } else {
	for ( PoolItemOrderSet::iterator ait = toadd.begin(); ait != toadd.end(); ++ait ) {
	  PoolItem_Ref split_candidate = *ait;
	  MIL << " ==> ADD (splitted): " << split_candidate << endl;
	  if ( probably_dropped
	       && split_candidate.status().staysUninstalled()
	       && doesObsoleteItem (split_candidate, installed))
	  {
	    probably_dropped = false;
	  }
	}
	addSplitted[installed] = toadd;
      }
      // count stats later
    }

    ///////////////////////////////////////////////////////////////////
    // now handle dropped package
    ///////////////////////////////////////////////////////////////////

    if ( probably_dropped ) {
      if ( opt_stats_r.delete_unmaintained ) {
	installed.status().setToBeUninstalled( ResStatus::APPL_HIGH );
      }
      ++opt_stats_r.chk_dropped;
      _update_items.push_back( installed );
    }

  } // pass 1 end

  ///////////////////////////////////////////////////////////////////
  // Now check the remembered packages and check non unique provided.
  // Maybe one of them was somehow selected. Otherwise we have to guess
  // one.
  ///////////////////////////////////////////////////////////////////
  MIL << "doUpgrade pass 2..." << endl;

  // look at the ones with a single provide first

  for ( TodoMap::iterator it = addProvided.begin(); it != addProvided.end(); ++it ) {

    PoolItemOrderSet & tset( it->second );		// these are the providers (well, just one)

    for ( PoolItemOrderSet::iterator sit = tset.begin(); sit != tset.end(); ++sit ) {
      PoolItem_Ref provider (*sit);

      if (provider.status().setToBeInstalled( ResStatus::APPL_HIGH )) {
	++opt_stats_r.chk_replaced;
      }

      // needs installed

      if ( doesObsoleteItem (provider, it->first ) ) {
	it->first.status().setToBeUninstalled( ResStatus::APPL_HIGH );
      }
    }

  }

  // look at the split providers

  for ( TodoMap::iterator it = addSplitted.begin(); it != addSplitted.end(); ++it ) {

    PoolItemOrderSet & tset( it->second );
    PoolItem_Ref lastItem = PoolItem_Ref();

    for ( PoolItemOrderSet::iterator sit = tset.begin(); sit != tset.end(); ++sit ) {
	if (!lastItem
	    || compareByN ( lastItem.resolvable(), sit->resolvable()) != 0) // do not install packages with the same NVR and other architecture
	{
	    PoolItem_Ref item( *sit );

	    // only install split if its actually a different edition

	    PoolItem_Ref already_installed = Helper::findInstalledItem( _pool, item );
	    if (!already_installed
		|| already_installed->edition().compare( item->edition() ) != 0 )
	    {
		if (item.status().setToBeInstalled( ResStatus::APPL_HIGH )) {
		    ++opt_stats_r.chk_add_split;
		}
	    }
	}
	lastItem = *sit;
    }

  }

  // look at the ones with multiple providers

  for ( TodoMap::iterator it = addMultiProvided.begin(); it != addMultiProvided.end(); ++it ) {
    MIL << "GET ONE OUT OF " << it->second.size() << " for " << it->first << endl;

    PoolItem_Ref guess;
    PoolItemOrderSet & gset( it->second );

    for ( PoolItemOrderSet::iterator git = gset.begin(); git != gset.end(); ++git ) {
      PoolItem_Ref item (*git);

      if (git == gset.begin())		// default to first of set; the set is ordered, first is the best
	guess = item;

      if ( item.status().isToBeInstalled()) {
	MIL << " ==> (pass 2: meanwhile set to install): " << item << endl;
	if ( ! doesObsoleteItem (item, it->first ) ) {
	  it->first.status().setToBeUninstalled( ResStatus::APPL_HIGH );
	}
	guess = PoolItem_Ref();
	break;
      } else {
	// Be prepared to guess.
	// Most common situation for guessing is something like:
	//   qt-devel
	//   qt-devel-experimental
	//   qt-devel-japanese
	// That's why currently the shortest package name wins.
	if ( !guess || guess->name().size() > item->name().size() ) {
	  guess = item;
	}
      }
    }

    if ( guess ) {
      guess.status().setToBeInstalled( ResStatus::APPL_HIGH );
      MIL << " ==> REPLACED by: (pass 2: guessed): " << guess << endl;
      if ( ! doesObsoleteItem (guess, it->first ) ) {
	it->first.status().setToBeUninstalled( ResStatus::APPL_HIGH );
      }
      ++opt_stats_r.chk_replaced_guessed;
    }
  }

  ///////////////////////////////////////////////////////////////////
  // done
  ///////////////////////////////////////////////////////////////////
  MIL << opt_stats_r << endl;

  // Setting Resolver to upgrade mode
  _upgradeMode = true;
}

///////////////////////////////////////////////////////////////////
    };// namespace detail
    /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
  };// namespace solver
  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
};// namespace zypp
/////////////////////////////////////////////////////////////////////////


