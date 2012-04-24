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
	created:	2011/07/16
	created:	16:7:2011   19:14
	file base:	Plane
	file ext:	h
	author:		Myoungsoo Jung
				Pennsylvania State University, MDL Lab
	
	purpose:	
*********************************************************************/

#ifndef _Plane_h__
#define _Plane_h__

#include "boost/shared_array.hpp"
#include "boost/iostreams/device/mapped_file.hpp"

namespace NANDFlashSim {

class Plane {
    typedef     boost::shared_array<UINT8>          PNOP_PGS;
    typedef     boost::iostreams::mapped_file       VIRTUABLK_FILE;
    
    
    std::vector < VIRTUABLK_FILE >      _vctVirtualBlk;
    std::vector < UINT8 * >             _vctpVirtualBlk;
    NandDeviceConfig                    _stDevConfig;
    UINT32                              _nId;
    std::vector<PNOP_PGS>               _vctsaNopPgInfo;
    std::vector<UINT32>                 _vctLppBlkInfo; // last programmed page offset
    std::vector<UINT32>                 _vctEcBlkInfo;

    UINT32                              _nStatNopViolation;
    UINT32                              _nStateDataCorruption;
    UINT32                              _nStatWearout;

public :
    Plane(NandDeviceConfig &stDevConfig);
    ~Plane();

    NV_RET  Write(UINT16 nCol, UINT32 nRow, UINT8 *pData);
    NV_RET  Read(UINT16 nCol, UINT32 nRow, UINT8 *pData);
    NV_RET  Erase(UINT32 nRow);
    UINT32  ID() const { return _nId; }
    void    ID(UINT32 val) { _nId = val; }
    void    HardReset(NandDeviceConfig &stDevConfig);

private :
    UINT8*  allocateVirtualNANDBlock(UINT32 nVirtualBlkIdx);
    void    resetPhysicalPlane();
};

}

#endif // _Plane_h__