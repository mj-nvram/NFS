/****************************************************************************
*	 NANDFlashSim: A Cycle Accurate NAND Flash Memory Simulator 
*	 
*	 Copyright (C) 2011   	Myoungsoo Jung (MJ)
*
*                           Pennsylvania State University
*                           Microsystems Design Laboratory, I/O Group of Mahmut Kandemir
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

#include <assert.h>
#include "TypeSystem.h"



#ifndef NO_STORAGE
#include "boost/filesystem/path.hpp"
#include "boost/filesystem.hpp"
#include "boost/iostreams/code_converter.hpp"
#include "boost/iostreams/device/mapped_file.hpp"
#endif

#include <iostream>
#include <sstream>
#include "boost/shared_array.hpp"

#include "Tools.h"
#include "Plane.h"
#include "NandLogger.h"

#ifndef NO_STORAGE
namespace fs    = boost::filesystem;
namespace io    = boost::iostreams;
#endif


namespace NANDFlashSim {

Plane::Plane( NandDeviceConfig &stDevConfig) 
    : 
    #ifndef NO_STORAGE
    _vctpVirtualBlk(stDevConfig._nNumsBlk / NAND_VIRTUAL_BLOCK_IDX_RESOLUTION, NULL),
    #endif
    _stDevConfig(stDevConfig)
{
#ifndef NO_STORAGE
    if(fs::exists(".\\phyicalData") == false)
    {
        fs::create_directory(".\\phyicalData");
    } 
    _vctVirtualBlk.resize(stDevConfig._nNumsBlk / NAND_VIRTUAL_BLOCK_IDX_RESOLUTION);
#endif

    _vctEcBlkInfo   = std::vector<UINT32>(stDevConfig._nNumsBlk, 0);
    _vctLppBlkInfo  = std::vector<UINT32>(stDevConfig._nNumsBlk, 0);
    _vctsaNopPgInfo.resize(stDevConfig._nNumsBlk);
    for (UINT32 nIdx = 0; nIdx < stDevConfig._nNumsBlk; nIdx++)
    {
        UINT8       *pNop        = new UINT8[stDevConfig._nNumsPgPerBlk];
        assert(pNop != NULL);
        memset(pNop, 0x0, stDevConfig._nNumsPgPerBlk);
        PNOP_PGS    psmtArr(pNop);
        _vctsaNopPgInfo[nIdx]     = psmtArr;
    }
}

Plane::~Plane()
{
#ifndef NO_STORAGE
    resetPhysicalPlane();
#endif
}

NV_RET Plane::Read( UINT16 nCol, UINT32 nRow, UINT8 *pData )
{
    NV_RET nRet = NAND_SUCCESS;
    if(nCol == NULL_SIG(UINT16) || nRow == NULL_SIG(UINT32))
    {
        nRet |= NAND_PLANE_ERROR_ADDRESS;
    }

    UINT16  nPbn    = NAND_PBN_PARSE_REGISTER(nRow);
    UINT16  nPgoff  = NAND_PGO_PARSE_REGISTER(nRow);

    REPORT_NAND(NANDLOG_SNOOP_NANDPLANE_READ, _nId << " , " << nPbn << " , " << nPgoff );

#ifndef NO_STORAGE

    size_t  nVirtualBlkIdx  = nPbn / NAND_VIRTUAL_BLOCK_IDX_RESOLUTION;
    size_t  nVirtualPgIdx   = (nPbn % NAND_VIRTUAL_BLOCK_IDX_RESOLUTION) * _stDevConfig._nNumsPgPerBlk + nPgoff;

    if(_vctpVirtualBlk[nVirtualBlkIdx] == NULL)
    {
        // for minimizing to allocate mapped file, it will be created on demand.
        _vctpVirtualBlk[nVirtualBlkIdx] = allocateVirtualNANDBlock(nVirtualBlkIdx);
        assert(_vctpVirtualBlk[nVirtualBlkIdx] != NULL);
    }

    if(pData != NULL)
    {
        UINT8 * memoryoffset = _vctpVirtualBlk[nVirtualBlkIdx] + (nVirtualPgIdx * _stDevConfig._nPgSize + nCol);
        memcpy(pData + nCol, memoryoffset, _stDevConfig._nPgSize - nCol);
    }
#endif
    return nRet;
}

NV_RET Plane::Write( UINT16 nCol, UINT32 nRow, UINT8 *pData )
{
    NV_RET nRet = NAND_SUCCESS;
    if(nCol == NULL_SIG(UINT16) || nRow == NULL_SIG(UINT32))
    {
        nRet = NAND_PLANE_ERROR_ADDRESS;
    }
    
    UINT16  nPbn    = NAND_PBN_PARSE_REGISTER(nRow);
    UINT16  nPgoff  = NAND_PGO_PARSE_REGISTER(nRow);
   
    REPORT_NAND(NANDLOG_SNOOP_NANDPLANE_WRITE, _nId << " , " << nPbn << " , " << nPgoff );

#ifndef NO_ENFORCING_NOP
    if(_vctsaNopPgInfo[nPbn][nPgoff] > _stDevConfig._nNop)
    {
        nRet |= NAND_PLANE_ERROR_NOP_VIOLOATION;
        NV_ERROR("NOP violation");
        //
        // TODO: generate data corruption code
        //
    }
    _vctsaNopPgInfo[nPbn][nPgoff]++;
#endif

#ifndef NO_ENFORCING_ORDER
    if(_vctLppBlkInfo[nPbn] > nPgoff)
    {
        nRet |= NAND_PLANE_ERROR_INODRDER_VIOLATION;
        NV_ERROR("in-place update violation");
        //
        // TODO: generate data corruption code
        //
    }
#endif

    if(_stDevConfig._nEc <= _vctEcBlkInfo[nPbn])
    {
        nRet |= NAND_PLANE_ERROR_WEAROUT;
        NV_ERROR("try to write to wear out block :");
        //
        // TODO: generate data corruption code
        //
    }

    _vctLppBlkInfo[nPbn]    = nPgoff;

#ifndef NO_STORAGE
    size_t  nVirtualBlkIdx  = nPbn / NAND_VIRTUAL_BLOCK_IDX_RESOLUTION;
    size_t  nVirtualPgIdx   = (nPbn % NAND_VIRTUAL_BLOCK_IDX_RESOLUTION) * _stDevConfig._nNumsPgPerBlk + nPgoff;

    if(_vctpVirtualBlk[nVirtualBlkIdx] == NULL)
    {
        // for minimizing to allocate mapped file, it will be created on demand.
        _vctpVirtualBlk[nVirtualBlkIdx] = allocateVirtualNANDBlock(nVirtualBlkIdx);
        assert(_vctpVirtualBlk[nVirtualBlkIdx] != NULL);
    }

    if(pData != NULL)
    {
        UINT8 * memoryoffset = _vctpVirtualBlk[nVirtualBlkIdx] + (nVirtualPgIdx * _stDevConfig._nPgSize + nCol);
        memcpy(memoryoffset, pData + nCol, _stDevConfig._nPgSize - nCol);

    }
#endif    
    return nRet;
}

UINT8* Plane::allocateVirtualNANDBlock(UINT32 nVirtualBlkIdx)
{
    UINT8*                  pAllocatedBlk = NULL;
#ifndef NO_STORAGE
    //
    // load nand plane
    //
    io::mapped_file_params  mapParam;

    std::ostringstream  strstream;
    strstream << ".\\phyicalData\\DEVID_" << _stDevConfig._nDeviceId << "_PLID" << _nId << "_VND" << nVirtualBlkIdx;

    mapParam.path           = strstream.str();
    mapParam.flags          = io::mapped_file::readwrite;

    if(fs::exists(mapParam.path) == false)
    {
        assert((NAND_BLOCK_SIZE(_stDevConfig) * NAND_VIRTUAL_BLOCK_IDX_RESOLUTION) != 0);
        mapParam.new_file_size  = NAND_BLOCK_SIZE(_stDevConfig) * NAND_VIRTUAL_BLOCK_IDX_RESOLUTION;
    }
    else
    {
        mapParam.length         = NAND_BLOCK_SIZE(_stDevConfig) * NAND_VIRTUAL_BLOCK_IDX_RESOLUTION;   
    }

    _vctVirtualBlk[nVirtualBlkIdx].open(mapParam);
    if(_vctVirtualBlk[nVirtualBlkIdx].is_open()  == true)
    {
        pAllocatedBlk = (UINT8 *)_vctVirtualBlk[nVirtualBlkIdx].data();
    }
    else
    {
        NV_ERROR("open fail for NAND plane data : " + strstream.str());
    }
#endif
    return pAllocatedBlk;

}


NV_RET Plane::Erase( UINT32 nRow )
{
    UINT16  nPbn    = NAND_PBN_PARSE_REGISTER(nRow);
    NV_RET nRet = NAND_SUCCESS;

    if (nPbn == NULL_SIG(UINT16))
    {
        nRet |= NAND_PLANE_ERROR_ADDRESS;
    }
    _vctLppBlkInfo[nPbn]  = 0;
    memset(_vctsaNopPgInfo[nPbn].get(), 0x0, _stDevConfig._nNumsPgPerBlk);

    //
    // Assume that the erase operation fill '0' rather than '1' that is used by real NAND.
    // This is because memory mapped file is initialized by '0'
    //
#ifndef NO_STORAGE
    size_t  nVirtualBlkIdx = nPbn / NAND_VIRTUAL_BLOCK_IDX_RESOLUTION;
    memset((void *)_vctpVirtualBlk[nVirtualBlkIdx], 0x0, NAND_BLOCK_SIZE(_stDevConfig) * NAND_VIRTUAL_BLOCK_IDX_RESOLUTION);
#endif
   _vctEcBlkInfo[nPbn]++;

   if(_stDevConfig._nEc <= _vctEcBlkInfo[nPbn])
   {
       nRet |= NAND_PLANE_ERROR_WEAROUT;
       //
       // TODO: generate data corruption code
       //
   }
   return nRet;
}

void Plane::resetPhysicalPlane()
{
#ifndef NO_STORAGE
	size_t nTotalVirtualBlk = _stDevConfig._nNumsBlk / NAND_VIRTUAL_BLOCK_IDX_RESOLUTION;
	for(size_t nIdx = 0; nIdx < nTotalVirtualBlk; ++nIdx)
	{
		if(_vctpVirtualBlk[nIdx] != NULL)
		{
			if(_vctVirtualBlk[nIdx].is_open())
			{
				_vctVirtualBlk[nIdx].close();
			}
			_vctpVirtualBlk[nIdx] = NULL;
		}
	}
#endif
}

void Plane::HardReset( NandDeviceConfig &stDevConfig )
{
    _stDevConfig = stDevConfig;
    resetPhysicalPlane();

    for (UINT32 nIdx = 0; nIdx < _stDevConfig._nNumsBlk; nIdx++)
    {
        _vctEcBlkInfo[nIdx]     = 0;
        _vctLppBlkInfo[nIdx]    = 0;
        
        memset(_vctsaNopPgInfo[nIdx].get(), 0x0, _stDevConfig._nNumsPgPerBlk);
    }
}

}