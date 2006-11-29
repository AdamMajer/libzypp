/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/target/store/serialize.cc
*
*/
#include <iostream>
#include <ctime>
#include <fstream>
#include <sstream>
#include <streambuf>

#include "zypp/base/Logger.h"
#include "zypp/CapFactory.h"
#include "zypp/Source.h"
#include "zypp/Url.h"

#include "zypp/ResObject.h"
#include "zypp/detail/ImplConnect.h"
#include "zypp/detail/ResObjectImplIf.h"
#include "zypp/detail/SelectionImplIf.h"

#include "serialize.h"
#include "xml_escape_parser.hpp"

using namespace std;
///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
namespace storage
{ /////////////////////////////////////////////////////////////////

static std::string xml_escape( const std::string &text )
{
  iobind::parser::xml_escape_parser parser;
  return parser.escape(text);
}

static std::string xml_tag_enclose( const std::string &text, const std::string &tag, bool escape = false )
{
  std::string result;
  result += "<" + tag + ">";

  if ( escape)
   result += xml_escape(text);
  else
   result += text;

  result += "</" + tag + ">";
  return result;
}

static std::ostream & operator<<( std::ostream & str, const boost::tribool obj )
{
  if (obj)
    return str << "true";
  else if (!obj)
    return str << "false";
  else
    return str << "indeterminate";
}

/**
 * helper function that builds
 * <tagname lang="code">text</tagname>
 *
 *
 */
static std::string translatedTextToXML(const TranslatedText &text, const std::string &tagname)
{
  std::set<Locale> locales = text.locales();
  //ERR << "locale contains " << locales.size() << " translations" << std::endl;
  std::stringstream out;
  for ( std::set<Locale>::const_iterator it = locales.begin(); it != locales.end(); ++it)
  {
    //ERR << "serializing " << (*it).code() << std::endl;
    if ( *it == Locale() )
      out << "<" << tagname << ">" << xml_escape(text.text(*it)) << "</" << tagname << ">" << std::endl;
    else
      out << "<" << tagname << " lang=\"" << (*it).code() << "\">" << xml_escape(text.text(*it)) << "</" << tagname << ">" << std::endl;
  }
  return out.str();
}

template<class T>
std::string toXML( const T &obj ); //undefined

template<> 
std::string toXML( const Edition &edition )
{
  stringstream out;
  // sad the yum guys did not acll it edition
  out << "<version ver=\"" << xml_escape(edition.version()) << "\" rel=\"" << xml_escape(edition.release()) << "\" epoch=\"" << edition.epoch() << "\" />";
  return out.str();
}

template<> 
std::string toXML( const Arch &arch )
{
  stringstream out;
  out << xml_tag_enclose(xml_escape(arch.asString()), "arch");
  return out.str();
}

template<> 
std::string toXML( const Capability &cap )
{
  stringstream out;
  CapFactory factory;

  out << "<capability kind=\"" << cap.refers() << "\" >" <<  xml_escape(factory.encode(cap)) << "</capability>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const CapSet &caps )
{
  stringstream out;
  CapSet::iterator it = caps.begin();
  for ( ; it != caps.end(); ++it)
  {
    out << toXML((*it));
  }
  return out.str();
}

template<> 
std::string toXML( const Dependencies &dep )
{
  stringstream out;
  if ( dep[Dep::PROVIDES].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::PROVIDES]), "provides") << std::endl;
  if ( dep[Dep::PREREQUIRES].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::PREREQUIRES]), "prerequires") << std::endl;
  if ( dep[Dep::CONFLICTS].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::CONFLICTS]), "conflicts") << std::endl;
  if ( dep[Dep::OBSOLETES].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::OBSOLETES]), "obsoletes") << std::endl;
  // why the YUM tag is freshen without s????
  if ( dep[Dep::FRESHENS].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::FRESHENS]), "freshens") << std::endl;
  if ( dep[Dep::REQUIRES].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::REQUIRES]), "requires") << std::endl;  
  if ( dep[Dep::RECOMMENDS].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::RECOMMENDS]), "recommends") << std::endl;
  if ( dep[Dep::ENHANCES].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::ENHANCES]), "enhances") << std::endl;
  if ( dep[Dep::SUPPLEMENTS].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::SUPPLEMENTS]), "supplements") << std::endl;
  if ( dep[Dep::SUGGESTS].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::SUGGESTS]), "suggests") << std::endl;
  return out.str();

}

