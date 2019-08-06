// EnergyPlus, Copyright (c) 1996-2019, The Board of Trustees of the University of Illinois,
// The Regents of the University of California, through Lawrence Berkeley National Laboratory
// (subject to receipt of any required approvals from the U.S. Dept. of Energy), Oak Ridge
// National Laboratory, managed by UT-Battelle, Alliance for Sustainable Energy, LLC, and other
// contributors. All rights reserved.
//
// NOTICE: This Software was developed under funding from the U.S. Department of Energy and the
// U.S. Government consequently retains certain rights. As such, the U.S. Government has been
// granted for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable,
// worldwide license in the Software to reproduce, distribute copies to the public, prepare
// derivative works, and perform publicly and display publicly, and to permit others to do so.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice, this list of
//     conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National Laboratory,
//     the University of Illinois, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without specific prior
//     written permission.
//
// (4) Use of EnergyPlus(TM) Name. If Licensee (i) distributes the software in stand-alone form
//     without changes from the version obtained under this License, or (ii) Licensee makes a
//     reference solely to the software portion of its product, Licensee must refer to the
//     software as "EnergyPlus version X" software, where "X" is the version number Licensee
//     obtained under this License and may not use a different name for the software. Except as
//     specifically required in this Section (4), Licensee shall not use in a company name, a
//     product name, in advertising, publicity, or other promotional activities any name, trade
//     name, trademark, logo, or other designation of "EnergyPlus", "E+", "e+" or confusingly
//     similar designation, without the U.S. Department of Energy's prior written consent.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <EnergyPlus.hh>
#include <Scheduling/Manager.hh>
#include <Scheduling/Base.hh>
#include <Scheduling/Day.hh>
#include <Scheduling/TypeLimits.hh>
#include <Scheduling/Week.hh>
#include <Scheduling/YearConstant.hh>
#include <Scheduling/YearCompact.hh>
#include <Scheduling/YearWeekly.hh>
#include <Scheduling/YearFile.hh>

