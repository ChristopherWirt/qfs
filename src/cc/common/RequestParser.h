//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id$
//
// Created 2010/05/14
// Author: Kate Labeeva, Mike Ovsiannikov
//
// Copyright 2010-2012 Quantcast Corp.
//
// This file is part of Kosmos File System (KFS).
//
// Licensed under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
//
// Classes to build high performance rfc822 style request header parsers.
//
//----------------------------------------------------------------------------

#ifndef REQUEST_PARSER_H
#define REQUEST_PARSER_H

#include "StBuffer.h"

#include <utility>
#include <string>
#include <algorithm>
#include <vector>

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

namespace KFS
{
using std::string;
using std::min;
using std::make_pair;
using std::pair;
using std::vector;
using std::lower_bound;

class DecIntParser
{
public:
    template<typename T>
    static bool Parse(
        const char*& ioPtr,
        size_t       inLen,
        T&           outValue)
    {
        const char*       thePtr    = ioPtr;
        const char* const theEndPtr = thePtr + inLen;
        while (thePtr < theEndPtr && (*thePtr & 0xFF) <= ' ') {
            thePtr++;
        }
        if (thePtr >= theEndPtr) {
            return false;
        }
        const bool theMinusFlag = *thePtr == '-';
        if ((theMinusFlag || *thePtr == '+') && ++thePtr >= theEndPtr) {
            return false;
        }
        // Do not use std::numeric_limits<>::max(), the code with these appears
        // to be less efficient than the constants below, probably due to
        // function call / exception handling overhead for
        // std::numeric_limits<>::max() throw() invocation.
        const int         kRadix         = 10;
        const T           kMax           = (T)(
            ~T(0) > 0 ? ~T(0) : ~(T(1) << (sizeof(T) * 8 - 1)));
        const T           theMaxDivRadix = kMax / kRadix;
        const char* const theStartPtr    = thePtr;
        T                 theVal         = 0;
        while (thePtr < theEndPtr) {
            const int theDigit = (*thePtr & 0xFF) - '0';
            if (theDigit < 0 || theDigit >= kRadix) {
                if (thePtr <= theStartPtr) {
                    return false;
                }
                break;
            }
            thePtr++;
            if (theVal > theMaxDivRadix ||
                    (theVal == theMaxDivRadix &&
                    T(theDigit) > kMax - theMaxDivRadix * kRadix)) {
                // Overflow. Negative overflow for signed types will always be
                // min() + 1, instead of min(), but this should be OK for now.
                theVal = kMax;
                break;
            }
            theVal = theVal * kRadix + theDigit;
            // theVal = (theVal << 3) + (theVal << 1) + theDigit;
        }
        outValue = theMinusFlag ? -theVal : theVal;
        ioPtr = thePtr;
        return true;
    }
};

class HexIntParser
{
public:
    template<typename T>
    static bool Parse(
        const char*& ioPtr,
        size_t       inLen,
        T&           outValue)
    {
        if (inLen <= 0) {
            return -1;
        }
        const unsigned char* thePtr =
            reinterpret_cast<const unsigned char*>(ioPtr);
        const unsigned char* const theEndPtr = thePtr + inLen;
        while (thePtr < theEndPtr && (*thePtr & 0xFF) <= ' ') {
            thePtr++;
        }
        if (thePtr >= theEndPtr) {
            return false;
        }
        const bool theMinusFlag = *thePtr == '-';
        if ((theMinusFlag || *thePtr == '+') && ++thePtr >= theEndPtr) {
            return false;
        }
        T theVal = 0;
        const unsigned char* const theNEndPtr = thePtr + sizeof(theVal) * 2 + 1;
        while (thePtr < theEndPtr) {
            const unsigned char theHex = sChar2Hex[*thePtr];
            if (theHex == (unsigned char)0xFF || thePtr == theNEndPtr) {
                if ((*thePtr & 0xFF) > ' ') {
                    return false;
                }
                break;
            }
            theVal = (theVal << 4) | theHex;
            ++thePtr;
        }
        outValue = theMinusFlag ? -theVal : theVal;
        ioPtr    = reinterpret_cast<const char*>(thePtr);
        return true;
    }
    static const unsigned char* GetChar2Hex() { return sChar2Hex; }
private:
    static const unsigned char sChar2Hex[256];
};

class TokenValue
{
public:
    TokenValue(
        const char* inPtr = 0,
        size_t      inLen = 0)
        : mPtr(inPtr),
          mLen(inLen)
        {}
    void clear()
    {
        mPtr = 0;
        mLen = 0;
    }
    bool empty() const
        { return (mLen <= 0); }
    bool operator==(
        const TokenValue& inRhs) const
    {
        return (mLen == inRhs.mLen &&
            (mLen <= 0 || mPtr == inRhs.mPtr ||
                memcmp(mPtr, inRhs.mPtr, 0) == 0));
    }
    const char* mPtr;
    size_t      mLen;
};

template<typename INT_PARSER=DecIntParser>
class ValueParserT
{
public:
    // The following three do not trim whitespace.
    // This is intentional, and it is up to the caller to handle this
    // appropriately.
    // For example PropertiesTokenizer trims white space.
    static void SetValue(
        const char*       inPtr,
        size_t            inLen,
        const TokenValue& inDefaultValue,
        TokenValue&       outValue)
    {
        if (inLen <= 0) {
            outValue = inDefaultValue;
        } else {
            outValue.mPtr = inPtr;
            outValue.mLen = inLen;
        }
    }
    static void SetValue(
        const char*   inPtr,
        size_t        inLen,
        const string& inDefaultValue,
        string&       outValue)
    {
        if (inLen <= 0) {
            outValue = inDefaultValue;
        } else {
            outValue.assign(inPtr, inLen);
        }
    }
    template<size_t DEFAULT_CAPACITY>
    static void SetValue(
        const char*                         inPtr,
        size_t                              inLen,
        const StringBufT<DEFAULT_CAPACITY>& inDefaultValue,
        StringBufT<DEFAULT_CAPACITY>&       outValue)
    {
        if (inLen <= 0) {
            outValue = inDefaultValue;
        } else {
            outValue.Copy(inPtr, inLen);
        }
    }
    template<typename T>
    static bool ParseFloat(
        const char*   inPtr,
        size_t        inLen,
        T&            outValue)
    {
        if (inLen <= 0) {
            return false;
        }
        char* theEndPtr = 0;
        if ((inPtr[inLen - 1] & 0xFF) <= ' ') {
            ParseFloatSelf(inPtr, &theEndPtr, outValue);
        } else {
            StringBufT<64> theTmp(inPtr, inLen);
            ParseFloatSelf(theTmp.GetPtr(), &theEndPtr, outValue);
        }
        return ((*theEndPtr & 0xFF) <= ' ');
    }
    static void ParseFloatSelf(
        const char*   inPtr,
        char**        inEndPtr,
        float&        outValue)
        { outValue = strtof(inPtr, inEndPtr); }
    static void ParseFloatSelf(
        const char*   inPtr,
        char**        inEndPtr,
        double&       outValue)
        { outValue = strtod(inPtr, inEndPtr); }
    static void SetValue(
        const char*  inPtr,
        size_t       inLen,
        const float& inDefaultValue,
        float&       outValue)
    {
        if (! ParseFloat(inPtr, inLen, outValue)) {
            outValue = inDefaultValue;
        }
    }
    static void SetValue(
        const char*   inPtr,
        size_t        inLen,
        const double& inDefaultValue,
        double&       outValue)
    {
        if (! ParseFloat(inPtr, inLen, outValue)) {
            outValue = inDefaultValue;
        }
    }
    // The following is used for integer overloaded versions of SetValue, in
    // the hope that this would be more efficient than the preceding generic
    // version the above.
    template<typename T>
    static bool ParseInt(
        const char*& ioPtr,
        size_t       inLen,
        T&           outValue)
    {
        return INT_PARSER::Parse(ioPtr, inLen, outValue);
    }

#define _KFS_ValueParser_IntTypes(f)                                           \
    f(char)                                                                    \
    f(signed char)   f(signed short int)   f(signed int)   f(signed long int)  \
    f(unsigned char) f(unsigned short int) f(unsigned int) f(unsigned long int)\
    f(long long int) f(unsigned long long int)

#define _KFS_DEFINE_ValueParser_IntSetValue(IT)   \
    static void SetValue(                         \
        const char* inPtr,                        \
        size_t      inLen,                        \
        const IT&   inDefaultValue,               \
        IT&         outValue)                     \
    {                                             \
        if (! ParseInt(inPtr, inLen, outValue)) { \
            outValue = inDefaultValue;            \
        }                                         \
    }

_KFS_ValueParser_IntTypes(_KFS_DEFINE_ValueParser_IntSetValue)
#undef _KFS_DEFINE_ValueParser_IntSetValue
#undef _KFS_DEFINE_ValueParser_IntTypes

