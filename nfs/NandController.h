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
	created:	6:8:2011   13:02
	file base:	NandController
	file ext:	h
	author:		Myoungsoo Jung
				Pennsylvania State University, MDL Lab
	
	purpose:	
*********************************************************************/

#ifndef _NandController_h__
#define _NandController_h__

namespace NANDFlashSim {

class NandFlashSystem;

class NandController {
    std::vector<LogicalUnit>                    _vctLuns;
    std::vector< std::list<NandStagePacket> >   _vctCommandChains;
    std::vector<NAND_COMMAND>                   _vctnPrevNandCmd;
    std::vector<NAND_TRANS_OP>                  _vctnPrevTransOp;
    std::vector<bool>                           _vctTransCompletion;
    std::vector<UINT64>                         _vctLunLevelHostIdleTime;
    std::vector< std::vector<UINT64> >          _vctDieLevelNandClockIdleTime;
    std::vector< std::vector<UINT64> >          _vctDieLevelHostClockIdleTime;
    std::vector< std::vector<UINT64> >          _vctResourceContentionTime;
    std::vector< std::vector<UINT32> >          _vctReadReqStat;
    std::vector< std::vector<UINT32> >          _vctWriteReqStat;
    std::vector< std::vector<UINT32> >          _vctEraseReqStat;
    NandDeviceConfig                            _stDevConfig;
    NandStageBuilderTool                        _stageBuilder;
    std::vector<UINT32>                         _vctOpenAddress;
    std::vector< std::list<NandStagePacket> >   _vctAddressedNxPacket;
    std::vector<UINT16>                         _vctPaneIdx;
    
    NandSystemIsr                               *_pIsr;

    UINT64                                      _nCurrentTime;
    UINT32                                      _nFineGrainTransId;

    UINT64                                      _nMinNextActivate;

    UINT64                                      _nBubbleTime;           // this is used for a host side simulator. For NANDFlashSim, this variable might be not used to simulation.
    UINT64                                      _nIdleTime;             // this is used for a host side simulator. For NANDFlashSim, this variable might be not used to simulation

private :
    inline UINT32           genFineGrainTransId()                   { return _nFineGrainTransId++; }
    inline void             invalidateOpenAddr(UINT32 nBusId)       { _vctOpenAddress[nBusId] = NULL_SIG(UINT32); }
public :
    void                    SetSystemIsr(NandSystemIsr *pIsr)       { _pIsr = pIsr; }
    NV_RET                  BuildandAddStage(Transaction &stTrans);
    void                    Update(UINT64 nTime);
    // DelayUpdate emulate the situation that a host model cannot commit NAND commands for either operation or control.
    // This function is only available in the case that the host model leverages BuildandAddStage function to handle LUN.
    // If the host doesn't use it and directly handles command chains being respect to ONFI then there is no need to call this function.
    void                    DelayUpdate(UINT64 nBubbleTime)         { _nBubbleTime = nBubbleTime; }
    void                    AddDelayUpdate(UINT64 nBubbleTime)      { _nBubbleTime += nBubbleTime; }
    void                    CommitStage(UINT32 &nBusId, NandStagePacket &stagePacket)                           { _vctCommandChains[nBusId].push_back(stagePacket); }

    void                    TickOver(UINT64 nClockTime)             { _nIdleTime += nClockTime; }
    UINT64                  MinNextActivity();
    UINT64                  MinIoBusActivity();

    void                    HardReset(UINT32 nSystemClock, NandDeviceConfig &stDevConfig);
    
    void                    ReportPerformance();
    void                    ReportStatistics();
    bool                    IsIoBusActive() { return _vctLuns[0].IsIoBusActive();}
    UINT64                  CurrentTime(UINT32 nDieIdx)                        { return _vctLuns[0].CurrentTime(nDieIdx);}  
    UINT64                  CurrentTime() {return _nCurrentTime + _nIdleTime;}
    UINT64                  TickOverTime()  {return _nIdleTime;}

    UINT64                  GetNandClockIdleTime(UINT32 nLunId, UINT32 nDieId) { return _vctDieLevelNandClockIdleTime[nLunId][nDieId]; }
    UINT64                  GetHostClockIdleTime(UINT32 nLunId, UINT32 nDieId) { return _vctDieLevelHostClockIdleTime[nLunId][nDieId]; }
    UINT64                  GetHostClockIdleTime(UINT32 nLunId)                { return _vctLunLevelHostIdleTime[nLunId];}
    UINT64                  GetResourceContentionTime(UINT32 nLunId, UINT32 nDieId) { return _vctResourceContentionTime[nLunId][nDieId]; }
    UINT64                  GetActiveBusTime(UINT32 nLunId, UINT32 nDieId);


    NandController(UINT64 nSystemClock, NandDeviceConfig &stConfig);
    ~NandController();
};

}

#endif // _NandController_h__