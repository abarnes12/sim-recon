/*
 *  hddm-cpp :	tool that reads in a HDDM document (Hall D Data Model)
 *		and writes a c++ header file that embodies the model in
 *		c++ classes.  It also generates input/output member
 *		functions to translate the model between the memory
 *		representation and a default binary representation that
 *		is suitable for passing over a pipe or storing on disk.
 *
 *  Original version - Richard Jones, May 25 2001.
 *
 *
 *  Programmer's Notes:
 *  -------------------
 * This translator has yet to be implemented, but it should be simple
 * to do.  Just take the hddm-c.cpp code and make the following changes.
 *
 * 1. In the header file replace the make_x_yyy function declarations
 *    with class definitions named x_yyy, using the signature of the
 *    make_x_yyy for the constructor, and the default destructor.
 *
 * 2. Implement the data members of the c structures as public data 
 *    members of the parent class, for efficient access.
 *
 * 3. The pointers become pointers to class instance.
 *
 * 4. The i/o functions are implemented as member functions of the
 *    by top-level class x_HDDM.
 *
 * 5. The pop-stack helper functions are implemented in an auxilliary
 *    class popstack.
 *
 */


#define MAX_POPLIST_LENGTH 99

#include "hddm-cpp.hpp"

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include <fstream>

#define X(XString) XString.unicodeForm()
#define S(XString) XString.localForm()

char* hFilename = 0;
ofstream hFile;
ofstream cFile;

const char* classPrefix;
int tagListLength = 0;
DOMElement* tagList[100000];
bool verifyOnly = false;
char containerList[65536]="";
char constructorCalls[65536]="";
char destructorCalls[65536]="";

void usage()
{
   cerr << "\nUsage:\n"
        << "    hddm-cpp [-v | -o <filename>] {HDDM file}\n\n"
        << "Options:\n"
        <<  "    -v			validate only\n"
        <<  "    -o <filename>	write to <filename>.h"
        << endl;
}

/* Generate the plural form of a noun */

char* plural(const char* str)
{
   int len = strlen(str);
   char* p = new char [len+10];
   strcpy(p,str);
   if ((len > 3) && (strcmp(&p[len-3],"tum")  == 0))
   {
      strcpy(&p[len-3],"ta");
   }
   else if ((len > 2) && (strcmp(&p[len-2],"ex")  == 0))
   {
      strcpy(&p[len-2],"ices");
   }
   else if ((len > 2) && (strcmp(&p[len-2],"sh")  == 0))
   {
      strcpy(&p[len-2],"shes");
   }
   else if ((len > 1) && (strcmp(&p[len-1],"s")  == 0))
   {
      strcpy(&p[len-1],"ses");
   }
   else if (len > 1)
   {
      strcat(p,"s");
   }
   return p;
}

/* Map from tag name to name of the corresponding c++-class
 * for the case of simple tags (those that do not repeat)
 */
char* simpleStructType(const char* tag)
{
   int len = strlen(tag) + strlen(classPrefix);
   char* p = new char [len + 10];
   char* q = new char [len];
   strcpy(q,tag);
   q[0] = toupper(q[0]);
   sprintf(p,"%s_%s_t",classPrefix,q);
   delete [] q;
   return p;
}

/* Map from tag name to name of the corresponding c++-class
 * for the case of list tags (those that may repeat)
 */
char* listStructType(const char* tag)
{
   int len = strlen(tag) + strlen(classPrefix);
   char* p = new char [len + 10];
   char* tags = plural(tag);
   tags[0] = toupper(tags[0]);
   sprintf(p,"%s_%s_t",classPrefix,tags);
   delete [] tags;
   return p;
}

/* Verify that the tag group under this element does not collide
 * with existing tag group t, otherwise exit with fatal error
 */
void checkConsistency(DOMElement* el, int t)
{
   XString tagS(el->getTagName());
   DOMNamedNodeMap* oldAttr = tagList[t]->getAttributes();
   DOMNamedNodeMap* newAttr = el->getAttributes();
   int listLength = oldAttr->getLength();
   for (int n = 0; n < listLength; n++)
   {
      XString nameS(oldAttr->item(n)->getNodeName());
      XString oldS(tagList[t]->getAttribute(X(nameS)));
      XString newS(el->getAttribute(X(nameS)));
      if (nameS.equals("minOccurs"))
      {
         continue;
      }
      else if (nameS.equals("maxOccurs"))
      {
         int maxold = (oldS.equals("unbounded"))? 9999 : atoi(S(oldS));
         int maxnew = (newS.equals("unbounded"))? 9999 : atoi(S(newS));
	 if (maxold*maxnew <= maxold)
         {
            cerr << "hddm-cpp error: inconsistent maxOccurs usage by tag "
                 << "\"" << S(tagS) << "\" in xml document." << endl;
            exit(1);
         }
      }
      else if (! newS.equals(oldS))
      {
         cerr << "hddm-cpp error: inconsistent usage of attribute "
              << "\"" << S(nameS) << "\" in tag "
              << "\"" << S(tagS) << "\" in xml document." << endl;
         exit(1);
      }
   }
   listLength = newAttr->getLength();
   for (int n = 0; n < listLength; n++)
   {
      XString nameS(newAttr->item(n)->getNodeName());
      XString oldS(tagList[t]->getAttribute(X(nameS)));
      XString newS(el->getAttribute(X(nameS)));
      if (nameS.equals("minOccurs"))
      {
         continue;
      }
      else if (nameS.equals("maxOccurs"))
      {
         int maxold = (oldS.equals("unbounded"))? 9999 : atoi(S(oldS));
         int maxnew = (newS.equals("unbounded"))? 9999 : atoi(S(newS));
	 if (maxold*maxnew <= maxnew)
         {
            cerr << "hddm-cpp error: inconsistent maxOccurs usage by tag "
                 << "\"" << S(tagS) << "\" in xml document." << endl;
            exit(1);
         }
      }
      else if (! newS.equals(oldS))
      {
         cerr << "hddm-cpp error: inconsistent usage of attribute "
              << "\"" << S(nameS) << "\" in tag "
              << "\"" << S(tagS) << "\" in xml document." << endl;
         exit(1);
      }
   }
   DOMNodeList* oldList = tagList[t]->getChildNodes();
   DOMNodeList* newList = el->getChildNodes();
   listLength = oldList->getLength();
   if (newList->getLength() != listLength)
   {
      cerr << "hddm-cpp error: inconsistent usage of tag "
           << "\"" << S(tagS) << "\" in xml document." << endl;
   exit(1);
   }
   for (int n = 0; n < listLength; n++)
   {
      DOMNode* cont = oldList->item(n);
      XString nameS(cont->getNodeName());
      short type = cont->getNodeType();
      if (type == DOMNode::ELEMENT_NODE)
      {
         DOMNodeList* contList = el->getElementsByTagName(X(nameS));
         if (contList->getLength() != 1)
         {
             cerr << "hddm-cpp error: inconsistent usage of tag "
                  << "\"" << S(tagS) << "\" in xml document." << endl;
             exit(1);
         }
      }
   }
}

