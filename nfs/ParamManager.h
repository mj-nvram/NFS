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

/********************************************************************
	created:	2011/07/11
	created:	11:7:2011   10:29
	file base:	ParamManager
	file ext:	h
	author:		Myoungsoo Jung
				Pennsylvania State University, MDL Lab
	
	purpose:	Manage configuration based on ini file.
*********************************************************************/
#ifndef _ParamManager_h__
#define _ParamManager_h__


namespace NANDFlashSim {

typedef enum {
    INI_DEVICE,
    INI_ENVIRONMENT,
    INI_MAX_FILES
} INI_TYPE;

typedef enum {
    IRV_SNOOP_NAND_PLANE_READ,
    IRV_SNOOP_NAND_PLANE_WRITE,
    IRV_SNOOP_INTERNAL_STATE,
    IRV_SNOOP_BUS_TRANSACTION,
    IRV_SNOOP_IO_COMPLETION,
    IRV_CYCLES_FOR_EACH_STATE,
    IRV_POWER_CYCLES_FOR_EACH_DCPARAM,

    IEV_NO_IDLE_CYCLES,
    IEV_PARAM_BASED_SIMULATION,
    IEV_CMLC_STYLE_VARIATION,

    INI_ENV_MAX
} INI_ENV_VALUE;

typedef enum
{
    ITV_tADL,
    ITV_tALH,
    ITV_tALS,       
    ITV_tCH,        
    ITV_tCLH,       
    ITV_tCLS,
    ITV_tCS,
    ITV_tDH,
    ITV_tDS,
    ITV_tWC,
    ITV_tWH,
    ITV_tWP,
    ITV_tWW,
    ITV_tAR,
    ITV_tCEA,
    ITV_tCHZ,
    ITV_tCOH,
    ITV_tDCBSYR1,
    ITV_tDCBSYR2,
    ITV_tIR,
    ITV_tR,
    ITV_tRC,
    ITV_tREA,
    ITV_tREH,
    ITV_tHOH,
    ITV_tRHZ,
    ITV_tRLOH,
    ITV_tRP,        
    ITV_tRR,
    ITV_tRST,
    ITV_tWB,
    ITV_tWHR,
    ITV_tBERS,
    ITV_tCBSY,
    ITV_tDBSY,
    ITV_tPROG,
    
    IMV_tPROG,
    IMV_tDCBSYR1,
    IMV_tDCBSYR2,
    IMV_tBERS,
    IMV_tCBSY,
    IMV_tDBSY,

    IDV_VCC,
    IDV_ICC1,
    IDV_ICC2,
    IDV_ICC3,
    IDV_ISB1,
    IDV_ISB2,
    IDV_ILI,
    IDV_ILO,

    ISV_NOP,
    ISV_NUMS_PLANE,
    ISV_NUMS_DIE,
    ISV_NUMS_BLOCKS,
    ISV_NUMS_PAGES,
    ISV_NUMS_PGSIZE,
    ISV_NUMS_SPARESIZE,
    ISV_NUMS_IOPINS,
    ISV_MAX_ERASE_CNT,
    ISV_CLOCK_PERIODS,

    INI_DEVICE_MAX
}INI_DEVICE_VALUE;

struct paramTypes
{
    char szTypeIniName[100];        // Ini file parameter name
    char szTypeParamName[20];       // Command line parameter name
    INI_ENV_VALUE eEnvValue;
    INI_DEVICE_VALUE eDeviceValue;
    BOOL bIsEnv;
    BOOL bValueExist;
};

extern paramTypes gParamTypes[];

class ParamManager {
private :
	// singletone
	ParamManager();
    static std::string                      _inipath[INI_MAX_FILES];    
    static void     readIni(std::string &iniPath, std::map< std::string, UINT32 > &paramTable);

public :
    static void     SetIniInfo(const char * deviceIniPath, const char * envIniPath);
    static UINT32   GetParam(INI_DEVICE_VALUE eValue);
    static UINT32   GetEnv(INI_ENV_VALUE eValue);

    static void     SetParam(INI_DEVICE_VALUE eType, UINT32 nTypeIdx, UINT32 nValue);
    static void     SetEnv(INI_ENV_VALUE eType, UINT32 nTypeIdx, UINT32 nValue);

    static char*    HasAllParameterValues(void);

private:
    static UINT32   m_nEnvVal[INI_ENV_MAX];
    static UINT32   m_nDeviceVal[INI_DEVICE_MAX];
};

#define     NFS_GET_PARAM(_Name)          (ParamManager::GetParam(_Name))
#define     NFS_GET_ENV(_Name)            (ParamManager::GetEnv(_Name))

}

#endif // _ParamManager_h__
