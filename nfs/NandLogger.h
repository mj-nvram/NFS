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

#ifndef _NandLogger_h__
#define _NandLogger_h__

namespace NANDFlashSim {

typedef enum {
    NANDLOG_SNOOP_NANDPLANE_READ,
    NANDLOG_SNOOP_NANDPLANE_WRITE,
    NANDLOG_SNOOP_INTERNAL_STATE,
    NANDLOG_SNOOP_INTERNAL_ACC_CYCLE,
    NANDLOG_SNOOP_BUS_TRANSACTION,
    NANDLOG_SNOOP_IOCOMPLETION,
    NANDLOG_TOTAL_CYCLE_FOR_EACH_STATE,
    NANDLOG_TOTAL_POWERCYCLE_FOR_EACH_DCPARAM,
    NANDLOG_MAX_TYPES
} NANDLOG_TYPE;


class NandLogger {
private :
    static std::ofstream _logstream[NANDLOG_MAX_TYPES];
    NandLogger();           // single-tone
    static void             cleanUp();
    static void             printFieldInfo(NANDLOG_TYPE nLogType);
public :
    static std::ofstream*   GetStream(NANDLOG_TYPE nLogType);
    static void             MarkupHardReset();
};

#define REPORT_NAND(_nNandLogType, _str)  \
    { \
        std::ofstream* stream = NandLogger::GetStream(_nNandLogType); \
        if(stream != NULL) (*stream) << _str << std::endl; \
    } 

}

#endif // _NandLogger_h__