template<> 
std::string toXML( const Resolvable::constPtr &obj )
{
  stringstream out;

  out << "  <name>" << xml_escape(obj->name()) << "</name>" << std::endl;
  // is this shared? uh
  out << "  " << toXML(obj->edition()) << std::endl;
  out << "  " << toXML(obj->arch()) << std::endl;
  out << "  " << toXML(obj->deps()) << std::endl;
  return out.str();
}

template<> 
std::string toXML( const ResObject::constPtr &obj )
{
  stringstream out;

  // access implementation
  detail::ResImplTraits<ResObject::Impl>::constPtr pipp( detail::ImplConnect::resimpl( obj ) );
  out << translatedTextToXML(pipp->summary(), "summary");
  out << translatedTextToXML(pipp->description(), "description");
  
  out << translatedTextToXML(pipp->insnotify(), "install-notify");
  out << translatedTextToXML(pipp->delnotify(), "delete-notify");
  //out << "  <license-to-confirm>" << xml_escape(obj->licenseToConfirm()) << "</license-to-confirm>" << std::endl;
  out << translatedTextToXML(pipp->licenseToConfirm(), "license-to-confirm");
  out << "  <vendor>" << xml_escape(obj->vendor()) << "</vendor>" << std::endl;
  out << "  <size>" << static_cast<ByteCount::SizeType>(obj->size()) << "</size>" << std::endl;
  out << "  <archive-size>" << static_cast<ByteCount::SizeType>(obj->archivesize()) << "</archive-size>" << std::endl;
  out << "  <install-only>" << ( obj->installOnly() ? "true" : "false" ) << "</install-only>" << std::endl;
  out << "  <build-time>" << obj->buildtime().asSeconds()  << "</build-time>" << std::endl;
  // we assume we serialize on storeObject, set install time to NOW
  out << "  <install-time>" << Date::now().asSeconds() << "</install-time>" << std::endl;
  
  return out.str();
}

