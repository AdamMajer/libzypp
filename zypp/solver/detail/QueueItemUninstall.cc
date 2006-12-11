/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* QueueItemUninstall.cc
 *
 * Copyright (C) 2000-2002 Ximian, Inc.
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

#include "zypp/CapSet.h"
#include "zypp/base/Logger.h"
#include "zypp/base/String.h"
#include "zypp/base/Gettext.h"

#include "zypp/base/Algorithm.h"
#include "zypp/ResPool.h"
#include "zypp/ResFilters.h"
#include "zypp/CapFilters.h"
#include "zypp/CapFactory.h"
#include "zypp/Patch.h"

#include "zypp/solver/detail/QueueItemUninstall.h"
#include "zypp/solver/detail/QueueItemEstablish.h"
#include "zypp/solver/detail/QueueItemRequire.h"
#include "zypp/solver/detail/QueueItem.h"
#include "zypp/solver/detail/ResolverContext.h"
#include "zypp/solver/detail/ResolverInfoMisc.h"
#include "zypp/solver/detail/ResolverInfoMissingReq.h"

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

IMPL_PTR_TYPE(QueueItemUninstall);

//---------------------------------------------------------------------------

std::ostream &
QueueItemUninstall::dumpOn( std::ostream & os ) const
{
    os << "[" << (_soft?"Soft":"") << "Uninstall: ";

    os << _item;
    os << " (";
    switch (_reason) {
	case QueueItemUninstall::CONFLICT:	os << "conflicts"; break;
	case QueueItemUninstall::OBSOLETE:	os << "obsoletes"; break;
	case QueueItemUninstall::UNSATISFIED:	os << "unsatisfied dependency"; break;
	case QueueItemUninstall::BACKOUT:	os << "uninstallable"; break;
	case QueueItemUninstall::UPGRADE:	os << "upgrade"; break;
	case QueueItemUninstall::DUPLICATE:	os << "duplicate"; break;
	case QueueItemUninstall::EXPLICIT:	os << "explicit"; break;
    }
    os << ")";
    if (_cap_leading_to_uninstall != Capability::noCap) {
	os << ", Triggered By ";
	os << _cap_leading_to_uninstall;
    }
    if (_upgraded_to) {
	os << ", Upgraded To ";
	os << _upgraded_to;
    }
    if (_explicitly_requested) os << ", Explicit";
    if (_remove_only) os << ", Remove Only";
    if (_due_to_conflict) os << ", Due To Conflict";
    if (_due_to_obsolete)
	os << ", Due To Obsolete:" << _obsoletes_item;
    if (_unlink) os << ", Unlink";
    os << "]";
    return os;
}

//---------------------------------------------------------------------------

QueueItemUninstall::QueueItemUninstall (const ResPool & pool, PoolItem_Ref item, UninstallReason reason, bool soft)
    : QueueItem (QUEUE_ITEM_TYPE_UNINSTALL, pool)
    , _item (item)
    , _reason (reason)
    , _soft (soft)
    , _cap_leading_to_uninstall (Capability())
    , _upgraded_to (NULL)
    , _explicitly_requested (false)
    , _remove_only (false)
    , _due_to_conflict (false)
    , _due_to_obsolete (false)
    , _unlink (false)
    , _obsoletes_item (NULL)
{
    _XDEBUG("QueueItemUninstall::QueueItemUninstall(" << item << ")");
}


QueueItemUninstall::~QueueItemUninstall()
{
}

//---------------------------------------------------------------------------

void
QueueItemUninstall::setUnlink ()
{
    _unlink = true;
    /* Reduce the priority so that unlink items will tend to get
       processed later.  We want to process unlinks as late as possible...
       this will make our "is this item in use" check more accurate. */
    setPriority (0);

    return;
}

//---------------------------------------------------------------------------

struct UnlinkCheck
{
    ResolverContext_Ptr context;
    bool cancel_unlink;

    // An item is to be uninstalled, it provides a capability
    //   which requirer needs (as match)
    //   if requirer is installed or to-be-installed:
    //     check if anyone else provides it to the requirer
    //	   or if the uninstall breaks the requirer
    //	     in this case, we have to cancel the uninstallation

    bool operator()( const CapAndItem & cai )
    {
	if (cancel_unlink)				// already cancelled
	    return true;

	if (! context->isPresent (cai.item))		// item is not (to-be-)installed
	    return true;

	if (context->requirementIsMet (cai.cap))	// another resolvable provided match
	    return true;

	cancel_unlink = true;				// cancel, as this would break dependencies

	return true;
    }
};