/* Write declaration of c-structure for this tag to c-header file */

void writeHeader(DOMElement* el)
{
   XString tagS(el->getTagName());
   char* ctypeDef = simpleStructType(S(tagS));

#if 0
   hFile << endl
	 << "#ifndef SAW_" << ctypeDef 				<< endl
	 << "#define SAW_" << ctypeDef				<< endl
								<< endl
	 << "typedef struct {"					<< endl;

   DOMNamedNodeMap* varList = el->getAttributes();
   int varCount = varList->getLength();
   for (int v = 0; v < varCount; v++)
   {
      DOMNode* var = varList->item(v);
      XString typeS(var->getNodeValue());
      XString nameS(var->getNodeName());
      if (typeS.equals("int"))
      {
         hFile << "   int                  " << S(nameS) << ";" << endl;
      }
      else if (typeS.equals("long"))
      {
         hFile << "   long long            " << S(nameS) << ";" << endl;
      }
      else if (typeS.equals("float"))
      {
         hFile << "   float                " << S(nameS) << ";" << endl;
      }
      else if (typeS.equals("double"))
      {
         hFile << "   double               " << S(nameS) << ";" << endl;
      }
      else if (typeS.equals("boolean"))
      {
         hFile << "   bool_t               " << S(nameS) << ";" << endl;
      }
      else if (typeS.equals("string"))
      {
         hFile << "   char*                " << S(nameS) << ";" << endl;
      }
      else if (typeS.equals("anyURI"))
      {
         hFile << "   char*                " << S(nameS) << ";" << endl;
      }
      else if (typeS.equals("Particle_t"))
      {
         hFile << "   Particle_t           " << S(nameS) << ";" << endl;
      }
      else
      {
         /* ignore attributes with unrecognized values */
      }
   }

   DOMNodeList* contList = el->getChildNodes();
   int contLength = contList->getLength();
   for (int c = 0; c < contLength; c++)
   {
      DOMNode* cont = contList->item(c);
      XString nameS(cont->getNodeName());
      short type = cont->getNodeType();
      if (type == DOMNode::ELEMENT_NODE)
      {
         DOMElement* contEl = (DOMElement*) cont;
	 XString repAttS("maxOccurs");
         XString repS(contEl->getAttribute(X(repAttS)));
	 int rep = (repS.equals("unbounded"))? 9999 : atoi(S(repS));
         char* ctypeRef = (rep > 1) ? listStructType(S(nameS))
	                            : simpleStructType(S(nameS));

         hFile << "   " << ctypeRef << "*";
         for (int i = 0; i < 20-strlen(ctypeRef); i++)
         {
            hFile << " ";
         }
         char* names = plural(S(nameS));
         hFile <<  ((rep > 1) ? names : S(nameS)) << ";" << endl;
         delete [] ctypeRef;
         delete [] names;
      }
   }

   hFile << "} " << ctypeDef << ";" << endl;
#endif

   XString repAttS("maxOccurs");
   XString repS(el->getAttribute(X(repAttS)));
   int rep = (repS.equals("unbounded"))? 9999 : atoi(S(repS));
   if (rep > 1)
   {
      char* ctypeRef = listStructType(S(tagS));
		char cDef[256];
		strcpy(cDef,ctypeDef);
		cDef[strlen(cDef)-2] = 0;
		char cRef[256];
		strcpy(cRef,&ctypeRef[2]);
		cRef[strlen(cRef)-2] = 0;
		hFile << "//------------- "<<ctypeRef<<" --------------"<<endl
            << endl << "class "<<ctypeRef<<":public DContainer"<<endl
		      << "{" << endl
				<< "	public:"<<endl
            << "		"<<ctypeRef<<"(void)"
				<< ":DContainer((void**)&"<<cDef<<", sizeof("<<ctypeDef<<"), \""<<ctypeDef<<"\"){}" << endl
            << "   	" << ctypeDef << " *"<<cDef<<";" << endl
            << "};"<< endl
				<< "//-------------------------------------------"<<endl;
		sprintf(containerList,"%s\n	%s %s",containerList,ctypeRef, cRef);
		sprintf(constructorCalls,"%s\n	hddm->%s 	= new %s();", constructorCalls, cRef, ctypeRef);
		sprintf(destructorCalls,"%s\n	delete hddm->%s;", destructorCalls, cRef);
      delete [] ctypeRef;
   }

   //hFile << "#endif /* " << ctypeDef << " */" 			<< endl;
}

/* Generate c-structure declarations for this tag and its descendants;
 * this function calls itself recursively
 */
int constructGroup(DOMElement* el)
{
   XString tagS(el->getTagName());
   int t;
   for (t = 0; t < tagListLength; t++)
   {
      if (tagS.equals(tagList[t]->getTagName()))
      {
         checkConsistency(el,t);
         return t;
      }
   }

   tagList[t] = el;
   tagListLength++;

   DOMNodeList* contList = el->getChildNodes();
   int contLength = contList->getLength();
   for (int c = 0; c < contLength; c++)
   {
      DOMNode* cont = contList->item(c);
      short type = cont->getNodeType();
      if (type == DOMNode::ELEMENT_NODE)
      {
         DOMElement* contEl = (DOMElement*) cont;
         constructGroup(contEl);
      }
   }

   writeHeader(el);
   return t;
}

/* Generate c code for make_<class letter>_<group name> functions */

void constructMakeFuncs()
{
	#if 0
   for (int t = 0; t < tagListLength; t++)
   {
      DOMElement* tagEl = tagList[t];
      XString tagS(tagEl->getTagName());
      char* listType = listStructType(S(tagS));
      char* simpleType = simpleStructType(S(tagS));

      hFile << endl;
      cFile << endl;

      XString repAttS("maxOccurs");
      XString repS = tagEl->getAttribute(X(repAttS));
      int rep = (repS.equals("unbounded"))? 9999 : atoi(S(repS));
      if (rep > 1)
      {
         hFile << listType << "* ";
         cFile << listType << "* ";
         char listT[500];
         strncpy(listT,listType,500);
         char* term = rindex(listT,'_');
         *term = 0;
         hFile << "make_" << listT;
         cFile << "make_" << listT;
         hFile << "(int n);" 					<< endl;
         cFile << "(int n)" 					<< endl
               << "{"						<< endl
               << "   int rep = (n > 1) ? n-1 : 0;"		<< endl
               << "   int size = sizeof(" << listType
               << ") + rep * sizeof(" << simpleType << ");"	<< endl
               << "   " << listType
               << "* p = (" << listType << "*)CALLOC(size,\""
               << listType << "\");"				<< endl;
      }
      else
      {
         hFile << simpleType << "* ";
         cFile << simpleType << "* ";
         char simpleT[500];
         strncpy(simpleT,simpleType,500);
         char* term = rindex(simpleT,'_');
         *term = 0;
         hFile << "make_" << simpleT;
         cFile << "make_" << simpleT;
         hFile << "();"	 					<< endl;
         cFile << "()"	 					<< endl
               << "{"						<< endl
               << "   int size = sizeof(" << simpleType << ");"	<< endl
               << "   " << simpleType << "* p = "
               << "(" << simpleType << "*)CALLOC(size,\""
               << simpleType << "\");"				<< endl;
      }
      cFile << "   return p;"					<< endl
            << "}"						<< endl;
   }
	
	#endif
}