namespace Scheduling {
std::vector<IndexBasedScheduleData> indexToSubtypeMap;
bool scheduleInputProcessed = false;

void clear_state()
{
    // clear out anything from this particular module
    scheduleInputProcessed = false;
    indexToSubtypeMap.clear();
    // but this clear state also needs to manage the child clear states
    ScheduleDay::clear_state();
    ScheduleTypeData::clear_state();
    ScheduleWeek::clear_state();
    ScheduleYear::clear_state();
    ScheduleConstant::clear_state();
    ScheduleCompact::clear_state();
    ScheduleFile::clear_state();
}

int GetScheduleIndex(const std::string &scheduleName)
{
    // check if input has been processed yet for schedules, if not then call it now
    if (!scheduleInputProcessed) {
        processAllSchedules();
    }
    // then just ignore zero and look through each of the types and return the index in the subtype map
    for (size_t mappingIndex = 1; mappingIndex < indexToSubtypeMap.size(); mappingIndex++) {
        if (indexToSubtypeMap[mappingIndex].name == scheduleName) {
            return mappingIndex;
        }
    }
    return -1; // should this be zero?  Fatal error?
}

ScheduleBase *getScheduleReference(const std::string &scheduleName)
{
    // check if input has been processed yet for schedules, if not then call it now
    if (!scheduleInputProcessed) {
        processAllSchedules();
    }
    // then just look through each of the types and return a direct reference
    for (auto const &mapping : indexToSubtypeMap) {
        if (mapping.name == scheduleName) {
            switch (mapping.thisType) {
            case ScheduleType::CONSTANT:
                return &scheduleConstants[mapping.indexInTypeArray];
            case ScheduleType::COMPACT:
                return &scheduleCompacts[mapping.indexInTypeArray];
            case ScheduleType::YEAR:
                return &scheduleYears[mapping.indexInTypeArray];
            case ScheduleType::FILE:
            case ScheduleType::SHADING_FILE:
                return &scheduleFiles[mapping.indexInTypeArray];
            case ScheduleType::UNKNOWN:
                // Fatal Error
                return nullptr;
            }
        }
    }
    // Fatal Error
    return nullptr;
}

void updateAllSchedules(int const simTime)
{
    for (auto &thisSchedule : scheduleConstants) {
        thisSchedule.updateValue(simTime);
    }
    for (auto &thisSchedule : scheduleCompacts) {
        thisSchedule.updateValue(simTime);
    }
    for (auto &thisSchedule : scheduleYears) {
        thisSchedule.updateValue(simTime);
    }
    for (auto &thisSchedule : scheduleFiles) {
        thisSchedule.updateValue(simTime);
    }
}

void prepareSchedulesForNewEnvironment()
{
    // right now Schedule:Constant objects are not stored as time-series for efficiency
    for (auto &thisSchedule : scheduleConstants) {
        thisSchedule.prepareForNewEnvironment();
    }
    for (auto &thisSchedule : scheduleCompacts) {
        thisSchedule.prepareForNewEnvironment();
    }
    for (auto &thisSchedule : scheduleYears) {
        thisSchedule.prepareForNewEnvironment();
    }
    for (auto &thisSchedule : scheduleFiles) {
        thisSchedule.prepareForNewEnvironment();
    }
}

void resetAllTimeStartIndex()
{
    // right now Schedule:Constant objects are not stored as time-series for efficiency
//    for (auto &thisSchedule : scheduleConstants) {
//        thisSchedule.createTimeSeries();
//    }
    for (auto &thisSchedule : scheduleCompacts) {
        thisSchedule.resetTimeStartIndex();
    }
    for (auto &thisSchedule : scheduleYears) {
        thisSchedule.resetTimeStartIndex();
    }
    for (auto &thisSchedule : scheduleFiles) {
        thisSchedule.resetTimeStartIndex();
    }
}

void processAllSchedules()
{
    // This function will process all schedule related inputs.  Start with type limits, then call each derived type.
    // ok, so a zero schedule index is a magic value for always returning zero from schedule manager for some reason
    // To make this work efficiently, we're going to add a simple unnamed constant schedule that always returns zero
    // It shouldn't be accessible to overriding with EMS or any other voodoo since it will be unnamed, and
    //  it shouldn't need any type limits since it will just return zero
    // The only gotcha is that constant schedules need to be processed before all other types so that the zero schedule
    //  gets the zeroeth index in the mapping vector

    // call type limits first so we don't have to check each time we call the type limits factory from each schedule input
    ScheduleTypeData::processInput();

    // then we'll go through and call each subtype factory and accumulate index values into our map
    ScheduleConstant::processInput();
    for (size_t subTypeIndex = 0; subTypeIndex < scheduleConstants.size(); subTypeIndex++) {
        if (subTypeIndex > 0) scheduleConstants[subTypeIndex].setupOutputVariables();  // skip the zeroeth, built-in, schedule:constant object
        indexToSubtypeMap.emplace_back(scheduleConstants[subTypeIndex].name, ScheduleType::CONSTANT, subTypeIndex);
    }
    ScheduleCompact::processInput();
    for (size_t subTypeIndex = 0; subTypeIndex < scheduleCompacts.size(); subTypeIndex++) {
        scheduleCompacts[subTypeIndex].setupOutputVariables();
        indexToSubtypeMap.emplace_back(scheduleCompacts[subTypeIndex].name, ScheduleType::COMPACT, subTypeIndex);
    }
    ScheduleYear::processInput();
    for (size_t subTypeIndex = 0; subTypeIndex < scheduleYears.size(); subTypeIndex++) {
        scheduleYears[subTypeIndex].setupOutputVariables();
        indexToSubtypeMap.emplace_back(scheduleYears[subTypeIndex].name, ScheduleType::YEAR, subTypeIndex);
    }
    ScheduleFile::processInput();
    for (size_t subTypeIndex = 0; subTypeIndex < scheduleFiles.size(); subTypeIndex++) {
        scheduleFiles[subTypeIndex].setupOutputVariables();
        indexToSubtypeMap.emplace_back(scheduleFiles[subTypeIndex].name, ScheduleType::FILE, subTypeIndex);
    }
    scheduleInputProcessed = true;
}

Real64 GetScheduleValue(int scheduleIndex)
{
    // the scheduleIndex is actually the index into the map vector, so get that mapping item
    auto const &mapping(indexToSubtypeMap[scheduleIndex]);
    switch (mapping.thisType) {
    case ScheduleType::CONSTANT:
        return scheduleConstants[mapping.indexInTypeArray].value;
    case ScheduleType::COMPACT:
        return scheduleCompacts[mapping.indexInTypeArray].value;
    case ScheduleType::YEAR:
        return scheduleYears[mapping.indexInTypeArray].value;
    case ScheduleType::FILE:
    case ScheduleType::SHADING_FILE:
        return scheduleFiles[mapping.indexInTypeArray].value;
    case ScheduleType::UNKNOWN:
        return -1;
    }
    return -1;
}

int numSchedules()
{
    return (int)indexToSubtypeMap.size() - 1; // don't report the zeroeth item
}

std::string scheduleName(int const scheduleIndex) {
    auto const &mapping(indexToSubtypeMap[scheduleIndex]);
    switch (mapping.thisType) {
    case ScheduleType::CONSTANT:
        return scheduleConstants[mapping.indexInTypeArray].name;
    case ScheduleType::COMPACT:
        return scheduleCompacts[mapping.indexInTypeArray].name;
    case ScheduleType::YEAR:
        return scheduleYears[mapping.indexInTypeArray].name;
    case ScheduleType::FILE:
    case ScheduleType::SHADING_FILE:
        return scheduleFiles[mapping.indexInTypeArray].name;
    case ScheduleType::UNKNOWN:
        return "";
    }
    return ""; // hush up compiler warning
}

std::string scheduleType(int const scheduleIndex) {
    auto const &mapping(indexToSubtypeMap[scheduleIndex]);
    switch (mapping.thisType) {
    case ScheduleType::CONSTANT:
        return "Schedule:Constant";
    case ScheduleType::COMPACT:
        return "Schedule:Compact";
    case ScheduleType::YEAR:
        return "Schedule:Year";
    case ScheduleType::FILE:
        return "Schedule:File";
    case ScheduleType::SHADING_FILE:
        return "Schedule:File:Shading";
    case ScheduleType::UNKNOWN:
        return "";
    }
    return ""; // hush up compiler warning
}

Real64 scheduleMinValue(int const scheduleIndex) {
    ScheduleBase *scheduleReference; // no ownership, just reference
    auto const &mapping(indexToSubtypeMap[scheduleIndex]);
    switch (mapping.thisType) {
    case ScheduleType::CONSTANT:
        return scheduleConstants[mapping.indexInTypeArray].value;
    case ScheduleType::COMPACT:
        scheduleReference = &scheduleCompacts[mapping.indexInTypeArray];
        break;
    case ScheduleType::YEAR:
        scheduleReference = &scheduleYears[mapping.indexInTypeArray];
        break;
    case ScheduleType::FILE:
    case ScheduleType::SHADING_FILE:
        scheduleReference = &scheduleFiles[mapping.indexInTypeArray];
        break;
    default:
        EnergyPlus::ShowFatalError("Schedule Min Value called with unknown index type");
        return -999;
    }
    return *std::min_element(scheduleReference->values.begin(), scheduleReference->values.end());
}

Real64 scheduleMaxValue(int const scheduleIndex) {
    ScheduleBase *scheduleReference; // no ownership, just reference
    auto const &mapping(indexToSubtypeMap[scheduleIndex]);
    switch (mapping.thisType) {
    case ScheduleType::CONSTANT:
        return scheduleConstants[mapping.indexInTypeArray].value;
    case ScheduleType::COMPACT:
        scheduleReference = &scheduleCompacts[mapping.indexInTypeArray];
        break;
    case ScheduleType::YEAR:
        scheduleReference = &scheduleYears[mapping.indexInTypeArray];
        break;
    case ScheduleType::FILE:
    case ScheduleType::SHADING_FILE:
        scheduleReference = &scheduleFiles[mapping.indexInTypeArray];
        break;
    default:
        EnergyPlus::ShowFatalError("Schedule Min Value called with unknown index type");
        return 999;
    }
    return *std::max_element(scheduleReference->values.begin(), scheduleReference->values.end());
}

}