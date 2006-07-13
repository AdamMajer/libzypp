/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/CheckSum.h
 *
*/
#ifndef ZYPP_CHECKSUM_H
#define ZYPP_CHECKSUM_H

#include <iosfwd>
#include <string>

#include "zypp/Pathname.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////


  class CheckSum
  {
  public:
    CheckSum();

    CheckSum( const std::string & type, const std::string & checksum );

    CheckSum( const std::string & type, std::istream & input_r );

  public:
    static const std::string & md5Type();
    static const std::string & shaType();
    static const std::string & sha1Type();
    static const std::string & sha256Type();

    static CheckSum md5( const std::string & checksum )
    { return  CheckSum( md5Type(), checksum); }
    static CheckSum sha( const std::string & checksum )
    { return  CheckSum( shaType(), checksum); }
    static CheckSum sha1( const std::string & checksum )
    { return  CheckSum( sha1Type(), checksum); }
    static CheckSum sha256( const std::string & checksum )
    { return  CheckSum( sha256Type(), checksum); }

    static CheckSum md5( std::istream & input_r )
    { return  CheckSum( md5Type(), input_r ); }
    static CheckSum sha( std::istream & input_r )
    { return  CheckSum( sha1Type(), input_r ); }
    static CheckSum sha1( std::istream & input_r )
    { return  CheckSum( sha1Type(), input_r ); }
    static CheckSum sha256( std::istream & input_r )
    { return  CheckSum( sha256Type(), input_r ); }

  public:
    std::string type() const
    { return _type; }

    std::string checksum() const
    { return _checksum; }

    bool empty() const
    { return (checksum().empty() || type().empty()); }

  private:
    std::string _type;
    std::string _checksum;
  };

  /** \relates CheckSum Stream output. */
  std::ostream & operator<<( std::ostream & str, const CheckSum & obj );

  /** \relates CheckSum */
  inline bool operator==( const CheckSum & lhs, const CheckSum & rhs )
  { return lhs.checksum() == rhs.checksum() && lhs.type() == rhs.type(); }

  /** \relates CheckSum */
  inline bool operator!=( const CheckSum & lhs, const CheckSum & rhs )
  { return ! ( lhs == rhs ); }

} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_CHECKSUM_H