    static void SetValue(
        const char* inPtr,
        size_t      inLen,
        const bool& inDefaultValue,
        bool&       outValue)
    {
        int theVal = 0;
        if (! ParseInt(inPtr, inLen, theVal)) {
            outValue = inDefaultValue;
        }
        outValue = theVal != 0;
    }
};

typedef ValueParserT<DecIntParser> ValueParser;

template<char SEPARATOR, char DELIMITER>
class PropertiesTokenizerT
{
public:
    struct Token
    {
        Token(
            const char* inPtr,
            const char* inEndPtr)
            : mPtr(inPtr),
              mLen(inEndPtr <= inPtr ? 0 : inEndPtr - inPtr)
            {}
        Token(
            const char* inPtr,
            size_t      inLen)
            : mPtr(inPtr),
              mLen(inLen)
            {}
        Token(
            const char* inPtr = 0)
            : mPtr(inPtr),
              mLen(inPtr ? strlen(inPtr) : 0)
            {}
        Token& operator=(
            const Token& inToken)
        {
            const_cast<const char*&>(mPtr) = inToken.mPtr;
            const_cast<size_t&>(mLen)      = inToken.mLen;
            return *this;
        }
        bool operator<(
            const Token& inToken) const
        {
            const int theRet = memcmp(
                mPtr, inToken.mPtr, min(mLen, inToken.mLen));
            return (theRet < 0 || (theRet == 0 && mLen < inToken.mLen));
        }
        bool operator==(
            const Token& inToken) const
        {
            return (
                mLen == inToken.mLen &&
                memcmp(mPtr, inToken.mPtr, mLen) == 0
            );
        }
        bool operator!=(
            const Token& inToken) const
            { return (! operator==(inToken)); }
        const char* GetEndPtr() const
            { return (mPtr + mLen); }
        string ToString() const
            { return string(mPtr, mLen); }
        bool operator==(
            const string& inRhs) const
            { return (*this == Token(inRhs.data(), inRhs.size())); }
        const char* data() const
            { return mPtr; }
        size_t size() const
            { return mLen; }
        const char* const mPtr;
        size_t const      mLen;
    };