/* Generate c functions for unpacking binary stream into c-structures */

void constructUnpackers()
{
	#if 0
   cFile << endl;
   for (int t = 0; t < tagListLength; t++)
   {
      DOMElement* tagEl = tagList[t];
      XString tagS(tagEl->getTagName());
      char* listType = listStructType(S(tagS));
      char* simpleType = simpleStructType(S(tagS));

      cFile << endl << "static ";

      char* tagType;
      XString repAttS("maxOccurs");
      XString repS = tagEl->getAttribute(X(repAttS));
      int rep = (repS.equals("unbounded"))? 9999 : atoi(S(repS));
      if (rep > 1)
      {
         tagType = listType;
      }
      else
      {
         tagType = simpleType;
      }
      char tagT[500];
      strncpy(tagT,tagType,500);
      char* term = rindex(tagT,'_');
      *term = 0;
      cFile << tagType << "* unpack_" << tagT
            << "(XDR* xdrs, popNode* pop)"
								<< endl
            << "{"						<< endl
            << "   " << tagType << "* this1 = 0;"		<< endl
            << "   unsigned int size;"				<< endl
	    << "   if (! xdr_u_int(xdrs,&size))"		<< endl
            << "   {"						<< endl
	    << "       return 0;"				<< endl
            << "   }"						<< endl
            << "   else if (size > 0)"				<< endl
            << "   {"						<< endl
            << "      int start = xdr_getpos(xdrs);"		<< endl;

      if (rep > 1)
      {
         cFile << "      int m;"				<< endl
               << "      unsigned int mult;"			<< endl
	       << "      xdr_u_int(xdrs,&mult);"		<< endl
               << "      this1 = make_" << tagT << "(mult);"	<< endl
               << "      this1->mult = mult;"			<< endl
               << "      for (m = 0; m < mult; m++ )"		<< endl
               << "      {"					<< endl;
      }
      else
      {
         cFile << "      this1 = make_" << tagT << "();"	<< endl
               << "      {"					<< endl;
      }

      int hasContents = 0;
      DOMNodeList* contList = tagEl->getChildNodes();
      for (int c = 0; c < contList->getLength(); c++)
      {
         DOMNode* cont = contList->item(c);
         short type = cont->getNodeType();
         if (type == DOMNode::ELEMENT_NODE)
         {
            hasContents = 1;
            DOMElement* contEl = (DOMElement*) cont;
            XString nameS(contEl->getTagName());
            XString reS(contEl->getAttribute(X(repAttS)));
	    int re = (reS.equals("unbounded"))? 9999 : atoi(S(reS));
            char* names = plural(S(nameS));
            cFile << "         int p;"				<< endl
                  << "         void* (*ptr) = (void**) &this1->"
                  << ((rep > 1) ? "in[m]." : "" )
                  << ((re > 1) ? names : S(nameS)) << ";"	<< endl;
            delete [] names;
            break;
         }
      }

      DOMNamedNodeMap* attList = tagEl->getAttributes();
      for (int a = 0; a < attList->getLength(); a++)
      {
         DOMNode* att = attList->item(a);
         XString typeS(att->getNodeValue());
         XString nameS(att->getNodeName());
         char nameStr[500];
         if (rep > 1)
         {
            sprintf(nameStr,"in[m].%s",S(nameS));
         }
         else
         {
            sprintf(nameStr,"%s",S(nameS));
         }
         if (typeS.equals("int"))
         {
            cFile << "         xdr_int(xdrs,&this1->"
	          << nameStr << ");"				 << endl;
         }
	 else if (typeS.equals("long"))
         {
            cFile << "#ifndef XDR_LONGLONG_MISSING"		 << endl
                  << "         xdr_longlong_t(xdrs,&this1->"
	          << nameStr << ");"				 << endl
                  << "#else"					 << endl
                  << "         {"				 << endl
                  << "            int* " << nameStr << "_ = "
                  << "(int*)&this1->" << nameStr << ";"		 << endl
                  << "# if __BIG_ENDIAN__"			 << endl
                  << "            xdr_int(xdrs,&"
                  << nameStr << "_[0]);"			 << endl
                  << "            xdr_int(xdrs,&"
                  << nameStr << "_[1]);"			 << endl
                  << "# else"					 << endl
                  << "            xdr_int(xdrs,&"
                  << nameStr << "_[1]);"			 << endl
                  << "            xdr_int(xdrs,&"
                  << nameStr << "_[0]);"			 << endl
                  << "# endif"					 << endl
                  << "         }"				 << endl
                  << "#endif"					 << endl;
         }
         else if (typeS.equals("float"))
         {
            cFile << "         xdr_float(xdrs,&this1->"
	          << nameStr << ");"				 << endl;
         }
         else if (typeS.equals("double"))
         {
            cFile << "         xdr_double(xdrs,&this1->"
	          << nameStr << ");"				 << endl;
         }
         else if (typeS.equals("boolean"))
         {
            cFile << "         xdr_bool(xdrs,&this1->"
	          << nameStr << ");"				 << endl;
         }
         else if (typeS.equals("Particle_t"))
         {
            cFile << "         xdr_int(xdrs,(int*)&this1->"
	          << nameStr << ");"				 << endl;
         }
         else if (typeS.equals("string"))
         {
            cFile << "         xdr_string(xdrs,&this1->"
	          << nameStr << ", 1000000);"			 << endl;
         }
         else if (typeS.equals("anyURI"))
         {
            cFile << "         xdr_string(xdrs,&this1->"
	          << nameStr << ", 1000000);"			 << endl;
         }
         else
         {
            /* ignore attributes with unrecognized values */
         }
      }

      if (hasContents)
      {
         cFile << "         for (p = 0; p < pop->popListLength; p++)"	<< endl
               << "         {"						<< endl
               << "            popNode* pnode = pop->popList[p];"	<< endl
               << "            if (pnode)"				<< endl
               << "            {"					<< endl
               << "               int kid = pnode->inParent;"		<< endl
               << "               ptr[kid] = pnode->unpacker(xdrs,pnode);"
	       								<< endl
               << "            }"					<< endl
               << "            else"					<< endl
               << "            {"					<< endl
               << "               unsigned int skip;"			<< endl
	       << "               xdr_u_int(xdrs,&skip);"		<< endl
               << "               xdr_setpos(xdrs,xdr_getpos(xdrs)+skip);"
	       								<< endl
               << "            }"					<< endl
               << "         }"						<< endl;
      }
      cFile << "      }"						<< endl
            << "      xdr_setpos(xdrs,start+size);"			<< endl
            << "   }"							<< endl
            << "   return this1;"					<< endl
            << "}"							<< endl;
   }
	
	#endif
}
 
