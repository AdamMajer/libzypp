/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/Date.h
 *
*/
#ifndef ZYPP_DATE_H
#define ZYPP_DATE_H

#include <ctime>
#include <iosfwd>
#include <string>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : Date
  //
  /** Store and operate on date (time_t).
  */
  class Date
  {
    friend std::ostream & operator<<( std::ostream & str, const Date & obj );

  public:

    typedef time_t ValueType;

    /** Default ctor: 0 */
    Date()
    : _date( 0 )
    {}
    /** Ctor taking time_t value. */
    Date( ValueType date_r )
    : _date( date_r )
    {}
    /** Ctor taking time_t value as string. */
    Date( const std::string & seconds_r );

    /** Return the current time. */
    static Date now()
    { return ::time( 0 ); }

  public:
    /** Conversion to time_t. */
    operator ValueType() const
    { return _date; }

    /** \name Arithmetic operations.
     * \c + \c - \c * \c / are provided via conversion to time_t.
    */
    //@{
    Date & operator+=( const time_t rhs ) { _date += rhs; return *this; }
    Date & operator-=( const time_t rhs ) { _date -= rhs; return *this; }
    Date & operator*=( const time_t rhs ) { _date *= rhs; return *this; }
    Date & operator/=( const time_t rhs ) { _date /= rhs; return *this; }

    Date & operator++(/*prefix*/) { _date += 1; return *this; }
    Date & operator--(/*prefix*/) { _date -= 1; return *this; }

    Date operator++(int/*postfix*/) { return _date++; }
    Date operator--(int/*postfix*/) { return _date--; }
    //@}

  public:
    /** Return string representation according to format.
     * \see 'man strftime' (which is used internaly) for valid
     * conversion specifiers in format.
     *
     * \return An empty string on illegal format.
     **/
    std::string form( const std::string & format_r ) const;

    /** Default string representation of Date.
     * The preferred date and time representation for the current locale.
     **/
    std::string asString() const
    { return form( "%c" ); }

    /** Convert to string representation of calendar time in
     *  numeric form (like "1029255142").
     **/
    std::string asSeconds() const
    { return form( "%s" ); }

  private:
    /** Calendar time.
     * The number of seconds elapsed since 00:00:00 on January 1, 1970,
     * Coordinated Universal Time (UTC).
     **/
    ValueType _date;
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates Date Stream output */
  inline std::ostream & operator<<( std::ostream & str, const Date & obj )
  { return str << obj.asString(); }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_DATE_H