    enum { kSeparator = SEPARATOR };
    enum { kDelimiter = DELIMITER };

    PropertiesTokenizerT(
        const char* inPtr,
        size_t      inLen,
        bool        inIgnoreMalformedFlag = true)
        : mPtr(inPtr),
          mEndPtr(inPtr + inLen),
          mKey(),
          mValue(),
          mIgnoreMalformedFlag(inIgnoreMalformedFlag)
        {}
    static bool IsWSpace(
        char inChar)
        { return ((inChar & 0xFF) <= ' '); }
    bool Next(int inSeparator = SEPARATOR)
    {
        while (mPtr < mEndPtr) {
            // Skip leading white space.
            while (mPtr < mEndPtr && (IsWSpace(*mPtr) || DELIMITER == *mPtr)) {
                mPtr++;
            }
            if (mPtr >= mEndPtr) {
                break;
            }
            // Find delimiter, and discard white space before delimeter.
            const char* const theKeyPtr = mPtr;
            while (mPtr < mEndPtr && *mPtr != inSeparator &&
                    ! IsWSpace(*mPtr)) {
                mPtr++;
            }
            if (mPtr >= mEndPtr) {
                break;
            }
            const char* theKeyEndPtr = mPtr;
            while (mPtr < mEndPtr && *mPtr != inSeparator &&
                    *mPtr != '\r' && DELIMITER != *mPtr) {
                if (! IsWSpace(*mPtr)) {
                    theKeyEndPtr = mPtr + 1;
                }
                mPtr++;
            }
            if (*mPtr != inSeparator) {
                // Ignore malformed line.
                while (mPtr < mEndPtr && DELIMITER != *mPtr) {
                    mPtr++;
                }
                if (mIgnoreMalformedFlag) {
                    continue;
                }
                mKey = Token(theKeyPtr, theKeyEndPtr);
                break;
            }
            mPtr++;
            // Skip leading white space after the delimiter.
            while (mPtr < mEndPtr && IsWSpace(*mPtr) &&
                    *mPtr != '\r' && DELIMITER != *mPtr) {
                mPtr++;
            }
            // Find end of line and discard trailing white space.
            const char* const theValuePtr    = mPtr;
            const char*       theValueEndPtr = mPtr;
            while (mPtr < mEndPtr && *mPtr != '\r' && DELIMITER != *mPtr) {
                if (! IsWSpace(*mPtr)) {
                    theValueEndPtr = mPtr + 1;
                }
                mPtr++;
            }
            mKey   = Token(theKeyPtr,   theKeyEndPtr);
            mValue = Token(theValuePtr, theValueEndPtr);
            return true;
        }
        return false;
    }
    const Token& GetKey() const
        { return mKey; }
    const Token& GetValue() const
        { return mValue; }
private:
    const char*       mPtr;
    const char* const mEndPtr;
    Token             mKey;
    Token             mValue;
    const bool        mIgnoreMalformedFlag;
};

typedef PropertiesTokenizerT<':', '\n'> PropertiesTokenizer;

template <typename TOKEN, typename VALUE, typename TOKEN2KEY>
class RequestParserDictionaryT
{
private:
    typedef VALUE                   Value;
    typedef TOKEN2KEY               Token2Key;
    typedef typename Token2Key::Key Key;
    typedef pair<Key, Value>        Entry;

