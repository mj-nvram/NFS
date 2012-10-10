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
	created:	6:8:2011   23:21
	file base:	NandFlashSystem
	file ext:	cpp
	author:		Myoungsoo Jung
				Pennsylvania State University, MDL Lab
	
	purpose:	
*********************************************************************/

#include "TypeSystem.h"
#include "Tools.h"
#include "NandLogger.h"
#include "ParamManager.h"
#include "IoCompletion.h"
#include "Plane.h"
#include "Die.h"
#include "LogicalUnit.h"
#include "NandStageBuilderTool.h"
#include "NandController.h"
#include "NandFlashSystem.h"

namespace NANDFlashSim {

NandFlashSystem::NandFlashSystem( UINT64 nSystemClock, NandDeviceConfig &stConfig, NandIoCompletion * pCallback) :_vctIncomingTrans(stConfig._nNumsLun * stConfig._nNumsDie),
            _controller(nSystemClock, stConfig),
            _pHostCallback(pCallback)
            
{
    _nCurrentTime           = nSystemClock;
    _stDevConfig            = stConfig;

    NandSystemIsr *pIsr = new CallbackRepository<NandFlashSystem, void, NAND_ISR_TYPE, UINT32, UINT64>(this, &NandFlashSystem::InterruptService);
    _controller.SetSystemIsr(pIsr);
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    AddTransaction
// FullName:  NandFlashSystem::AddTransaction
// Access:    public 
// Returns:   NV_RET
// Parameter: NAND_TRANS_OP nTransOp
// Parameter: UINT32 nSrcAddr
// Parameter: UINT32 nDestAddr
//
// Descriptions -
// The API for internal date move operation
//////////////////////////////////////////////////////////////////////////////
NV_RET NandFlashSystem::AddTransaction( UINT32 nHostTransId, NAND_TRANS_OP nTransOp, UINT32 nSrcAddr, UINT32 nDestAddr)
{
    Transaction         nandTrans;

    nandTrans._nHostTransId     = nHostTransId;
    nandTrans._nTransOp         = nTransOp;
    nandTrans._nAddr            = nSrcAddr;
    nandTrans._nDestAddr        = nDestAddr;
    nandTrans._nByteOff         = 0;
    nandTrans._nNumsByte        = _stDevConfig._nPgSize;

    return AddTransaction(nandTrans);
}



NV_RET NandFlashSystem::AddTransaction( UINT32 nHostTransId, NAND_TRANS_OP nTransOp, UINT32 nAddr)
{
    NV_RET  nRet    = NAND_SUCCESS;
    bool    bBusy   = false;
    UINT16  nLun    = 0;
	UINT16	nDieId	= NFS_PARSE_DIE_ADDR(nAddr, _stDevConfig._bits);
    UINT32  nBusId  = nLun * nDieId + nDieId;

    if (nTransOp == NAND_OP_PROG_MULTIPLANE_CACHE || 
        nTransOp == NAND_OP_PROG_MULTIPLANE_RANDOM )
    {
        // For using these operations, a host model have to leverage AddTransaction function with lastRequest flag or with transaction data structure.
        // this is because multi-plane cache mode program needs a tag for distinguish the last I/O requests.
        // The reason why I do not exploit c++ default parameter for lastRequest, I want to remind a caller that cache and random mode programming
        // need the last request flag.

        return NAND_FLASH_ERROR_UNSUPPORTED;
    }

    if(nTransOp == NAND_OP_INTERNAL_DATAMOVEMENT || 
       nTransOp == NAND_OP_INTERNAL_DATAMOVEMENT_MULTIPLANE)
    {
        return NAND_FLASH_ERROR_UNSUPPORTED;
    }


    if (_vctIncomingTrans[nBusId]._nTransOp != NAND_OP_NOT_DETERMINED)
    {
        if(nTransOp == NAND_OP_READ || nTransOp == NAND_OP_PROG )
        {
            // single plane operation
            bBusy   = true;
        }
        else if(_vctIncomingTrans[nBusId]._nTransOp != nTransOp)
        {
            // Nx or cache mode operation can be issued in multiple times.
            // This situation that operation of the transaction is not same to the one in the incoming traction bus means 
            // a host model commits command which is not related to previous one.
            // Thus, the host model have to wait until the transaction in the incoming traction bus are released.
            //
            // Note that AddTransaction interface doesn't manage correctness for NAND command sequence.
            // If the host continues to insist committing a wrong command, it will be detected by FSM of dies associated to the die,
            // and it will report error status by specific return code.
            bBusy = true;
        }
    }

    if(bBusy == false)
    {
        Transaction nandTrans;
        nandTrans._nHostTransId = nHostTransId;
        nandTrans._nTransOp     = nTransOp;
        nandTrans._nAddr        = nAddr;
        nandTrans._nByteOff     = 0;
        nandTrans._nNumsByte    = _stDevConfig._nPgSize;

        nRet |= _controller.BuildandAddStage(nandTrans);
        
        if(nRet == NAND_SUCCESS)
        {
            _vctIncomingTrans[nBusId] = nandTrans;
        }
    }
    else
    {
        nRet |= NAND_FLASH_ERROR_BUSY;
    }

    return nRet;
}


NV_RET NandFlashSystem::AddTransaction( Transaction &nandTrans )
{
    NV_RET  nRet    = NAND_SUCCESS;
    bool    bBusy   = false;
    UINT16  nLun    = 0;
	UINT16  nDieId  = NFS_PARSE_DIE_ADDR(nandTrans._nAddr, _stDevConfig._bits);

	UINT32  nBusId  = nLun * nDieId + nDieId;


    if (_vctIncomingTrans[nBusId]._nTransOp != NAND_OP_NOT_DETERMINED)
    {
        if(nandTrans._nTransOp == NAND_OP_READ || nandTrans._nTransOp == NAND_OP_PROG )
        {
            // single plane operation
            bBusy   = true;
        }
        else if(_vctIncomingTrans[nBusId]._nTransOp != nandTrans._nTransOp)
        {
            // Nx or cache mode operation can be issued in multiple times.
            // see NandFlashSystem::AddTransaction( NAND_TRANS_OP nTransOp, UINT32 nAddr ) comments
            bBusy = true;
        }
    }

    if(bBusy == false)
    {
        nRet |= _controller.BuildandAddStage(nandTrans);
        if(nRet == NAND_SUCCESS)
        {
            _vctIncomingTrans[nBusId] = nandTrans;
        }
    }
    else
    {
        nRet |= NAND_FLASH_ERROR_BUSY;
    }

    return nRet;
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    AddTransaction
// FullName:  NandFlashSystem::AddTransaction
// Access:    public 
// Returns:   NV_RET
// Parameter: NAND_TRANS_OP nTransOp
// Parameter: UINT32 nAddr
// Parameter: bool bLastRequest
//
// Descriptions -
// This function is used for NAND_OP_PROG_MULTIPLANE_CACHE or NAND_OP_PROG_MULTIPLANE_RANDOM
//////////////////////////////////////////////////////////////////////////////
NV_RET NandFlashSystem::AddTransaction( UINT32 nHostTransId, NAND_TRANS_OP nTransOp, UINT32 nAddr, bool bLastRequest)
{
    Transaction         nandTrans;
    
    nandTrans._nHostTransId     = nHostTransId;
    nandTrans._nTransOp         = nTransOp;
    nandTrans._nAddr            = nAddr;
    nandTrans._nByteOff         = 0;
    nandTrans._nNumsByte        = _stDevConfig._nPgSize;
    nandTrans._bLastNxSubTrans  = bLastRequest;

    return AddTransaction(nandTrans);
}


//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    UpdateWithoutIdleCycles
// FullName:  NandFlashSystem::UpdateWithoutIdleCycles
// Access:    public 
// Returns:   void
// Parameter: void
//
// Descriptions -
// Update NAND flash system cycles by skipping meaningless cycle times.
//////////////////////////////////////////////////////////////////////////////
UINT64 NandFlashSystem::UpdateWithoutIdleCycles( void )
{
    UINT64 nMinTime = _controller.MinNextActivity();
    const UINT32 nClockPeriods = NFS_GET_PARAM(ISV_CLOCK_PERIODS);
    assert(nClockPeriods != 0);

    if(nMinTime != 0)
    {
        if(nMinTime % nClockPeriods)
            nMinTime += (nClockPeriods - (nMinTime % nClockPeriods));

        _nCurrentTime += nMinTime;
        _controller.Update(nMinTime);
    }

    return nMinTime / nClockPeriods;
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    DelayUpdate
//
// FullName:  NANDFlashSim::NandFlashSystem::DelayUpdate
// Access:    public 
// Returns:   void
// Parameter: UINT64 nBubbleCycle
//
// Descriptions -
// DelayUpdate will make flash system idle as much as the given nBubbleCycle
//////////////////////////////////////////////////////////////////////////////
void NandFlashSystem::DelayUpdate( UINT64 nBubbleCycle )
{
    _controller.DelayUpdate(nBubbleCycle * NFS_GET_PARAM(ISV_CLOCK_PERIODS));
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    Update
//
// FullName:  NANDFlashSim::NandFlashSystem::Update
// Access:    public 
// Returns:   void
// Parameter: UINT64 nCycles
//
// Descriptions -
// This function feeds cycles to the flash system by the given parameter 
//////////////////////////////////////////////////////////////////////////////
void NandFlashSystem::Update( UINT64 nCycles )
{
    const UINT32 nClockPeriods = NFS_GET_PARAM(ISV_CLOCK_PERIODS);
    assert(nClockPeriods != 0);

    if(nCycles != 0) 
    {
        _nCurrentTime += nCycles * nClockPeriods;
        _controller.Update(nCycles * nClockPeriods);
    }
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    UpdateBackToBack
//
// FullName:  NANDFlashSim::NandFlashSystem::UpdateBackToBack
// Access:    public 
// Returns:   void
// Parameter: UINT64 nCycles
//
// Descriptions -
// This function will update multiple stages which are sitting on NAND I/O bus
// as much as the given nCycles.
// If their is no stage, the given cycles will be counted as idle, and memory
// system will be in idle.
//////////////////////////////////////////////////////////////////////////////
void NandFlashSystem::UpdateBackToBack( UINT64 nCycles )
{
    UINT64 nMinTime = _controller.MinNextActivity();
    const UINT32 nClockPeriods = NFS_GET_PARAM(ISV_CLOCK_PERIODS);
    assert(nClockPeriods != 0);

	const UINT32 MAX_BUS = _stDevConfig._nNumsLun * _stDevConfig._nNumsDie;

    if(nMinTime == 0 )
    {
        for(UINT32 nBusIdx = 0; nBusIdx < MAX_BUS; nBusIdx++)
        {
            if(IsBusy(nBusIdx))
            {
                _controller.Update(ZERO_TIME);
                nMinTime = _controller.MinNextActivity();
                break;
            }
        }

        _controller.TickOver(nCycles * nClockPeriods);
        return;
    }
    
    UINT64 nMinCycles = nMinTime / nClockPeriods;
    if(nMinTime % nClockPeriods) nMinCycles++;

    while(nMinCycles < nCycles)
    {
        _nCurrentTime += nMinCycles * nClockPeriods;
        _controller.Update(nMinCycles * nClockPeriods);

        nCycles -= nMinCycles;

        nMinTime = _controller.MinNextActivity();
        if(nMinTime == 0)
        {
            _controller.TickOver(nCycles * nClockPeriods);
            return;
        }

        nMinCycles = nMinTime / nClockPeriods;
        if(nMinTime % nClockPeriods) nMinCycles++;
    }

    if(nCycles != 0) 
    {
        _nCurrentTime += nCycles * nClockPeriods;
        _controller.Update(nCycles * nClockPeriods);
    }
}


UINT32 NandFlashSystem::BusyDieNums()
{
	const UINT32 MAX_BUS = _stDevConfig._nNumsLun * _stDevConfig._nNumsDie;

    UINT32 nDie =0;
    for(UINT32 nBusIdx = 0; nBusIdx < MAX_BUS; nBusIdx++)
    {
        if (_vctIncomingTrans[nBusIdx]._nTransOp != NAND_OP_NOT_DETERMINED)
        {
            nDie++;
        }   
    }

    return nDie;
}


bool NandFlashSystem::IsBusy(UINT16 nBusId)
{
    if (_vctIncomingTrans[nBusId]._nTransOp != NAND_OP_NOT_DETERMINED)
    {
        return true;
    }   
    
    return false;
}

UINT64 NandFlashSystem::GetCyclesFromTime(UINT64 nTime)
{
    const UINT32 nClockPeriods = NFS_GET_PARAM(ISV_CLOCK_PERIODS);
    assert(nClockPeriods != 0);

    if(nTime != 0)
    {
        if(nTime % nClockPeriods)
            nTime += (nClockPeriods - (nTime % nClockPeriods));
    }

    return nTime / nClockPeriods;
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    IsActiveMode
// FullName:  NandFlashSystem::IsActiveMode
// Access:    public 
// Returns:   bool
// Parameter: void
//
// Descriptions -
// Check NAND flash memory system is on a stage. If there is any die to be activated,
// this function return true. Otherwise return false
//////////////////////////////////////////////////////////////////////////////
bool NandFlashSystem::IsActiveMode( void )
{
    bool bActivate = false;
    for(std::vector<Transaction>::iterator iTrans = _vctIncomingTrans.begin(); iTrans != _vctIncomingTrans.end(); ++iTrans)
    {
        if(iTrans->_nTransOp != NAND_OP_NOT_DETERMINED)
        {
            bActivate = true;
            break;
        }
    }

    return bActivate;
}

void NandFlashSystem::InterruptService( NAND_ISR_TYPE nIsrType, UINT32 nArg1, UINT64 nArg2 )
{
    switch(nIsrType)
    {
    case NAND_ISR_COMPLETE_TRANS :
        assert(nArg1 < _stDevConfig._nNumsLun * _stDevConfig._nNumsDie);
        if(_pHostCallback != NULL)
        {
            (*_pHostCallback)(_vctIncomingTrans[nArg1]._nHostTransId, _stDevConfig._nDeviceId/*_vctIncomingTrans[nArg1]._nTransOp*/, nArg2);
        }
        _vctIncomingTrans[nArg1]._nTransOp  = NAND_OP_NOT_DETERMINED;
        
        break;
    }
}

void NandFlashSystem::ReportStatistics( void )
{
    _controller.ReportPerformance();
    _controller.ReportStatistics();    
}

void NandFlashSystem::HardReset( UINT32 nSystemClock, NandDeviceConfig &stDevConfig )
{
    _stDevConfig            = stDevConfig;
    _nCurrentTime           = nSystemClock;
    for(UINT32 nBusId = 0; nBusId < stDevConfig._nNumsLun * stDevConfig._nNumsDie; nBusId++)
    {
        _vctIncomingTrans[nBusId]    = Transaction();
    }
    _controller.HardReset(nSystemClock, _stDevConfig);
    NandLogger::MarkupHardReset();
}


void NandFlashSystem::ReportConfiguration( void )
{
    using namespace std;

    cout   << "NAND flash device configurations ***********************"<< endl;
    cout   << "Device ID            : " << hex << "0x"<< _stDevConfig._nDeviceId        << endl; 
    cout   << "Page size            : " << _stDevConfig._nPgSize        << endl; 
    cout   << "# of pages per block : " << _stDevConfig._nNumsPgPerBlk  << endl; 
    cout   << "# of blocks          : " << _stDevConfig._nNumsBlk       << endl; 
    cout   << "# of planes          : " << _stDevConfig._nNumsPlane     << endl; 
    cout   << "# of dies            : " << _stDevConfig._nNumsDie     << endl; 
    cout   << "# of I/O pins        : " << _stDevConfig._nNumsIoPins << endl; 
    cout   << "NOP                  : " << _stDevConfig._nNop << endl; 
    cout   << "Max erase count      : " << dec <<_stDevConfig._nEc << endl << endl; 


    cout   << "Foot-print Information  *******************************"<< endl;
    cout   << "Read request history is available      :" << (NFS_GET_ENV(IRV_SNOOP_NAND_PLANE_READ) ? "true" : "false") << endl;
    cout   << "Write request history is available     :" << (NFS_GET_ENV(IRV_SNOOP_NAND_PLANE_WRITE) ? "true" : "false") << endl;
    cout   << "Multi-stage foot-print is available    :" << (NFS_GET_ENV(IRV_SNOOP_INTERNAL_STATE) ? "true" : "false") << endl;
    cout   << "Bus transaction are printed            :" << (NFS_GET_ENV(IRV_SNOOP_BUS_TRANSACTION) ? "true" : "false") << endl;
    cout   << "I/O completion foot-print is available :" << (NFS_GET_ENV(IRV_SNOOP_IO_COMPLETION) ? "true" : "false") << endl;
    cout   << "Analysis of cycles is available        :" << (NFS_GET_ENV(IRV_CYCLES_FOR_EACH_STATE) ? "true" : "false") << endl;
    cout   << "Analysis of power cycles is available  :" << (NFS_GET_ENV(IRV_POWER_CYCLES_FOR_EACH_DCPARAM) ? "true" : "false") << endl << endl;
}

UINT64 NandFlashSystem::GetHostClockIdleTime()
{
    UINT64 nAccumulatedIdle =0;
    for (UINT32 nLunId = 0; nLunId < _stDevConfig._nNumsLun; nLunId++)
    {
        nAccumulatedIdle += _controller.GetHostClockIdleTime(nLunId);
    }
    return nAccumulatedIdle;
}

UINT64 NandFlashSystem::GetResourceContentionTime()
{
    UINT64 nResContentionTime =0;
    for (UINT32 nLunId = 0; nLunId < _stDevConfig._nNumsLun; nLunId++)
    {
        for(UINT32 nDieId = 0; nDieId < _stDevConfig._nNumsDie; nDieId++)
        {
            nResContentionTime += _controller.GetResourceContentionTime(nLunId, nDieId);
        }
    }
    return nResContentionTime;
}

UINT64 NandFlashSystem::GetActivateBusTime()
{
    UINT64 nActiveBusTime =0;
    for (UINT32 nLunId = 0; nLunId < _stDevConfig._nNumsLun; nLunId++)
    {
        for(UINT32 nDieId = 0; nDieId < _stDevConfig._nNumsDie; nDieId++)
        {
            nActiveBusTime += _controller.GetActiveBusTime(nLunId, nDieId);
        }
    }
    return nActiveBusTime;
}

UINT64 NandFlashSystem::GetCellActiveTime()
{
    return _controller.CurrentTime() - GetActivateBusTime() - _controller.TickOverTime();
}

}