/* Generate c function to read from binary stream into c-structures */

void constructReadFunc(DOMElement* topEl)
{
#if 0
   XString topS(topEl->getTagName());
   char* topType = simpleStructType(S(topS));
   char topT[500];
   strncpy(topT,topType,500);
   char* term = rindex(topT,'_');
   *term = 0;
   hFile								<< endl
	 << topType << "* read_" << topT
	 << "(" << classPrefix << "_iostream_t* fp" << ");"		<< endl;

   cFile								<< endl
	 << topType << "* read_" << topT
	 << "(" << classPrefix << "_iostream_t* fp" << ")"		<< endl
	 << "{"								<< endl
	 << "   return unpack_" << topT << "(fp->xdrs,fp->popTop);"	<< endl
	 << "}"								<< endl;
   delete [] topType;
#endif
}

/* Generate streamers for packing c-structures onto a binary stream
 * and deleting them from memory when output is complete
 */

void constructPackers()
{
#if 0
   cFile << endl;
   for (int t = 0; t < tagListLength; t++)
   {
      DOMElement* tagEl = tagList[t];
      XString tagS(tagList[t]->getTagName());
      char* listType = listStructType(S(tagS));
      char* simpleType = simpleStructType(S(tagS));

      cFile << "static ";

      char* tagType;
      XString repAttS("maxOccurs");
      XString repS(tagEl->getAttribute(X(repAttS)));
      int rep = (repS.equals("unbounded"))? 9999 : atoi(S(repS));
      if (rep > 1)
      {
         tagType = listType;
      }
      else
      {
         tagType = simpleType;
      }
      char tagT[500];
      strncpy(tagT,tagType,500);
      char* term = rindex(tagT,'_');
      *term = 0;
      cFile << "int pack_" << tagT << "(XDR* xdrs, "
            << tagType << "* this1);"				<< endl;
   }

   for (int t = 0; t < tagListLength; t++)
   {
      DOMElement* tagEl = tagList[t];
      XString tagS(tagList[t]->getTagName());
      char* listType = listStructType(S(tagS));
      char* simpleType = simpleStructType(S(tagS));

      cFile << endl << "static ";

      char* tagType;
      XString repAttS("maxOccurs");
      XString repS(tagEl->getAttribute(X(repAttS)));
      int rep = (repS.equals("unbounded"))? 9999 : atoi(S(repS));
      if (rep > 1)
      {
         tagType = listType;
      }
      else
      {
         tagType = simpleType;
      }
      char tagT[500];
      strncpy(tagT,tagType,500);
      char* term = rindex(tagT,'_');
      *term = 0;
      cFile << "int pack_" << tagT << "(XDR* xdrs, "
            << tagType << "* this1)"				<< endl
            << "{"						<< endl
            << "   int m;"					<< endl
            << "   unsigned int size=0;"			<< endl
            << "   int base,start,end;"				<< endl
            << "   base = xdr_getpos(xdrs);"			<< endl
            << "   xdr_u_int(xdrs,&size);"			<< endl
            << "   start = xdr_getpos(xdrs);"			<< endl
    								<< endl;
      if (rep > 1)
      {
         cFile << "   xdr_u_int(xdrs,&this1->mult);"		<< endl
               << "   for (m = 0; m < this1->mult; m++)"	<< endl
               << "   {"					<< endl;
      }
      else
      {
         cFile << "   {"					<< endl;
      }

      DOMNamedNodeMap* attList = tagEl->getAttributes();
      for (int a = 0; a < attList->getLength(); a++)
      {
         DOMNode* att = attList->item(a);
         XString typeS(att->getNodeValue());
         XString nameS(att->getNodeName());
         char nameStr[500];
         if (rep > 1)
         {
            sprintf(nameStr,"in[m].%s",S(nameS));
         }
         else
         {
            sprintf(nameStr,"%s",S(nameS));
         }
         if (typeS.equals("int"))
         {
            cFile << "      xdr_int(xdrs,&this1->"
                  << nameStr << ");"				 << endl;
         }
         if (typeS.equals("long"))
         {
            cFile << "#ifndef XDR_LONGLONG_MISSING"		 << endl
                  << "         xdr_longlong_t(xdrs,&this1->"
	          << nameStr << ");"				 << endl
                  << "#else"					 << endl
                  << "         {"				 << endl
                  << "            int* " << nameStr << "_ = "
                  << "(int*)&this1->" << nameStr << ";"		 << endl
                  << "# if __BIG_ENDIAN__"			 << endl
                  << "            xdr_int(xdrs,&"
                  << nameStr << "_[0]);"			 << endl
                  << "            xdr_int(xdrs,&"
                  << nameStr << "_[1]);"			 << endl
                  << "# else"					 << endl
                  << "            xdr_int(xdrs,&"
                  << nameStr << "_[1]);"			 << endl
                  << "            xdr_int(xdrs,&"
                  << nameStr << "_[0]);"			 << endl
                  << "# endif"					 << endl
                  << "         }"				 << endl
                  << "#endif"					 << endl;
         }
         else if (typeS.equals("float"))
         {
            cFile << "      xdr_float(xdrs,&this1->"
                  << nameStr << ");"				 << endl;
         }
         else if (typeS.equals("double"))
         {
            cFile << "      xdr_double(xdrs,&this1->" 
                  << nameStr << ");" 				 << endl;
         }
         else if (typeS.equals("boolean"))
         {
            cFile << "      xdr_bool(xdrs,&this1->"
                  << nameStr << ");"				 << endl;
         }
         else if (typeS.equals("Particle_t"))
         {
            cFile << "      xdr_int(xdrs,(int*)&this1->"
                  << nameStr << ");"  				 << endl;
         }
         else if (typeS.equals("string"))
         {
            cFile << "      xdr_string(xdrs,&this1->" 
                  << nameStr << ", 1000000);"			 << endl;
         }
         else if (typeS.equals("anyURI"))
         {
            cFile << "      xdr_string(xdrs,&this1->" 
                  << nameStr << ", 1000000);" 			 << endl;
         }
         else
         {
            /* ignore attributes with unrecognized values */
         }
      }

      DOMNodeList* contList = tagEl->getChildNodes();
      for (int c = 0; c < contList->getLength(); c++)
      {
         DOMNode* cont = contList->item(c);
         short type = cont->getNodeType();
         if (type == DOMNode::ELEMENT_NODE)
         {
            DOMElement* contEl = (DOMElement*) cont;
            XString nameS(contEl->getTagName());
            char* listType = listStructType(S(nameS));
            char* simpleType = simpleStructType(S(nameS));
            char* names = plural(S(nameS));
            XString reS(contEl->getAttribute(X(repAttS)));
	    int re = (reS.equals("unbounded"))? 9999 : atoi(S(reS));
            char* contType;
            if (re > 1)
            {
               contType = listType;
            }
            else
            {
               contType = simpleType;
            }
            char contT[500];
            strncpy(contT,contType,500);
            char* term = rindex(contT,'_');
            *term = 0;
            cFile << "      if (this1->"
                  << ((rep > 1)? "in[m]." : "")
                  << ((re > 1)? names : S(nameS)) << ")"		<< endl
                  << "      {"						<< endl
                  << "         pack_" << contT << "(xdrs,this1->"
                  << ((rep > 1)? "in[m]." : "")
                  << ((re > 1)? names : S(nameS)) << ");"		<< endl
                  << "      }"						<< endl
                  << "      else"					<< endl
                  << "      {"						<< endl
		  << "         int zero=0;"				<< endl
                  << "         xdr_int(xdrs,&zero);"			<< endl
                  << "      }"						<< endl;
            delete [] listType;
            delete [] simpleType;
            delete [] names;
         }
      }

      cFile << "   }"							<< endl
            << "   FREE(this1);"						<< endl
            << "   end = xdr_getpos(xdrs);"				<< endl
            << "   xdr_setpos(xdrs,base);"				<< endl
	    << "   size = end-start;"					<< endl
            << "   xdr_u_int(xdrs,&size);"				<< endl
            << "   xdr_setpos(xdrs,end);"				<< endl
            << "   return size;"					<< endl
            << "}"							<< endl;
   }
#endif
}

