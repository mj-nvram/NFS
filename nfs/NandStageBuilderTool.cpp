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
	created:	2011/08/06
	created:	6:8:2011   13:53
	file base:	NandStageBuilderTool
	file ext:	h
	author:		Myoungsoo Jung
				Pennsylvania State University, MDL Lab

    purpose:    You may want to schedule NAND flash commands with fine
    granularity at host- or system-level. In this case, you should 
    appropriately handle NAND commands and data movements based on 
    what the datasheet specifies. The information which is required to handle 
    such activities (commands and data movements can be specified by the 
    structure "NandStagePacket". This builder helps to fill information to
    each StagePacket structure.

    Please check the sequences, described above the builder's functions.
    If you schedule commands and data movements using the NandStagePacket 
    structure in a wrong way (or different way with what flash datasheet specifies),
    the state of NANDFlashSim will be in the error status. 

    note:       We recommend users to use memory transaction interface
    (NandFlashSystem.AddTransaction()).

*********************************************************************/

#include "TypeSystem.h"
#include "Tools.h"
#include "ParamManager.h"
#include "IoCompletion.h"
#include "Plane.h"
#include "Die.h"
#include "LogicalUnit.h"
#include "NandStageBuilderTool.h"

namespace NANDFlashSim {

NandStageBuilderTool::NandStageBuilderTool( NandDeviceConfig &stDevConfig )
{
    _stNandDevConfig    = stDevConfig;

}


NV_RET NandStageBuilderTool::ReadStatus(NandStagePacket &stDataPacket, UINT8 nDie, UINT32 *pStatusData)
{
    NV_RET  nRet                    = NAND_SUCCESS;
    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, 0, 0, 0);
    stDataPacket._nCommand          = NAND_CMD_READ_STATUS;
    stDataPacket._pStatusData       = pStatusData;
    stDataPacket._bLastCmdForFgTrans  = true;
    return nRet;
}

NV_RET NandStageBuilderTool::Reset(NandStagePacket &stDataPacket, UINT8 nDie)
{
    NV_RET  nRet                    = NAND_SUCCESS;
    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, 0, 0, 0);
    stDataPacket._nCommand          = NAND_CMD_RESET;
    stDataPacket._bLastCmdForFgTrans  = true;
    return nRet;
}


void NandStageBuilderTool::breakdownSemiPhysicalAddr( UINT32 nDestAddr,  UINT8 &nDie, UINT8 &nPlane, UINT16 &nPbn, UINT8 &nPpo )
{
    assert(nDestAddr != NULL_SIG(UINT32));
    UINT32      nTempAddr;
    // for user configurable NAND plane and die, I do not use specific mask and bit shift. 
    // Performance can be degraded a little tough.
    nPpo        = nDestAddr % _stNandDevConfig._nNumsPgPerBlk;
    nTempAddr   = nDestAddr / _stNandDevConfig._nNumsPgPerBlk;
    nPlane      = nTempAddr % _stNandDevConfig._nNumsPlane;
    nTempAddr   = nTempAddr / _stNandDevConfig._nNumsPlane;
    nPbn        = nTempAddr % _stNandDevConfig._nNumsBlk;
    nDie        = (nTempAddr / _stNandDevConfig._nNumsBlk) % _stNandDevConfig._nNumsDie;
}

NV_RET NandStageBuilderTool::ReadPageCacheAddAddr(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow )
{
    NV_RET  nRet    = NAND_SUCCESS;

    
    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);

    stDataPacket._nCommand          = NAND_CMD_READ_CACHE_ADDR_INIT;
    stDataPacket._nRandomBytes      = _stNandDevConfig._nPgSize;
    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie,  nPlane, nPbn, nPpo);
    stDataPacket._bLastCmdForFgTrans  = false;

    return nRet;

}


NV_RET NandStageBuilderTool::ReadPageCache( NandStagePacket &stDataPacket, UINT32 nOpenRow, UINT8 *pData )
{

    NV_RET  nRet    = NAND_SUCCESS;
    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nOpenRow, nDie, nPlane, nPbn, nPpo);

    // initialize data packet
    stDataPacket._nCommand          = NAND_CMD_READ_CACHE;
    stDataPacket._nRandomBytes      = _stNandDevConfig._nPgSize;
    stDataPacket._pData             = pData;
    stDataPacket._bLastCmdForFgTrans  = true;
    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);    

    return nRet;
}

