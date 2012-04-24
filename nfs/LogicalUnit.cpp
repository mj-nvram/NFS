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
#include "NandLogger.h"
#include <iomanip>

namespace NANDFlashSim {

LogicalUnit::LogicalUnit(UINT64 nSystemClock, NandDeviceConfig &stDevConfig) : 
    _nTransactionBusDepth(stDevConfig._nTransBusDepth),
    _vctNandBus(stDevConfig._nNumsDie),
    _vctRequestTraffic(stDevConfig._nNumsDie, 0),
    _vctIoCompletion(stDevConfig._nNumsDie, false),
    _vctNeedforCallback(stDevConfig._nNumsDie, false),
    _vctFirstArrivalCycleForInitialCommand(stDevConfig._nNumsDie, NULL_SIG(UINT64)),
    _vctCurHostClockIdleTime(stDevConfig._nNumsDie, 0)
{
    _nCurrentTime                   = nSystemClock;
    _nMinNextActivate               = 0;
    _bBusy                          = false;
    _nIoBusOwnerDieId               = NULL_SIG(UINT16);

    for (UINT32 nDieIdx = 0; nDieIdx < stDevConfig._nNumsDie; nDieIdx++)
    {
        _vctDies.push_back(Die(nSystemClock, stDevConfig));
    }
}

NV_RET LogicalUnit::IssueNandStage( NandStagePacket &stPacket )
{
    UINT8   nDieAddr    = NAND_DIE_PARSE_REGISTER(stPacket._nRow);
    NV_RET  nResult     = NAND_SUCCESS;
    switch(stPacket._nCommand)
    {
    case NAND_CMD_RESET :
        _vctNandBus[nDieAddr].clear();
        break;
    default :
        if(_vctNandBus[nDieAddr].size() >= _nTransactionBusDepth)
        {
            // bus for the destination die is already busy.
            nResult = NAND_DIE_ERROR_BUSY;
        }
    }

    if(nResult != NAND_DIE_ERROR_BUSY)
    {
        if(stPacket._nRandomBytes != NULL_SIG(UINT32) &&
           stPacket._nCommand != NAND_CMD_READ_MULTIPLANE_INIT &&
           stPacket._nCommand != NAND_CMD_READ_MULTIPLANE_INIT_FIN && 
           stPacket._nCommand != NAND_CMD_READ_CACHE_ADDR_INIT)
        {
            _vctRequestTraffic[nDieAddr]    += stPacket._nRandomBytes;
        }
        // delta time
        _vctNandBus[nDieAddr].push_back(stPacket);
        Update(ZERO_TIME);
    }
    return nResult;
}
 
void LogicalUnit::Update(UINT64 nTime)
{
    size_t   nDieIdx   = 0;

    _nCurrentTime     += nTime;
    _nMinNextActivate = NULL_SIG(UINT64);

    const UINT32 nClockPeriods = NFS_GET_PARAM(ISV_CLOCK_PERIODS);
    assert(nClockPeriods != 0);

    UINT64 nAdjustTime;
    UINT64 nGivenTime;

    bool   bTransitFailed = false;

    // update each die states
    for(std::vector<Die>::iterator iterDie = _vctDies.begin(); iterDie != _vctDies.end(); ++iterDie)
    {
        _vctCurHostClockIdleTime[nDieIdx] = 0;
        if(nTime != ZERO_TIME)
        {
            nAdjustTime = iterDie->NextActivate();
            if(nAdjustTime % nClockPeriods)
                nAdjustTime += (nClockPeriods - (nAdjustTime % nClockPeriods));

            nGivenTime = nTime;
            if(nGivenTime > nAdjustTime) 
            {
                _vctCurHostClockIdleTime[nDieIdx] = (nGivenTime - nAdjustTime);
                nGivenTime = nAdjustTime;
            }
            
            iterDie->Update(nGivenTime);
        }

        // processing callback
        if(iterDie->IsFree() == true  && _vctIoCompletion[nDieIdx] == true)
        {
            NandIoCompletion    *pIoCallback       = NULL;/*= _pParentSystem->GetIoCompletion();*/
            NandStagePacket      completeDataPacket = _vctNandBus[nDieIdx].front();

            if(_vctNeedforCallback[nDieIdx])
            {
                UINT64 nArrivalCycle    = completeDataPacket._nArrivalCycle;
                if(_vctFirstArrivalCycleForInitialCommand[nDieIdx] != NULL_SIG(UINT64))
                {
                    // initiation commands has no callback
                    nArrivalCycle           = _vctFirstArrivalCycleForInitialCommand[nDieIdx];
                    _vctFirstArrivalCycleForInitialCommand[nDieIdx]   = NULL_SIG(UINT64);
                }
                REPORT_NAND(NANDLOG_SNOOP_IOCOMPLETION, _nId << " , " << completeDataPacket._nStageId << " , " << nArrivalCycle << " , "<< _nCurrentTime << " , " << _nCurrentTime - nArrivalCycle );
                if( pIoCallback != NULL)
                {
                    (*pIoCallback)(completeDataPacket._nStageId, nArrivalCycle, _nCurrentTime);
                }
                _vctNeedforCallback[nDieIdx] = false;
            }

            // clean up for active transaction packet from bus.
            _vctNandBus[nDieIdx].pop_front();
            _vctIoCompletion[nDieIdx] = false;
        }        
        
        // update IoBus lock state
        if((iterDie->CheckRb() != true && _vctNandBus[nDieIdx].empty() != true) || _vctNandBus[nDieIdx].empty() == true)
        {
            releaseIobus(nDieIdx);
        }

        // this die waits for commands
        transitStage((UINT8)nDieIdx, bTransitFailed);

        if(iterDie->NextActivate() > 0)
        {
            _nMinNextActivate = (_nMinNextActivate > iterDie->NextActivate()) ? iterDie->NextActivate() : _nMinNextActivate;
        }

        nDieIdx++;
    }

    // About bus arbitration.
    if(bTransitFailed && getBusOwnerDieId() == NULL_SIG(UINT16))
    {
        bool bTransitDone = false;

        nDieIdx = 0;
        for(std::vector<Die>::iterator iterDie = _vctDies.begin(); iterDie != _vctDies.end(); ++iterDie)
        {
            if(bTransitDone == false) 
                bTransitDone = transitStage((UINT8)nDieIdx, bTransitFailed);

            if(iterDie->NextActivate() > 0)
            {
                _nMinNextActivate = (_nMinNextActivate > iterDie->NextActivate()) ? iterDie->NextActivate() : _nMinNextActivate;
            }

            nDieIdx++;
        }
    }

    if(_nMinNextActivate == NULL_SIG(UINT64)) _nMinNextActivate = 0;
}

bool LogicalUnit::transitStage(UINT8 nDieIdx, bool &bTransitFailed)
{
    if(_vctDies[nDieIdx].CheckFsmBusy() == false && _vctNandBus[nDieIdx].empty() != true)
    {
        NAND_STAGE nNextExpectedState = _vctDies[nDieIdx].ExpectedNextStage();

        // issuing next transaction to appropriate dies.
        if(nNextExpectedState == NAND_STAGE_IDLE)
        {
            if (getBusOwnerDieId() == NULL_SIG(UINT16))
            {
                acquireIoBus(nDieIdx);
                // scheduled packet won't be removed from NAND bus until the transaction is completed.
                NandStagePacket &scheduledPacket     = _vctNandBus[nDieIdx].front();

                NAND_STAGE      nNandStage  = NAND_STAGE_CLE;
                if (scheduledPacket._nCommand   == NAND_CMD_READ_STATUS)
                {
                    nNandStage  = NAND_STAGE_READ_STATUS;
                }
                else if ((scheduledPacket._nCommand == NAND_CMD_READ_MULTIPLANE_INIT 
                    || scheduledPacket._nCommand == NAND_CMD_READ_INTERNAL
                    || scheduledPacket._nCommand == NAND_CMD_READ_INTERNAL_MULTIPLANE
                    || scheduledPacket._nCommand == NAND_CMD_READ_INTERNAL_MULTIPLANE_FIN
                    || scheduledPacket._nCommand == NAND_CMD_BLOCK_MULTIPLANE_ERASE
                    || scheduledPacket._nCommand == NAND_CMD_PROG_RANDOM
                    || scheduledPacket._nCommand == NAND_CMD_PROG_MULTIPLANE
                    || scheduledPacket._nCommand == NAND_CMD_PROG_MULTIPLANE_RANDOM)
                    && _vctFirstArrivalCycleForInitialCommand[nDieIdx] == NULL_SIG(UINT64))
                {
                    // for callback
                    _vctFirstArrivalCycleForInitialCommand[nDieIdx]   = scheduledPacket._nArrivalCycle;
                }

                // issue new state for current transaction
                _vctDies[nDieIdx].TransitStage(nNandStage, scheduledPacket);
                return true;
            }
            else 
                bTransitFailed = true;
        }
        else if(nNextExpectedState != NAND_STAGE_NOT_DETERMINED)
        {
            // die is not busy, but there is an expected next step and active transaction
            // this routine handles NAND I/O stage chain for a request which was already issued.
            // ;thus, there is no need to acquire I/O bus
            NandStagePacket &scheduledPacket     = _vctNandBus[nDieIdx].front();

            if(nNextExpectedState == NAND_STAGE_CLE)
            {
                // this will be right after address latch
                // transiting command chain
                scheduledPacket._nCommand = getConfirmCommand(scheduledPacket._nCommand);
            }

            bool bPermissionToTransit   = true;
            // if the next state is kind of stage using NAND bus, it requires to acquire lock
            if(nNextExpectedState < NAND_DELIMITER_STAGE_FOR_NAND_ARRAY_TASK)
            {
                if(getBusOwnerDieId() != nDieIdx)
                {
                    acquireIoBus(nDieIdx);
                    if(getBusOwnerDieId() != nDieIdx)
                    {
                        bPermissionToTransit = false;
                        bTransitFailed = true;
                    }
                }
            }
            else 
            {
                releaseIobus(nDieIdx);
            }

            if(bPermissionToTransit)
            {
                if(_vctDies[nDieIdx].TransitStage(nNextExpectedState, scheduledPacket) == NAND_STAGE_IDLE)
                {
                    if(scheduledPacket._bLastCmdForFgTrans == true)
                    {
                        _vctNeedforCallback[nDieIdx] = true;
                    }
                    _vctIoCompletion[nDieIdx] = true;
                }
                return true;
            }
        }
    }

    return false;
}

bool LogicalUnit::CheckBusy( UINT8 nDie )
{
    bool bBusy  = false;
    if(nDie == NULL_SIG(UINT8))
    {
        for (nDie =0; nDie < _vctDies.size(); ++nDie)
        {
            if(_vctDies[nDie].CheckFsmBusy() == true || _vctNandBus[nDie].empty() == false)
            {
                 bBusy = true;
                 break;
            }
        }
    }
    else if (_vctDies[nDie].CheckFsmBusy() == true || _vctNandBus[nDie].empty() == false)
    {
        bBusy =  true;
    }

    return bBusy;
}

bool LogicalUnit::IsDieIdle(UINT8 nDie)
{
    if(nDie >= _vctDies.size()) return false;
    return !_vctDies[nDie].CheckFsmBusy();
}

void LogicalUnit::ID( UINT32 val )
{
    _nId = val;

    UINT8 nDie = 0;
    for (std::vector<Die>::iterator iterDie = _vctDies.begin(); iterDie != _vctDies.end(); ++iterDie)
    {
        iterDie->ID((val * _vctDies.size()) + nDie);
        ++nDie;
    }
}


UINT64 LogicalUnit::AccumulatedTraffic()
{
    UINT32  nDie = 0;
    UINT64  nAccumulatedTraffic = 0;
    // printing bandwidth for each die
    for (std::vector<Die>::iterator iterDie = _vctDies.begin(); iterDie != _vctDies.end(); ++iterDie)
    {
        nAccumulatedTraffic += _vctRequestTraffic[nDie];
        nDie++;
    }

    return nAccumulatedTraffic;
}



void LogicalUnit::ReportBandwidth()
{
    using namespace std;
    UINT32  nDie = 0;
    // printing bandwidth for each die
    for (std::vector<Die>::iterator iterDie = _vctDies.begin(); iterDie != _vctDies.end(); ++iterDie)
    {
        UINT64 nAccumulatedCycles   =0;
        if(_vctRequestTraffic[nDie] != 0)
        {
            for(UINT16 nIter = 0; nIter < NAND_FSM_MAX; ++nIter)
            {
                nAccumulatedCycles += iterDie->GetAccumulatedFSMTime((NAND_FSM_STATE)nIter); 
            }
        }
        float nBandwidth = (_vctRequestTraffic[nDie] != 0) ? _vctRequestTraffic[nDie] / ((float)nAccumulatedCycles/1000000) : 0  ;

        cout << "Die ID                          :" << nDie << endl;
        cout << "Die bandwidth (KB/s)            :" << nBandwidth << endl; 
        cout << "Die cycles                      :" << dec << nAccumulatedCycles << endl; 
        cout << "Die I/O traffic (Bytes)         :" << dec << _vctRequestTraffic[nDie] << endl;
        nDie++;
    }
}



void   LogicalUnit::ReportTimePerEachState()
{
    for (std::vector<Die>::iterator iterDie = _vctDies.begin(); iterDie != _vctDies.end(); ++iterDie)
    {
        for(UINT16 nIter = 0; nIter < NAND_FSM_MAX; ++nIter)
        {
            REPORT_NAND(NANDLOG_TOTAL_CYCLE_FOR_EACH_STATE, _nId << " , " << iterDie->ID() << " , " << nIter << " , " << iterDie->GetAccumulatedFSMTime((NAND_FSM_STATE)nIter));
        }
    }
}

void LogicalUnit::ReportPowerTimePerEachDcParam()
{
    for (std::vector<Die>::iterator iterDie = _vctDies.begin(); iterDie != _vctDies.end(); ++iterDie)
    {
        for(UINT16 nIter = 0; nIter < NAND_DC_MAX; ++nIter)
        {
            UINT64 nPowerClock      = iterDie->GetPowerTime((NAND_DC)nIter);
            float  nCurrent         = -1;

            switch ((NAND_DC) nIter)
            {
            case NAND_DC_READ :
                nCurrent    = ((float)nPowerClock/1000) * NFS_GET_PARAM(IDV_ICC1);
                break;
            case NAND_DC_PROG :
                nCurrent = ((float)nPowerClock/1000) * NFS_GET_PARAM(IDV_ICC2);
                break;
            case NAND_DC_ERASE :
                nCurrent = ((float)nPowerClock/1000) * NFS_GET_PARAM(IDV_ICC3);
                break;
            case NAND_DC_STANDBY :
                nCurrent = ((float)nPowerClock/1000) * (NFS_GET_PARAM(IDV_ISB1) + NFS_GET_PARAM(IDV_ISB2));
                break;
            case NAND_DC_LEAKAGE :
                nCurrent = ((float)nPowerClock/1000) * (NFS_GET_PARAM(IDV_ILI) + NFS_GET_PARAM(IDV_ILO));
                break;
            }
            assert(nCurrent != -1);

            float nPower      = nCurrent * NFS_GET_PARAM(IDV_VCC) / 1000;
            //    "Controller ID , Die ID, DC Param , Cycles, Current, Power(uA)" 
            REPORT_NAND(NANDLOG_TOTAL_POWERCYCLE_FOR_EACH_DCPARAM, _nId << " , " << iterDie->ID() << " , " << nIter << " , " << nPowerClock << " , " << std::setprecision(24) <<  nCurrent << " , " << nPower);
        }
    }
}


NAND_COMMAND LogicalUnit::getConfirmCommand(NAND_COMMAND nCommand)
{
    NAND_COMMAND nConfirm = NAND_CMD_NOT_DETERMINED;
    switch(nCommand)
    {
    case NAND_CMD_READ_PAGE :
        nConfirm   = NAND_CMD_READ_PAGE_CONF;
        break;
    case NAND_CMD_PROG_PAGE :
        nConfirm   = NAND_CMD_PROG_PAGE_CONF;
        break;
    case NAND_CMD_PROG_CACHE :
        nConfirm   = NAND_CMD_PROG_CACHE_CONF;
        break;
    case NAND_CMD_READ_CACHE_ADDR_INIT :
        nConfirm   = NAND_CMD_READ_CACHE_ADDR_INIT_CONF;
        break;
    case NAND_CMD_READ_MULTIPLANE_INIT :
        nConfirm   = NAND_CMD_READ_MULTIPLANE_INIT_CONF;
        break;
    case NAND_CMD_READ_MULTIPLANE_INIT_FIN :
        nConfirm   = NAND_CMD_READ_MULTIPLANE_INIT_FIN_CONF;
        break;
    case NAND_CMD_READ_MULTIPLANE :
        nConfirm   = NAND_CMD_READ_MULTIPLANE_CONF;
        break;
    case NAND_CMD_PROG_MULTIPLANE :
        nConfirm   = NAND_CMD_PROG_MULTIPLANE_CONF;
        break;
    case NAND_CMD_PROG_MULTIPLANE_FIN :
        nConfirm   = NAND_CMD_PROG_MULTIPLANE_FIN_CONF;
        break;
    case NAND_CMD_PROG_MULTIPLANE_CACHE :
        nConfirm   = NAND_CMD_PROG_MULTIPLANE_CACHE_CONF;
        break;
    case NAND_CMD_PROG_MULTIPLANE_CACHE_FIN :
        nConfirm   = NAND_CMD_PROG_MULTIPLANE_CACHE_FIN_CONF;
        break;
    case NAND_CMD_READ_INTERNAL :
        nConfirm   = NAND_CMD_READ_INTERNAL_CONF;
        break;
    case NAND_CMD_PROG_INTERNAL:
        nConfirm   = NAND_CMD_PROG_INTERNAL_CONF;
        break;
    case NAND_CMD_READ_INTERNAL_MULTIPLANE_FIN :
        nConfirm   = NAND_CMD_READ_INTERNAL_MULTIPLANE_FIN_CONF;
        break;
    case NAND_CMD_PROG_INTERNAL_MULTIPLANE:
        nConfirm   = NAND_CMD_PROG_INTERNAL_MULTIPLANE_CONF;
        break;
    case NAND_CMD_PROG_INTERNAL_MULTIPLANE_FIN:
        nConfirm   = NAND_CMD_PROG_INTERNAL_MULTIPLANE_FIN_CONF;
        break;
    case NAND_CMD_BLOCK_ERASE:
        nConfirm   = NAND_CMD_BLOCK_ERASE_CONF;
        break;
    case NAND_CMD_BLOCK_MULTIPLANE_ERASE_FIN:
        nConfirm   = NAND_CMD_BLOCK_MULTIPLANE_ERASE_FIN_CONF;
        break;
    case NAND_CMD_PROG_RANDOM_FIN:
        nConfirm   = NAND_CMD_PROG_RANDOM_FIN_CONF;
        break;
    case NAND_CMD_READ_RANDOM :
        nConfirm   = NAND_CMD_READ_RANDOM_CONF;
        break;
    case NAND_CMD_PROG_MULTIPLANE_RANDOM_DUMMY:
        nConfirm   = NAND_CMD_PROG_MULTIPLANE_RANDOM_DUMMY_CONF;
        break;
    case NAND_CMD_PROG_MULTIPLANE_FIN_RANDOM:
        nConfirm   = NAND_CMD_PROG_MULTIPLANE_FIN_RANDOM_CONF;
        break;
    }

    return nConfirm;
}

void LogicalUnit::HardReset( UINT32 nSystemClock, NandDeviceConfig &stDevConfig )
{
    _nCurrentTime                   = nSystemClock;
    _nMinNextActivate               = 0;
    _bBusy                          = false;
    _nIoBusOwnerDieId               = NULL_SIG(UINT16);
    _nTransactionBusDepth           = stDevConfig._nTransBusDepth; 

    for (UINT32 nDieId = 0; nDieId < stDevConfig._nNumsDie; nDieId++)
    {
        _vctCurHostClockIdleTime[nDieId]  = 0;
        _vctRequestTraffic[nDieId]      = 0;
        _vctIoCompletion[nDieId]        = false;
        _vctNeedforCallback[nDieId]     = false;
        _vctFirstArrivalCycleForInitialCommand[nDieId] = NULL_SIG(UINT64);
        _vctNandBus[nDieId].clear();
        _vctDies[nDieId].HardReset(nSystemClock, stDevConfig);
    }
}

}