template<> 
std::string toXML( const Package::constPtr &obj )
{
  stringstream out;
  out << "<package>" << std::endl;
  // reuse Resolvable information serialize function
  out << toXML(static_cast<Resolvable::constPtr>(obj));
  out << toXML(static_cast<ResObject::constPtr>(obj));
  //out << "  <do>" << std::endl;
  //out << "      " << obj->do_script() << std::endl;
  //out << "  </do>" << std::endl;
  out << "</package>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Script::constPtr &obj )
{
  stringstream out;
  out << "<script>" << std::endl;
  // reuse Resolvable information serialize function
  out << toXML(static_cast<Resolvable::constPtr>(obj));
  out << toXML(static_cast<ResObject::constPtr>(obj));
  out << "  <do>" << std::endl;
  out << "  <![CDATA[" << std::endl;
  
  // read script
  ifstream infile;
  infile.open(obj->do_script().asString().c_str());
  while (infile.good())
  {
    char c = (char)infile.get();
    if (! infile.eof())
      out << c;
  }
  infile.close();
  
  out << "  ]]>" << std::endl;
  out << "  </do>" << std::endl;

  if ( obj->undo_available() )
  {
    out << "  <undo>" << std::endl;
    out << "  <![CDATA[" << std::endl;
  
  // read script
    infile.open(obj->undo_script().asString().c_str());
    while (infile.good())
    {
      char c = (char)infile.get();
      if (! infile.eof())
	out << c;
    }
    infile.close();
  
    out << "  ]]>" << std::endl;
    out << "  </undo>" << std::endl;

  }
  out << "</script>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Message::constPtr &obj )
{
  stringstream out;
  out << "<message>" << std::endl;
  // reuse Resolvable information serialize function
  out << toXML(static_cast<Resolvable::constPtr>(obj));
  out << toXML(static_cast<ResObject::constPtr>(obj));
  out << "  <text>" << xml_escape(obj->text().text()) << "</text>" << std::endl;
  out << "</message>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Language::constPtr &obj )
{
  stringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  out << "<language version=\"" << SERIALIZER_VERSION << "\" xmlns=\"http://www.novell.com/metadata/zypp/xml-store\">" << std::endl;
  out << toXML(static_cast<Resolvable::constPtr>(obj)) << std::endl;
  out << toXML(static_cast<ResObject::constPtr>(obj));
  out << "</language>" << std::endl;
  return out.str();
}


template<> 
std::string toXML( const Selection::constPtr &obj )
{
  stringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  out << "<pattern version=\"" << SERIALIZER_VERSION << "\" xmlns=\"http://www.novell.com/metadata/zypp/xml-store\">" << std::endl;
  
  out << toXML(static_cast<Resolvable::constPtr>(obj)) << std::endl;
  out << toXML(static_cast<ResObject::constPtr>(obj));
  
  //out << "  <default>" << (obj->isDefault() ? "true" : "false" ) << "</default>" << std::endl;
  out << "  <uservisible>" << (obj->visible() ? "true" : "false" ) << "</uservisible>" << std::endl;
  out << "  <category>" << xml_escape(obj->category()) << "</category>" << std::endl;
  out << "  <icon></icon>" << std::endl;
  out << "</pattern>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Atom::constPtr &obj )
{
  stringstream out;
  out << "<atom>" << std::endl;
  out << toXML(static_cast<Resolvable::constPtr>(obj)) << std::endl;
  out << toXML(static_cast<ResObject::constPtr>(obj));
  out << "</atom>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Pattern::constPtr &obj )
{
  stringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  out << "<pattern version=\"" << SERIALIZER_VERSION << "\" xmlns=\"http://www.novell.com/metadata/zypp/xml-store\">" << std::endl;

  out << toXML(static_cast<Resolvable::constPtr>(obj)) << std::endl;
  out << toXML(static_cast<ResObject::constPtr>(obj));
  
  out << "  <default>" << (obj->isDefault() ? "true" : "false" ) << "</default>" << std::endl;
  out << "  <uservisible>" << (obj->userVisible() ? "true" : "false" ) << "</uservisible>" << std::endl;
  out << "  <category>" << xml_escape(obj->category()) << "</category>" << std::endl;
  out << "  <icon>" << xml_escape(obj->icon().asString()) << "</icon>" << std::endl;
  out << "  <script>" << xml_escape(obj->script().asString()) << "</script>" << std::endl;
  out << "</pattern>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Product::constPtr &obj )
{
  stringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  out << "<product version=\"" << SERIALIZER_VERSION << "\" xmlns=\"http://www.novell.com/metadata/zypp/xml-store\" type=\"" << xml_escape(obj->category()) << "\">" << std::endl;
  out << toXML(static_cast<Resolvable::constPtr>(obj)) << std::endl;
  #warning "FIXME description and displayname of products"
  
  out << toXML(static_cast<ResObject::constPtr>(obj));
  
  // access implementation
  detail::ResImplTraits<Product::Impl>::constPtr pipp( detail::ImplConnect::resimpl( obj ) );
  out << translatedTextToXML(pipp->shortName(), "shortname");
  
  out << "  <distribution-name>" << xml_escape(obj->distributionName()) << "</distribution-name>" << std::endl;
  out << "  <distribution-edition>" << xml_escape(obj->distributionEdition().asString()) << "</distribution-edition>" << std::endl;
  out << "  <source>" << xml_escape(obj->source().alias()) << "</source>" << std::endl;  
  out << "  <release-notes-url>" << xml_escape(obj->releaseNotesUrl().asString()) << "</release-notes-url>" << std::endl;
  
  out << "  <update-urls>" << std::endl;
  std::list<Url> updateUrls = obj->updateUrls();
  for ( std::list<Url>::const_iterator it = updateUrls.begin(); it != updateUrls.end(); ++it)
  {
    out << "    <update-url>" << xml_escape(it->asString()) << "</update-url>" << std::endl; 
  }
  out << "  </update-urls>" << std::endl;
  
  out << "  <extra-urls>" << std::endl;
  std::list<Url> extraUrls = obj->extraUrls();
  for ( std::list<Url>::const_iterator it = extraUrls.begin(); it != extraUrls.end(); ++it)
  {
    out << "    <extra-url>" << xml_escape(it->asString()) << "</extra-url>" << std::endl; 
  }
  out << "  </extra-urls>" << std::endl;
  
  out << "  <optional-urls>" << std::endl;
  std::list<Url> optionalUrls = obj->optionalUrls();
  for ( std::list<Url>::const_iterator it = optionalUrls.begin(); it != optionalUrls.end(); ++it)
  {
    out << "    <optional-url>" << xml_escape(it->asString()) << "</optional-url>" << std::endl; 
  }
  out << "  </optional-urls>" << std::endl;
  
  out << "  <product-flags>" << std::endl;
  std::list<std::string> flags = obj->flags();
  for ( std::list<std::string>::const_iterator it = flags.begin(); it != flags.end(); ++it)
  {
    out << "    <product-flag>" << xml_escape(*it) << "</product-flag>" << std::endl; 
  }
  out << "  </product-flags>" << std::endl;
  
  out << "</product>" << std::endl;

  return out.str();
}


std::string castedToXML( const Resolvable::constPtr &resolvable )
{
  stringstream out;
  if ( isKind<Package>(resolvable) )
     out << toXML(asKind<const Package>(resolvable)) << std::endl;
  if ( isKind<Patch>(resolvable) )
     out << toXML(asKind<const Patch>(resolvable)) << std::endl;
  if ( isKind<Message>(resolvable) )
     out << toXML(asKind<const Message>(resolvable)) << std::endl;
  if ( isKind<Script>(resolvable) )
     out << toXML(asKind<const Script>(resolvable)) << std::endl;
  if ( isKind<Atom>(resolvable) )
     out << toXML(asKind<const Atom>(resolvable)) << std::endl;
  if ( isKind<Product>(resolvable) )
     out << toXML(asKind<const Product>(resolvable)) << std::endl;
  if ( isKind<Pattern>(resolvable) )
     out << toXML(asKind<const Pattern>(resolvable)) << std::endl;
  if ( isKind<Selection>(resolvable) )
     out << toXML(asKind<const Selection>(resolvable)) << std::endl;
  if ( isKind<Language>(resolvable) )
    out << toXML(asKind<const Language>(resolvable)) << std::endl;
  return out.str();
}

std::string resolvableTypeToString( const Resolvable::constPtr &resolvable, bool plural )
{
  return resolvableKindToString(resolvable->kind(), plural);
}

std::string resolvableKindToString( const Resolvable::Kind &kind, bool plural)
{
  std::string k = kind.asString();
  if (k.substr(k.size() - 2, 2) == "ch")
    return k + (plural?"es":"");
  else
    return k + (plural?"s":"");
}

template<> 
std::string toXML( const Patch::constPtr &obj )
{
  stringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  out << "<patch version=\"" << SERIALIZER_VERSION << "\" xmlns=\"http://www.novell.com/metadata/zypp/xml-store\">" << std::endl; 
  
  // reuse Resolvable information serialize function
  out << toXML(static_cast<Resolvable::constPtr>(obj));
  out << toXML(static_cast<ResObject::constPtr>(obj));
  
  out << "<id>" << xml_escape(obj->id()) << "</id>" << std::endl;
  out << "<timestamp>" << obj->timestamp().asSeconds() << "</timestamp>" << std::endl;
  
  out << "<category>" << obj->category() << "</category>" << std::endl;
  out << "<affects-package-manager>" << ( obj->affects_pkg_manager() ? "true" : "false" ) << "</affects-package-manager>" << std::endl;
  out << "<reboot-needed>" << ( obj->reboot_needed() ? "true" : "false" ) << "</reboot-needed>" << std::endl;
  
  Patch::AtomList at = obj->atoms();
  out << "  <atoms>" << std::endl;
  for (Patch::AtomList::iterator it = at.begin(); it != at.end(); it++)
  {
    Resolvable::Ptr one_atom = *it;
    out << castedToXML(one_atom) << std::endl;
  }
  out << "  </atoms>" << std::endl;
  out << "</patch>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const source::SourceInfo &obj )
{
  stringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  out << "<source version=\"" << SERIALIZER_VERSION << "\" xmlns=\"http://www.novell.com/metadata/zypp/xml-store\">" << std::endl;
  out << "  <enabled>" << obj.enabled() << "</enabled>" << std::endl;
  out << "  <auto-refresh>" << obj.autorefresh() << "</auto-refresh>" << std::endl;
  out << "  <product-dir>" << obj.path() << "</product-dir>" << std::endl;
  out << "  <cache-dir>" << obj.cacheDir() << "</cache-dir>" << std::endl;
  out << "  <type>" << xml_escape(obj.type()) << "</type>" << std::endl;
  out << "  <url>" << xml_escape(obj.url().asCompleteString()) << "</url>" << std::endl;
  out << "  <alias>" << xml_escape(obj.alias()) << "</alias>" << std::endl;
  out << "</source>" << std::endl;
  return out.str();
}

/////////////////////////////////////////////////////////////////
} // namespace storage
	///////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