NV_RET NandStageBuilderTool::WritePageCache(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData )
{
    
    NV_RET  nRet    = NAND_SUCCESS;

    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);

    // initialize data packet
    if(pStatusData != NULL) 
    {
        (*pStatusData)              = 0;
    }

    stDataPacket._nCommand          = NAND_CMD_PROG_CACHE;
    stDataPacket._nCol              = nCol;
    stDataPacket._nRandomBytes      = _stNandDevConfig._nPgSize - nCol;
    stDataPacket._pData             = pData;
    stDataPacket._pStatusData       = pStatusData;
    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);    
    stDataPacket._bLastCmdForFgTrans  = true;

    return nRet;
}





NV_RET NandStageBuilderTool::ReadNxPlaneAddAddr(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT16 nCol /*= 0*/, bool bLast /*= false*/ )
{
    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);
    NV_RET      nRet = NAND_SUCCESS;

    assert(nCol <= _stNandDevConfig._nPgSize);

    stDataPacket._nCommand          = (bLast) ? NAND_CMD_READ_MULTIPLANE_INIT_FIN :NAND_CMD_READ_MULTIPLANE_INIT;
    stDataPacket._nCol              = nCol;
    stDataPacket._nRandomBytes      = _stNandDevConfig._nPgSize - nCol;
    stDataPacket._bLastCmdForFgTrans  = false;
    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);

   
    return nRet;        
}

NV_RET NandStageBuilderTool::ReadNxPlaneSelection(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT8 *pData, UINT32 nNumsPulse, UINT16 nCol /*= 0*/, bool bLast /*= false*/ )
{

    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);
    NV_RET      nRet = NAND_SUCCESS;

    if(nNumsPulse > _stNandDevConfig._nPgSize - nCol)
    {
        nRet        |= NAND_CTRL_ERROR_IOLENGTH;
        nNumsPulse  = _stNandDevConfig._nPgSize - nCol;
    }

    assert(nCol <= _stNandDevConfig._nPgSize);

    stDataPacket._nCommand          = NAND_CMD_READ_MULTIPLANE;
    stDataPacket._nCol              = nCol;
    stDataPacket._nRandomBytes      = nNumsPulse;
    stDataPacket._pData             = pData;
    stDataPacket._bLastCmdForFgTrans  = true;
    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);
 
    return nRet;        

}

NV_RET NandStageBuilderTool::WriteNxPlane(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData, bool bLastPlane )
{
    
    NV_RET  nRet    = NAND_SUCCESS;
    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);


    assert(nCol <= _stNandDevConfig._nPgSize);

    // initialize data packet
    if (pStatusData != NULL && bLastPlane)
    {
        (*pStatusData)              = 0;
    }
    stDataPacket._nCommand          = (bLastPlane) ?  NAND_CMD_PROG_MULTIPLANE_FIN : NAND_CMD_PROG_MULTIPLANE;
    stDataPacket._nCol              = nCol;
    stDataPacket._nRandomBytes      = _stNandDevConfig._nPgSize - nCol;
    stDataPacket._pData             = pData;
    stDataPacket._pStatusData       = (bLastPlane) ? pStatusData : NULL;
    stDataPacket._bLastCmdForFgTrans  = true;


    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);
    return nRet;    
}


NV_RET NandStageBuilderTool::WriteNxPlaneForRandom(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 nNumsPulse)
{
    
    NV_RET  nRet    = NAND_SUCCESS;
    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);

    if(nNumsPulse > _stNandDevConfig._nPgSize - nCol)
    {
        nRet        |= NAND_CTRL_ERROR_IOLENGTH;
        nNumsPulse  = _stNandDevConfig._nPgSize - nCol;
    }

    assert(nCol <= _stNandDevConfig._nPgSize);

    stDataPacket._nCommand          = NAND_CMD_PROG_MULTIPLANE_RANDOM;
    stDataPacket._nCol              = nCol;
    stDataPacket._nRandomBytes      = nNumsPulse;
    stDataPacket._pData             = pData;
    stDataPacket._pStatusData       = NULL;
    stDataPacket._bLastCmdForFgTrans  = false;
    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);

    return nRet;    

}

