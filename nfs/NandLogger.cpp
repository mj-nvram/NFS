/****************************************************************************
*	 NANDFlashSim: A Cycle Accurate NAND Flash Memory Simulator 
*	 
*	 Copyright (C) 2011   	Myoungsoo Jung (MJ)
*
*                           Pennsylvania State University
*                           Microsystems Design Laboratory
*                           I/O Group
*
*	 This program is free software: you can redistribute it and/or modify
*	 it under the terms of the GNU Lesser General Public License as published by
*	 the Free Software Foundation.
*
*	 This program is distributed in the hope that it will be useful,
*	 but WITHOUT ANY WARRANTY; without even the implied warranty of
*	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	 GNU Lesser General Public License for more details.
*
*	 You should have received a copy of the GNU Lesser General Public License
*	 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*****************************************************************************/
#include "boost/filesystem/path.hpp"
#include "boost/filesystem.hpp"

#include "TypeSystem.h"
#include "Tools.h"
#include "ParamManager.h"
#include "NandLogger.h"

namespace fs    = boost::filesystem;

namespace NANDFlashSim {

std::ofstream NandLogger::_logstream[NANDLOG_MAX_TYPES];

std::ofstream* NandLogger::GetStream( NANDLOG_TYPE nLogType )
{
    static char     *filePath[] = {"SnoopNandPlaneRead",\
                                   "SnoopNandPlaneWrite",\
                                   "SnoopInternalState",\
                                   "SnoopInternalAccCycle",\
                                   "SnoopBusTransaction",\
                                   "SnoopIoCompletion",\
                                   "CyclesForEachState",\
                                   "PowerCyclesForEachDcParam"};
    std::ofstream        *pfstream = NULL;

    UINT32 nValue = NFS_GET_ENV((INI_ENV_VALUE)nLogType);
    assert(nValue != NULL_SIG(UINT32));
    if(nValue == 1)
    {
        if(!_logstream[nLogType].is_open())
        {
            std::string path = ".\\logData";
            if(fs::exists(path.c_str()) == false)
            {
                fs::create_directory(path.c_str());
            } 
            path    = path + "\\" + filePath[nLogType];
            _logstream[nLogType].open(path.c_str());
            printFieldInfo(nLogType);
            atexit(cleanUp);
        }
        pfstream    = &_logstream[nLogType];
    }

    return pfstream;
}

void NandLogger::cleanUp()
{
    for (UINT32 niter =0; niter < NANDLOG_MAX_TYPES; niter++)
    {
        if(_logstream[niter].is_open())
        {
            _logstream[niter].close();
        }
    }
}

void NandLogger::printFieldInfo( NANDLOG_TYPE nLogType )
{
    switch(nLogType)
    {
    case NANDLOG_SNOOP_IOCOMPLETION :
        _logstream[nLogType] << "Controller ID , TransId , Arrival Cycle , Current Cycle , Latency (Cycle)" << std::endl;
        break;
    case NANDLOG_SNOOP_NANDPLANE_WRITE :
    case NANDLOG_SNOOP_NANDPLANE_READ :
        _logstream[nLogType] << "Plane ID , Physical Block Number , Page Offset" << std::endl;
        break;
    case NANDLOG_SNOOP_INTERNAL_STATE :
        _logstream[nLogType] << "Die ID , TransId , Command , Stage , Current Cycles , Cycle" << std::endl;
        break;
    case NANDLOG_SNOOP_INTERNAL_ACC_CYCLE:
        _logstream[nLogType] << "Die ID , TransId , Fsm State, Cycles" << std::endl;
        break;
    case NANDLOG_TOTAL_CYCLE_FOR_EACH_STATE :
        _logstream[nLogType] << "Controller ID , Die ID, Fsm State , Cycles" << std::endl;
        break;
    case NANDLOG_TOTAL_POWERCYCLE_FOR_EACH_DCPARAM :
        _logstream[nLogType] << "Controller ID , Die ID, DC Param , Cycles, Current, Power" << std::endl;
        break;

    }
}

void NandLogger::MarkupHardReset()
{
    for (UINT32 niter =0; niter < NANDLOG_MAX_TYPES; niter++)
    {
        REPORT_NAND((NANDLOG_TYPE)niter, "########################################flash system have been reset#############");
    }
}

}