//---------------------------------------------------------- -*- Mode: C++ -*-
// $Id$
//
// Created 2015/01/27
// Author: Mike Ovsiannikov
//
// Copyright 2015 Quantcast Corp.
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
// Class to keep track of idempotent requests.
//
//
//----------------------------------------------------------------------------

#ifndef META_IDEMPOTENT_REQUEST_H
#define META_IDEMPOTENT_REQUEST_H

#include "common/kfstypes.h"

#include <iosfwd>

namespace KFS
{

using std::ostream;

struct MetaIdempotentRequest;
struct MetaAck;
class Properties;

class IdempotentRequestTracker
{
public:
    IdempotentRequestTracker();
    ~IdempotentRequestTracker();

    void SetParameters(
        const char*       inPrefixPtr,
        const Properties& inProps);
    bool Handle(
        MetaIdempotentRequest& inRequest);
    bool Remove(
        MetaIdempotentRequest& inRequest);
    void Handle(
        MetaAck& inAck);
    int Write(
        ostream& inStream) const;
    int Read(
        const char* inPtr,
        size_t      inLen);
    void Clear();
    void SetDisableTimerFlag(
        bool inFlag);
private:
    class Impl;
    Impl& mImpl; 
};

} // namespace KFS

#endif /* META_IDEMPOTENT_REQUEST_H */
