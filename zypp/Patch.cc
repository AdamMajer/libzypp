/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/Patch.cc
 *
*/
#include <iostream>

#include "zypp/base/LogTools.h"
#include "zypp/Patch.h"
#include "zypp/sat/WhatProvides.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  IMPL_PTR_TYPE( Patch );

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Patch::Patch
  //	METHOD TYPE : Ctor
  //
  Patch::Patch( const sat::Solvable & solvable_r )
  : ResObject( solvable_r )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Patch::~Patch
  //	METHOD TYPE : Dtor
  //
  Patch::~Patch()
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	Patch interface forwarded to implementation
  //
  ///////////////////////////////////////////////////////////////////

  Patch::Category Patch::categoryEnum() const
  {
    std::string cat( category() );
    switch ( cat[0] )
    {
      //	CAT_YAST
      case 'y':
      case 'Y':
	if ( str::compareCI( cat, "yast" ) == 0 )
	  return CAT_YAST;
	break;

      //	CAT_SECURITY
      case 's':
      case 'S':
	if ( str::compareCI( cat, "security" ) == 0 )
	  return CAT_SECURITY;
	break;

      //	CAT_RECOMMENDED
      case 'r':
      case 'R':
	if ( str::compareCI( cat, "recommended" ) == 0 )
	  return CAT_RECOMMENDED;
	break;
      case 'b':
      case 'B':
	if ( str::compareCI( cat, "bugfix" ) == 0 )	// rhn
	  return CAT_RECOMMENDED;
	break;

      //	CAT_OPTIONAL
      case 'o':
      case 'O':
	if ( str::compareCI( cat, "optional" ) == 0 )
	  return CAT_OPTIONAL;
	break;
      case 'f':
      case 'F':
	if ( str::compareCI( cat, "feature" ) == 0 )
	  return CAT_OPTIONAL;
	break;
      case 'e':
      case 'E':
	if ( str::compareCI( cat, "enhancement" ) == 0 )	// rhn
	  return CAT_OPTIONAL;
	break;

      //	CAT_DOCUMENT
      case 'd':
      case 'D':
	if ( str::compareCI( cat, "document" ) == 0 )
	  return CAT_DOCUMENT;
	break;
    }
    // default:
    return CAT_OTHER;
  }

  std::string Patch::message( const Locale & lang_r ) const
  { return lookupStrAttribute( sat::SolvAttr::message, lang_r ); }

  std::string Patch::category() const
  { return lookupStrAttribute( sat::SolvAttr::patchcategory ); }

  bool Patch::rebootSuggested() const
  { return lookupBoolAttribute( sat::SolvAttr::rebootSuggested ); }

  bool Patch::restartSuggested() const
  { return lookupBoolAttribute( sat::SolvAttr::restartSuggested ); }

  bool Patch::reloginSuggested() const
  { return lookupBoolAttribute( sat::SolvAttr::reloginSuggested ); }

  Patch::InteractiveFlags Patch::interactiveFlags() const
  {
    InteractiveFlags patchFlags (NoFlags);
    if ( rebootSuggested() )
      patchFlags |= Reboot;

    if ( ! message().empty() )
      patchFlags |= Message;

    if ( ! licenseToConfirm().empty() )
      patchFlags |= License;

    Patch::Contents c( contents() );
    for_( it, c.begin(), c.end() )
    {
      if ( ! makeResObject(*it)->licenseToConfirm().empty() )
      {
        patchFlags |= License;
        break;
      }
    }
    return patchFlags;
  }

  bool Patch::interactiveWhenIgnoring( InteractiveFlags flags_r ) const
  {
    if ( interactiveFlags() & ( ~flags_r ) )
    {
      return true;
    }
    else
    {
      return false;
    }
  }

  bool Patch::interactive() const
  {
    return interactiveWhenIgnoring();
  }

  Patch::Contents Patch::contents() const
  {
    Contents result;
    // DBG << *this << endl;
    sat::LookupAttr updateCollection( sat::SolvAttr::updateCollection, satSolvable() );
    for_( entry, updateCollection.begin(), updateCollection.end() )
    {
      IdString name    ( entry.subFind( sat::SolvAttr::updateCollectionName ).idStr() );
      Edition  edition ( entry.subFind( sat::SolvAttr::updateCollectionEvr ).idStr() );
      Arch     arch    ( entry.subFind( sat::SolvAttr::updateCollectionArch ).idStr() );
      if ( name.empty() )
      {
        WAR << "Ignore malformed updateCollection entry: " << name << "-" << edition << "." << arch << endl;
        continue;
      }

      // The entry is relevant if there is an installed
      // package with the same name and arch.
      bool relevant = false;
      sat::WhatProvides providers( (Capability( name.id() )) );
      for_( it, providers.begin(), providers.end() )
      {
        if ( it->isSystem() && it->ident() == name && it->arch() == arch )
        {
          relevant = true;
          break;
        }
      }
      if ( ! relevant )
      {
        // DBG << "Not relevant: " << name << "-" << edition << "." << arch << endl;
        continue;
      }

      /* find exact providers first (this matches the _real_ 'collection content' of the patch */
      providers = sat::WhatProvides( Capability( arch, name.c_str(), Rel::EQ, edition, ResKind::package ) );
      if ( providers.empty() )
      {
        /* no exact providers: find 'best' providers: those with a larger evr */
        providers = sat::WhatProvides( Capability( arch, name.c_str(), Rel::GT, edition, ResKind::package ) );
        if ( providers.empty() )
        {
          // Hmm, this patch is not installable, no one is providing the package in the collection
          // FIXME: raise execption ? fake a solvable ?
          WAR << "Missing provider: " << name << "-" << edition << "." << arch << endl;
          continue;
        }
      }

      // FIXME ?! loop over providers and try to find installed ones ?
      // DBG << "Found " << name << "-" << edition << "." << arch << ": " << *(providers.begin()) << endl;
      result.get().insert( *(providers.begin()) );
    }

    return result;
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : Patch::ReferenceIterator
  //
  ///////////////////////////////////////////////////////////////////

  Patch::ReferenceIterator::ReferenceIterator( const sat::Solvable & val_r )
  { base_reference() = sat::LookupAttr( sat::SolvAttr::updateReference, val_r ).begin(); }

  std::string Patch::ReferenceIterator::id() const
  { return base_reference().subFind( sat::SolvAttr::updateReferenceId ).asString(); }
  std::string Patch::ReferenceIterator::href() const
  { return base_reference().subFind( sat::SolvAttr::updateReferenceHref ).asString(); }
  std::string Patch::ReferenceIterator::title() const
  { return base_reference().subFind( sat::SolvAttr::updateReferenceTitle ).asString(); }
  std::string Patch::ReferenceIterator::type() const
  { return base_reference().subFind( sat::SolvAttr::updateReferenceType ).asString(); }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