    struct Less
    {
        bool operator()(
            const Entry& inLhs,
            const Entry& inRhs) const
            { return (inLhs.first < inRhs.first); }
    };
    typedef vector<Entry> Vector;

public:
    typedef typename Token2Key::ScratchBuf  ScratchBuf;
    typedef TOKEN                           Token;
    typedef typename Vector::const_iterator const_iterator;
    typedef typename Vector::iterator       iterator;

    RequestParserDictionaryT()
        : mVector()
        {}
    ~RequestParserDictionaryT()
        {}
    const_iterator begin() const
        { return mVector.begin(); }
    const_iterator end() const
        { return mVector.end(); }
    iterator begin()
        { return mVector.begin(); }
    iterator end()
        { return mVector.end(); }
    const_iterator find(
        const Token& inToken) const
    {
        if (! Token2Key::IsValidKey(inToken)) {
            return end();
        }
        const Key theKey = Token2Key::ToKey(inToken);
        const_iterator const theIt = lower_bound(begin(), end(),
            make_pair(theKey, Value()), Less());
        return ((theIt == end() || theIt->first == theKey) ? theIt : end());
    }
    pair<iterator, bool> insert(
        const pair<Token, Value>& inKv)
    {
        if (! Token2Key::IsValidKey(inKv.first)) {
            return make_pair(end(), false);
        }
        const Key theKey = Token2Key::ToKey(inKv.first);
        iterator const theIt = lower_bound(begin(), end(),
            make_pair(theKey, Value()), Less());
        if (theIt != mVector.end() && theIt->first == theKey) {
            return make_pair(theIt, false);
        }
        return make_pair(mVector.insert(
            theIt, make_pair(theKey, inKv.second)), true);
    }
    static Token GetName(
            Key        inKey,
            ScratchBuf inBuf)
        { return Token2Key::ToName(inKey, inBuf); }
private:
    Vector mVector;
};

template<typename TOKEN>
class NameTonDictionaryKey
{
public:
    typedef TOKEN Token;
    typedef Token Key;
    struct ScratchBuf {};

    static bool IsValidKey(
        const Token& /* inToken */)
        { return true; }
    static const Token& ToName(
        const Key&  inKey,
        ScratchBuf& /* inBuf */)
        { return inKey; }
    static const Key& ToKey(
        const Token& inToken)
        { return inToken; }
};

template<typename TOKEN>
class ShortNameToDictionaryKey
{
public:
    typedef TOKEN        Token;
    typedef unsigned int Key;
    typedef char         ScratchBuf[4];