/* Generate c functions for exporting c-structures onto a binary stream */
 
void constructFlushFunc(DOMElement* el)
{
#if 0
   DOMElement* topEl = tagList[0];
   XString topS(topEl->getTagName());
   char* topType = simpleStructType(S(topS));
   char topT[500];
   strncpy(topT,topType,500);
   char* term = rindex(topT,'_');
   *term = 0;

   constructPackers();

   hFile 							<< endl
	 << "int flush_" << topT << "(" << topType << "* this1,"
	 << classPrefix << "_iostream_t* fp" << ");"		<< endl;

   cFile 							<< endl
	 << "int flush_" << topT << "(" << topType << "* this1,"
	 << classPrefix << "_iostream_t* fp" << ")"		<< endl
	 << "{"							<< endl
         << "   if (this1 == 0)"				<< endl
         << "   {"						<< endl
	 << "      return 0;"					<< endl
         << "   }"						<< endl
         << "   else if (fp == 0)"				<< endl
         << "   {"						<< endl
	 << "      XDR* xdrs = (XDR*)malloc(sizeof(XDR));"	<< endl
	 << "      int max_buffer_size = 1000000;"		<< endl
	 << "      char* dump = (char*)malloc(max_buffer_size);"<< endl
	 << "      xdrmem_create(xdrs,dump,max_buffer_size,XDR_ENCODE);"
	 							<< endl
	 << "      pack_" << topT << "(xdrs,this1);"		<< endl
	 << "      xdr_destroy(xdrs);"				<< endl
	 << "      free(xdrs);"					<< endl
	 << "      free(dump);"					<< endl
	 << "      return 0;"					<< endl
         << "   }"						<< endl
         << "   else if (fp->iomode == HDDM_STREAM_OUTPUT)"	<< endl
	 << "   {"						<< endl
	 << "      pack_" << topT << "(fp->xdrs,this1);"		<< endl
	 << "      return 0;"					<< endl
	 << "   }"						<< endl
	 << "}"							<< endl;
   delete [] topType;
#endif
}

/* Generate c functions that match up corresponding elements between
 * the c structures and the data model that appears on the input
 * binary stream.  If successful, these routines build a hierarchical
 * structure (the "pop tree") that gives directions to the unpackers.
 *
 * The matching rules are as follows:
 *
 *  1) The attribute list for any given tag must be identical in content
 *     and order wherever it appears, otherwise there is a collision.
 *
 *  2) The content list for any given tag must be internally consistent
 *     within each model, but there are no requirements for agreement
 *     between the c-structures and the binary stream models.  Only the
 *     contents which appear in both models will be unpacked, however.
 */

void writeMatcher()
{
#if 0
   cFile							<< endl
	 << "static int getTag(char* d, char* tag)"		<< endl
	 << "{"							<< endl
	 << "   int level;"					<< endl
	 << "   char* token;"					<< endl
	 << "   char line[500];"				<< endl
	 << "   strncpy(line,d,500);"				<< endl
	 << "   line[499] = 0;"					<< endl
	 << "   level = index(line,'<')-line;"			<< endl
	 << "   if (level < 500 &&"				<< endl
	 << "      (token = strtok(line+level+1,\" >\")))"	<< endl
	 << "   {"						<< endl
	 << "      strncpy(tag,token,500);"			<< endl
	 << "      return level/2;"				<< endl
	 << "   }"						<< endl
	 << "   return -1;"						<< endl
	 << "}"							<< endl
   								<< endl
	 << "static char* getEndTag(char* d, char* tag)"	<< endl
	 << "{"							<< endl
	 << "   char line[500];"				<< endl
	 << "   char endTag[510];"				<< endl
	 << "   strncpy(line,d,500);"				<< endl
	 << "   line[499] = 0;"					<< endl
         << "   if (strstr(strtok(line,\"\\n\"),\"/>\") == 0)"	<< endl
	 << "   {"						<< endl
	 << "      sprintf(endTag,\"</%s>\",tag);"		<< endl
	 << "   }"						<< endl
	 << "   else"						<< endl
	 << "   {"						<< endl
         << "      strcpy(endTag,\"/>\");"			<< endl
	 << "   }"						<< endl
	 << "   return strstr(d,endTag);"			<< endl
	 << "}"							<< endl
        							<< endl
	 << "static void collide(char* tag)"			<< endl
	 << "   {"						<< endl
	 << "      fprintf(stderr,\"HDDM Error: \");"		<< endl
	 << "      fprintf(stderr,\"input template model for tag \");"	<< endl
	 << "      fprintf(stderr,\"%s does not match c code.\", tag);"	<< endl
	 << "      fprintf(stderr,\"\\nPlease recompile.\\n\");"	<< endl
	 << "      exit(9);"					<< endl
	 << "   }"						<< endl
								<< endl
	 << "static popNode* matches(char* b, char* c)"		<< endl
	 << "{"							<< endl
	 << "   char btag[500];"				<< endl
	 << "   char ctag[500];"				<< endl
	 << "   int blevel, clevel;"				<< endl
         << "   int ptrSeqNo = 0;"				<< endl
	 << "   blevel = getTag(b,btag);"			<< endl
	 << "   while ((clevel = getTag(c,ctag)) == blevel)"	<< endl
	 << "   {"						<< endl
	 << "      if "
	 << "((clevel == blevel) && (strcmp(ctag,btag) == 0))"	<< endl
	 << "      {"						<< endl
         << "         popNode* this1 = "
         << "(popNode*)malloc(sizeof(popNode));"		<< endl
	 << "         int len = index(c+1,'\\n') - c;"		<< endl
	 << "         if (strncmp(c,b,len) != 0)"		<< endl
	 << "         {"					<< endl
         << "            collide(btag);"			<< endl
	 << "         }"					<< endl;

   int firstTag = 1;
   for (int t = 0; t < tagListLength; t++)
   {
      XString tagS(tagList[t]->getTagName());
      XString repAttS("maxOccurs");
      XString repS(tagList[t]->getAttribute(X(repAttS)));
      int rep = (repS.equals("unbounded"))? 9999 : atoi(S(repS));
      char* tagType;
      if (rep > 1)
      {
         tagType = listStructType(S(tagS));
      }
      else
      {
         tagType = simpleStructType(S(tagS));
      }
      char tagT[500];
      strncpy(tagT,tagType,500);
      char* term = rindex(tagT,'_');
      *term = 0;

      if (firstTag)
      {
         firstTag = 0;
         cFile << "         if ";
      }
      else
      {
         cFile << "         else if ";
      }
      cFile << "(strcmp(btag,\"" << S(tagS) << "\") == 0)"	<< endl
            << "         {"					<< endl
	    << "            this1->unpacker = "
	    << "(void*(*)(XDR*,popNode*))"
            << "unpack_" << tagT << ";"				<< endl
            << "         }"					<< endl;
   }

   cFile << "         else"					<< endl
	 << "         {"					<< endl
         << "            collide(btag);"			<< endl
	 << "         }"					<< endl
         << "         this1->inParent = ptrSeqNo;"		<< endl
         << "         this1->popListLength = 0;"		<< endl
	 << "         c = index(c+1,'\\n');"			<< endl
	 << "         b = index(b+1,'\\n');"			<< endl
	 << "         while (getTag(b,btag) > blevel)"		<< endl
	 << "         {"					<< endl
         << "            this1->popList[this1->popListLength++] = matches(b,c);"
								<< endl 
         << "            if (this1->popListLength > "
         << MAX_POPLIST_LENGTH << ")"				<< endl
         << "            {"					<< endl
         << "               fprintf(stderr,"
         << "\"hddm error - posList overflow.\\n\");"		<< endl
         << "               fprintf(stderr,"
         << "\"Increase MAX_POPLIST_LENGTH and recompile.\\n\");" << endl
         << "               exit(9);"				<< endl
         << "            }"					<< endl
	 << "            b = getEndTag(b,btag);"		<< endl
	 << "            b = index(b+1,'\\n');"			<< endl
	 << "         }"					<< endl
	 << "         return this1;"				<< endl
	 << "      }"						<< endl
	 << "      else"					<< endl
	 << "      {"						<< endl
	 << "         c = getEndTag(c,ctag);"			<< endl
	 << "         c = index(c+1,'\\n');"			<< endl
	 << "         ++ptrSeqNo;"				<< endl
	 << "      }"						<< endl
	 << "   }"						<< endl
	 << "   return 0;"					<< endl
	 << "}"							<< endl;
#endif
}

