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


#include "TypeSystem.h"
#include "Tools.h"
#include "ParamManager.h"
#include "IoCompletion.h"
#include "Plane.h"
#include "Die.h"
#include "LogicalUnit.h"
#include "NandStageBuilderTool.h"
#include "NandLogger.h"
#include "NandController.h"

#include <iomanip>

namespace NANDFlashSim {

NandController::NandController(UINT64 nSystemClock, NandDeviceConfig &stConfig) : 
        _vctCommandChains(stConfig._nNumsLun * stConfig._nNumsDie),
        _vctnPrevNandCmd(stConfig._nNumsLun * stConfig._nNumsDie),
        _vctnPrevTransOp(stConfig._nNumsLun * stConfig._nNumsDie),
        _vctTransCompletion(stConfig._nNumsLun * stConfig._nNumsDie, false),
        _vctDieLevelNandClockIdleTime(stConfig._nNumsLun),
        _vctDieLevelHostClockIdleTime(stConfig._nNumsLun),
        _vctLunLevelHostIdleTime(stConfig._nNumsLun, 0),
        _vctReadReqStat(stConfig._nNumsLun),
        _vctWriteReqStat(stConfig._nNumsLun),
        _vctEraseReqStat(stConfig._nNumsLun),
        _vctResourceContentionTime(stConfig._nNumsLun),
        _vctAddressedNxPacket(stConfig._nNumsLun * stConfig._nNumsDie),
        _vctLuns(stConfig._nNumsLun, LogicalUnit(nSystemClock, stConfig)),
        _vctOpenAddress(stConfig._nNumsDie * stConfig._nNumsDie, NULL_SIG(UINT32)),
        _vctPaneIdx(stConfig._nNumsDie * stConfig._nNumsDie, NULL_SIG(UINT16)),
        _stageBuilder(stConfig)
{
    _nCurrentTime      = nSystemClock;
    _stDevConfig       = stConfig;    
    _nBubbleTime       = 0;
    _nIdleTime         = 0;
    for(UINT16  nLunIdx = 0; nLunIdx < stConfig._nNumsLun; nLunIdx++)
    {
        _vctLuns[nLunIdx].ID(nLunIdx);
        _vctDieLevelNandClockIdleTime[nLunIdx].reserve(stConfig._nNumsDie);
        _vctDieLevelHostClockIdleTime[nLunIdx].reserve(stConfig._nNumsDie);
        _vctResourceContentionTime[nLunIdx].reserve(stConfig._nNumsDie);
        _vctReadReqStat[nLunIdx].reserve(stConfig._nNumsDie);
        _vctWriteReqStat[nLunIdx].reserve(stConfig._nNumsDie);
        _vctEraseReqStat[nLunIdx].reserve(stConfig._nNumsDie);
        for(UINT16 nDieIdx = 0; nDieIdx < stConfig._nNumsDie; nDieIdx++)
        {
            _vctDieLevelNandClockIdleTime[nLunIdx].push_back(0);
            _vctDieLevelHostClockIdleTime[nLunIdx].push_back(0);
            _vctResourceContentionTime[nLunIdx].push_back(0);
            _vctReadReqStat[nLunIdx].push_back(0);
            _vctWriteReqStat[nLunIdx].push_back(0);
            _vctEraseReqStat[nLunIdx].push_back(0);
        }
    }

    _nFineGrainTransId      = 0;
    _nMinNextActivate       = 0;
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    MinNextActivity
//
// FullName:  NANDFlashSim::NandController::MinNextActivity
// Access:    public 
// Returns:   UINT64
//
// Descriptions -
// return minimum cycles needed to update.
//////////////////////////////////////////////////////////////////////////////
UINT64 NandController::MinNextActivity()                       
{ 
    if(_nBubbleTime != 0)
    {
        return _nBubbleTime;
    }

    return _nMinNextActivate; 
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    MinIoBusActivity
//
// FullName:  NANDFlashSim::NandController::MinIoBusActivity
// Access:    public 
// Returns:   UINT64
//
// Descriptions -
// return minimum cycles if there is an activity related to I/O buses
//////////////////////////////////////////////////////////////////////////////
UINT64 NandController::MinIoBusActivity()
{
    // this is different to an use by combining MinNextActiviate and IsIoBusActivate because of buble
    if(IsIoBusActive() == true)
    {
        return _nMinNextActivate;
    }
    else
    {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    Update
//
// FullName:  NANDFlashSim::NandController::Update
// Access:    public 
// Returns:   void
// Parameter: UINT64 nTime
//
// Descriptions -
// Update internal components based on the given nTime.
//////////////////////////////////////////////////////////////////////////////
void NandController::Update(UINT64 nTime)
{
    UINT16 nLunIdx = 0;

    _nCurrentTime    += nTime;
    _nMinNextActivate = NULL_SIG(UINT64);


    if(_nBubbleTime == 0 || nTime > _nBubbleTime)
    {
        nTime -= _nBubbleTime;
		_vctLunLevelHostIdleTime[nLunIdx] += _nBubbleTime;

        _vctLuns[nLunIdx].Update(nTime);
       
        UINT64 nMinIdle     = NULL_SIG(UINT64);
        UINT64 nHostIdle    = 0;
        for(UINT8 nDieIdx=0; nDieIdx < _stDevConfig._nNumsDie; nDieIdx++)
        {
            UINT32 nBusIdx = nLunIdx * _stDevConfig._nNumsDie + nDieIdx;

            if(nTime != ZERO_TIME)
            {
                if(_vctTransCompletion[nBusIdx] == true && _vctLuns[nLunIdx].CheckBusy(nDieIdx) == false)
                {
                    UINT64 nCycles = _vctCommandChains[nBusIdx].front()._nArrivalCycle;
                    _vctCommandChains[nBusIdx].pop_front();
                    // interrupt service routine
                    if(_pIsr != NULL && _vctCommandChains[nBusIdx].empty())
                    {
                        // each stage chain is capable of handing one I/O transaction
                        // TIP: if you want to schedule transactions, you may do that by modifying NandFlashSystem's LunTransactionBus 
                        (*_pIsr)(NAND_ISR_COMPLETE_TRANS, nBusIdx,  CurrentTime());
                    }
                    _vctTransCompletion[nBusIdx]    = false;
                    // a Die is in ready status at this moment; 
                    // it does not mean that this cycle is idle even though FSM is idle. 
                    // After updating this cycle, this system will be in idle state
                    //bDieIdle = false;
                }
            }

            if(_vctCommandChains[nBusIdx].empty() == false)
            {
                NandStagePacket &stagePacket = _vctCommandChains[nBusIdx].front();

                // Lun Bus Activity will be internally emulated.
                // Thus, controller can simply commit the I/O command to LUN when FSM is idle.
                if(_vctLuns[nLunIdx].CheckBusy(nDieIdx) == false)
                {
                    if(_vctLuns[nLunIdx].IssueNandStage(stagePacket) == NAND_SUCCESS)
                    {
                        _vctTransCompletion[nBusIdx] = true;
                    }
                }

                if(_vctLuns[nLunIdx].IsDieIdle(nDieIdx))
                {
                    _vctResourceContentionTime[nLunIdx][nDieIdx] += nTime;
                }
            }

            _vctDieLevelNandClockIdleTime[nLunIdx][nDieIdx] += _vctLuns[nLunIdx].GetCurNandClockIdleTime(nDieIdx);
            nHostIdle = _vctLuns[nLunIdx].GetCurHostClockIdleTime(nDieIdx);
            
			_vctDieLevelHostClockIdleTime[nLunIdx][nDieIdx] += _nBubbleTime;
			_vctDieLevelHostClockIdleTime[nLunIdx][nDieIdx] += nHostIdle;
            
			if(nHostIdle < nMinIdle)
            {
                nMinIdle = nHostIdle;
            }
        }
        _vctLunLevelHostIdleTime[nLunIdx] += nMinIdle;
        _nBubbleTime = 0;
    }
    else 
    {
        // Even though in an update process on bubble times, commands waiting in the command chain should be issued.
        for(UINT8 nDieIdx=0; nDieIdx < _stDevConfig._nNumsDie; nDieIdx++)
        {
            UINT32 nBusIdx = nLunIdx * _stDevConfig._nNumsDie + nDieIdx;
            if(_vctCommandChains[nBusIdx].empty() == false)
            {
                NandStagePacket &stagePacket = _vctCommandChains[nBusIdx].front();

                // Lun Bus Activity will be internally emulated. 
                // Thus, controller can simply commit the I/O command to LUN when FSM is idle.
                if(_vctLuns[nLunIdx].CheckBusy(nDieIdx) == false)
                {
                    if(_vctLuns[nLunIdx].IssueNandStage(stagePacket) == NAND_SUCCESS)
                    {
                        _vctTransCompletion[nBusIdx]    = true;
                    }
                }
            }

            _vctDieLevelHostClockIdleTime[nLunIdx][nDieIdx] += nTime;
        }
        
		_vctLunLevelHostIdleTime[nLunIdx] += nTime;
        _nBubbleTime = _nBubbleTime - nTime;
    }
    
    if(_vctLuns[nLunIdx].MinNextActivity() > 0)
    {
        _nMinNextActivate = (_nMinNextActivate > _vctLuns[nLunIdx].MinNextActivity()) ? _vctLuns[nLunIdx].MinNextActivity() : _nMinNextActivate;
    }

    if(_nMinNextActivate == NULL_SIG(UINT64)) _nMinNextActivate = 0;
}



//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    BuildandAddStage
//
// FullName:  NANDFlashSim::NandController::BuildandAddStage
// Access:    public 
// Returns:   NV_RET
// Parameter: Transaction & stTrans
//
// Descriptions -
// Build stages based on NAND flash command related to identify I/O operation type.
// Built stages are all inserted into the command chain container.
//////////////////////////////////////////////////////////////////////////////
NV_RET NandController::BuildandAddStage( Transaction &stTrans )
{
    NandStagePacket stagePacket(genFineGrainTransId(), _nCurrentTime);
    UINT16  nLunId = 0;
	UINT16  nDieId = NFS_PARSE_DIE_ADDR(stTrans._nAddr, _stDevConfig._bits);
	UINT32  nBusId = nLunId * nDieId + nDieId;
    NV_RET  nRet   =   NAND_SUCCESS;

    switch(stTrans._nTransOp)
    {
    case NAND_OP_READ:
        nRet |= _stageBuilder.ReadPage(stagePacket, stTrans._nAddr, stTrans._nByteOff, stTrans._pData, stTrans._nNumsByte);
        if(nRet == NAND_SUCCESS)
        {
            _vctCommandChains[nBusId].push_back(stagePacket);
            _vctReadReqStat[nLunId][nBusId]++;
        }
        break;

    case NAND_OP_READ_CACHE:
        if(_vctOpenAddress[nBusId] != stTrans._nAddr)
        {
            // start addressing
            nRet |= _stageBuilder.ReadPageCacheAddAddr(stagePacket, stTrans._nAddr);
            if(_vctnPrevNandCmd[nBusId] == stagePacket._nCommand)
            {
                // addressing for cache mode can not be redundant.
                nRet |= NAND_CTRL_ERROR_UNDEFINED_ORDER;
            }

            // actual data out processes start when flash memory receive the read cache command, not the confirmation command for addressing.
            // This is an effort to make timing compatibility ONFI.
            if(nRet == NAND_SUCCESS)
            {
                _vctCommandChains[nBusId].push_back(stagePacket);
                _vctReadReqStat[nLunId][nBusId]++;
            }
            nRet |= _stageBuilder.ReadPageCache(stagePacket, stTrans._nAddr, stTrans._pData);
        }
        else
        {
            if(_vctOpenAddress[nBusId] < stTrans._nAddr)
            {
                // in read with cache mode, the order of addressing should be ascending
                nRet |= NAND_CTRL_ERROR_UNDEFINED_ORDER;
            }
            else
            {
                nRet |= _stageBuilder.ReadPageCache(stagePacket, _vctOpenAddress[nBusId], stTrans._pData);
            }
        }

        if(nRet == NAND_SUCCESS)
        {
            _vctCommandChains[nBusId].push_back(stagePacket);
            _vctOpenAddress[nBusId]    = stTrans._nAddr + 1;
            _vctReadReqStat[nLunId][nBusId]++;
        }
        break;

    case NAND_OP_READ_MULTIPLANE :
        if(stTrans._bAutoPlaneAddressing == true)
        {
            // auto plane addressing mode           
            bool    bLastPlane  = false;
            UINT16  nPlane      = _vctPaneIdx[nBusId];
            if(_vctPaneIdx[nBusId]    == NULL_SIG(UINT16))
            {
                _vctPaneIdx[nBusId] = 0;
                nPlane              = 0;
            }
            else if(_vctPaneIdx[nBusId] == _stDevConfig._nNumsPlane -1)
            {
                bLastPlane            = true;
                _vctPaneIdx[nBusId]   = NULL_SIG(UINT16);
            }
            
            nRet |= _stageBuilder.ReadNxPlaneAddAddr(stagePacket, stTrans._nAddr, stTrans._nByteOff, bLastPlane);
            stagePacket._nRow = NAND_SET_PLANE_REGISTER(stagePacket._nRow, nPlane);
            UINT32  nRow = stagePacket._nRow;
            if(nRet == NAND_SUCCESS)
            {
                _vctCommandChains[nBusId].push_back(stagePacket);
                _vctReadReqStat[nLunId][nBusId]++;
            }

            nRet |= _stageBuilder.ReadNxPlaneSelection(stagePacket, stTrans._nAddr, stTrans._pData, stTrans._nNumsByte, stTrans._nByteOff, bLastPlane);
            stagePacket._nRow = nRow;
            if(nRet == NAND_SUCCESS)
            {
                _vctAddressedNxPacket[nBusId].push_back(stagePacket);
            }

            // processing the stage for the last plane
            if(nRet == NAND_SUCCESS && bLastPlane == true)
            {
                if(_vctAddressedNxPacket[nBusId].empty() || _vctnPrevTransOp[nBusId] != NAND_OP_READ_MULTIPLANE)
                {
                    nRet |= NAND_CTRL_ERROR_UNDEFINED_ORDER;
                }
                else
                {
                    _vctCommandChains[nBusId].splice(_vctCommandChains[nBusId].end(), _vctAddressedNxPacket[nBusId]);
                }
            }

            if(bLastPlane == false)
            {
                _vctPaneIdx[nBusId]++;
            }
        }
        else
        {
            nRet |= _stageBuilder.ReadNxPlaneAddAddr(stagePacket, stTrans._nAddr, stTrans._nByteOff, stTrans._bLastPlane);
            if(nRet == NAND_SUCCESS)
            {
                _vctCommandChains[nBusId].push_back(stagePacket);
                _vctReadReqStat[nLunId][nBusId]++;
            }

            nRet |= _stageBuilder.ReadNxPlaneSelection(stagePacket, stTrans._nAddr, stTrans._pData, stTrans._nNumsByte, stTrans._nByteOff, stTrans._bLastPlane);
            if(nRet == NAND_SUCCESS)
            {
                _vctAddressedNxPacket[nBusId].push_back(stagePacket);
            }

            // processing the stage for the last plane
            if(nRet == NAND_SUCCESS && stTrans._bLastPlane == true)
            {
                if(_vctAddressedNxPacket[nBusId].empty() || _vctnPrevTransOp[nBusId] != NAND_OP_READ_MULTIPLANE)
                {
                    nRet |= NAND_CTRL_ERROR_UNDEFINED_ORDER;
                }
                else
                {
                    _vctCommandChains[nBusId].splice(_vctCommandChains[nBusId].end(), _vctAddressedNxPacket[nBusId]);
                }
            }

        }
        break;

    case NAND_OP_PROG:
        nRet |= _stageBuilder.WritePage(stagePacket,stTrans._nAddr, stTrans._nByteOff, stTrans._pData, stTrans._pStatusData, stTrans._nNumsByte);
        if(nRet == NAND_SUCCESS)
        {
            _vctCommandChains[nBusId].push_back(stagePacket);
            _vctWriteReqStat[nLunId][nBusId]++;
        }
        break;

    case NAND_OP_PROG_CACHE :
        nRet |= _stageBuilder.WritePageCache(stagePacket, stTrans._nAddr, stTrans._nByteOff, stTrans._pData, stTrans._pStatusData);
        if(nRet == NAND_SUCCESS)
        {
            _vctCommandChains[nBusId].push_back(stagePacket);
            _vctWriteReqStat[nLunId][nBusId]++;
        }       
        break;

    case NAND_OP_PROG_MULTIPLANE :
        if(stTrans._bAutoPlaneAddressing == true)
        {
            // auto plane addressing mode           
            bool    bLastPlane  = false;
            UINT16  nPlane      = _vctPaneIdx[nBusId];
            if(_vctPaneIdx[nBusId]    == NULL_SIG(UINT16))
            {
                _vctPaneIdx[nBusId]   = 0;
                nPlane                = 0;
            }
            else if(_vctPaneIdx[nBusId] == _stDevConfig._nNumsPlane -1)
            {
                bLastPlane = true;
                _vctPaneIdx[nBusId]   = NULL_SIG(UINT16);
            }

            nRet |= _stageBuilder.WriteNxPlane(stagePacket, stTrans._nAddr, stTrans._nByteOff, stTrans._pData, stTrans._pStatusData, bLastPlane);
            stagePacket._nRow = NAND_SET_PLANE_REGISTER(stagePacket._nRow, nPlane);

            if(bLastPlane == false)
            {
                _vctPaneIdx[nBusId]++;
            }
        }
        else
        {
            nRet |= _stageBuilder.WriteNxPlane(stagePacket, stTrans._nAddr, stTrans._nByteOff, stTrans._pData, stTrans._pStatusData, stTrans._bLastPlane);
        }

        if(nRet == NAND_SUCCESS)
        {
            _vctCommandChains[nBusId].push_back(stagePacket);
            _vctWriteReqStat[nLunId][nBusId]++;
        }        
        break;

    case NAND_OP_PROG_MULTIPLANE_CACHE :
        if(stTrans._bAutoPlaneAddressing == true)
        {
            // auto plane addressing mode           
            bool    bLastPlane = false;
            UINT16  nPlane  = _vctPaneIdx[nBusId];
            if(_vctPaneIdx[nBusId]    == NULL_SIG(UINT16))
            {
                _vctPaneIdx[nBusId]     = 0;
                nPlane                  = 0;
            }
            else if(_vctPaneIdx[nBusId] == _stDevConfig._nNumsPlane -1)
            {
                bLastPlane = true;
                _vctPaneIdx[nBusId]   = NULL_SIG(UINT16);
            }

            nRet |= _stageBuilder.WriteNxPlaneCache(stagePacket, stTrans._nAddr, stTrans._nByteOff, stTrans._pData, stTrans._pStatusData, bLastPlane, stTrans._bLastNxSubTrans);
            stagePacket._nRow = NAND_SET_PLANE_REGISTER(stagePacket._nRow, nPlane);

            if(bLastPlane == false)
            {
                _vctPaneIdx[nBusId]++;
            }
        }
        else
        {
            nRet |= _stageBuilder.WriteNxPlaneCache(stagePacket, stTrans._nAddr, stTrans._nByteOff, stTrans._pData, stTrans._pStatusData, stTrans._bLastPlane, stTrans._bLastNxSubTrans);
        }


        if(nRet == NAND_SUCCESS)
        {
            _vctCommandChains[nBusId].push_back(stagePacket);
            _vctWriteReqStat[nLunId][nBusId]++;
        }       
        break;

    case NAND_OP_INTERNAL_DATAMOVEMENT :
        nRet |= _stageBuilder.ReadInternalPage(stagePacket, stTrans._nAddr);
        if(nRet == NAND_SUCCESS)
        {
            _vctCommandChains[nBusId].push_back(stagePacket);
            _vctReadReqStat[nLunId][nBusId]++;
        }
        nRet |= _stageBuilder.WriteInternalPage(stagePacket, stTrans._nDestAddr, stTrans._pStatusData);
        if(nRet == NAND_SUCCESS)
        {
            _vctCommandChains[nBusId].push_back(stagePacket);
            _vctReadReqStat[nLunId][nBusId]++;
        }
        break;

    case NAND_OP_INTERNAL_DATAMOVEMENT_MULTIPLANE :
        if(stTrans._bAutoPlaneAddressing == true)
        {
            // auto plane addressing mode           
            bool    bLastPlane      = false;
            UINT16  nPlane          = _vctPaneIdx[nBusId];
            if(_vctPaneIdx[nBusId]  == NULL_SIG(UINT16))
            {
                _vctPaneIdx[nBusId] = 0;
                nPlane              = 0;
            }
            else if(_vctPaneIdx[nBusId] == _stDevConfig._nNumsPlane -1)
            {
                bLastPlane          = true;
                _vctPaneIdx[nBusId] = NULL_SIG(UINT16);
            }

            nRet |= _stageBuilder.ReadInternalPageNxPlane(stagePacket, stTrans._nAddr, bLastPlane);
            stagePacket._nRow = NAND_SET_PLANE_REGISTER(stagePacket._nRow, nPlane);
            UINT32  nRow = stagePacket._nRow;
            if(nRet == NAND_SUCCESS)
            {
                _vctCommandChains[nBusId].push_back(stagePacket);
                _vctReadReqStat[nLunId][nBusId]++;
            }

            nRet |= _stageBuilder.WriteInternalPageNxPlane(stagePacket, stTrans._nDestAddr, stTrans._pStatusData, bLastPlane);
            stagePacket._nRow = nRow;
            if(nRet == NAND_SUCCESS)
            {
                _vctAddressedNxPacket[nBusId].push_back(stagePacket);
                _vctWriteReqStat[nLunId][nBusId]++;
            }

            // processing the stage for the last plane
            if(nRet == NAND_SUCCESS && bLastPlane == true)
            {
                if(_vctAddressedNxPacket[nBusId].empty() || _vctnPrevTransOp[nBusId] != NAND_OP_READ_MULTIPLANE)
                {
                    nRet |= NAND_CTRL_ERROR_UNDEFINED_ORDER;
                }
                else
                {
                    _vctCommandChains[nBusId].splice(_vctCommandChains[nBusId].end(), _vctAddressedNxPacket[nBusId]);
                }
            }


            if(bLastPlane == false)
            {
                _vctPaneIdx[nBusId]++;
            }
        }
        else
        {
            // manual mode
            nRet |= _stageBuilder.ReadInternalPageNxPlane(stagePacket, stTrans._nAddr, stTrans._bLastPlane);
            if(nRet == NAND_SUCCESS)
            {
                _vctCommandChains[nBusId].push_back(stagePacket);
                _vctReadReqStat[nLunId][nBusId]++;
            }

            nRet |= _stageBuilder.WriteInternalPageNxPlane(stagePacket, stTrans._nAddr, stTrans._pStatusData, stTrans._bLastPlane);
            if(nRet == NAND_SUCCESS)
            {
                _vctAddressedNxPacket[nBusId].push_back(stagePacket);
                _vctWriteReqStat[nLunId][nBusId]++;
            }

            // processing the stage for the last plane
            if(nRet == NAND_SUCCESS && stTrans._bLastPlane == true)
            {
                if(_vctAddressedNxPacket[nBusId].empty() || _vctnPrevTransOp[nBusId] != NAND_OP_READ_MULTIPLANE)
                {
                    nRet |= NAND_CTRL_ERROR_UNDEFINED_ORDER;
                }
                else
                {
                    _vctCommandChains[nBusId].splice(_vctCommandChains[nBusId].end(), _vctAddressedNxPacket[nBusId]);
                }
            }
        }
        break;

    case NAND_OP_BLOCK_ERASE :
        nRet |= _stageBuilder.EraseBlock(stagePacket, stTrans._nAddr, stTrans._pStatusData);
        if(nRet == NAND_SUCCESS)
        {
            _vctCommandChains[nBusId].push_back(stagePacket);
            _vctEraseReqStat[nLunId][nBusId]++;
        }
        break;

    case NAND_OP_BLOCK_ERASE_MULTIPLANE :
        if(stTrans._bAutoPlaneAddressing == true)
        {
            // auto plane addressing mode           
            bool    bLastPlane = false;
            UINT16  nPlane  = _vctPaneIdx[nBusId];
            if(_vctPaneIdx[nBusId]    == NULL_SIG(UINT16))
            {
                _vctPaneIdx[nBusId]   = 0;
                nPlane      = 0;
            }
            else if(_vctPaneIdx[nBusId] == _stDevConfig._nNumsPlane -1)
            {
                bLastPlane = true;
                _vctPaneIdx[nBusId]   = NULL_SIG(UINT16);
            }

            nRet |= _stageBuilder.EraseNxBlock(stagePacket, stTrans._nAddr, stTrans._pStatusData, bLastPlane);
            if(bLastPlane == false)
            {
                _vctPaneIdx[nBusId]++;
            }
        }
        else
        {
            // manual mode
            nRet |= _stageBuilder.EraseNxBlock(stagePacket, stTrans._nAddr, stTrans._pStatusData, stTrans._bLastPlane);
        }

        if(nRet == NAND_SUCCESS)
        {
            _vctCommandChains[nBusId].push_back(stagePacket);
            _vctEraseReqStat[nLunId][nBusId]++;
        }
        break;
    }

    if(nRet == NAND_SUCCESS)
    {
        _vctnPrevTransOp[nBusId] = stTrans._nTransOp;
        if(stTrans._nTransOp != NAND_OP_READ_CACHE)
        {
            invalidateOpenAddr(nBusId);
        }
        // patch the first stage from stage chain with delta time.
        Update(ZERO_TIME);

    }
 
    _vctnPrevNandCmd[nBusId]    = stagePacket._nCommand;
    _vctnPrevTransOp[nBusId]    = stTrans._nTransOp;

    return nRet;
}

NandController::~NandController()
{
    delete _pIsr;
}

void NandController::ReportPerformance()
{
    using namespace std;
    for(UINT16 nLunIdx = 0; nLunIdx < _stDevConfig._nNumsLun; nLunIdx++)
    {
        UINT64 nAccumluatedTraffic  = _vctLuns[nLunIdx].AccumulatedTraffic();

        cout << "LUN ID [" << nLunIdx << "] *******************************************************"<<endl;
        float   nBandwidth = (nAccumluatedTraffic != 0) ? ((float)nAccumluatedTraffic / ((float)_nCurrentTime / 1000000.0f)) : 0;
        UINT32  nReadPgCnt = 0, nWritePgCnt = 0, nEraseBlkCnt = 0;
        for(UINT8 nDieIdx = 0; nDieIdx < _stDevConfig._nNumsDie; nDieIdx++)
        {
            nReadPgCnt  += _vctReadReqStat[nLunIdx][nDieIdx];   
            nWritePgCnt += _vctWriteReqStat[nLunIdx][nDieIdx];
            nEraseBlkCnt+= _vctEraseReqStat[nLunIdx][nDieIdx];
        }

        cout << "System cycles                                :" << dec << _nCurrentTime << endl;
        cout << "System idle cycles                           :" << dec << _vctLunLevelHostIdleTime[nLunIdx] << endl;
        cout << "System working cycles                        :" << dec << _nCurrentTime - _vctLunLevelHostIdleTime[nLunIdx] << endl;
        cout << "The number of page read request              :" << nReadPgCnt << endl;
        cout << "The number of page write request             :" << nWritePgCnt << endl;
        cout << "The number of block erase request            :" << nEraseBlkCnt << endl;
        cout << "IOPS - including idle time                   :" << dec << (nReadPgCnt + nWritePgCnt + nEraseBlkCnt) / ((float)_nCurrentTime / 1000000000.0) << endl;
        cout << "Throughput(KB/s) - including idle time       :" << nBandwidth << endl;
        nBandwidth = (nAccumluatedTraffic != 0) ? ((float)nAccumluatedTraffic / ((float)(_nCurrentTime - _vctLunLevelHostIdleTime[nLunIdx] - TickOverTime()) / 1000000.0f)) : 0;
        cout << "LUN resource utilization (%)                 :" <<  (((float)(_nCurrentTime - _vctLunLevelHostIdleTime[nLunIdx])*100) / (float)_nCurrentTime) << endl;
        cout << "LUN resource utilization including idle (%)  :" <<  (((float)(_nCurrentTime - _vctLunLevelHostIdleTime[nLunIdx] - TickOverTime())*100) / (float)_nCurrentTime) << endl;
        cout << "The amount of requests (Bytes)               :" << dec <<  nAccumluatedTraffic << endl << endl;        
        

        // printing bandwidth for each die
        for(UINT8 nDieIdx = 0; nDieIdx < _stDevConfig._nNumsDie; nDieIdx++)
        {
            UINT64 nAccumulatedCycles   =0;
            if(_vctLuns[nLunIdx].GetRequestTraffic(nDieIdx) != 0)
            {
                for(UINT16 nIter = 0; nIter < NAND_FSM_MAX; ++nIter)
                {
                    nAccumulatedCycles += _vctLuns[nLunIdx].GetAccumulatedFSMTime((NAND_FSM_STATE)nIter, nDieIdx); 
                }
            }
            float nBandwidth    = (_vctLuns[nLunIdx].GetRequestTraffic(nDieIdx) != 0) ? _vctLuns[nLunIdx].GetRequestTraffic(nDieIdx) / ((float)nAccumulatedCycles/1000000) : 0  ;
            float nUtil         = (((float)(_nCurrentTime - _vctDieLevelHostClockIdleTime[nLunIdx][nDieIdx])*100) / (float)_nCurrentTime);
            cout << "Die ID                                       :" << (UINT32)nDieIdx << endl;
            cout << "Die the number of page read request          :" << _vctReadReqStat[nLunIdx][nDieIdx] << endl;
            cout << "Die the number of page write request         :" << _vctWriteReqStat[nLunIdx][nDieIdx] << endl;
            cout << "Die the number of block erase request        :" << _vctEraseReqStat[nLunIdx][nDieIdx] << endl;
            cout << "Die IOPS                                     :" << (_vctReadReqStat[nLunIdx][nDieIdx] + _vctWriteReqStat[nLunIdx][nDieIdx] + _vctEraseReqStat[nLunIdx][nDieIdx]) / ((float)nAccumulatedCycles / 1000000000.0)<< endl; 
            cout << "Die bandwidth (KB/s)                         :" << nBandwidth << endl; 
            cout << "Die working cycle                            :" << dec << nAccumulatedCycles << endl; 
            cout << "Die system cycle                             :" << dec << _nCurrentTime << endl; 
            cout << "Die I/O traffic (Bytes)                      :" << dec << _vctLuns[nLunIdx].GetRequestTraffic(nDieIdx) << endl;
            cout << "Die NAND Clock Idle Time                     :" << _vctDieLevelNandClockIdleTime[nLunIdx][nDieIdx] << endl;
            cout << "Die Host Clock Idle Time                     :" << _vctDieLevelHostClockIdleTime[nLunIdx][nDieIdx] << endl;
			cout << "Die idle fraction (%)                        :" << ((float)_vctDieLevelHostClockIdleTime[nLunIdx][nDieIdx] * 100) / (float) _nCurrentTime << endl;
            cout << "Die resource utilization (%)                 :" <<  nUtil << endl;
            cout << "Die resource contention time                 :" << _vctResourceContentionTime[nLunIdx][nDieIdx] << endl;
            cout << "Die resource contention ratio (%)            :" << ((float)_vctResourceContentionTime[nLunIdx][nDieIdx] * 100) / (float) _nCurrentTime << endl;
			cout << endl;
        }
    }
}

void NandController::ReportStatistics()
{
    for(UINT16 nLunIdx =0; nLunIdx <_stDevConfig._nNumsLun; nLunIdx++)
    {
        _vctLuns[nLunIdx].ReportTimePerEachState();
        _vctLuns[nLunIdx].ReportPowerTimePerEachDcParam();
    }
}

void NandController::HardReset( UINT32 nSystemClock, NandDeviceConfig &stDevConfig )
{
    _stDevConfig        = stDevConfig ;
    UINT32 nMaxBuses    = _stDevConfig._nNumsLun * _stDevConfig._nNumsDie;
    _nCurrentTime       = nSystemClock;
    _nFineGrainTransId  = 0;
    _nMinNextActivate   = 0;
    _nBubbleTime        = 0;
    _nIdleTime          = 0;

    _stageBuilder.SetDeviceConfig(_stDevConfig);
 
    for(UINT32 nBusId = 0; nBusId < nMaxBuses; nBusId++)
    {
        _vctCommandChains[nBusId].clear();
        _vctAddressedNxPacket[nBusId].clear();
        _vctnPrevNandCmd[nBusId]    = NAND_CMD_NOT_DETERMINED;
        _vctnPrevTransOp[nBusId]    = NAND_OP_NOT_DETERMINED;
        _vctTransCompletion[nBusId] = false;
        _vctOpenAddress[nBusId]     =  NULL_SIG(UINT32);
        _vctPaneIdx[nBusId]         = NULL_SIG(UINT16);
    }

    for(UINT16  nLunIdx = 0; nLunIdx < _stDevConfig._nNumsLun; nLunIdx++)
    {
        for(UINT16 nDieIdx = 0; nDieIdx < _stDevConfig._nNumsDie; nDieIdx++)
        {
            _vctDieLevelNandClockIdleTime[nLunIdx][nDieIdx] = 0;
            _vctDieLevelHostClockIdleTime[nLunIdx][nDieIdx] = 0;
            _vctResourceContentionTime[nLunIdx][nDieIdx] = 0;
            _vctReadReqStat[nLunIdx][nDieIdx] = 0;
            _vctWriteReqStat[nLunIdx][nDieIdx] = 0;
            _vctEraseReqStat[nLunIdx][nDieIdx] = 0;
        }

        _vctLuns[nLunIdx].HardReset(nSystemClock, _stDevConfig);
        _vctLuns[nLunIdx].ID(nLunIdx);
    }
}

UINT64 NandController::GetActiveBusTime( UINT32 nLunId, UINT32 nDieId )
{    
    UINT64 nBusTime = _vctLuns[nLunId].GetAccumulatedFSMTime(NAND_FSM_ALE, nDieId);
    nBusTime += _vctLuns[nLunId].GetAccumulatedFSMTime(NAND_FSM_CLE, nDieId);
    nBusTime += _vctLuns[nLunId].GetAccumulatedFSMTime(NAND_FSM_TIR, nDieId);
    nBusTime += _vctLuns[nLunId].GetAccumulatedFSMTime(NAND_FSM_TOR, nDieId);
    return nBusTime;
}
} 