/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/thread/Thread.h
 *
 *
 *
 */
#ifndef ZYPP_THREAD_THREAD_H
#define ZYPP_THREAD_THREAD_H

#include <iosfwd>

#if ( _REENTRANT )
#include <thread>
#include <mutex>
#endif

#if ( _REENTRANT )
#define mt_volatile volatile
#else
#define mt_volatile
#endif


///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace mt
  {




  } // namespace mt
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_THREAD_THREAD_H