    static bool IsValidKey(
        const Token& inToken)
        { return (inToken.mLen <= min(sizeof(Key), size_t(4))); }
    static const Token ToName(
            Key        inKey,
            ScratchBuf inBuf)
    {
        size_t theLen = 0;
        if ((inBuf[0] = (char)(inKey >> 24))) {
            theLen++;
            if ((inBuf[1] = (char)(inKey >> 16))) {
                theLen++;
                if ((inBuf[2] = (char)(inKey >> 8))) {
                    theLen++;
                    if ((inBuf[3] = (char)inKey)) {
                        theLen++;
                    }
                }
            }
        }
        return Token(inBuf, theLen);
    }
    static Key ToKey(
        const Token& inToken)
    {
        // Lexicographic order.
        Key theKey = 0;
        switch (inToken.mLen) {
            default: theKey |= Key(inToken.mPtr[3] & 0xFF);
            case  3: theKey |= Key(inToken.mPtr[2] & 0xFF) << 8;
            case  2: theKey |= Key(inToken.mPtr[1] & 0xFF) << 16;
            case  1: theKey |= Key(inToken.mPtr[0] & 0xFF) << 24;
            case  0: break;
        }
        return theKey;
    }
};

template <typename TOKEN, typename VALUE>
class RequestParserLongNamesDictionary : public RequestParserDictionaryT<
    TOKEN, VALUE, NameTonDictionaryKey<TOKEN> >
{};

template <typename TOKEN, typename VALUE>
class RequestParserShortNamesDictionary : public RequestParserDictionaryT<
    TOKEN, VALUE, ShortNameToDictionaryKey<TOKEN> >
{};

class NopOstream
{
public:
    template<typename T>
    NopOstream& WriteKeyVal(
            const char* /* inKeyPtr */,
            size_t      /* inKeyLen */,
            const T&    /* inVal */,
            char        /* inSeparator */,
            char        /* inDelimiter */)
        { return *this; }
    NopOstream& WriteName(
        const char* /* inPtr */,
        size_t      /* inLen */,
        char        /* inDelimiter */)
        { return *this; }
};

class RequestDeleter
{
public:
    template<typename T>
    static void Delete(T* inObj)
        { delete inObj; }
};

// Create parser for object fields, and invoke appropriate parsers based on the
// request header names.
template <
    typename OBJ,
    typename VALUE_PARSER         = ValueParser,
    bool     SHORT_NAMES_FLAG     = false,
    typename PROPERTIES_TOKENIZER = PropertiesTokenizer,
    typename ST                   = NopOstream,
    bool     VALIDATE_FLAG        = true,
    typename REQUEST_DELETER      = RequestDeleter,
    template <
        typename /* KEY */,
        typename /* VALUE */
    > class FIELDS_MAP            = RequestParserLongNamesDictionary
>
class ObjectParser
{
public:
    typedef PROPERTIES_TOKENIZER      Tokenizer;
    typedef typename Tokenizer::Token Token;

    // inNamePtr arguments are assumed to be static strings.
    // The strings must remain constant and valid during the lifetime of
    // this object.

    ObjectParser()
        : mDefDoneFlag(false),
          mFields()
        {}
    virtual ~ObjectParser()
    {
        for (typename Fields::iterator theIt = mFields.begin();
                theIt != mFields.end();
                ++theIt) {
            delete theIt->second;
            theIt->second = 0;
        }
    }
    template<typename T, typename OT>
    ObjectParser& Def(
        const char* inNamePtr,
        T OT::*     inFieldPtr,
        T           inDefault = T())
    {
        if (! mDefDoneFlag && ! mFields.insert(make_pair(
                    Key(inNamePtr), new Field<T,OT>(inFieldPtr, inDefault)
                )).second) {
            // Duplicate key name in the definition.
            abort();
        }
        return *this;
    }
    template<typename T, typename OT>
    ObjectParser& Def2(
        const char* inLongNamePtr,
        const char* inShortNamePtr,
        T OT::*     inFieldPtr,
        T           inDefault = T())
    {
        return Def(SHORT_NAMES_FLAG ? inShortNamePtr : inLongNamePtr,
            inFieldPtr, inDefault);
    }
    ObjectParser& DefDone()
    {
        mDefDoneFlag = true;
        return *this;
    }
    bool IsDefined() const
        { return mDefDoneFlag; }
    void Parse(
        Tokenizer& inTokenizer,
        OBJ*       inObjPtr) const
    {
        while (inTokenizer.Next()) {
            const Token& theKey = inTokenizer.GetKey();
            typename Fields::const_iterator const theIt = mFields.find(theKey);
            if (theIt == mFields.end()) {
                const Token& theValue = inTokenizer.GetValue();
                if (! inObjPtr->HandleUnknownField(
                        theKey.mPtr,    theKey.mLen,
                        theValue.mPtr, theValue.mLen)) {
                    break;
                }
            } else {
                theIt->second->Set(inObjPtr, inTokenizer.GetValue());
            }
        }
    }
    void Write(
        ST&        inStream,
        const OBJ* inObjPtr,
        bool       inOmitDefaultFlag = false,
        char       inSeparator       = ':',
        char       inDelimiter       = '\n') const
    {
        typename Fields::ScratchBuf theBuf;
        for (typename Fields::const_iterator theIt = mFields.begin();
                theIt != mFields.end();
                ++theIt) {
            theIt->second->Get(inObjPtr, inStream,
                Fields::GetName(theIt->first, theBuf),
                inOmitDefaultFlag, inSeparator, inDelimiter);
        }
    }
private:
    typedef typename Tokenizer::Token Key;
    typedef typename Tokenizer::Token Value;

