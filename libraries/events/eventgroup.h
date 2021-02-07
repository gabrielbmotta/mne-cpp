//=============================================================================================================
/**
 * @file     eventgroup.h
 * @author   Gabriel Motta <gbmotta@mgh.harvard.edu>;
 *           Juan Garcia-Prieto <juangpc@gmail.com>
 * @since    0.1.8
 * @date     February, 2021
 *
 * @section  LICENSE
 *
 * Copyright (C) 2021, Gabriel Motta, Juan Garcia-Prieto. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *     * Redistributions of source code must retain the above copyright notice, this list of conditions and the
 *       following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
 *       the following disclaimer in the documentation and/or other materials provided with the distribution.
 *     * Neither the name of MNE-CPP authors nor the names of its contributors may be used
 *       to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * @brief     EventGroup declaration.
 *
 */

#ifndef EVENTGROUP_EVENTS_H
#define EVENTGROUP_EVENTS_H

#include "events_global.h"
#include <string>

namespace EVENTSINTERNAL {
class EventGroup;
}

namespace EVENTSLIB {

struct EVENTS_EXPORT RgbColor
{
    RgbColor();
    RgbColor(const uchar rRhs, const uchar gRhs, const uchar bRhs);
    RgbColor(const uchar rRhs, const uchar gRhs,
             const uchar bRhs, const uchar aRhs);

    uchar r;
    uchar g;
    uchar b;
    uchar a;
};

struct EVENTS_EXPORT Group
{
    Group(int idRhs, const char* nameRhs, const RgbColor& cRhs );
    Group(const EVENTSINTERNAL::EventGroup& gRhs);
    unsigned int    id;
    std::string     name;
    RgbColor        color;
};

}

namespace EVENTSINTERNAL {

class EventGroup
{
public:
    //=========================================================================================================
    EventGroup(const char* name);

    //=========================================================================================================
    EventGroup(const char* name, const EVENTSLIB::RgbColor& color);

    //=========================================================================================================
    void setColor(const EVENTSLIB::RgbColor& color);

    void setRandomColor();

    EVENTSLIB::RgbColor getColor() const;

    //=========================================================================================================
    const std::string& getName() const;

    //=========================================================================================================
    void setName(const std::string& sName);

    //=========================================================================================================
    unsigned int getId() const;

private:


    std::string         m_sName;
    EVENTSLIB::RgbColor m_Color;
    unsigned int        m_Id;

    static unsigned int eventGroupIdCounter;
};

} //namespace
#endif // EVENTGROUP_H
