/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/source/Applydeltarpm.cc
 *
*/
#include <iostream>

#include "zypp/base/Logger.h"
#include "zypp/source/Applydeltarpm.h"
#include "zypp/ExternalProgram.h"
#include "zypp/AutoDispose.h"
#include "zypp/PathInfo.h"
#include "zypp/TriBool.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace applydeltarpm
  { /////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////
    namespace
    { /////////////////////////////////////////////////////////////////

      const Pathname applydeltarpm_prog( "/usr/bin/applydeltarpm" );

      /******************************************************************
       **
       **	FUNCTION NAME : applydeltarpm
       **	FUNCTION TYPE : bool
      */
      bool applydeltarpm( const char *const argv_r[] )
      {
        ExternalProgram prog( argv_r, ExternalProgram::Stderr_To_Stdout );
        for ( std::string line = prog.receiveLine(); ! line.empty(); line = prog.receiveLine() )
          {
            DBG << "Applydeltarpm : " << line;
        }
        return( prog.close() == 0 );
      }

      /////////////////////////////////////////////////////////////////
    } // namespace
    ///////////////////////////////////////////////////////////////////

    /******************************************************************
     **
     **	FUNCTION NAME : haveApplydeltarpm
     **	FUNCTION TYPE : bool
    */
    bool haveApplydeltarpm()
    {
      // To track changes in availability of applydeltarpm.
      static TriBool _last = indeterminate;
      PathInfo prog( applydeltarpm_prog );
      bool have = prog.isX();
      if ( _last == have )
        ; // TriBool! 'else' is not '_last != have'
      else
        {
          // _last is 'indeterminate' or '!have'
          if ( (_last = have) )
            MIL << "Found executable " << prog << endl;
          else
            WAR << "No executable " << prog << endl;
        }
      return _last;
    }

    /******************************************************************
     **
     **	FUNCTION NAME : check
     **	FUNCTION TYPE : bool
    */
    bool check( const std::string & sequenceinfo_r, bool quick_r )
    {
      if ( ! haveApplydeltarpm() )
        return false;

      const char *const argv[] = {
        "/usr/bin/applydeltarpm",
        ( quick_r ? "-C" : "-c" ),
        "-s", sequenceinfo_r.c_str(),
        NULL
      };

      return( applydeltarpm( argv ) );
    }

    /******************************************************************
     **
     **	FUNCTION NAME : check
     **	FUNCTION TYPE : bool
    */
    bool check( const Pathname & delta_r, bool quick_r )
    {
      if ( ! haveApplydeltarpm() )
        return false;

      const char *const argv[] = {
        "/usr/bin/applydeltarpm",
        ( quick_r ? "-C" : "-c" ),
        delta_r.asString().c_str(),
        NULL
      };

      return( applydeltarpm( argv ) );
    }

    /******************************************************************
     **
     **	FUNCTION NAME : provide
     **	FUNCTION TYPE : bool
    */
    bool provide( const Pathname & delta_r, const Pathname & new_r )
    {
      // cleanup on error
      AutoDispose<const Pathname> guard( new_r, filesystem::unlink );

      if ( ! haveApplydeltarpm() )
        return false;

      const char *const argv[] = {
        "/usr/bin/applydeltarpm",
        "-p",
        delta_r.asString().c_str(),
        new_r.asString().c_str(),
        NULL
      };

      if ( ! applydeltarpm( argv ) )
        return false;

      guard.resetDispose(); // no cleanup on success
      return true;
    }

    /******************************************************************
     **
     **	FUNCTION NAME : provide
     **	FUNCTION TYPE : bool
    */
    bool provide( const Pathname & old_r, const Pathname & delta_r,
                  const Pathname & new_r )
    {
      // cleanup on error
      AutoDispose<const Pathname> guard( new_r, filesystem::unlink );

      if ( ! haveApplydeltarpm() )
        return false;

      const char *const argv[] = {
        "/usr/bin/applydeltarpm",
        "-p",
        "-r", old_r.asString().c_str(),
        delta_r.asString().c_str(),
        new_r.asString().c_str(),
        NULL
      };

      if ( ! applydeltarpm( argv ) )
        return false;

      guard.resetDispose(); // no cleanup on success
      return true;
    }

    /////////////////////////////////////////////////////////////////
  } // namespace applydeltarpm
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