//---------------------------------------------------------------------------


struct UninstallProcess
{
    ResPool pool;
    ResolverContext_Ptr context;
    PoolItem_Ref uninstalled_item;
    PoolItem_Ref upgraded_item;
    QueueItemList & qil;
    bool remove_only;
    bool soft;

    UninstallProcess (const ResPool & p, ResolverContext_Ptr ct, PoolItem_Ref u1, PoolItem_Ref u2, QueueItemList & l, bool ro, bool s)
	: pool (p)
	, context (ct)
	, uninstalled_item (u1)
	, upgraded_item (u2)
	, qil (l)
	, remove_only (ro)
	, soft (s)
    { }

    // the uninstall of uninstalled_item breaks the dependency 'match' of resolvable 'requirer'

    bool operator()( const CapAndItem & cai )
    {
	PoolItem requirer( cai.item );
	if (! context->isPresent (requirer))				// its not installed -> dont care
	    return true;

	if (context->requirementIsMet( cai.cap ))		// its provided by another installed resolvable -> dont care
	    return true;

	if (context->getStatus(requirer).isSatisfied()) {		// it is just satisfied, check freshens and supplements
#warning If an uninstall incompletes a satisfied, the uninstall should be cancelled
	    QueueItemEstablish_Ptr establish_item = new QueueItemEstablish (pool, requirer, soft);	// re-check if its still needed
	    qil.push_back (establish_item);
	    return true;
	}
	QueueItemRequire_Ptr require_item = new QueueItemRequire( pool, cai.cap );	// issue a new require to fulfill this dependency
	require_item->addPoolItem (requirer);
	if (remove_only) {
	    require_item->setRemoveOnly ();
	}
	require_item->setUpgradedPoolItem (upgraded_item);
	require_item->setLostPoolItem (uninstalled_item);				// this is what we lost, dont re-install it

	qil.push_front (require_item);

	return true;
    }
};


// Handle items which freshen or supplement us -> re-establish them

struct UninstallEstablishItem
{
    const ResPool & pool;
    QueueItemList & qil;
    bool soft;

    UninstallEstablishItem (const ResPool & p, QueueItemList &l, bool s)
	: pool(p)
	, qil(l)
	, soft(s)
    { }


    // provider has a freshens on a just to-be-installed item
    //   re-establish provider, maybe its incomplete now

    bool operator()( const CapAndItem & cai )
    {
	_XDEBUG("QueueItemUninstall::UninstallEstablishItem (" << cai.item << ", " << cai.cap << ")");

	// re-establish only installed items which are not scheduled for removal yet.

	if (cai.item.status().staysInstalled()) {
	    QueueItemEstablish_Ptr establish_item = new QueueItemEstablish (pool, cai.item, soft);
	    qil.push_back (establish_item);
	}
	return true;
    }
};

// Handle installed items which provides a recommend -> remove it soft

struct ProvidesItem
{
    const ResPool & pool;
    QueueItemList & qil;
    bool soft;

    ProvidesItem (const ResPool & p, QueueItemList &l, bool s)    
	: pool(p)
	, qil(l)
	, soft(s)
    { }


    bool operator()( const CapAndItem & cai )
    {
	_XDEBUG("remove soft item (" << cai.item << ", " << cai.cap << ")");
	PoolItem_Ref item( cai.item );
	if (!item.status().transacts() // not scheduled for transaction yet
	    && item.status().maySetToBeUninstalledSoft()) // checking the permission to delete it (Bug 217574)	    
	{
	    QueueItemUninstall_Ptr uninstall_item = new QueueItemUninstall (pool, item, QueueItemUninstall::EXPLICIT, soft);
	    uninstall_item->setUnlink ();
	    qil.push_back (uninstall_item);
	} else {
	    _XDEBUG(" ---> do not remove cause it has been set for transaction or can not set for uninstallation due right problems.");	    
	}
	return true;
    }
};


// Uninstall atom of a to-be-uninstalled patch

struct UninstallItem
{
    ResPool pool;
    ResolverContext_Ptr context;
    QueueItemList & qil;
    bool soft;

    UninstallItem( const ResPool & p, ResolverContext_Ptr ct, QueueItemList & l, bool s )
	: pool( p )
	, context( ct )
	, qil( l )
	, soft( s )
    { }

    bool operator()( const CapAndItem & cai )
    {
	PoolItem item( cai.item );

	_XDEBUG( "UninstallItem (unlink) " << item );
	QueueItemUninstall_Ptr uninstall_item = new QueueItemUninstall( pool, item, QueueItemUninstall::EXPLICIT, soft );
	uninstall_item->setUnlink();
	qil.push_front( uninstall_item );

	return true;
    }
};


