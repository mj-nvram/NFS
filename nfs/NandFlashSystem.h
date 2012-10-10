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
	created:	6:8:2011   23:22
	file base:	NandFlashSystem
	file ext:	h
	author:		Myoungsoo Jung
				Pennsylvania State University, MDL Lab
	
	purpose:	
*********************************************************************/


#ifndef _NandFlashSystem_h__
#define _NandFlashSystem_h__

namespace NANDFlashSim {

class NandFlashSystem {
    NandController              _controller;
    UINT64                      _nCurrentTime;
    NandDeviceConfig            _stDevConfig;
    NandIoCompletion            *_pHostCallback;

    std::vector<Transaction>    _vctIncomingTrans;

public :
    NandFlashSystem( UINT64 nSystemClock, NandDeviceConfig &stConfig, NandIoCompletion * pCallback = NULL );

    //////////////////////////////////////////////////////////////////////////
    // memory transaction-based interface.
    //////////////////////////////////////////////////////////////////////////
    NV_RET          AddTransaction( Transaction &nandTrans );
    NV_RET          AddTransaction( UINT32 nHostTransId, NAND_TRANS_OP nTransOp, UINT32 nAddr);
    NV_RET          AddTransaction( UINT32 nHostTransId, NAND_TRANS_OP nTransOp, UINT32 nAddr, bool bLastRequest);
    NV_RET          AddTransaction( UINT32 nHostTransId, NAND_TRANS_OP nTransOp, UINT32 nSrcAddr, UINT32 nDestAddr);

    //////////////////////////////////////////////////////////////////////////
    //
    //////////////////////////////////////////////////////////////////////////
    void            CommitStage(UINT32 &nBusId, NandStagePacket &stagePacket)  {_controller.CommitStage(nBusId, stagePacket);}

    //////////////////////////////////////////////////////////////////////////
    // cycle update interfaces.
    //////////////////////////////////////////////////////////////////////////
    void            Update( UINT64 nCycles );
    void            UpdateBackToBack( UINT64 nCycles );
    UINT64          UpdateWithoutIdleCycles( void );
    void            DelayUpdate( UINT64 nBubbleCycle );


    //////////////////////////////////////////////////////////////////////////
    // interfaces for inquiring NAND flash status.
    //////////////////////////////////////////////////////////////////////////
    bool            IsBusy(UINT16 nBusId);
	bool            IsBusy() { return IsActiveMode(); }
    UINT32          BusyDieNums();
    bool            IsActiveMode( void );
    bool            IsIoBusActive( void ) {return _controller.IsIoBusActive();}

    //////////////////////////////////////////////////////////////////////////
    // API related to attributes.
    //////////////////////////////////////////////////////////////////////////
    UINT64          GetNandClockIdleTime(UINT32 nLunId, UINT32 nDieId) { return _controller.GetNandClockIdleTime(nLunId, nDieId); }
    UINT64          GetHostClockIdleTime(UINT32 nDieId) { return _controller.GetHostClockIdleTime(0, nDieId); };
    UINT64          GetHostClockIdleTime();
    UINT64          GetResourceContentionTime();
    UINT64          GetActivateBusTime();
    UINT64          GetCellActiveTime();
    UINT64          GetClockPeriods(void) { return NFS_GET_PARAM(ISV_CLOCK_PERIODS); }  
    UINT64          CurrentTime()                   { return _controller.CurrentTime(); } 
    UINT64          TickOverTime()                  { return _controller.TickOverTime();}
    inline UINT64   CurrentTime(UINT32 nDieId)      { return _controller.CurrentTime(nDieId);}
    UINT64          MinNextActivity() { return GetCyclesFromTime(_controller.MinNextActivity()); }
    UINT64          MinIoBusActivity() { return GetCyclesFromTime(_controller.MinIoBusActivity()); }
    inline NandDeviceConfig GetDeviceConfig( void )         { return _stDevConfig; }

    //////////////////////////////////////////////////////////////////////////
    // control interfaces
    //////////////////////////////////////////////////////////////////////////
    void            HardReset( UINT32 nSystemClock, NandDeviceConfig &stDevConfig );
    void            InterruptService( NAND_ISR_TYPE nIsrType, UINT32 nArg1, UINT64 nArg2 );

    //////////////////////////////////////////////////////////////////////////
    // statistics
    //////////////////////////////////////////////////////////////////////////
    void            ReportStatistics( void );
    void            ReportConfiguration( void );

private:
    UINT64          GetCyclesFromTime(UINT64 nTime);
};

}

#endif // _NandFlashSystem_h__