/* Generate c code to open a hddm file for reading */

void constructOpenFunc(DOMElement* el)
{
#if 0
   XString tagS(el->getTagName());
   char* tagType = simpleStructType(S(tagS));
   char tagT[500];
   strncpy(tagT,tagType,500);
   char* term = rindex(tagT,'_');
   *term = 0;
   hFile							<< endl
	 << classPrefix << "_iostream_t* "
	 << "open_" << tagT << "(char* filename);"		<< endl;

   writeMatcher();

   cFile							<< endl
	 << classPrefix << "_iostream_t* "
	 << "open_" << tagT << "(char* filename)"		<< endl
	 << "{"							<< endl
	 << "   " << classPrefix << "_iostream_t* fp = "
	 << "(" << classPrefix << "_iostream_t*)"
	 << "malloc(sizeof(" << classPrefix << "_iostream_t));"	<< endl
	 << "   char* p;"					<< endl
	 << "   char* head;"					<< endl
         << "   if (filename)"					<< endl
         << "   {"						<< endl
	 << "      fp->fd = fopen(filename,\"r\");"		<< endl
         << "   }"						<< endl
         << "   else"						<< endl
         << "   {"						<< endl
	 << "      fp->fd = fdopen(0,\"r\");"			<< endl
         << "   }"						<< endl
	 << "   if (fp->fd == 0)"				<< endl
	 << "   {"						<< endl
	 << "      free(fp);"					<< endl
	 << "      return 0;"					<< endl
	 << "   }"						<< endl
	 << "   fp->iomode = HDDM_STREAM_INPUT;"		<< endl
	 << "   head = (char*)malloc(1000000);"			<< endl
	 << "   *head = 0;"					<< endl
	 << "   for (p = head;"					<< endl
	 << "        strstr(head,\"</HDDM>\") == 0;"		<< endl
	 << "        p += strlen(p))"				<< endl
	 << "   {"						<< endl
	 << "      if (p-head < 999000)"			<< endl
	 << "      {"						<< endl
	 << "         fgets(p,1000,fp->fd);"			<< endl
	 << "      }"						<< endl
	 << "      else"					<< endl
	 << "      {"						<< endl
	 << "         break;"					<< endl
	 << "      }"						<< endl
	 << "   }"						<< endl
	 << "   fp->popTop = matches(head,HDDM_" << classPrefix
	 << "_DocumentString);"					<< endl
         << "   if (fp->popTop == 0)"				<< endl
	 << "   {"						<< endl
	 << "      fprintf(stderr,\"HDDM Error: \");"		<< endl
	 << "      fprintf(stderr,\"input template model \");"	<< endl
	 << "      fprintf(stderr,\"does not match c code.\");"	<< endl
	 << "      fprintf(stderr,\"  Please recompile.\\n\");"	<< endl
	 << "      exit(9);"					<< endl
	 << "   }"						<< endl
	 << "   fp->filename = "
         << "(char*)malloc(strlen(filename) + 1);"		<< endl
	 << "   strcpy(fp->filename,filename);"			<< endl
	 << "   fp->xdrs = (XDR*)malloc(sizeof(XDR));"		<< endl
         << "   xdrstdio_create(fp->xdrs,fp->fd,XDR_DECODE);"	<< endl
	 << "   return fp;"					<< endl
	 << "}"							<< endl;
   delete [] tagType;
#endif
}

/* Generate the c code to open a hddm file for writing */