//-----------------------------------------------------------------------------


bool
QueueItemUninstall::process (ResolverContext_Ptr context, QueueItemList & qil)
{
    ResStatus status = context->getStatus(_item);

    _XDEBUG("QueueItemUninstall::process(<" << status << ">" << _item << ( _unlink ? "[unlink]" : ""));

    /* In the case of an unlink, we only want to uninstall the item if it is
       being used by something else.  We can't really determine this with 100%
       accuracy, since some later queue item could cause something that requires
       the item to be uninstalled.  The alternative is to try to do something
       really clever... but I'm not clever enough to think of an algorithm that
	 (1) Would do the right thing.
	 (2) Is guaranteed to terminate. (!)
       so this will have to do.  In practice, I don't think that this is a serious
       problem. */

    if (_unlink) {
	/* If the item is to-be-installed, obviously it is being use! */
	if (status.isToBeInstalled()) {
	    ResolverInfo_Ptr misc_info = new ResolverInfoMisc (RESOLVER_INFO_TYPE_UNINSTALL_TO_BE_INSTALLED, _item, RESOLVER_INFO_PRIORITY_VERBOSE);
	    context->addInfo (misc_info);
	    goto finished;

	}
	else if (status.staysInstalled()) {

	    UnlinkCheck info;

	    /* Flag the item as to-be-uninstalled so that it won't
	       satisfy any other item's deps during this check. */

	    context->setStatus(_item, ResStatus::toBeUninstalled);

	    info.context = context;
	    info.cancel_unlink = false;

	    // look at the provides of the to-be-uninstalled resolvable and
	    //   check if anyone (installed) needs it

	    CapSet provides = _item->dep(Dep::PROVIDES);
	    for (CapSet::const_iterator iter = provides.begin(); iter != provides.end() && ! info.cancel_unlink; iter++) {

		//world()->foreachRequiringPoolItem (*iter, unlink_check_cb, &info);

		Dep dep( Dep::REQUIRES);

		invokeOnEach( pool().byCapabilityIndexBegin( iter->index(), dep ),
			      pool().byCapabilityIndexEnd( iter->index(), dep ),
			      resfilter::ByCapMatch( *iter ),
			      functor::functorRef<bool,CapAndItem>(info) );

	    }

	    /* Set the status back to normal. */

	    context->setStatus(_item, status);

	    if (info.cancel_unlink) {
		ResolverInfo_Ptr misc_info = new ResolverInfoMisc (RESOLVER_INFO_TYPE_UNINSTALL_INSTALLED, _item, RESOLVER_INFO_PRIORITY_VERBOSE);
		context->addInfo (misc_info);
		goto finished;
	    }
	}

    }
    
    this->logInfo (context);
    
    context->uninstall (_item, _upgraded_to /*bool*/, _due_to_obsolete, _unlink);
    if (status.staysInstalled()) {
	if (! _explicitly_requested
	    && _item.status().isLocked()) {

	    ResolverInfoMisc_Ptr misc_info = new ResolverInfoMisc (RESOLVER_INFO_TYPE_UNINSTALL_LOCKED,
								   _item, RESOLVER_INFO_PRIORITY_VERBOSE,
								   _cap_leading_to_uninstall);
	    if (_due_to_obsolete)
	    {
		misc_info->setOtherPoolItem (_obsoletes_item);
		misc_info->addTrigger (ResolverInfoMisc::OBSOLETE);
	    } else if (_due_to_conflict)
	    {
		misc_info->addTrigger (ResolverInfoMisc::CONFLICT);		
	    }
	    
	    context->addError (misc_info);
	    goto finished;
	}

	if (_cap_leading_to_uninstall != Capability()		// non-empty _cap_leading_to_uninstall
	    && !_due_to_conflict
	    && !_due_to_obsolete)
	{
	    ResolverInfo_Ptr info = new ResolverInfoMissingReq (_item, _cap_leading_to_uninstall);
	    context->addInfo (info);
	}

	// we're uninstalling an installed item
	//   loop over all its provides and check if any installed item requires
	//   one of these provides
	CapSet provides = _item->dep(Dep::PROVIDES);

	for (CapSet::const_iterator iter = provides.begin(); iter != provides.end(); iter++) {
	    UninstallProcess info ( pool(), context, _item, _upgraded_to, qil, _remove_only, _soft);

	    //world()->foreachRequiringPoolItem (*iter, uninstall_process_cb, &info);
	    Dep dep( Dep::REQUIRES );

	    invokeOnEach( pool().byCapabilityIndexBegin( iter->index(), dep ),
			  pool().byCapabilityIndexEnd( iter->index(), dep ),
			  resfilter::ByCapMatch( *iter ),
			  functor::functorRef<bool,CapAndItem>(info) );

	    // re-establish all which supplement or freshen a provides of the just uninstalled item

	    UninstallEstablishItem establish( pool(), qil, _soft );

	    dep = Dep::SUPPLEMENTS;
	    invokeOnEach( pool().byCapabilityIndexBegin( iter->index(), dep ),
			  pool().byCapabilityIndexEnd( iter->index(), dep ),
			  resfilter::ByCapMatch( *iter ),
			  functor::functorRef<bool,CapAndItem>( establish ) );

	    dep = Dep::FRESHENS;
	    invokeOnEach( pool().byCapabilityIndexBegin( iter->index(), dep ),
			  pool().byCapabilityIndexEnd( iter->index(), dep ),
			  resfilter::ByCapMatch( *iter ),
			  functor::functorRef<bool,CapAndItem>( establish ) );
	}

	// if its a patch, uninstall all its atoms

	if (_item->kind() == ResTraits<Patch>::kind) {
	    Patch::constPtr patch = asKind<Patch>( _item );
	    Patch::AtomList atoms = patch->atoms();
	    UninstallItem callback( pool(), context, qil, _soft );
	    CapFactory factory;
	    Dep dep(Dep::PROVIDES);

	    // loop over atom, find matching installed PoolItem and schedule this for removal

	    for (Patch::AtomList::const_iterator it = atoms.begin(); it != atoms.end(); ++it) {
		Resolvable::constPtr res = *it;
		Capability capAtom =  factory.parse ( res->kind(), res->name(), Rel::EQ, res->edition());
		invokeOnEach( pool().byCapabilityIndexBegin( capAtom.index(), dep ),
			      pool().byCapabilityIndexEnd( capAtom.index(), dep ),
			      functor::chain( resfilter::ByCaIInstalled(), resfilter::ByCapMatch( capAtom ) ),
			      functor::functorRef<bool,CapAndItem>( callback ) );
	    }
	}

	// soft-remove the installed items which have been recommended by the to-be-uninstalled
	// but not when upgrade, and not for packages

        if (_upgraded_to				// its an upgrade
	    || _item->kind() == ResTraits<Package>::kind)
	{
	    goto finished;
	}

	CapSet recomments = _item->dep (Dep::RECOMMENDS);
	for (CapSet::const_iterator iter = recomments.begin(); iter != recomments.end(); iter++) {
	    const Capability cap = *iter;
	    _XDEBUG("this recommends " << cap);
	    ProvidesItem provides( pool(), qil, true ); // soft	    

	    Dep dep(Dep::PROVIDES);
	    invokeOnEach( pool().byCapabilityIndexBegin( iter->index(), dep ),
			  pool().byCapabilityIndexEnd( iter->index(), dep ),
			  functor::chain( resfilter::ByCaIInstalled(), resfilter::ByCapMatch( *iter ) ),
			  functor::functorRef<bool,CapAndItem>( provides ) );
	}

    }

 finished:
    return true;
}

