/***************************************************************
 * Name:      wxcompat.h
 * Purpose:   minimal wx-types compatibility layer for the
 *            wxEDID core parser (de-wx'ed GTK4 port).
 *            Implements exactly the wxString API subset used by
 *            the EDID/CEA core, with wxWidgets semantics.
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#ifndef WXCOMPAT_H
#define WXCOMPAT_H 1

#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cctype>
#include <cstdlib>

//------------
// wxString -> std::string adapter
class wxc_String {
   protected:
      std::string str;

   public:
      wxc_String() {}
      wxc_String(const char* cstr) : str(cstr ? cstr : "") {}
      wxc_String(const std::string& s) : str(s) {}

      operator const char* () const { return str.c_str(); }
      operator const std::string& () const { return str; }

      wxc_String& operator=(const char* cstr) { str = (cstr ? cstr : ""); return *this; }
      wxc_String& operator=(const std::string& s) { str = s; return *this; }

      const char* c_str() const { return str.c_str(); }
      const std::string& std_str() const { return str; }

      size_t Len() const { return str.size(); }
      bool   IsEmpty() const { return str.empty(); }
      void   Empty() { str.clear(); }

      // Append: in-place concatenation, wx semantics
      wxc_String& Append(const char* cstr) { str.append(cstr ? cstr : ""); return *this; }
      wxc_String& Append(const wxc_String& s) { str.append(s.str); return *this; }
      wxc_String& Append(char c) { str.push_back(c); return *this; }

      // operator<< : appends and returns *this
      wxc_String& operator<<(const char* cstr) { str.append(cstr ? cstr : ""); return *this; }
      wxc_String& operator<<(const wxc_String& s) { str.append(s.str); return *this; }
      wxc_String& operator<<(char c) { str.push_back(c); return *this; }
      wxc_String& operator<<(int val) { char b[32]; snprintf(b, sizeof(b), "%d", val); str.append(b); return *this; }
      wxc_String& operator<<(unsigned int val) { char b[32]; snprintf(b, sizeof(b), "%u", val); str.append(b); return *this; }
      wxc_String& operator<<(long val) { char b[32]; snprintf(b, sizeof(b), "%ld", val); str.append(b); return *this; }
      wxc_String& operator<<(unsigned long val) { char b[32]; snprintf(b, sizeof(b), "%lu", val); str.append(b); return *this; }

      // Printf: sprintf-style formatting
      // NOTE: 512b buffer, matches upstream usage (log lines, hex strings).
      int Printf(const char* fmt, ...) {
         va_list argp;
         va_start(argp, fmt);
         char buf[512];
         int len = vsnprintf(buf, sizeof(buf), fmt, argp);
         va_end(argp);
         if (len < 0) { str.clear(); return -1; }
         str.assign(buf, (size_t) ((len < (int)sizeof(buf)) ? len : sizeof(buf) - 1));
         return len;
      }

      // SubString(from, to): BOTH indexes inclusive, wx semantics
      wxc_String SubString(size_t from, size_t to) const {
         if (from >= str.size()) return wxc_String();
         if (to >= str.size())   to = str.size() - 1;
         return wxc_String(str.substr(from, to - from + 1));
      }

      wxc_String Mid(size_t pos) const {
         if (pos >= str.size()) return wxc_String();
         return wxc_String(str.substr(pos));
      }

      int Cmp(const char* cstr) const { return str.compare(cstr); }
      int Cmp(const wxc_String& s) const { return str.compare(s.str); }

      // ToULong: returns false on invalid input (wx semantics: leading
      // whitespace allowed, trailing garbage rejected)
      bool ToULong(unsigned long* val, int base = 10) const {
         if (str.empty()) return false;
         const char* begin = str.c_str();
         char* end = NULL;
         unsigned long v = strtoul(begin, &end, base);
         // skip trailing whitespace before garbage check
         while ((end != NULL) && (isspace((unsigned char) *end))) end++;
         if (end == begin) return false;
         if ((end != NULL) && (*end != 0)) return false;
         if (val != NULL) *val = v;
         return true;
      }

      bool ToDouble(double* val) const {
         if (str.empty()) return false;
         const char* begin = str.c_str();
         char* end = NULL;
         double v = strtod(begin, &end);
         if (end == begin) return false;
         if ((end != NULL) && (*end != 0)) return false;
         if (val != NULL) *val = v;
         return true;
      }

      // FromAscii: wxString overload set -> single static factory
      static const wxc_String FromAscii(const char* cstr) { return wxc_String(cstr); }
      static const wxc_String FromAscii(const char* cstr, size_t len) {
         wxc_String s;
         if (cstr != NULL) { s.str.assign(cstr, strnlen(cstr, len)); }
         return s;
      }

      // raw byte accessors: ToAscii()/To8BitData() returned const char*;
      // From8BitData(const char*, size_t) copied len bytes
      const char* ToAscii() const { return str.c_str(); }
      const char* To8BitData() const { return str.c_str(); }
      static const wxc_String From8BitData(const char* data, size_t len) {
         wxc_String s;
         if (data != NULL) { s.str.assign(data, len); }
         return s;
      }
      static const wxc_String From8BitData(const char* cstr) {
         return wxc_String(cstr);
      }

      bool operator==(const wxc_String& s) const { return str == s.str; }
      bool operator==(const char* cstr) const { return str == cstr; }
      bool operator!=(const wxc_String& s) const { return str != s.str; }
      bool operator!=(const char* cstr) const { return str != cstr; }
};

//------------
// wxWidgets objarray -> std::vector of pointers
// NOTE: wxObjArray Detach() returns a pointer to the array slot;
// std::vector-based replacement returns the detached pointer directly.

template <class T>
class wxc_PtrArray {
   protected:
      std::vector<T> ar;

   public:
      wxc_PtrArray() {}
      ~wxc_PtrArray() {}

      void  Alloc(size_t n) { ar.reserve(n); }

      size_t GetCount() const { return ar.size(); }
      bool   IsEmpty() const { return ar.empty(); }

      T& operator[](size_t idx) { return ar[idx]; }
      const T& operator[](size_t idx) const { return ar[idx]; }

      T& Item(size_t idx) { return ar[idx]; }
      // int/enum overloads: core code indexes with enum constants
      // (e.g. FieldsAr.Item(STI_IDX_DMT2)), which would otherwise be
      // ambiguous against the const value-returning overload.
      template<typename I> T& Item(I idx) { return ar[(size_t) idx]; }

      void Add(T item) { ar.push_back(item); }
      void Insert(T item, size_t idx) {
         if (idx >= ar.size()) { ar.push_back(item); }
         else { ar.insert(ar.begin() + idx, item); }
      }
      void RemoveAt(size_t idx) {
         if (idx < ar.size()) ar.erase(ar.begin() + idx);
      }

      // Detach: remove entry, return it. Caller owns it now.
      T Detach(size_t idx) {
         T item = ar[idx];
         ar.erase(ar.begin() + idx);
         return item;
      }

      // Empty(): wxObjArray deletes owned objects; here the ported core
      // calls Empty()/Clear() where upstream arrays owned the objects.
      // NOT deleting here: ownership is handled by clear_fields()/dtors
      // in the ported core. Kept as alias pair for source compatibility.
      void Empty() { ar.clear(); }
      void Clear() { ar.clear(); }
};

//------------
// wx include shims used by the core headers

// wxObjArray semantics: the array stores objects by value and Item()
// returns a reference to the stored object. The core arrays store pointers
// (T = edi_grp_cl*), so wxArGrpField[i] yielded the pointer itself.
// => store T directly (elements are already pointers), no extra indirection.
#define WX_DECLARE_OBJARRAY(T, name) typedef wxc_PtrArray<T> name

class wxTreeItemData {};
class wxMenu {};

#endif /* WXCOMPAT_H */
