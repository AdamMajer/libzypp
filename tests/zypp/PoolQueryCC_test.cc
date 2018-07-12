#include "TestSetup.h"
#include <zypp/base/String.h>
#include <zypp/base/LogTools.h>

#include "zypp/PoolQuery.h"
#include "zypp/PoolQueryUtil.tcc"

//#define BOOST_TEST_MODULE PoolQuery

using boost::unit_test::test_case;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using namespace zypp;

static TestSetup test;

/////////////////////////////////////////////////////////////////////////////
template <class TCont>
std::ostream & nlist( std::ostream & str, const TCont & set_r )
{
  str << "[" << set_r.size() << "]: ";
  for ( const auto & solv : set_r )
    str << " \"" << solv.name() << "\"";
  return str << endl;
}

BOOST_AUTO_TEST_CASE(init)
{
  test.loadTargetHelix( TESTS_SRC_DIR "/zypp/data/PoolQueryCC/rxnames.xml" );
  nlist( cout << "repo ", ResPool::instance() );
}

/////////////////////////////////////////////////////////////////////////////
// Basic issue: Multiple match strings are compiled into a singe regex. The
// semantic of the individual match strings must be preserved. I.e. a literal
// "." must become "\.". Globbing patterns must match the whole string, so they
// need to be anchored within the regec. Etc.
/////////////////////////////////////////////////////////////////////////////
void qtest( const std::string & pattern_r, Match::Mode mode_r, bool verbose_r = false )
{
  typedef std::set<sat::Solvable> Result;
  PoolQuery q;
  switch ( mode_r )
  {
    case Match::STRING:		q.setMatchExact();	break;
    case Match::SUBSTRING:	q.setMatchSubstring();	break;
    case Match::OTHER:		q.setMatchWord();	break;	// missused for matchWord()
    case Match::GLOB:		q.setMatchGlob();	break;
    case Match::REGEX:		q.setMatchRegex();	break;
    default:
      throw( "unhandled match mode" );
      break;
  }
  q.addString( pattern_r );
  Result o( q.begin(), q.end() );	// original query

  q.addString( "more" );
  BOOST_CHECK_NO_THROW( q.begin() );
  try {
    Result r( q.begin(), q.end() );	// compiles into RX (o|more)

    BOOST_CHECK( o == r );
    if ( o != r || 1||verbose_r )
    {
      cout << '"' << pattern_r << '"' << endl;
      nlist( cout << "    o", o );
      nlist( cout << "    r", r );
    }
  }
  catch ( const zypp::MatchInvalidRegexException & excpt )
  {
    cout << excpt << endl;
  }
}

inline void qtest( const std::string & pattern_r, bool verbose_r = false )
{ return qtest( pattern_r, Match::SUBSTRING, verbose_r ); }

/////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(pool_query_init)
{
  qtest( "?", Match::SUBSTRING );
  qtest( "?", Match::STRING );
  qtest( "?", Match::OTHER );	// missused for matchWord()
  qtest( "?", Match::GLOB );
  qtest( "?", Match::REGEX );
  return;
  auto chars = {
    ""
    , "."	// RX
    , "?"	// RX GLOB
    , "*"	// RX GLOB
    , "+"	// RX
    , "["	// RX GLOB
    , "]"	// RX GLOB
    , "("	// RX
    , ")"	// RX
    , "{"	// RX
    , "}"	// RX
    , "|"	// RX
    , "/"	//    GLOB (delimits match?)
  };

  // literal (sub)string:
  for ( auto c : chars )
    qtest( c );

  qtest( ".?" );
  qtest( ".*" );
  qtest( ".+" );
}

/////////////////////////////////////////////////////////////////////////////
#if 0
setMatchWord
setMatchExact
setMatchGlob
setMatchRegex


#endif