void constructInitFunc(DOMElement* el)
{
#if 0
   XString tagS(el->getTagName());
   char* tagType = simpleStructType(S(tagS));
   char tagT[500];
   strncpy(tagT,tagType,500);
   char* term = rindex(tagT,'_');
   *term = 0;
   hFile							<< endl
	 << classPrefix << "_iostream_t* "
	 << "init_" << tagT << "(char* filename);"		<< endl;
   cFile							<< endl
	 << classPrefix << "_iostream_t* "
	 << "init_" << tagT << "(char* filename)"		<< endl
	 << "{"							<< endl
	 << "   int len;"					<< endl	
	 << "   char* head;"					<< endl
	 << "   " << classPrefix << "_iostream_t* fp = "
	 << "(" << classPrefix << "_iostream_t*)"
	 << "malloc(sizeof(" << classPrefix << "_iostream_t));"	<< endl
         << "   if (filename)"					<< endl
         << "   {"						<< endl
	 << "      fp->fd = fopen(filename,\"w\");"		<< endl
         << "   }"						<< endl
         << "   else"						<< endl
         << "   {"						<< endl
	 << "      fp->fd = fdopen(1,\"w\");"			<< endl
         << "   }"						<< endl
	 << "   if (fp->fd == 0)"				<< endl
	 << "   {"						<< endl
	 << "      free(fp);"					<< endl
	 << "      return 0;"					<< endl
	 << "   }"						<< endl
	 << "   fp->iomode = HDDM_STREAM_OUTPUT;"		<< endl
	 << "   len = strlen(HDDM_" 
	 << classPrefix << "_DocumentString);"			<< endl
	 << "   head = (char*)malloc(len+1);"			<< endl
	 << "   strcpy(head,HDDM_"
	 << classPrefix << "_DocumentString);"			<< endl
	 << "   if (fwrite(head,1,len,fp->fd) != len)"		<< endl
	 << "   {"						<< endl
	 << "      fprintf(stderr,\"HDDM Error: \");"		<< endl
	 << "      fprintf(stderr,\"error writing to \");"	<< endl
	 << "      fprintf(stderr,\"output file %s\\n\",filename);" << endl
	 << "      exit(9);"					<< endl
	 << "   }"						<< endl
	 << "   fp->filename = "
         << "(char*)malloc(strlen(filename) + 1);"		<< endl
	 << "   strcpy(fp->filename,filename);"			<< endl
         << "   fp->popTop = 0;"				<< endl
	 << "   fp->xdrs = (XDR*)malloc(sizeof(XDR));"		<< endl
         << "   xdrstdio_create(fp->xdrs,fp->fd,XDR_ENCODE);"	<< endl
	 << "   free(head);"					<< endl
	 << "   return fp;"					<< endl
	 << "}"							<< endl;
   delete [] tagType;
#endif
}

/* Generate the c code to close an open hddm file */

void constructCloseFunc(DOMElement* el)
{
#if 0
   XString tagS(el->getTagName());
   char* tagType = simpleStructType(S(tagS));
   char tagT[500];
   strncpy(tagT,tagType,500);
   char* term = rindex(tagT,'_');
   *term = 0;
   hFile							<< endl
	 << "void close_" << tagT << "("
	 << classPrefix << "_iostream_t* fp);"			<< endl;

   cFile							<< endl
         << "void popaway(popNode* p)"				<< endl
         << "{"							<< endl
         << "   if (p)"						<< endl
         << "   {"						<< endl
         << "      int n;"					<< endl
         << "      for (n = 0; n < p->popListLength; n++)"	<< endl
         << "      {"						<< endl
         << "         popaway(p->popList[n]);"			<< endl
         << "      }"						<< endl
         << "      free(p);"					<< endl
         << "   }"						<< endl
         << "}"							<< endl
								<< endl
	 << "void close_" << tagT
	 << "(" << classPrefix << "_iostream_t* fp)"		<< endl
	 << "{"							<< endl
	 << "   xdr_destroy(fp->xdrs);"				<< endl	
	 << "   free(fp->xdrs);"				<< endl
	 << "   fclose(fp->fd);"				<< endl	
	 << "   free(fp->filename);"				<< endl	
         << "   popaway(fp->popTop);"				<< endl
	 << "   free(fp);"					<< endl	
	 << "}"							<< endl;
   delete [] tagType;
#endif
}

/* Generate the xml template in normal form and store in a string */

void constructDocument(DOMElement* el)
{
   static int indent = 0;
   cFile << "\"";
   for (int n = 0; n < indent; n++)
   {
      cFile << "  ";
   }
   
   XString tagS(el->getTagName());
   cFile << "<" << S(tagS);
   DOMNamedNodeMap* attrList = el->getAttributes();
   int attrListLength = attrList->getLength();
   for (int a = 0; a < attrListLength; a++)
   {
      DOMNode* node = attrList->item(a);
      XString nameS(node->getNodeName());
      XString valueS(node->getNodeValue());
      cFile << " " << S(nameS) << "=\\\"" << S(valueS) << "\\\"";
   }

   DOMNodeList* contList = el->getChildNodes();
   int contListLength = contList->getLength();
   if (contListLength > 0)
   {
      cFile << ">\\n\"" << endl;
      indent++;
      for (int c = 0; c < contListLength; c++)
      {
         DOMNode* node = contList->item(c);
         if (node->getNodeType() == DOMNode::ELEMENT_NODE)
         {
            DOMElement* contEl = (DOMElement*) node;
            constructDocument(contEl);
         }
      }
      indent--;
      cFile << "\"";
      for (int n = 0; n < indent; n++)
      {
         cFile << "  ";
      }
      cFile << "</" << S(tagS) << ">\\n\"" << endl;
   }
   else
   {
      cFile << " />\\n\"" << endl;
   }
}

