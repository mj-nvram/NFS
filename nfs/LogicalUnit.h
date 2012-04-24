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
	created:	2011/07/19
	created:	19:7:2011   18:00
	file base:	NandFlashController
	file ext:	h
	author:		Myoungsoo Jung
				Pennsylvania State University, MDL Lab
	
	purpose:	
*********************************************************************/

#ifndef _NandFlashController_h__
#define _NandFlashController_h__

namespace NANDFlashSim {

typedef enum {
    NAND_MULTIDIE_READ,
    NAND_MULTIDIE_MULTIPLANE_READ,
    NAND_MULTIDIE_MULTIPLANE_CACHE_READ,
    NAND_MUTILDIE_PROG,
    NAND_MULTIDIE_MULTIPLANE_PROG,
    NAND_MULTIDIE_MULTIPLANE_CACHE_PROG,
} NAND_INTERLEAVED_DIE_COMMAND;


class LogicalUnit {
    UINT32              _nId;
    UINT64              _nCurrentTime;
    UINT64              _nMinNextActivate;
    UINT32              _nTransactionBusDepth;
    std::vector<Die>    _vctDies;
    std::vector<UINT64> _vctRequestTraffic;
    std::vector< std::list<NandStagePacket> > _vctNandBus;    // nums of dies.

    bool                _bBusy;
    UINT16              _nIoBusOwnerDieId;

    std::vector<bool>   _vctIoCompletion;
    std::vector<bool>   _vctNeedforCallback;

    std::vector<UINT64> _vctFirstArrivalCycleForInitialCommand;

    std::vector<UINT64> _vctCurHostClockIdleTime;

    
public:
    LogicalUnit(UINT64 nSystemClock, NandDeviceConfig &stDevConfig);
    
    void                Update(UINT64 nTime);
    NV_RET              IssueNandStage(NandStagePacket &stPacket);
    void                ReportPowerTimePerEachDcParam();
    void                ReportTimePerEachState();
    void                ReportBandwidth();
    bool                CheckBusy(UINT8 nDie = NULL_SIG(UINT8));
    bool                IsDieIdle(UINT8 nDie);
    UINT64              MinNextActivity() { return _nMinNextActivate; }
    inline UINT32       ID() const { return _nId; }
    void                ID(UINT32 val);
    UINT64              AccumulatedTraffic();
    void                HardReset(UINT32 nSystemClock, NandDeviceConfig &stDevConfig);
    inline bool         IsIoBusActive()             { return (_nIoBusOwnerDieId == NULL_SIG(UINT16)) ? false : true;}

    UINT64              CurrentTime(UINT8 nDie)             { return _vctDies[nDie].CurrentTime(); }
    UINT64              GetCurNandClockIdleTime(UINT8 nDie) { return _vctDies[nDie].GetCurNandClockIdleTime(); }
    UINT64              GetCurHostClockIdleTime(UINT8 nDie) { return _vctCurHostClockIdleTime[nDie]; }
    
    UINT64              GetRequestTraffic(UINT8 nDie) { return _vctRequestTraffic[nDie]; }
    UINT64              GetAccumulatedFSMTime(NAND_FSM_STATE nFsmState, UINT8 nDie) { return _vctDies[nDie].GetAccumulatedFSMTime(nFsmState); }

private :
    inline UINT16       getBusOwnerDieId()          {return _nIoBusOwnerDieId;}
    inline void         acquireIoBus(UINT16 nDieId) { if(_nIoBusOwnerDieId == NULL_SIG(UINT16)) _nIoBusOwnerDieId = nDieId;}
    inline void         releaseIobus(UINT16 nDieId) { if(_nIoBusOwnerDieId == nDieId) _nIoBusOwnerDieId = NULL_SIG(UINT16);}
    NAND_COMMAND        getConfirmCommand(NAND_COMMAND nCommand);

    bool                transitStage(UINT8 nDieIdx, bool &bTransitFailed);
};

}

#endif // _NandFlashController_h__