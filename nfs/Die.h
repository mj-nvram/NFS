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
	created:	2011/07/15
	created:	15:7:2011   22:04
	file base:	Die
	file ext:	h
	author:		Myoungsoo Jung
				Pennsylvania State University, MDL Lab
*********************************************************************/

#ifndef _Die_h__
#define _Die_h__

namespace NANDFlashSim {

class Die 
{
    /************************************************************************/
    /* device attributes                                                    */
    /************************************************************************/
    UINT32              _nId;
    UINT64			    _nCurrentTime;
    NandDeviceConfig    _stDevConfig;
#ifndef WITHOUT_PLANE_STATS
    std::vector<Plane>  _vctPlanes;
#endif
    /************************************************************************/
    /* registers                                                            */
    /************************************************************************/
    // each die has its own registers to handle multi-plane operations (especially read case).
    std::vector<UINT32> _vctRowRegister;
    std::vector<UINT16> _vctColRegister;
    std::vector<UINT32> _vctRandomBytes;
    NAND_COMMAND        _nCommandRegister;
    // for simulation data/cache register, this simulation leverage cache register rather than both registers.
    // This enable this to remove unnecessary memory copy operation.
    // Simulator, however, works with cycle (ns) accurately.
    std::vector< boost::shared_array<UINT8> > _vctpCacheRegister;


    /************************************************************************/
    /* information for internal state                                       */
    /************************************************************************/
    bool			    _bPowerSupply;
    bool                _bNeedReset;
    NAND_STAGE		    _nCurrentStage;
    NAND_STAGE          _nExpectedStage;
    UINT64              _nNextActivate;
    UINT64              _nCurNandClockIdleTime;     // This is the rest time of 1 cycle. For example, if 1 cycle is 16ns and CLE takes 10ns, then rest time is 6ns.
    bool                _bNandBusy;
    UINT8               _nLastAleBytes;
    UINT8               _nNxCommandCnt;
    bool                _bStanbyDc;
    bool                _bLeakDc;

    /************************************************************************/
    /* flag for operation classification                                    */
    /************************************************************************/
    bool                _bCacheLoadFirst;
    bool                _bCacheNohideTon;

    /************************************************************************/
    /* statistics                                                           */
    /************************************************************************/
    std::vector<UINT64> _vctAccumulatedTime;
    std::vector<UINT64> _vctPowerTime;

    /************************************************************************/
    /* For Logs                                                             */
    /************************************************************************/
    NAND_FSM_STATE _eUpdatedState;
    UINT64 _nUpdatedAccTime;

public :

    Die(UINT64 nSystemClock, NandDeviceConfig &devConfig);

    void                SoftReset();
    void                HardReset(UINT32 nSystemClock, NandDeviceConfig &stDevConfig);
    void                Update(UINT64 nTime);
    void                Poweron();
    void                Poweroff();
    NAND_STAGE          TransitStage( NAND_STAGE nState, NandStagePacket &stPacket);
    UINT32              MaxAddress();
    inline UINT64       CurrentTime()   {return _nCurrentTime;}

    /************************************************************************/
    /* attribute                                                            */
    /************************************************************************/
    UINT64              GetPowerTime(NAND_DC nDcState);
    inline UINT64       NextActivate() {return _nNextActivate;}
    inline bool         CheckFsmBusy() {return (_nNextActivate == 0) ? false : true;};
    inline NAND_STAGE   ExpectedNextStage() {return _nExpectedStage;}
    inline NAND_STAGE   CurrentStage()      {return _nCurrentStage;}
    UINT64              GetAccumulatedFSMTime(NAND_FSM_STATE nFsmState);
    UINT32              ID() const { return _nId; }
    void                ID(UINT32 val);
    bool                CheckRb();
    inline bool         IsFree() { return (_nNextActivate == 0 && _nExpectedStage == NAND_STAGE_IDLE) ? true : false;}
    UINT64              GetCurNandClockIdleTime(void) { return _nCurNandClockIdleTime; }

private :
    void                resetRegisters();
    UINT64              nandArrayTimeParam(UINT32 nRow, INI_DEVICE_VALUE eName);
};

}

#endif // _Die_h__