    class AbstractField
    {
    public:
        AbstractField()
            {}
        virtual ~AbstractField()
            {}
        virtual void Set(
            OBJ*         inObjPtr,
            const Value& inValue) const = 0;
        virtual void Get(
            const OBJ* inObjPtr,
            ST&        inStream,
            const Key& inKey,
            bool       inOmitDefaultFlag,
            char       inSeparator,
            char       inDelimiter) const = 0;
    };

    template<typename T, typename OT>
    class Field : public AbstractField
    {
    public:
        Field(
            T OT::*  inFieldPtr,
            const T& inDefault)
            : AbstractField(),
              mFieldPtr(inFieldPtr),
              mDefault(inDefault)
            {}
        virtual ~Field()
            {}
        virtual void Set(
            OBJ*         inObjPtr,
            const Value& inValue) const
        {
            // The correct pointer to member scope "OT::" below is crucial.
            // This is the primary reason why this code *is not* in the
            // AbstractRequestParser, and why the parser definition can not be
            // done with the AbstractRequestParser.
            // In other words this is the reason why the definition has to be
            // in this class, and can not be "inherited" from the super classes
            // of the OBJ with abstract parser.
            VALUE_PARSER::SetValue(
                inValue.mPtr,
                inValue.mLen,
                mDefault,
                inObjPtr->*mFieldPtr
            );
        }
        virtual void Get(
            const OBJ* inObjPtr,
            ST&        inStream,
            const Key& inKey,
            bool       inOmitDefaultFlag,
            char       inSeparator,
            char       inDelimiter) const
        {
            if (inOmitDefaultFlag && inObjPtr->*mFieldPtr == mDefault) {
                return;
            }
            inStream.WriteKeyVal(
                inKey.mPtr,
                inKey.mLen,
                inObjPtr->*mFieldPtr,
                inSeparator,
                inDelimiter
            );
        }
    private:
        T OT::* const mFieldPtr;
        T const       mDefault;
    };

    typedef FIELDS_MAP<Token, AbstractField*> Fields;

    bool   mDefDoneFlag;
    Fields mFields;
};

template <typename ABSTRACT_OBJ, typename ST>
class AbstractRequestParser
{
public:
    typedef unsigned int Checksum;