//---------------------------------------------------------------------------

int
QueueItemUninstall::cmp (QueueItem_constPtr item) const
{
    int cmp = this->compare (item);		// assures equal type
    if (cmp != 0)
	return cmp;

    QueueItemUninstall_constPtr uninstall = dynamic_pointer_cast<const QueueItemUninstall>(item);
    return compareByNVRA (_item.resolvable(), uninstall->_item.resolvable());
}


QueueItem_Ptr
QueueItemUninstall::copy (void) const
{
    QueueItemUninstall_Ptr new_uninstall = new QueueItemUninstall (pool(), _item, _reason);
    new_uninstall->QueueItem::copy(this);


    new_uninstall->_item              	      = _item;
    new_uninstall->_cap_leading_to_uninstall  = _cap_leading_to_uninstall;
    new_uninstall->_upgraded_to               = _upgraded_to;

    new_uninstall->_explicitly_requested      = _explicitly_requested;
    new_uninstall->_remove_only               = _remove_only;
    new_uninstall->_due_to_conflict           = _due_to_conflict;
    new_uninstall->_due_to_obsolete           = _due_to_obsolete;
    new_uninstall->_obsoletes_item	      = _obsoletes_item;
    new_uninstall->_unlink                    = _unlink;

    return new_uninstall;
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