NV_RET NandStageBuilderTool::WriteNxPlaneRandomColSelection( NandStagePacket &stDataPacket, UINT32 nOpenRow, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData, UINT32 nNumsPulse, bool bLastRand /*= false*/, bool bLastStage/*= false */ )
{
    NV_RET  nRet    = NAND_SUCCESS;

    if(nNumsPulse > _stNandDevConfig._nPgSize - nCol)
    {
        nRet        |= NAND_CTRL_ERROR_IOLENGTH;
        nNumsPulse  = _stNandDevConfig._nPgSize - nCol;
    }
    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nOpenRow, nDie, nPlane, nPbn, nPpo);

    assert(nCol <= _stNandDevConfig._nPgSize);
    // initialize data packet
    if (pStatusData != NULL && bLastStage)
    {
        (*pStatusData)              = 0;
    }

    if (bLastStage)
    {
        stDataPacket._nCommand      = NAND_CMD_PROG_MULTIPLANE_FIN_RANDOM;
    }
    else
    {
        stDataPacket._nCommand      = (bLastRand) ?  NAND_CMD_PROG_MULTIPLANE_RANDOM_DUMMY : NAND_CMD_PROG_MULTIPLANE_RANDOM;
    }


    stDataPacket._nCol              = nCol;
    stDataPacket._nRandomBytes      = nNumsPulse;
    stDataPacket._pData             = pData;
    stDataPacket._pStatusData       = (bLastStage) ? pStatusData : NULL;
    stDataPacket._bLastCmdForFgTrans  = bLastStage;
    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);

    return nRet;
}

NV_RET NandStageBuilderTool::WriteNxPlaneCache( NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData, bool bLastPlane /*= false*/, bool bLastStage/*= false*/ )
{
    
    NV_RET      nRet    = NAND_SUCCESS;
    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);

    assert(nCol <= _stNandDevConfig._nPgSize);

    // initialize data packet
    if (pStatusData != NULL && bLastPlane)
    {
        (*pStatusData)              = 0;
    }

    if(bLastStage == true)
    {
        stDataPacket._nCommand      = NAND_CMD_PROG_MULTIPLANE_CACHE_FIN; 
    }
    else
    {
        stDataPacket._nCommand      = (bLastPlane) ?  NAND_CMD_PROG_MULTIPLANE_CACHE : NAND_CMD_PROG_MULTIPLANE;
    }

    stDataPacket._nCol              = nCol;
    stDataPacket._nRandomBytes      = _stNandDevConfig._nPgSize - nCol;
    stDataPacket._pData             = pData;
    stDataPacket._pStatusData       = (bLastPlane) ? pStatusData : NULL;
    stDataPacket._bLastCmdForFgTrans  = bLastPlane;

    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);
    return nRet;    
    
}

NV_RET NandStageBuilderTool::ReadInternalPage(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow )
{
    
    NV_RET  nRet    = NAND_SUCCESS;
    // initialize data packet
    stDataPacket._nCommand          = NAND_CMD_READ_INTERNAL;
    stDataPacket._nCol              = 0;
    stDataPacket._nRandomBytes      = _stNandDevConfig._nPgSize;
    stDataPacket._pData             = NULL;
    stDataPacket._pStatusData       = NULL;
    stDataPacket._bLastCmdForFgTrans  = false;

    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);

    stDataPacket._nRow      = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);
    return nRet;
}


NV_RET NandStageBuilderTool::WriteInternalPage(NandStagePacket &stDataPacket,  UINT32 nSemiPhyRow, UINT32 *pStatusData )
{

    
    NV_RET  nRet    = NAND_SUCCESS;    
    // initialize data packet
    if (pStatusData != NULL)
    {
        (*pStatusData)              = 0;
    }

    // initialize data packet
    stDataPacket._nCommand          = NAND_CMD_PROG_INTERNAL;
    stDataPacket._nCol              = 0;
    stDataPacket._nRandomBytes      = _stNandDevConfig._nPgSize;
    stDataPacket._pData             = NULL;
    stDataPacket._pStatusData       = pStatusData;
    stDataPacket._bLastCmdForFgTrans  = true;

    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);

    stDataPacket._nRow      = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);


    return nRet;
}

