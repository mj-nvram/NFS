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


#include <iostream>
#include <fstream>

#include <boost/config.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>

#include "TypeSystem.h"
#include "ParamManager.h"


namespace NANDFlashSim {

std::string  ParamManager::_inipath[INI_MAX_FILES];    
UINT32       ParamManager::m_nEnvVal[INI_ENV_MAX];
UINT32       ParamManager::m_nDeviceVal[INI_DEVICE_MAX];

paramTypes gParamTypes[] = 
{
    { "TIME.tADL",       "", INI_ENV_MAX, ITV_tADL, FALSE, FALSE  },
    { "TIME.tALH",       "", INI_ENV_MAX, ITV_tALH, FALSE, FALSE  },
    { "TIME.tALS",       "", INI_ENV_MAX, ITV_tALS, FALSE, FALSE  },
    { "TIME.tCH",        "", INI_ENV_MAX, ITV_tCH, FALSE, FALSE  },
    { "TIME.tCLH",       "", INI_ENV_MAX, ITV_tCLH, FALSE, FALSE  },
    { "TIME.tCLS",       "", INI_ENV_MAX, ITV_tCLS, FALSE, FALSE  },
    { "TIME.tCS",        "", INI_ENV_MAX, ITV_tCS, FALSE, FALSE  },
    { "TIME.tDH",        "", INI_ENV_MAX, ITV_tDH, FALSE, FALSE  },
    { "TIME.tDS",        "", INI_ENV_MAX, ITV_tDS, FALSE, FALSE  },
    { "TIME.tWC",        "", INI_ENV_MAX, ITV_tWC, FALSE, FALSE  },
    { "TIME.tWH",        "", INI_ENV_MAX, ITV_tWH, FALSE, FALSE  },
    { "TIME.tWP",        "", INI_ENV_MAX, ITV_tWP, FALSE, FALSE  },
    { "TIME.tWW",        "", INI_ENV_MAX, ITV_tWW, FALSE, FALSE  },
    { "TIME.tAR",        "", INI_ENV_MAX, ITV_tAR, FALSE, FALSE  },
    { "TIME.tCEA",       "", INI_ENV_MAX, ITV_tCEA, FALSE, FALSE  },
    { "TIME.tCHZ",       "", INI_ENV_MAX, ITV_tCHZ, FALSE, FALSE  },
    { "TIME.tCOH",       "", INI_ENV_MAX, ITV_tCOH, FALSE, FALSE  },
    { "TIME.tDCBSYR1",   "", INI_ENV_MAX, ITV_tDCBSYR1, FALSE, FALSE  },
    { "TIME.tDCBSYR2",   "", INI_ENV_MAX, ITV_tDCBSYR2, FALSE, FALSE  },
    { "TIME.tIR",        "", INI_ENV_MAX, ITV_tIR, FALSE, FALSE  },
    { "TIME.tR",         "", INI_ENV_MAX, ITV_tR, FALSE, FALSE  },
    { "TIME.tRC",        "", INI_ENV_MAX, ITV_tRC, FALSE, FALSE  },
    { "TIME.tREA",       "", INI_ENV_MAX, ITV_tREA, FALSE, FALSE  },
    { "TIME.tREH",       "", INI_ENV_MAX, ITV_tREH, FALSE, FALSE  },
    { "TIME.tHOH",       "", INI_ENV_MAX, ITV_tHOH, FALSE, FALSE  },
    { "TIME.tRHZ",       "", INI_ENV_MAX, ITV_tRHZ, FALSE, FALSE  },
    { "TIME.tRLOH",      "", INI_ENV_MAX, ITV_tRLOH, FALSE, FALSE  },
    { "TIME.tRP",        "", INI_ENV_MAX, ITV_tRP, FALSE, FALSE  },
    { "TIME.tRR",        "", INI_ENV_MAX, ITV_tRR, FALSE, FALSE  },
    { "TIME.tRST",       "", INI_ENV_MAX, ITV_tRST, FALSE, FALSE  },
    { "TIME.tWB",        "", INI_ENV_MAX, ITV_tWB, FALSE, FALSE  },
    { "TIME.tWHR",       "", INI_ENV_MAX, ITV_tWHR, FALSE, FALSE  },
    { "TIME.tBERS",      "", INI_ENV_MAX, ITV_tBERS, FALSE, FALSE  },
    { "TIME.tCBSY",      "", INI_ENV_MAX, ITV_tCBSY, FALSE, FALSE  },
    { "TIME.tDBSY",      "", INI_ENV_MAX, ITV_tDBSY, FALSE, FALSE  },
    { "TIME.tPROG",      "wtprog", INI_ENV_MAX, ITV_tPROG, FALSE, FALSE  },

    { "TYPMINTIME.tPROG",       "ttprog", INI_ENV_MAX, IMV_tPROG, FALSE, FALSE  },
    { "TYPMINTIME.tDCBSYR1",    "", INI_ENV_MAX, IMV_tDCBSYR1, FALSE, FALSE  },
    { "TYPMINTIME.tDCBSYR2",    "", INI_ENV_MAX, IMV_tDCBSYR2, FALSE, FALSE  },
    { "TYPMINTIME.tBERS",       "tterase", INI_ENV_MAX, IMV_tBERS, FALSE, FALSE  },
    { "TYPMINTIME.tCBSY",       "", INI_ENV_MAX, IMV_tCBSY, FALSE, FALSE  },
    { "TYPMINTIME.tDBSY",       "", INI_ENV_MAX, IMV_tDBSY, FALSE, FALSE  },

    { "DC.VCC",     "", INI_ENV_MAX, IDV_VCC, FALSE, FALSE  },
    { "DC.ICC1",    "", INI_ENV_MAX, IDV_ICC1, FALSE, FALSE  },
    { "DC.ICC2",    "", INI_ENV_MAX, IDV_ICC2, FALSE, FALSE  },
    { "DC.ICC3",    "", INI_ENV_MAX, IDV_ICC3, FALSE, FALSE  },
    { "DC.ISB1",    "", INI_ENV_MAX, IDV_ISB1, FALSE, FALSE  },
    { "DC.ISB2",    "", INI_ENV_MAX, IDV_ISB2, FALSE, FALSE  },
    { "DC.ILI",     "", INI_ENV_MAX, IDV_ILI, FALSE, FALSE  },
    { "DC.ILO",     "", INI_ENV_MAX, IDV_ILO, FALSE, FALSE  },

    { "SYS.NOP",            "nop", INI_ENV_MAX, ISV_NOP, FALSE, FALSE  },
    { "SYS.NUMS_PLANE",     "plane", INI_ENV_MAX, ISV_NUMS_PLANE, FALSE, FALSE  },
    { "SYS.NUMS_DIE",       "die", INI_ENV_MAX, ISV_NUMS_DIE, FALSE, FALSE  },
    { "SYS.NUMS_BLOCKS",    "blocks", INI_ENV_MAX, ISV_NUMS_BLOCKS, FALSE, FALSE  },
    { "SYS.NUMS_PAGES",     "pages", INI_ENV_MAX, ISV_NUMS_PAGES, FALSE, FALSE  },
    { "SYS.NUMS_PGSIZE",    "pagesize", INI_ENV_MAX, ISV_NUMS_PGSIZE, FALSE, FALSE  },
    { "SYS.NUMS_SPARESIZE", "spare", INI_ENV_MAX, ISV_NUMS_SPARESIZE, FALSE, FALSE  },
    { "SYS.NUMS_IOPINS",    "pins", INI_ENV_MAX, ISV_NUMS_IOPINS, FALSE, FALSE  },
    { "SYS.MAX_ERASE_CNT",  "erasecnt", INI_ENV_MAX, ISV_MAX_ERASE_CNT, FALSE, FALSE  },
    { "SYS.CLOCK_PERIODS",  "cp", INI_ENV_MAX, ISV_CLOCK_PERIODS, FALSE, FALSE  },

    { "REPORT.SnoopNandPlaneRead", "readhistory", IRV_SNOOP_NAND_PLANE_READ, INI_DEVICE_MAX, TRUE, FALSE  },
    { "REPORT.SnoopNandPlaneWrite", "writehistory", IRV_SNOOP_NAND_PLANE_WRITE, INI_DEVICE_MAX, TRUE, FALSE  },
    { "REPORT.SnoopInternalState", "interstate", IRV_SNOOP_INTERNAL_STATE, INI_DEVICE_MAX, TRUE, FALSE  },
    { "REPORT.SnoopBusTransaction", "bustrans", IRV_SNOOP_BUS_TRANSACTION, INI_DEVICE_MAX, TRUE, FALSE  },
    { "REPORT.SnoopIoCompletion", "iocomp", IRV_SNOOP_IO_COMPLETION, INI_DEVICE_MAX, TRUE, FALSE  },
    { "REPORT.CyclesForEachState", "cycleanaly", IRV_CYCLES_FOR_EACH_STATE, INI_DEVICE_MAX, TRUE, FALSE  },
    { "REPORT.PowerCyclesForEachDcParam", "analypower", IRV_POWER_CYCLES_FOR_EACH_DCPARAM, INI_DEVICE_MAX, TRUE, FALSE  },

    { "ENV.NoIdleCycles", "fastmode", IEV_NO_IDLE_CYCLES, INI_DEVICE_MAX, TRUE, FALSE  },
    { "ENV.ParambasedSimulation", "", IEV_PARAM_BASED_SIMULATION, INI_DEVICE_MAX, TRUE, FALSE  },
    { "ENV.CMLCStyleVariation", "variation", IEV_CMLC_STYLE_VARIATION, INI_DEVICE_MAX, TRUE, FALSE  },

    { "", "", INI_ENV_MAX, INI_DEVICE_MAX, FALSE, FALSE }
};

void ParamManager::readIni( std::string &iniPath, std::map< std::string, UINT32 > &paramTable )
{
    namespace pod = boost::program_options::detail;
    if(iniPath.empty() != true)
    {
        std::ifstream                 configure(iniPath.c_str());

        if(configure.is_open() == false)
        {
            std::cerr<<"error on read ini file"<<std::endl;
            return;
        }
        
        std::set< std::string >       option;
        option.insert("*");

        for (pod::config_file_iterator i(configure, option), e ; i != e; ++i)
        {
            paramTable[i->string_key] = atoi(i->value[0].c_str());
        }
    }
}  

void ParamManager::SetIniInfo( const char * deviceIniPath, const char * envIniPath )
{
    if(deviceIniPath != NULL)
    {
        _inipath[INI_DEVICE] = deviceIniPath;
        GetParam(ISV_NOP);  // Dummy call to set values to the parameter variable.
    }
    
    if(envIniPath != NULL)
    {
        _inipath[INI_ENVIRONMENT] = envIniPath;
        GetEnv(IRV_SNOOP_NAND_PLANE_READ); // Dummy call to set values to the parameter variable.
    }
}

UINT32 ParamManager::GetEnv(INI_ENV_VALUE eValue)
{
    static std::map< std::string, UINT32 >  paramTable;

    if(paramTable.empty() == true)
    {
        memset(m_nEnvVal, 0xFFFFFFFF, sizeof(m_nEnvVal));

        assert(_inipath[INI_ENVIRONMENT].empty() == false);
        readIni(_inipath[INI_ENVIRONMENT], paramTable);

        std::map< std::string, UINT32 >::iterator it = paramTable.begin();
        for(; it != paramTable.end(); it++)
        {
            for(int i = 0; gParamTypes[i].szTypeIniName[0] != '\0'; i++)
            {
                if(gParamTypes[i].bIsEnv == TRUE && !it->first.compare(gParamTypes[i].szTypeIniName)) 
                {
                    m_nEnvVal[gParamTypes[i].eEnvValue] = it->second;
                    gParamTypes[i].bValueExist = TRUE;
                    break;
                }
            }
        }
    }

    return m_nEnvVal[eValue];
}


UINT32 ParamManager::GetParam(INI_DEVICE_VALUE eValue)
{
    static std::map< std::string, UINT32 >  paramTable;

    if(paramTable.empty() == true)
    {
        memset(m_nDeviceVal, 0xFFFFFFFF, sizeof(m_nDeviceVal));

        assert(_inipath[INI_DEVICE].empty() == false);
        readIni(_inipath[INI_DEVICE], paramTable);

        std::map< std::string, UINT32 >::iterator it = paramTable.begin();
        for(; it != paramTable.end(); it++)
        {
            for(int i = 0; gParamTypes[i].szTypeIniName[0] != '\0'; i++)
            {
                if(gParamTypes[i].bIsEnv == FALSE && !it->first.compare(gParamTypes[i].szTypeIniName)) 
                {
                    m_nDeviceVal[gParamTypes[i].eDeviceValue] = it->second;
                    gParamTypes[i].bValueExist = TRUE;
                    break;
                }
            }
        }

        if(m_nDeviceVal[ISV_CLOCK_PERIODS] == 0) m_nDeviceVal[ISV_CLOCK_PERIODS] = 1;
        if(m_nDeviceVal[ISV_NUMS_PLANE] == 0) m_nDeviceVal[ISV_NUMS_PLANE] = 2;
        if(m_nDeviceVal[ISV_NUMS_DIE] == 0) m_nDeviceVal[ISV_NUMS_DIE] = 2;
    }

    return m_nDeviceVal[eValue];
}


void ParamManager::SetParam(INI_DEVICE_VALUE eType, UINT32 nValue)
{
    m_nDeviceVal[eType] = nValue;
}


void ParamManager::SetEnv(INI_ENV_VALUE eType, UINT32 nValue)
{
    m_nEnvVal[eType] = nValue;
}


char* ParamManager::HasAllParameterValues(void)
{
    for(int i = 0; gParamTypes[i].szTypeIniName[0] != '\0'; i++)
    {
        if(gParamTypes[i].bValueExist == FALSE) return gParamTypes[i].szTypeIniName;
    }

    return NULL;
}

}