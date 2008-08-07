/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/repo/RepoFileReader.cc
 *
*/
#include <iostream>
#include "zypp/base/Logger.h"
#include "zypp/base/String.h"
#include "zypp/base/InputStream.h"
#include "zypp/base/UserRequestException.h"

#include "zypp/parser/IniDict.h"
#include "zypp/parser/RepoFileReader.h"

using std::endl;
using zypp::parser::IniDict;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace parser
  { /////////////////////////////////////////////////////////////////

    /**
   * \short List of RepoInfo's from a file.
   * \param file pathname of the file to read.
   */
    static void repositories_in_file( const Pathname &file,
                                      const RepoFileReader::ProcessRepo &callback,
                                      const ProgressData::ReceiverFnc &progress )
    {
      InputStream is(file);
      parser::IniDict dict(is);
      for ( parser::IniDict::section_const_iterator its = dict.sectionsBegin();
            its != dict.sectionsEnd();
            ++its )
      {
        RepoInfo info;
        info.setAlias(*its);

        for ( IniDict::entry_const_iterator it = dict.entriesBegin(*its);
              it != dict.entriesEnd(*its);
              ++it )
        {
          //MIL << (*it).first << endl;
          if (it->first == "name" )
            info.setName(it-> second);
          else if ( it->first == "enabled" )
            info.setEnabled( str::strToTrue( it->second ) );
          else if ( it->first == "priority" )
            info.setPriority( str::strtonum<unsigned>( it->second ) );
          else if ( it->first == "baseurl" && !it->second.empty())
            info.addBaseUrl( Url(it->second) );
          else if ( it->first == "path" )
            info.setPath( Pathname(it->second) );
          else if ( it->first == "type" )
            info.setType(repo::RepoType(it->second));
          else if ( it->first == "autorefresh" )
            info.setAutorefresh( str::strToTrue( it->second ) );
          else if ( it->first == "mirrorlist" && !it->second.empty())
            info.setMirrorListUrl(Url(it->second));
          else if ( it->first == "gpgkey" && !it->second.empty())
            info.setGpgKeyUrl( Url(it->second) );
          else if ( it->first == "gpgcheck" )
            info.setGpgCheck( str::strToTrue( it->second ) );
	  else if ( it->first == "keeppackages" )
	    info.setKeepPackages( str::strToTrue( it->second ) );
          else
            ERR << "Unknown attribute in [" << *its << "]: " << it->second << " ignored" << endl;
        }
        info.setFilepath(file);
        MIL << info << endl;
        // add it to the list.
        callback(info);
        //if (!progress.tick())
        //  ZYPP_THROW(AbortRequestException());
      }
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : RepoFileReader
    //
    ///////////////////////////////////////////////////////////////////

    RepoFileReader::RepoFileReader( const Pathname & repo_file,
                                    const ProcessRepo & callback,
                                    const ProgressData::ReceiverFnc &progress )
      : _callback(callback)
    {
      repositories_in_file(repo_file, _callback, progress);
      //MIL << "Done" << endl;
    }

    RepoFileReader::~RepoFileReader()
    {}

    std::ostream & operator<<( std::ostream & str, const RepoFileReader & obj )
    {
      return str;
    }

    /////////////////////////////////////////////////////////////////
  } // namespace parser
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