NV_RET NandStageBuilderTool::ReadInternalPageNxPlane(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, bool bLast /*= false*/ )
{
    
    NV_RET  nRet    = NAND_SUCCESS;

    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);
    


    // initialize data packet
    stDataPacket._nCommand          = (bLast) ? NAND_CMD_READ_INTERNAL_MULTIPLANE_FIN : NAND_CMD_READ_INTERNAL_MULTIPLANE;
    stDataPacket._nCol              = 0;
    stDataPacket._nRandomBytes      = _stNandDevConfig._nPgSize;
    stDataPacket._pData             = NULL;
    stDataPacket._pStatusData       = NULL;
    stDataPacket._bLastCmdForFgTrans  = false;

    stDataPacket._nRow      = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);

    return nRet;
}




NV_RET NandStageBuilderTool::WriteInternalPageNxPlane(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 *pStatusData, bool bLast /*= false*/ )
{

    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);


    
    NV_RET          nRet= NAND_SUCCESS;

    // initialize data packet
    if (pStatusData != NULL && bLast == true)
    {
        (*pStatusData)              = 0;
    }

    // initialize data packet
    stDataPacket._nCommand          = (bLast) ? NAND_CMD_PROG_INTERNAL_MULTIPLANE_FIN : NAND_CMD_PROG_INTERNAL_MULTIPLANE;
    stDataPacket._nCol              = 0;
    stDataPacket._nRandomBytes      = _stNandDevConfig._nPgSize;
    stDataPacket._pData             = NULL;
    stDataPacket._pStatusData       = (bLast) ? pStatusData : NULL ;
    stDataPacket._bLastCmdForFgTrans  = bLast;


    stDataPacket._nRow      = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);
    return nRet;

}

NV_RET NandStageBuilderTool::EraseBlock(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 *pStatusData )
{
    
    NV_RET  nRet    = NAND_SUCCESS;

    // initialize data packet
    if (pStatusData != NULL)
    {
        (*pStatusData)              = 0;
    }
    stDataPacket._nCommand          = NAND_CMD_BLOCK_ERASE;
    stDataPacket._pStatusData       = pStatusData;
    stDataPacket._bLastCmdForFgTrans  = true;

    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);

    stDataPacket._nRow  = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);

    return nRet;    
}

NV_RET NandStageBuilderTool::EraseNxBlock(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 *pStatusData, bool bLast /*= false*/ )
{
    
    NV_RET  nRet    = NAND_SUCCESS;

    // initialize data packet
    if (pStatusData != NULL)
    {
        (*pStatusData)              = 0;
    }
    stDataPacket._nCommand          = (bLast) ? NAND_CMD_BLOCK_MULTIPLANE_ERASE_FIN : NAND_CMD_BLOCK_MULTIPLANE_ERASE;
    stDataPacket._pStatusData       = pStatusData;
    stDataPacket._bLastCmdForFgTrans  = bLast;

    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);

    stDataPacket._nRow  = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);
    return nRet;    

}

NV_RET NandStageBuilderTool::ReadPage(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 nNumsPulse )
{
    
    NV_RET  nRet    = NAND_SUCCESS;

    assert(nCol <= _stNandDevConfig._nPgSize);
    if(nNumsPulse > _stNandDevConfig._nPgSize - nCol)
    {
        nRet        = NAND_CTRL_ERROR_IOLENGTH;
        nNumsPulse  = _stNandDevConfig._nPgSize - nCol;
    }

    stDataPacket._nCommand          = NAND_CMD_READ_PAGE;
    stDataPacket._nCol              = nCol;
    stDataPacket._nRandomBytes      = nNumsPulse;
    stDataPacket._pData             = pData;
    stDataPacket._bLastCmdForFgTrans  = true;

    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);

    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);
    return nRet;
}


