/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/tagret/rpm/RpmCallbacks.h
 *
*/

#ifndef ZYPP_MEDIA_MEDIACALLBACKS_H
#define ZYPP_MEDIA_MEDIACALLBACKS_H

#include <iosfwd>

#include "zypp/Url.h"
#include "zypp/Callback.h"
#include "zypp/base/Exception.h"
#include "zypp/Pathname.h"

namespace zypp {
  namespace target {
    namespace rpm {

      ///////////////////////////////////////////////////////////////////
      // Reporting progress of package removing
      ///////////////////////////////////////////////////////////////////
      class RpmRemoveReport : public HACK::Callback {
      public:
        virtual ~RpmRemoveReport()
        {}
        /** Start the operation */
        virtual void start( const std::string & name )
        { }
        /**
         * Inform about progress
         * Return true on abort
         */
        virtual bool progress( unsigned percent )
        { return false; }
        /** Finish operation in case of success */
        virtual void end()
        { }
        /** Finish operatino in case of fail, report fail exception */
        virtual void end( Exception & excpt_r )
        { }
      };

      extern RpmRemoveReport rpmRemoveReport;
  
      ///////////////////////////////////////////////////////////////////
      // Reporting progress of package installation
      ///////////////////////////////////////////////////////////////////
      struct RpmInstallReport : public callback::ReportBase {

	enum Action {
          ABORT,  // abort and return error
          RETRY,   // retry
	  IGNORE   // ignore
        };

        /** Start the operation */
        virtual void start( const Pathname & name ) 
        { }
        /**
         * Inform about progress
         * Return false on abort
         */
        virtual bool progress( unsigned percent )
        { return true; }

        /** Finish operation in case of success */
        virtual void finish()
        { }
	
	virtual Action problem( Exception & excpt_r )
	 { return ABORT; }

        /** Finish operation in case of fail, report fail exception */
        virtual void finish( Exception & excpt_r )
        { }
      };

      ///////////////////////////////////////////////////////////////////
      // Reporting database scanning
      ///////////////////////////////////////////////////////////////////
      class ScanDbReport : public HACK::Callback {
      public:
        virtual ~ScanDbReport()
        {}
        /** Start the operation */
        virtual void start() 
        { }
        /**
         * Inform about progress
         * Return true on abort
         */
        virtual bool progress( unsigned percent )
        { return false; }
        /** Finish operation in case of success */
        virtual void end()
        { }
        /** Finish operatino in case of fail, report fail exception */
        virtual void end( Exception & excpt_r )
        { }
      };
  
      extern ScanDbReport scanDbReport;

      ///////////////////////////////////////////////////////////////////
      // Reporting progress of database rebuild
      ///////////////////////////////////////////////////////////////////
      class RebuildDbReport : public HACK::Callback {
      public:
        virtual ~RebuildDbReport()
        {}
        /** Start the operation */
        virtual void start() 
        { }
        /**
         * Inform about progress
         * Return true on abort
         */
        virtual bool progress( unsigned percent )
        { return false; }
        /** Finish operation in case of success */
        virtual void end()
        { }
        /** Finish operatino in case of fail, report fail exception */
        virtual void end( Exception & excpt_r )
        { }
      };
  
      extern RebuildDbReport rebuildDbReport;

      ///////////////////////////////////////////////////////////////////
      // Reporting progress of database rebuild
      ///////////////////////////////////////////////////////////////////
      class ConvertDbReport : public HACK::Callback {
      public:
        virtual ~ConvertDbReport()
        {}
        /** Start the operation */
        virtual void start() 
        { }
        /**
         * Inform about progress
         * Return true on abort
         */
        virtual bool progress( unsigned percent )
        { return false; }
        /** Finish operation in case of success */
        virtual void end()
        { }
        /** Finish operatino in case of fail, report fail exception */
        virtual void end( Exception & excpt_r )
        { }
      };
  
      extern ConvertDbReport convertDbReport;

    } // namespace rpm
  } // namespace target
} // namespace zypp

#endif // ZYPP_MEDIA_MEDIACALLBACKS_H
