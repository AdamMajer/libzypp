/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/parser/yum/YUMPatchParser.h
 *
*/



#ifndef YUMPatchParser_h
#define YUMPatchParser_h

#include <zypp/parser/yum/YUMParserData.h>
#include <zypp/parser/XMLNodeIterator.h>
#include <zypp/parser/LibXMLHelper.h>
#include <zypp/Arch.h>
#include <list>

namespace zypp {
  namespace parser {
    namespace yum {

      /**
      * @short Parser for YUM primary.xml files (containing package metadata)
      * Use this class as an iterator that produces, one after one,
      * YUMPatchData_Ptr(s) for the XML package elements in the input.
      * Here's an example:
      *
      * for (YUMPatchParser iter(anIstream, baseUrl),
      *      iter != YUMOtherParser.end(),     // or: iter() != 0, or ! iter.atEnd()
      *      ++iter) {
      *    doSomething(*iter)
      * }
      *
      * The iterator owns the pointer (i.e., caller must not delete it)
      * until the next ++ operator is called. At this time, it will be
      * destroyed (and a new ENTRYTYPE is created.)
      *
      * If the input is fundamentally flawed so that it makes no sense to
      * continue parsing, XMLNodeIterator will log it and consider the input as finished.
      * You can query the exit status with errorStatus().
      */
      class YUMPatchParser : public XMLNodeIterator<YUMPatchData_Ptr>
      {
      public:
        YUMPatchParser(std::istream &is, const std::string &baseUrl);
        YUMPatchParser();
        YUMPatchParser(YUMPatchData_Ptr& entry);
        virtual ~YUMPatchParser();

      private:
        virtual bool isInterested(const xmlNodePtr nodePtr);
        virtual YUMPatchData_Ptr process(const xmlTextReaderPtr reader);
        void parseAtomsNode(YUMPatchData_Ptr dataPtr, xmlNodePtr formatNode);
        void parsePackageNode(YUMPatchData_Ptr dataPtr, xmlNodePtr formatNode);
        void parseMessageNode(YUMPatchData_Ptr dataPtr, xmlNodePtr formatNode);
        void parseScriptNode(YUMPatchData_Ptr dataPtr, xmlNodePtr formatNode);
        void parseFormatNode(YUMPatchPackage *dataPtr, xmlNodePtr formatNode);
        void parsePkgFilesNode(YUMPatchPackage *dataPtr, xmlNodePtr formatNode);
        void parsePkgPlainRpmNode(YUMPatchPackage *dataPtr, xmlNodePtr formatNode);
        void parsePkgPatchRpmNode(YUMPatchPackage *dataPtr, xmlNodePtr formatNode);
        void parsePkgDeltaRpmNode(YUMPatchPackage *dataPtr, xmlNodePtr formatNode);
        void parsePkgPatchBaseVersionNode(YUMPatchBaseVersion *dataPtr, xmlNodePtr formatNode);
        void parsePkgDeltaBaseVersionNode(YUMDeltaBaseVersion *dataPtr, xmlNodePtr formatNode);
        LibXMLHelper _helper;
	Arch _zypp_architecture;
      };
    } // namespace yum
  } // namespace parser
} // namespace zypp

#endif