NV_RET NandStageBuilderTool::ReadRandomColSelection( NandStagePacket &stDataPacket, UINT32 nOpenRow, UINT32 nCol, UINT8 *pData, UINT32 nNumsPulse )
{
    
    NV_RET  nRet    = NAND_SUCCESS;
    assert(nCol <= _stNandDevConfig._nPgSize);

    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nOpenRow, nDie, nPlane, nPbn, nPpo);

    if(nNumsPulse > _stNandDevConfig._nPgSize - nCol)
    {
        nRet        |= NAND_CTRL_ERROR_IOLENGTH;
        nNumsPulse  = _stNandDevConfig._nPgSize - nCol;
    }

    stDataPacket._nCommand          = NAND_CMD_READ_RANDOM;
    stDataPacket._nCol              = nCol;
    stDataPacket._nRandomBytes      = nNumsPulse;
    stDataPacket._pData             = pData;
    stDataPacket._bLastCmdForFgTrans  = true;
    stDataPacket._nRow              = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);    

    return nRet;

}

NV_RET NandStageBuilderTool::ReadNxPlaneRandomColSelection( NandStagePacket &stDataPacket, UINT32 nOpenRow, UINT32 nCol, UINT8 *pData, UINT32 nNumsPulse )
{
    // this is just wrapping method.
    // Originally, ReadRondomCol can be harmonized to readnxplaneselection method.
    return ReadRandomColSelection(stDataPacket, nOpenRow, nCol, pData, nNumsPulse);
}

NV_RET NandStageBuilderTool::WritePage(NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData, UINT32 nNumsPulse, bool bRandomIn /*= false*/ )
{
    
    NV_RET  nRet    = NAND_SUCCESS;

    assert(nCol <= _stNandDevConfig._nPgSize);
    if(nNumsPulse > _stNandDevConfig._nPgSize - nCol)
    {
        nRet        = NAND_CTRL_ERROR_IOLENGTH;
        nNumsPulse  = _stNandDevConfig._nPgSize - nCol;
    }

    // initialize data packet
    if (pStatusData != NULL && !bRandomIn)
    {
        (*pStatusData)              = 0;
    }
    stDataPacket._nCommand          = (bRandomIn) ? NAND_CMD_PROG_RANDOM : NAND_CMD_PROG_PAGE;
    stDataPacket._nCol              = nCol;
    stDataPacket._nRandomBytes      = nNumsPulse;
    stDataPacket._pData             = pData;
    stDataPacket._pStatusData       = (bRandomIn) ? NULL : pStatusData;
    stDataPacket._bLastCmdForFgTrans  = (bRandomIn) ? false : true;

    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nSemiPhyRow, nDie, nPlane, nPbn, nPpo);

    stDataPacket._nRow  = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);
    return nRet;

}


NV_RET NandStageBuilderTool::WriteRandomColSelection( NandStagePacket &stDataPacket, UINT32 nOpenRow, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData, UINT32 nNumsPulse, bool bRandomLast /*= false*/ )
{
    
    NV_RET  nRet    = NAND_SUCCESS;

    assert(nCol <= _stNandDevConfig._nPgSize);
    if(nNumsPulse > _stNandDevConfig._nPgSize - nCol)
    {
        nRet        |= NAND_CTRL_ERROR_IOLENGTH;
        nNumsPulse  = _stNandDevConfig._nPgSize - nCol;
    }
    UINT8       nDie, nPlane, nPpo;
    UINT16      nPbn;
    breakdownSemiPhysicalAddr(nOpenRow, nDie, nPlane, nPbn, nPpo);
    stDataPacket._nCommand          = (bRandomLast) ?  NAND_CMD_PROG_RANDOM_FIN : NAND_CMD_PROG_RANDOM;
    stDataPacket._nCol              = nCol;
    stDataPacket._nRandomBytes      = nNumsPulse;
    stDataPacket._pData             = pData;
    stDataPacket._pStatusData       = (bRandomLast) ? pStatusData : NULL ;
    stDataPacket._bLastCmdForFgTrans  = bRandomLast;

    stDataPacket._nRow  = NAND_COMPOSE_ROW_RIGSTER(nDie, nPlane, nPbn, nPpo);
   return nRet;
}

}