    AbstractRequestParser()
        {}
    virtual ~AbstractRequestParser()
        {}
    virtual ABSTRACT_OBJ* Parse(
        const char* inBufferPtr,
        size_t      inLen,
        const char* inRequestNamePtr,
        size_t      inRequestNameLen,
        bool        inHasHeaderChecksumFlag,
        Checksum    inChecksum) const = 0;
    virtual void Write(
        ST&                 inStream,
        const ABSTRACT_OBJ* inObjPtr,
        bool                inOmitDefaultFlag,
        char                inSeparator,
        char                inDelimiter) const = 0;
};

// Create concrete object and invoke corresponding parser.
template <
    typename ABSTRACT_OBJ,
    typename OBJ,
    typename VALUE_PARSER         = ValueParser,
    bool     SHORT_NAMES_FLAG     = false,
    typename PROPERTIES_TOKENIZER = PropertiesTokenizer,
    typename ST                   = NopOstream,
    bool     VALIDATE_FLAG        = true,
    typename REQUEST_DELETER      = RequestDeleter,
    template <
        typename /* KEY */,
        typename /* VALUE */
    > class FIELDS_MAP            = RequestParserLongNamesDictionary
>
class RequestParser :
    public AbstractRequestParser<ABSTRACT_OBJ, ST>,
    public ObjectParser<
        OBJ,
        VALUE_PARSER,
        SHORT_NAMES_FLAG,
        PROPERTIES_TOKENIZER,
        ST,
        VALIDATE_FLAG,
        REQUEST_DELETER,
        FIELDS_MAP
    >
{
public:
    typedef RequestParser                           RequestParserType;
    typedef PROPERTIES_TOKENIZER                    Tokenizer;
    typedef AbstractRequestParser<ABSTRACT_OBJ, ST> Super;
    typedef ObjectParser<
        OBJ,
        VALUE_PARSER,
        SHORT_NAMES_FLAG,
        PROPERTIES_TOKENIZER,
        ST,
        VALIDATE_FLAG,
        REQUEST_DELETER,
        FIELDS_MAP
    >                                               ObjParser;
    typedef typename Super::Checksum                Checksum;

    RequestParser()
        : Super(),
          ObjParser()
        {}
    virtual ~RequestParser()
        {}
    virtual ABSTRACT_OBJ* Parse(
        const char* inBufferPtr,
        size_t      inLen,
        const char* inRequestNamePtr,
        size_t      inRequestNameLen,
        bool        inHasHeaderChecksumFlag,
        Checksum    inChecksum) const
    {
        OBJ* const theObjPtr = new OBJ();
        if (! theObjPtr->ValidateRequestHeader(
                inRequestNamePtr,
                inRequestNameLen,
                inBufferPtr,
                inLen,
                inHasHeaderChecksumFlag,
                inChecksum,
                SHORT_NAMES_FLAG)) {
            REQUEST_DELETER::Delete(theObjPtr);
            return 0;
        }
        Tokenizer theTokenizer(inBufferPtr, inLen);
        ObjParser::Parse(theTokenizer, theObjPtr);
        if (! VALIDATE_FLAG || theObjPtr->Validate()) {
            return theObjPtr;
        }
        REQUEST_DELETER::Delete(theObjPtr);
        return 0;
    }
    virtual void Write(
        ST&                 inStream,
        const ABSTRACT_OBJ* inObjPtr,
        bool                inOmitDefaultFlag,
        char                inSeparator,
        char                inDelimiter) const
    {
        ObjParser::Write(inStream, static_cast<const OBJ*>(inObjPtr),
            inOmitDefaultFlag, inSeparator, inDelimiter);
    }
    template<typename T, typename OT>
    RequestParser& Def(
        const char* inNamePtr,
        T OT::*     inFieldPtr,
        T           inDefault = T())
    {
        ObjParser::Def(inNamePtr, inFieldPtr, inDefault);
        return *this;
    }
    template<typename T, typename OT>
    RequestParser& Def2(
        const char* inLongNamePtr,
        const char* inShortNamePtr,
        T OT::*     inFieldPtr,
        T           inDefault = T())
    {
        ObjParser::Def2(inLongNamePtr, inShortNamePtr, inFieldPtr, inDefault);
        return *this;
    }
    RequestParser& DefDone()
    {
        ObjParser::DefDone();
        return *this;
    }
};


template <typename ABSTRACT_OBJ, typename OBJ>
class ObjRequestParser : public RequestParser<ABSTRACT_OBJ, OBJ>
{};

class ParserDefinitionMethod
{
public:
    template<typename OBJ, typename PARSER>
    static PARSER& Define(
        PARSER&     inParser,
        const OBJ*  /* inNullPtr */)
        { return OBJ::ParserDef(inParser); }
};

// Invoke appropriate request parser based on RPC name.
template <
    typename ABSTRACT_OBJ,
    template <
        typename /* ABSTRACT_OBJ */,
        typename /* OBJ          */
    > class  REQUEST_PARSER = ObjRequestParser,
    typename PARSER_DEF     = ParserDefinitionMethod,
    typename ST             = NopOstream,
    char     DELIMITER      = '\n',
    typename TOKEN          = PropertiesTokenizer::Token,
    template <
        typename /* KEY */,
        typename /* VALUE */
    > class FIELDS_MAP      = RequestParserLongNamesDictionary,
    template <
        typename /* KEY */,
        typename /* VALUE */
    > class FIELD_IDS_MAP   = RequestParserLongNamesDictionary
>
class RequestHandler
{
public:
    typedef AbstractRequestParser<ABSTRACT_OBJ, ST> Parser;
    typedef typename Parser::Checksum               Checksum;

