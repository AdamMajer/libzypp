/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef XMLParserData_h
#define XMLParserData_h

#include "zypp/base/ReferenceCounted.h"
#include "zypp/base/NonCopyable.h"
#include "zypp/Pathname.h"
#include "zypp/ByteCount.h"
#include "zypp/Date.h"
#include "zypp/TranslatedText.h"
#include <string>
#include <list>
#include <iostream>
#include <zypp/base/PtrTypes.h>

using namespace zypp::base;


namespace zypp {
  namespace parser {
    namespace xmlstore {

      DEFINE_PTR_TYPE(XMLResObjectData);
      DEFINE_PTR_TYPE(XMLProductData);
      DEFINE_PTR_TYPE(XMLPatternData);
      DEFINE_PTR_TYPE(XMLLanguageData);
      DEFINE_PTR_TYPE(XMLPatchData);
      DEFINE_PTR_TYPE(XMLPatchAtomData);
      DEFINE_PTR_TYPE(XMLPatchMessageData);
      DEFINE_PTR_TYPE(XMLPatchScriptData);

      /**
      * @short Holds dependency data
      */
      class XMLDependency {
      public:
        XMLDependency();
        XMLDependency(const std::string& kind, const std::string& encoded);
        std::string kind;
        std::string encoded;
      };


      class XMLResObjectData : public base::ReferenceCounted, private base::NonCopyable {
      public:
        XMLResObjectData();
        std::string name;
        std::string epoch;
        std::string ver;
        std::string rel;
        std::string arch;
        std::list<XMLDependency> provides;
        std::list<XMLDependency> conflicts;
        std::list<XMLDependency> obsoletes;
        std::list<XMLDependency> freshens;
        std::list<XMLDependency> requires;
        std::list<XMLDependency> prerequires;
        std::list<XMLDependency> recommends;
        std::list<XMLDependency> suggests;
        std::list<XMLDependency> supplements;
        std::list<XMLDependency> enhances;

        // in the future move above to XMLResolvableData
        TranslatedText summary;
        TranslatedText description;

        TranslatedText install_notify;
        TranslatedText delete_notify;
        TranslatedText license_to_confirm;
        std::string vendor;
        ByteCount size;
        ByteCount archive_size;
        bool install_only;
        Date build_time;
        Date install_time;

      };

      /**
      * @short Describes the patterns in a target store
      **/
      class XMLPatternData : public XMLResObjectData
      {
      public:
        XMLPatternData();

        std::string default_;
        bool userVisible;
        TranslatedText category;
        std::string icon;
        std::string script;
      };

      class XMLLanguageData : public XMLResObjectData
      {
        public:
          XMLLanguageData() {};
          ~XMLLanguageData() {};
      };

      class XMLProductData : public XMLResObjectData
      {
      public:
        XMLProductData() {};
        ~XMLProductData() {};
        
        std::string parser_version;
        std::string type;
        TranslatedText short_name;
        // those are suse specific tags
        std::string releasenotesurl;
        std::list<std::string> update_urls;
        std::list<std::string> extra_urls;
        std::list<std::string> optional_urls;
        std::list<std::string> flags;
        std::string dist_name;
        std::string dist_version;
      };

      class XMLPatchAtomData : public XMLResObjectData
      {
        public:
          enum AtomType { Atom, Script, Message };
          virtual AtomType atomType() { return Atom; };
      };

      class XMLPatchScriptData : public XMLPatchAtomData
      {
        public:
          XMLPatchScriptData() {};
          virtual AtomType atomType() { return Script; };
          std::string do_script;
          std::string undo_script;
      };

      class XMLPatchMessageData : public XMLPatchAtomData
      {
        public:
          XMLPatchMessageData() {};
          virtual AtomType atomType() { return Message; };
          TranslatedText text;
      };

      class XMLPatchData : public XMLResObjectData
      {
        public:
          XMLPatchData() {};
          ~XMLPatchData()
          {
          }

          std::string patchId;
          std::string timestamp;
          std::string engine;
          std::string category;
          bool rebootNeeded;
          bool packageManager;
          std::string updateScript;
          std::list<XMLPatchAtomData_Ptr > atoms;
      };


      /* Easy output */
      std::ostream& operator<<(std::ostream &out, const XMLDependency& data);
      std::ostream& operator<<(std::ostream &out, const XMLPatternData& data);
      std::ostream& operator<<(std::ostream& out, const XMLProductData& data);


    } // namespace xmlstore
  } // namespace parser
} // namespace zypp






#endif
