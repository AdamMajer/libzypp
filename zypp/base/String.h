/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/base/String.h
 *
*/
#ifndef ZYPP_BASE_STRING_H
#define ZYPP_BASE_STRING_H

#include <iosfwd>
#include <string>
#include <string.h>

#include "zypp/base/PtrTypes.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  /** Convenience \c char* constructible from \c std::string and \c char*,
   *  it maps \c (char*)0 to an empty string.
   *
   * \code
   * bool hasPrefix( const std::string & str_r, const std::string & prefix_r )
   * { return( ::strncmp( str_r.c_str(), prefix_r.c_str(), prefix_r.size() ) == 0 ); }
   * \endcode
   *
   * Called with a plain \c char* as argument, the \c std::string is created form
   * for nothing. The implementation actually does not use the \c std::string.
   *
   * Best would be to implement \c hasPrefix for each combination of \c char*
   * and \c std::string arguments:
   *
   * \code
   * bool hasPrefix( const std::string & str_r, const std::string & prefix_r )
   * { return( ::strncmp( str_r.c_str(), prefix_r.c_str(), prefix_r.size() ) == 0 ); }
   *
   * bool hasPrefix( const std::string & str_r, const char * prefix_r )
   * { return( !prefix_r || ::strncmp( str_r.c_str(), prefix_r, ::strlen(prefix_r) ) == 0 ); }
   *
   * bool hasPrefix( const char * str_r, const std::string & prefix_r )
   * { return( str_r ? ::strncmp( str_r, prefix_r.c_str(), prefix_r.size() ) == 0 : prefix_r.empty() ); }
   *
   * bool hasPrefix( const char * str_r, const char * prefix_r )
   * { return( str && prefix_r ? ::strncmp( str_r, prefix_r, ::strlen(prefix_r) ) == 0
   *                           : !((str_r && *str_r) || (prefix_r && *prefix_r)); }
   * \endcode
   *
   * This is where \ref C_Str can help. Constructible from \c std::string and \c char*,
   * it \e reduces the \c std::string to it's \c char*. At the same time it converts
   * \c (char*)0 into an \c "" string.
   *
   * \code
   * bool hasPrefix( const C_Str & str_r, const C_Str & prefix_r )
   * { return( ::strncmp( str_r, prefix_r, prefix_r.size() ) == 0 ); }
   * \endcode
   */
  class C_Str
  {
    public:
      typedef std::string::size_type size_type;

    public:
      C_Str()                            : _val( 0 ),             _sze( 0 ) {}
      C_Str( char * c_str_r )            : _val( c_str_r ),       _sze( std::string::npos ) {}
      C_Str( const char * c_str_r )      : _val( c_str_r ),       _sze( std::string::npos ) {}
      C_Str( const std::string & str_r ) : _val( str_r.c_str() ), _sze( str_r.size() ) {}

    public:
      bool      isNull()       const { return !_val; }
      bool      empty()        const { return !(_val && *_val); }
      size_type size()         const
      {
        if ( _sze == std::string::npos )
        { _sze = _val ? ::strlen( _val ) : 0; }
        return _sze;
      };

      operator const char *() const { return c_str(); }
      const char * c_str()    const { return _val ? _val : ""; }

    private:
      const char *const _val;
      mutable size_type _sze;
  };

  /** \relates C_Str Stream output */
  inline std::ostream & operator<<( std::ostream & str, const C_Str & obj )
  { return str << obj.c_str(); }

  ///////////////////////////////////////////////////////////////////
  /** String related utilities and \ref ZYPP_STR_REGEX.
   \see \ref ZYPP_STR_REGEX
  */
  namespace str
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    /**
     * Global asString() that works with std::string too
     */
    template<class _T>
        inline std::string asString( const _T &t )
        { return t.asString(); }

    template<class _T>
        inline std::string asString( const intrusive_ptr<_T> &p )
        { return p->asString(); }

    template<class _T>
        inline std::string asString( const weak_ptr<_T> &p )
        { return p->asString(); }

    template<>
        inline std::string asString( const std::string &t )
        { return t; }

    ///////////////////////////////////////////////////////////////////
    /** Printf style construction of std::string. */
    std::string form( const char * format, ... )
    __attribute__ ((format (printf, 1, 2)));

    ///////////////////////////////////////////////////////////////////
    /** Return string describing the \a error_r code.
     * Like ::strerror, but the numerical value is included in
     * the string as well.
    */
    std::string strerror( int errno_r );

    ///////////////////////////////////////////////////////////////////
    /** Assert \c free called for allocated <tt>char *</tt>.
     * \code
     * ...
     * SafeBuf safe;
     * vasprintf( &safe._buf, format, ap );
     * return safe.asString();
     * \endcode
     *
     * \ingroup g_RAII
    */
    struct SafeBuf
    {
      char * _buf;
      SafeBuf() : _buf( 0 ) {}
      ~SafeBuf() { if ( _buf ) free( _buf ); }
      std::string asString() const
      { return _buf ? std::string(_buf) : std::string(); }
    };

    ///////////////////////////////////////////////////////////////////
    /** \name String representation of number.
     *
     * Optional second argument sets the minimal string width (' ' padded).
     * Negative values will cause the number to be left adjusted within the string.
     *
     * Default width is 0.
     * \code
     * numstring(42)           -> "42"
     * numstring(42, 4)        -> "  42"
     * numstring(42,-4)        -> "42  "
     * \endcode
     **/
    //@{
    inline std::string numstring( char n,               int w = 0 ) { return form( "%*hhd",  w, n ); }
    inline std::string numstring( unsigned char n,      int w = 0 ) { return form( "%*hhu",  w, n ); }
    inline std::string numstring( short n,              int w = 0 ) { return form( "%*hd",   w, n ); }
    inline std::string numstring( unsigned short n,     int w = 0 ) { return form( "%*hu",   w, n ); }
    inline std::string numstring( int n,                int w = 0 ) { return form( "%*d",    w, n ); }
    inline std::string numstring( unsigned n,           int w = 0 ) { return form( "%*u",    w, n ); }
    inline std::string numstring( long n,               int w = 0 ) { return form( "%*ld",   w, n ); }
    inline std::string numstring( unsigned long n,      int w = 0 ) { return form( "%*lu",   w, n ); }
    inline std::string numstring( long long n,          int w = 0 ) { return form( "%*lld",  w, n ); }
    inline std::string numstring( unsigned long long n, int w = 0 ) { return form( "%*llu",  w, n ); }
    //@}

    ///////////////////////////////////////////////////////////////////
    /** \name String representation of number as hex value with leading '0x'.
     * Optional second argument sets the minimal
     * string width (0 padded). Negative values will cause the number to be left adjusted
     * within the string. Default width is 10 (4 for char).
     * <PRE>
     * hexstring(42)           -> "0x0000002a"
     * hexstring(42, 4)        -> "0x2a"
     * hexstring(42,-4)        -> "0x2a"
     * </PRE>
     **/
    //@{
    inline std::string hexstring( char n,               int w = 4 ) { return form( "%#0*hhx", w, n ); }
    inline std::string hexstring( unsigned char n,      int w = 4 ) { return form( "%#0*hhx", w, n ); }
    inline std::string hexstring( short n,              int w = 10 ){ return form( "%#0*hx",  w, n ); }
    inline std::string hexstring( unsigned short n,     int w = 10 ){ return form( "%#0*hx",  w, n ); }
    inline std::string hexstring( int n,                int w = 10 ){ return form( "%#0*x",   w, n ); }
    inline std::string hexstring( unsigned n,           int w = 10 ){ return form( "%#0*x",   w, n ); }
    inline std::string hexstring( long n,               int w = 10 ){ return form( "%#0*lx",  w, n ); }
    inline std::string hexstring( unsigned long n,      int w = 10 ){ return form( "%#0*lx",  w, n ); }
    inline std::string hexstring( long long n,          int w = 0 ) { return form( "%#0*llx", w, n ); }
    inline std::string hexstring( unsigned long long n, int w = 0 ) { return form( "%#0*llx", w, n ); }
    //@}

    ///////////////////////////////////////////////////////////////////
    /** \name String representation of number as octal value with leading '0'.
     * Optional second argument sets the minimal
     * string width (0 padded). Negative values will cause the number to be left adjusted
     * within the string. Default width is 5 (4 for char).
     * <PRE>
     * octstring(42)           -> "00052"
     * octstring(42, 4)        -> "0052"
     * octstring(42,-4)        -> "052 "
     * </PRE>
     **/
    //@{
    inline std::string octstring( char n,               int w = 4 ) { return form( "%#0*hho",  w, n ); }
    inline std::string octstring( unsigned char n,      int w = 4 ) { return form( "%#0*hho",  w, n ); }
    inline std::string octstring( short n,              int w = 5 ) { return form( "%#0*ho",   w, n ); }
    inline std::string octstring( unsigned short n,     int w = 5 ) { return form( "%#0*ho",   w, n ); }
    inline std::string octstring( int n,                int w = 5 ) { return form( "%#0*o",    w, n ); }
    inline std::string octstring( unsigned n,           int w = 5 ) { return form( "%#0*o",    w, n ); }
    inline std::string octstring( long n,               int w = 5 ) { return form( "%#0*lo",   w, n ); }
    inline std::string octstring( unsigned long n,      int w = 5 ) { return form( "%#0*lo",   w, n ); }
    inline std::string octstring( long long n,          int w = 0 ) { return form( "%#0*llo",  w, n ); }
    inline std::string octstring( unsigned long long n, int w = 0 ) { return form( "%#0*llo",  w, n ); }
    //@}

    ///////////////////////////////////////////////////////////////////
    /** Parsing numbers from string.
    */
    //@{
    /** String to integer type determined by template arg.
     * \note Only specializations are defined.
     * \code
     * time_t t = strtonum<time_t>( "42" );
     * \endcode
    */
    template<typename _It>
      _It strtonum( const C_Str & str );

    template<>
      inline short              strtonum( const C_Str & str ) { return ::strtol  ( str, NULL, 0 ); }
    template<>
      inline int                strtonum( const C_Str & str ) { return ::strtol  ( str, NULL, 0 ); }
    template<>
      inline long               strtonum( const C_Str & str ) { return ::strtol  ( str, NULL, 0 ); }
    template<>
      inline long long          strtonum( const C_Str & str ) { return ::strtoll ( str, NULL, 0 ); }

    template<>
      inline unsigned short     strtonum( const C_Str & str ) { return ::strtoul ( str, NULL, 0 ); }
    template<>
      inline unsigned           strtonum( const C_Str & str ) { return ::strtoul ( str, NULL, 0 ); }
    template<>
      inline unsigned long      strtonum( const C_Str & str ) { return ::strtoul ( str, NULL, 0 ); }
    template<>
      inline unsigned long long strtonum( const C_Str & str ) { return ::strtoull( str, NULL, 0 ); }

    /** String to integer type detemined 2nd function arg \a i.
     * \code
     * time_t t; strtonum( "42", t );
     * \endcode
    */
    template<typename _It>
      inline _It strtonum( const C_Str & str, _It & i )
      { return i = strtonum<_It>( str ); }
    //@}

    ///////////////////////////////////////////////////////////////////
    /** Parsing boolean from string.
    */
    //@{
    /** Return \c true if str is <tt>1, true, yes, on</tt>. */
    bool strToTrue( const C_Str & str );

    /** Return \c false if str is <tt>0, false, no, off</tt>. */
    bool strToFalse( const C_Str & str );

    /** Parse \c str into a bool depending on the default value.
     * If the \c default is true, look for a legal \c false string.
     * If the \c default is false, look for a legal \c true string.
    */
    inline bool strToBool( const C_Str & str, bool default_r )
    { return( default_r ? strToFalse( str ) : strToTrue( str ) ); }
    //@}

    /**
     * \short Looks for text in a string and replaces it.
     *
     * \note It only perform substtution in one pass
     */
    std::string gsub( const std::string& sData, const std::string& sFrom, const std::string& sTo);

    /**
     * \short Looks for text in string and replaces it in place
     *
     *
     * \note It only perform substtution in one pass
     * \note use only if you replace same lenght strings, otherwise use gsub
     */
    std::string& replace_all( std::string & str, const std::string & from, const std::string & to);

    ///////////////////////////////////////////////////////////////////
    /** \name Split. */
    //@{
    /** Split \a line_r into words.
     * Any sequence of characters in \a sepchars_r is treated as
     * delimiter. The words are passed to OutputIterator \a result_r.
     * \code
     * std::vector<std::string> words;
     * str::split( "some line", std::back_inserter(words) )
     * \endcode
     *
    */
    template<class _OutputIterator>
      unsigned split( const C_Str &   line_r,
                      _OutputIterator result_r,
                      const C_Str &   sepchars_r = " \t" )
      {
        const char * beg = line_r;
        const char * cur = beg;
        // skip leading sepchars
        while ( *cur && ::strchr( sepchars_r, *cur ) )
          ++cur;
        unsigned ret = 0;
        for ( beg = cur; *beg; beg = cur, ++result_r, ++ret )
          {
            // skip non sepchars
            while( *cur && !::strchr( sepchars_r, *cur ) )
              ++cur;
            // build string
            *result_r = std::string( beg, cur-beg );
            // skip sepchars
            while ( *cur && ::strchr( sepchars_r, *cur ) )
              ++cur;
          }
        return ret;
      }

    /** Split \a line_r into words with respect to escape delimeters.
     * Any sequence of characters in \a sepchars_r is treated as
     * delimiter if not inside "" or "" or escaped by \, but not \\.
     * The words are passed to OutputIterator \a result_r.
     * \code
     * std::vector<std::string> words;
     * str::splitEscaped( "some line", std::back_inserter(words) )
     * \endcode
     *
     * \code
     * example splitted strings
     * normal line -> 2 elements ( "normal", "line" )
     * escaped\ line -> 1 element( "escaped line" )
     * "quoted line" -> 1 element same as above
     * 'quoted line' -> 1 element same as above
     * "escaped quote\'" -> 1 element ( "escaped quote'" )
     * \endcode
    */
    template<class _OutputIterator>
      unsigned splitEscaped( const C_Str &   line_r,
                      _OutputIterator result_r,
                      const C_Str &   sepchars_r = " \t" )
      {
        const char * beg = line_r;
        const char * cur = beg;
        // skip leading sepchars
        while ( *cur && ::strchr( sepchars_r, *cur ) )
          ++cur;
        unsigned ret = 0;
        for ( beg = cur; *beg; beg = cur, ++result_r, ++ret )
          {
            if ( *cur == '"'  || *cur == '\'' )
            {
              char closeChar = *cur;
              ++cur;
              bool cont = true;
              while (cont)
              {
                while ( *cur && *cur != closeChar)
                  ++cur;
                if ( *cur == '\0' )
                {
                  return ret; //TODO parsing exception no closing quote
                }
                int escCount = 0;
                const char * esc = cur-1;
                while ( esc != beg && *esc == '\\' )
                {
                  escCount++;
                  --esc;
                }
                cont = (escCount % 2 == 1); // find some non escaped escape char
                cur++; //skip quote
              }

              std::string s( beg+1, cur-beg-2 ); //without quotes
              //transform escaped escape
              replace_all( s, "\\\\", "\\" );
              //transform escaped quotes (only same as open
              char tmpn[2] = { closeChar, 0 };
              char tmpo[3] = { '\\', closeChar, 0 };
              replace_all( s, tmpo, tmpn );

              *result_r = s;
            }
            else
            {
              // skip non sepchars
              while( *cur && !::strchr( sepchars_r, *cur ) )
              {
                //ignore char after backslash
                if ( *cur == '\\' )
                {
                  ++cur;
                }
                ++cur;
              }
              // build string
              std::string s( beg, cur-beg );
              //transform escaped escape
              replace_all( s, "\\\\", "\\" );

              const char *delimeter = sepchars_r;
              while ( *delimeter )
              {
                std::string ds("\\");
                const char tmp[2] = { *delimeter, '\0' };
                std::string del(tmp);
                ds+= del;
                replace_all( s, ds, del );
                ++delimeter;
              }

              *result_r = s;
            }
            // skip sepchars
            while ( *cur && ::strchr( sepchars_r, *cur ) )
              ++cur;
          }
        return ret;
      }

    /** Split \a line_r into fields.
     * Any single character in \a sepchars_r is treated as a
     * field separator. The words are passed to OutputIterator
     * \a result_r.
     * \code
     * ""        -> words 0
     * ":"       -> words 2  |||
     * "a"       -> words 1  |a|
     * ":a"      -> words 2  ||a|
     * "a:"      -> words 2  |a||
     * ":a:"     -> words 3  ||a||
     *
     * \endcode
    *
     * \code
     * std::vector<std::string> words;
     * str::split( "some line", std::back_inserter(words) )
     * \endcode
     *
    */
    template<class _OutputIterator>
      unsigned splitFields( const C_Str &   line_r,
                            _OutputIterator result_r,
                            const C_Str &   sepchars_r = ":" )
      {
        const char * beg = line_r;
        const char * cur = beg;
        unsigned ret = 0;
        for ( beg = cur; *beg; beg = cur, ++result_r )
          {
            // skip non sepchars
            while( *cur && !::strchr( sepchars_r, *cur ) )
              ++cur;
            // build string
            *result_r = std::string( beg, cur-beg );
            ++ret;
            // skip sepchar
            if ( *cur )
            {
              ++cur;
              if ( ! *cur )                // ending with sepchar
              {
                *result_r = std::string(); // add final empty field
                ++ret;
                break;
              }
            }
          }
        return ret;
      }
    //@}

    ///////////////////////////////////////////////////////////////////
    /** \name Join. */
    //@{
    /** Join strings using separator \a sep_r (defaults to BLANK). */
    template <class _Iterator>
      std::string join( _Iterator begin, _Iterator end,
                        const C_Str & sep_r = " " )
      {
        std::string res;
        for ( _Iterator iter = begin; iter != end; ++ iter )
          {
            if ( iter != begin )
              res += sep_r;
            res += asString(*iter);
          }
        return res;
      }

    /** Join strings using separator \a sep_r (defaults to BLANK). */
    template <class _Container>
      std::string join( const _Container & cont_r,
                        const C_Str & sep_r = " " )
      { return join( cont_r.begin(), cont_r.end(), sep_r ); }
    //@}


    ///////////////////////////////////////////////////////////////////
    /** \name Case conversion. */
    //@{
    /** Return lowercase version of \a s
     * \todo improve
    */
    std::string toLower( const std::string & s );
    /** \overload */
    inline std::string toLower( const char * s )
    { return( s ? toLower( std::string(s) ) : std::string() ); }

    /** Return uppercase version of \a s
     * \todo improve
    */
    std::string toUpper( const std::string & s );
    /** \overload */
    inline std::string toUpper( const char * s )
    { return( s ? toUpper( std::string(s) ) : std::string() ); }
    //@}


    /** \name Case insensitive comparison. */
    //@{
    inline int compareCI( const C_Str & lhs, const C_Str & rhs )
    {
      if ( lhs == rhs )
        return 0;
      return ::strcasecmp( lhs, rhs );
    }
    //@}

    ///////////////////////////////////////////////////////////////////
    /** \name Trimming whitepace.
     * \todo optimize l/r trim.
    */
    //@{
    /** To define how to trim. */
    enum Trim {
      NO_TRIM = 0x00,
      L_TRIM  = 0x01,
      R_TRIM  = 0x02,
      TRIM    = (L_TRIM|R_TRIM)
    };

    std::string trim( const std::string & s, const Trim trim_r = TRIM );

    inline std::string ltrim( const std::string & s )
    { return trim( s, L_TRIM ); }

    inline std::string rtrim( const std::string & s )
    { return trim( s, R_TRIM ); }
    //@}

    std::string stripFirstWord( std::string & line, const bool ltrim_first );

    std::string stripLastWord( std::string & line, const bool rtrim_first );

    std::string getline( std::istream & str, bool trim = false );

    std::string getline( std::istream & str, const Trim trim_r );

    inline bool startsWith(const std::string& s, const char* str) { return s.find(str) == 0; }
    inline bool endsWith(const std::string& s, const char* str) { return s.find(str) == s.size() - strlen(str); }
    inline bool contains(const std::string& s, const char* str) { return s.find(str) != std::string::npos; }

    ///////////////////////////////////////////////////////////////////

    /** \name String prefix handling.
     */
    //@{
   /** Return whether \a str_r has prefix \a prefix_r. */
    inline bool hasPrefix( const C_Str & str_r, const C_Str & prefix_r )
    { return( ::strncmp( str_r, prefix_r, prefix_r.size() ) == 0 ); }

    /** Strip a \a prefix_r from \a str_r and return the resulting string. */
    inline std::string stripPrefix( const C_Str & str_r, const C_Str & prefix_r )
    { return( hasPrefix( str_r, prefix_r ) ? str_r + prefix_r.size() : str_r.c_str() ); }
    //@}
    /////////////////////////////////////////////////////////////////
  } // namespace str
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_BASE_STRING_H