    RequestHandler()
        {}
    ~RequestHandler()
        {}
    static bool IsWSpace(
        char inChar)
        { return ((inChar & 0xFF) <= ' '); }
    ABSTRACT_OBJ* Handle(
        const char* inBufferPtr,
        size_t      inLen) const
    {
        const char*       thePtr    = inBufferPtr;
        const char* const theEndPtr = thePtr + inLen;
        while (thePtr < theEndPtr && IsWSpace(*thePtr)) {
            thePtr++;
        }
        const char* const theNamePtr = thePtr;
        while (thePtr < theEndPtr && ! IsWSpace(*thePtr) &&
                DELIMITER != *thePtr) {
            thePtr++;
        }
        const size_t theNameLen = thePtr - theNamePtr;
        typename Parsers::const_iterator const theIt =
            mParsers.find(Name(theNamePtr, theNameLen));
        if (theIt == mParsers.end()) {
            return 0;
        }
        // Get optional header checksum.
        const char* theChecksumPtr = thePtr;
        while (thePtr < theEndPtr &&
                *thePtr != '\r' && DELIMITER != *thePtr) {
            thePtr++;
        }
        Checksum   theChecksum     = 0;
        const bool theChecksumFlag = ValueParser::ParseInt(
            theChecksumPtr, thePtr - theChecksumPtr, theChecksum);
        while (thePtr < theEndPtr &&
                (IsWSpace(*thePtr) || DELIMITER == *thePtr)) {
            thePtr++;
        }
        return theIt->second.second->Parse(
            thePtr,
            theEndPtr - thePtr,
            theNamePtr,
            theNameLen,
            theChecksumFlag,
            theChecksum
        );
    }
    TokenValue ObjIdToName(
        int inObjId) const
    {
        if (inObjId <= 0) {
            return TokenValue();
        }
        typename Writers::const_iterator const theIt = mWriters.find(inObjId);
        if (theIt == mWriters.end()) {
            return TokenValue();
        }
        return TokenValue(theIt->second.first.mPtr, theIt->second.first.mLen);
    }
    int NameToObjId(
        const TokenValue& inName) const
    {
        typename Parsers::const_iterator const theIt =
            mParsers.find(Name(inName.mPtr, inName.mLen));
        if (theIt == mParsers.end()) {
            return -1;
        }
        return theIt->second.first;
    }
    bool Write(
        ST&                 inStream,
        const ABSTRACT_OBJ* inObjPtr,
        int                 inObjId,
        bool                inOmitDefaultFlag = false,
        char                inSeparator       = ':',
        char                inDelimiter       = '\n') const
    {
        typename Writers::const_iterator const theIt = mWriters.find(inObjId);
        if (theIt == mWriters.end()) {
            return false;
        }
        inStream.WriteName(
            theIt->second.first.mPtr, theIt->second.first.mLen, DELIMITER);
        theIt->second.second->Write(inStream, inObjPtr,
            inOmitDefaultFlag, inSeparator, inDelimiter);
        return true;
    }
    template <typename OBJ>
    typename REQUEST_PARSER<ABSTRACT_OBJ, OBJ>::RequestParserType&
    BeginMakeParser(
        const OBJ* inNullPtr = 0)
    {
        static REQUEST_PARSER<ABSTRACT_OBJ, OBJ> sParser;
        return sParser;
    }
    template <typename T>
    RequestHandler& EndMakeParser(
        const char* inNamePtr,
        T&          inParser,
        int         inObjId)
    {
        if (! mParsers.insert(make_pair(
                Name(inNamePtr),
                make_pair(inObjId, &inParser.DefDone()))).second) {
            // Duplicate name -- definition error.
            abort();
        }
        if (0 <= inObjId && ! mWriters.insert(make_pair(
                inObjId,
                make_pair(Name(inNamePtr), &inParser))).second) {
            // Duplicate id -- definition error.
            abort();
        }
        return *this;
    }
    template <typename OBJ>
    RequestHandler& MakeParser(
        const char* inNamePtr,
        int         inObjId,
        const OBJ*  inNullPtr = 0)
    {
        return
            EndMakeParser(
                inNamePtr,
                PARSER_DEF::Define(
                    BeginMakeParser(inNullPtr),
                    inNullPtr
                ),
                inObjId
            );
    }
    template <typename OBJ>
    RequestHandler& MakeParser(
        const char* inNamePtr,
        const OBJ*  inNullPtr = 0)
        { return MakeParser(inNamePtr, -1, inNullPtr); }
private:
    typedef TOKEN Name;
    typedef FIELDS_MAP<
        Name,
        pair<int,  const Parser*>
    > Parsers;
    typedef FIELD_IDS_MAP<
        int,
        pair<Name, const Parser*>
    > Writers;

    Parsers mParsers;
    Writers mWriters;
};

}

#endif /* REQUEST_PARSER_H */
