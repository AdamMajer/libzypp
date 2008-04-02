/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/parser/yum/YUMProductParser.cc
 *
*/

#include <istream>
#include <string>
#include "zypp/ZYppFactory.h"
#include "zypp/ZYpp.h"
#include "zypp/parser/xml_parser_assert.h"
#include <libxml/xmlreader.h>
#include <libxml/tree.h>
#include "zypp/parser/yum/YUMProductParser.h"
#include "zypp/parser/yum/YUMPrimaryParser.h"
#include "zypp/parser/LibXMLHelper.h"
#include "zypp/base/Logger.h"
#include "zypp/parser/yum/schemanames.h"

using namespace std;
namespace zypp
{
namespace parser
{
namespace yum
{

YUMProductParser::~YUMProductParser()
{ }

YUMProductParser::YUMProductParser(std::istream &is, const std::string& baseUrl, parser::ParserProgress::Ptr progress )
    : XMLNodeIterator<YUMProductData_Ptr>(is, baseUrl,PRODUCTSCHEMA, progress)
      , _zypp_architecture( getZYpp()->architecture() )
{
  fetchNext();
}

YUMProductParser::YUMProductParser()
  : _zypp_architecture( getZYpp()->architecture() )
{ }

YUMProductParser::YUMProductParser(YUMProductData_Ptr& entry)
    : XMLNodeIterator<YUMProductData_Ptr>(entry)
      , _zypp_architecture( getZYpp()->architecture() )
{ }


// select for which elements process() will be called
bool
YUMProductParser::isInterested(const xmlNodePtr nodePtr)
{
  return _helper.isElement(nodePtr) && _helper.name(nodePtr) == "product";
}

// do the actual processing
YUMProductData_Ptr
YUMProductParser::process(const xmlTextReaderPtr reader)
{
  xml_assert(reader);
  YUMProductData_Ptr productPtr = new YUMProductData;
  xmlNodePtr dataNode = xmlTextReaderExpand(reader);
  xml_assert(dataNode);
  productPtr->type = _helper.attribute(dataNode,"type");

  // FIXME move the respective method to a common class, inherit it
  YUMPrimaryParser prim;

  for (xmlNodePtr child = dataNode->children;
       child && child != dataNode;
       child = child->next)
  {
    if (_helper.isElement(child))
    {
      string name = _helper.name(child);
      if (name == "name")
      {
        productPtr->name = _helper.content(child);
      }
      else if (name == "arch")
      {
        productPtr->arch = _helper.content(child);
        try
        {
          if (!Arch(productPtr->arch).compatibleWith( _zypp_architecture ))
          {
            productPtr = NULL;			// skip <package>, incompatible architecture
            ERR << "Skipping incompatible architecture product. " << endl;
            break;
          }
        }
        catch ( const Exception & excpt_r )
        {
          ZYPP_CAUGHT( excpt_r );
          DBG << "Skipping malformed " << productPtr->arch << endl;
          productPtr = NULL;
          break;
        }
      }
      else if (name == "vendor")
      {
        productPtr->vendor = _helper.content(child);
      }
      else if (name == "release-notes-url")
      {
        productPtr->releasenotesurl = _helper.content(child);
      }
      else if (name == "displayname")
      {
        productPtr->summary.setText(_helper.content(child), Locale(_helper.attribute(child,"lang")));
      }
      else if (name == "shortname")
      {
        productPtr->short_name.setText(_helper.content(child), Locale(_helper.attribute(child,"lang")));
      }
      else if (name == "description")
      {
        productPtr->description.setText(_helper.content(child), Locale(_helper.attribute(child,"lang")));
      }
      else if (name == "version")
      {
        productPtr->epoch = _helper.attribute(child,"epoch");
        productPtr->ver = _helper.attribute(child,"ver");
        productPtr->rel = _helper.attribute(child,"rel");
      }
      else if (name == "distribution-name")
      {
        productPtr->distribution_name = _helper.content(child);
      }
      else if (name == "distribution-edition")
      {
        productPtr->distribution_edition = _helper.content(child);
      }
      else if (name == "provides")
      {
        prim.parseDependencyEntries(& productPtr->provides, child);
      }
      else if (name == "conflicts")
      {
        prim.parseDependencyEntries(& productPtr->conflicts, child);
      }
      else if (name == "obsoletes")
      {
        prim.parseDependencyEntries(& productPtr->obsoletes, child);
      }
      else if (name == "prerequires")
      {
        prim.parseDependencyEntries(& productPtr->prerequires, child);
      }
      else if (name == "requires")
      {
        prim.parseDependencyEntries(& productPtr->requires, child);
      }
      else if (name == "recommends")
      {
        prim.parseDependencyEntries(& productPtr->recommends, child);
      }
      else if (name == "suggests")
      {
        prim.parseDependencyEntries(& productPtr->suggests, child);
      }
      else if (name == "supplements")
      {
        prim.parseDependencyEntries(& productPtr->supplements, child);
      }
      else if (name == "enhances")
      {
        prim.parseDependencyEntries(& productPtr->enhances, child);
      }
      else if (name == "freshens")
      {
        prim.parseDependencyEntries(& productPtr->freshens, child);
      }
      else
      {
        WAR << "YUM <data> contains the unknown element <" << name << "> "
        << _helper.positionInfo(child) << ", skipping" << endl;
      }
    }
  }
  return productPtr;
} /* end process */



} // namespace yum
} // namespace parser
} // namespace zypp