int main(int argC, char* argV[])
{
   try
   {
      XMLPlatformUtils::Initialize();
   }
   catch (const XMLException* toCatch)
   {
      XString msg(toCatch->getMessage());
      cerr << "hddm-cpp: Error during initialization! :\n"
           << S(msg) << endl;
      return 1;
   }

   if (argC < 2)
   {
      usage();
      return 1;
   }
   else if ((argC == 2) && (strcmp(argV[1], "-?") == 0))
   {
      usage();
      return 2;
   }

   const char*  xmlFile = 0;
   int argInd;
   for (argInd = 1; argInd < argC; argInd++)
   {
      if (argV[argInd][0] != '-')
      {
         break;
      }
      if (strcmp(argV[argInd],"-v") == 0)
      {
         verifyOnly = true;
      }
      else if (strcmp(argV[argInd],"-o") == 0)
      {
         hFilename = argV[++argInd];
      }
      else
      {
         cerr << "Unknown option \'" << argV[argInd]
              << "\', ignoring it\n" << endl;
      }
   }

   if (argInd != argC - 1)
   {
      usage();
      return 1;
   }
   xmlFile = argV[argInd];

#if defined OLD_STYLE_XERCES_PARSER
   DOMDocument* document = parseInputDocument(xmlFile,false);
#else
   DOMDocument* document = buildDOMDocument(xmlFile,false);
#endif
   if (document == 0)
   {
      cerr << "hddm-cpp : Error parsing HDDM document, "
           << "cannot continue" << endl;
      return 1;
   }

   DOMElement* rootEl = document->getDocumentElement();
   XString rootS(rootEl->getTagName());
   if (!rootS.equals("HDDM"))
   {
      cerr << "hddm-cpp error: root element of input document is "
           << "\"" << S(rootS) << "\", expected \"HDDM\""
           << endl;
      return 1;
   }

   XString classAttS("class");
   XString classS(rootEl->getAttribute(X(classAttS)));
   classPrefix = S(classS);

   char hname[510];
	char hppname[510];
   if (verifyOnly)
   {
      sprintf(hname,"/dev/null");
   }
   else if (hFilename)
   {
      sprintf(hname,"%s.h",hFilename);
   }
   else
   {
      sprintf(hname,"hddm_%s.h",classPrefix);
   }
	sprintf(hppname,"%spp",hname);

   hFile.open(hppname);
   if (! hFile.is_open())
   {
      cerr << "hddm-cpp error: unable to open output file "
           << hppname << endl;
      return 1;
   }

   char cname[510];
	sprintf(cname,"hddm_containers_%s.cc",classPrefix);
   cFile.open(cname);
   if (! cFile.is_open())
   {
      cerr << "hddm-cpp error: unable to open output file "
           << cname << endl;
      return 1;
   }

   hFile << "/*"						<< endl
	 << " * " << hppname << " - DO NOT EDIT THIS FILE"	<< endl
	 << " *"						<< endl
	 << " * This file was generated automatically by hddm-cpp"
	 << " from the file"					<< endl
    << " * " << xmlFile					<< endl
    << " * This header file defines the c++ structures that"
	 << " hold the data"					<< endl
	 << " * described in the data model"
    << " (from " << xmlFile << "). "			<< endl
	 << " *"						<< endl
	 << " * The hddm data model tool set was written by"	<< endl
	 << " * Richard Jones, University of Connecticut."	<< endl
	 << " *"						<< endl
	 << " *"						<< endl
	 << " * The C++ container system was written by"	<< endl
	 << " * David Lawrence, Jefferson Lab."	<< endl
	 << " *"						<< endl
	 << " * For more information see the following web site"<< endl
	 << " *"						<< endl
	 << " * http://zeus.phys.uconn.edu/halld/datamodel/doc"	<< endl
	 << " *"						<< endl
	 << " */"						<< endl
	 							<< endl;

   cFile	<< "/*"						<< endl
			<< " * " << cname << " - DO NOT EDIT THIS FILE"	<< endl
			<< " *"						<< endl
			<< " * This file was generated automatically by hddm-cpp"
			<< " from the file"					<< endl
			<< " * " << xmlFile					<< endl
			<< " *"						<< endl
			<< " * The hddm data model tool set was written by"	<< endl
			<< " * Richard Jones, University of Connecticut."	<< endl
			<< " *"						<< endl
			<< " *"						<< endl
			<< " * The C++ container system was written by"	<< endl
			<< " * David Lawrence, Jefferson Lab."	<< endl
			<< " *"						<< endl
			<< " * For more information see the following web site"<< endl
			<< " *"						<< endl
			<< " * http://zeus.phys.uconn.edu/halld/datamodel/doc"	<< endl
			<< " */"						<< endl
			<< endl;

   constructGroup(rootEl);
	
   hFile << "#include \""<<hname<<"\"" 		<< endl
			<< endl
			<< endl
			<< "//----------------------------------------------------------------------------"<<endl
			<< "//------------------------------- hddm_containers_t -------------------------------"<<endl
			<< "//----------------------------------------------------------------------------"<<endl
			<< "typedef struct{"<<endl
			<< endl
			<< "	/// This struct should consist ONLY of class pointers derived from DContainer"<<endl
			<< "	"<<containerList<<endl
			<< "}hddm_containers_t;"<<endl
			<< endl
			<< "// in DANA/hddm_containers.cc"<<endl
			<< "derror_t init_hddm_containers_t(hddm_containers_t *hddm);"<<endl
			<< "derror_t delete_hddm_containers_t(hddm_containers_t *hddm);"<<endl
			<< endl;

   cFile << "#include \"" << hppname << "\"" 			<< endl
			<< "//----------------------"<<endl
			<< "// init_hddm_containers_t"<<endl
			<< "//----------------------"<<endl
			<< "derror_t init_hddm_containers_"<<classPrefix<<"(hddm_containers_t *hddm)"<<endl
			<< "{"<<endl
			<< "	/// Call constructors for all DContainer derived classes"<<endl
			<< constructorCalls<<endl
			<< "}"<<endl
			<< endl
			<< "//----------------------"<<endl
			<< "// delete_hddm_containers_"<<classPrefix<<endl
			<< "//----------------------"<<endl
			<< "derror_t delete_hddm_containers_t(hddm_containers_t *hddm)"<<endl
			<< "{"<<endl
			<< "	/// Call destructors for all DContainer derived classes"<<endl
			<< destructorCalls<<endl
			<< "}"<<endl
			<< endl;



#if 0
   hFile							<< endl
	 << "#ifdef __cplusplus"				<< endl
	 << "extern \"C\" {"					<< endl
	 << "#endif"						<< endl;
   constructMakeFuncs();
   hFile							<< endl
	 << "#ifdef __cplusplus"				<< endl
	 << "}"							<< endl
	 << "#endif"						<< endl;

   hFile 							<< endl
	 << "#ifndef " << classPrefix << "_DocumentString" 	<< endl
	 << "#define " << classPrefix << "_DocumentString" 	<< endl
         							<< endl
	 << "extern "
	 << "char HDDM_" << classPrefix << "_DocumentString[];"	<< endl
         							<< endl
         << "#ifdef INLINE_PREPEND_UNDERSCORES"			<< endl
         << "#define inline __inline"				<< endl
         << "#endif"						<< endl
								<< endl
	 << "#endif /* " << classPrefix << "_DocumentString */"	<< endl;

   cFile							<< endl
	 << "char HDDM_" << classPrefix << "_DocumentString[]"
	 << " = "						<< endl;
   constructDocument(rootEl);
   cFile << ";"							<< endl;

   hFile 							<< endl
	 << "#ifndef HDDM_STREAM_INPUT"				<< endl
	 << "#define HDDM_STREAM_INPUT -91"			<< endl
	 << "#define HDDM_STREAM_OUTPUT -92"			<< endl
           							<< endl
	 << "struct popNode_s {"				<< endl
         << "   void* (*unpacker)(XDR*, struct popNode_s*);"	<< endl
         << "   int inParent;"					<< endl
         << "   int popListLength;"				<< endl
         << "   struct popNode_s* popList["
         << MAX_POPLIST_LENGTH << "];"				<< endl
         << "};"						<< endl
         << "typedef struct popNode_s popNode;"			<< endl
                                                                << endl
	 << "typedef struct {"					<< endl
	 << "   FILE* fd;"					<< endl
	 << "   int iomode;"					<< endl
	 << "   char* filename;"				<< endl
         << "   XDR* xdrs;"					<< endl
	 << "   popNode* popTop;"				<< endl
	 << "} " << classPrefix << "_iostream_t;"		<< endl
								<< endl
	 << "#endif /* HDDM_STREAM_INPUT */"			<< endl;

   constructUnpackers();

   hFile							<< endl
	 << "#ifdef __cplusplus"				<< endl
	 << "extern \"C\" {"					<< endl
	 << "#endif"						<< endl;
   constructReadFunc(rootEl);
   constructFlushFunc(rootEl);
   constructOpenFunc(rootEl);
   constructInitFunc(rootEl);
   constructCloseFunc(rootEl);
   hFile							<< endl
	 << "#ifdef __cplusplus"				<< endl
	 << "}"							<< endl
	 << "#endif"						<< endl;

#endif

   XMLPlatformUtils::Terminate();
   return 0;
}
