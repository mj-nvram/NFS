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
#include "NandLogger.h"

#define COMMAND_LETCH_TIME                 (NFS_GET_PARAM(ITV_tWP) +\
                                             NFS_GET_PARAM(ITV_tDS) +\
                                             NFS_GET_PARAM(ITV_tDH))

#define ADDRESS_LETCH_TIME(_nNumsAccess)   ((NFS_GET_PARAM(ITV_tCS) - NFS_GET_PARAM(ITV_tDS)) +\
                                             (NFS_GET_PARAM(ITV_tDS) + NFS_GET_PARAM(ITV_tDH)) * _nNumsAccess)

#define DATA_OUT_TIME(_nNumsAccess)        (NFS_GET_PARAM(ITV_tRR) +\
                                             NFS_GET_PARAM(ITV_tRC) * _nNumsAccess)

#define DATA_IN_TIME(_nNumsAccess)         (NFS_GET_PARAM(ITV_tWC)*_nNumsAccess)

#define READ_STATUS_TIME                   (NFS_GET_PARAM(ITV_tDS) +\
                                             NFS_GET_PARAM(ITV_tWHR) +\
                                             NFS_GET_PARAM(ITV_tREA) +\
                                             NFS_GET_PARAM(ITV_tRC))

#define NAND_PLANE_WRAPAROUND(_nCol, _stDevConfig)        (_nCol % _nNumsPlane)

namespace NANDFlashSim {

Die::Die(UINT64 nSystemClock, NandDeviceConfig &devConfig ) : 
        _vctPowerTime(NAND_DC_MAX,0),
        _vctAccumulatedTime(NAND_FSM_MAX,0),
        _vctColRegister(devConfig._nNumsPlane, NULL_SIG(UINT16)),
        _vctRowRegister(devConfig._nNumsPlane, NULL_SIG(UINT32)),
        _vctRandomBytes(devConfig._nNumsPlane, 0)
{
    _stDevConfig    = devConfig;
    // build multi-plane
    _vctpCacheRegister.resize(devConfig._nNumsPlane);

    for(UINT8 nIdx = 0; nIdx < devConfig._nNumsPlane; ++nIdx)
    {
        boost::shared_array<UINT8> sharedCacheReg(new UINT8[_stDevConfig._nPgSize]);
        _vctpCacheRegister[nIdx]    = sharedCacheReg;
#ifndef WITHOUT_PLANE_STATS
        _vctPlanes.push_back(Plane(devConfig));
#endif
    }

    assert(nSystemClock != NULL_SIG(UINT64));
    
    _nCurNandClockIdleTime = 0;
    _nCurrentTime    = nSystemClock;
    _bPowerSupply    = false;
    
    SoftReset();
}


//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    TransitStage
// FullName:  Die::TransitStage
// Access:    public 
// Returns:   NAND_STAGE
// Parameter: NAND_STAGE nStage
// Parameter: StatePacket & stPacket
//
// Descriptions -
// state transition and command chain will be handled by caller.
// This method is going to return that NAND flash expected as next step.
// But if it is busy or caller operate NAND flash as wrong way, it will return 
// busy or not determined state respectively.
// 
//////////////////////////////////////////////////////////////////////////////
NAND_STAGE Die::TransitStage(NAND_STAGE nStage, NandStagePacket &stPacket)
{
    NAND_STAGE  nNextStage  = NAND_STAGE_NOT_DETERMINED;
    _bStanbyDc              = true;
    _bLeakDc                = true;
    if(_nNextActivate == 0 && _bNeedReset == false)
    {
        UINT8   nPlane  = NAND_PLN_PARSE_REGISTER(stPacket._nRow);
        if(nStage == NAND_STAGE_ALE)
        {
            //
            //   address latch
            //
            assert(stPacket._nCol != NULL_SIG(UINT16));
            _bLeakDc        = false;
            _bNandBusy      = false;

            // For accurate simulating NAND bus activity, 
            // assume that cycles for I/O is given at address latch cycle.
            _vctRandomBytes[nPlane]   = stPacket._nRandomBytes;

            // check whether row register value is same with don't care value
            // dont-care value means this command leverages opened row address value
            if(stPacket._nRow != NULL_SIG(UINT32))
            {
                if(_nCommandRegister == NAND_CMD_READ_MULTIPLANE && _vctRowRegister[nPlane] != stPacket._nRow)
                {
                    _bNeedReset = true;
                    NV_ERROR("Row address for read(multplane mode) has invalid (there is no plane selection in row address)");
                }
                _vctRowRegister[nPlane]   = stPacket._nRow;
            }

            if  (_nCommandRegister == NAND_CMD_READ_INTERNAL ||
                 _nCommandRegister == NAND_CMD_READ_INTERNAL_MULTIPLANE ||
                 _nCommandRegister == NAND_CMD_READ_INTERNAL_MULTIPLANE_FIN)
            {
                // even though addressing protocol of internal data movement(copyback) 
                // need col addressing cycle, the addresses will be ignored.
                _vctColRegister[nPlane]   = 0;
            }

            _vctColRegister[nPlane]   = stPacket._nCol;

            if(_nCommandRegister == NAND_CMD_BLOCK_ERASE ||
               _nCommandRegister == NAND_CMD_BLOCK_MULTIPLANE_ERASE ||
               _nCommandRegister == NAND_CMD_BLOCK_MULTIPLANE_ERASE_FIN)
            {
                _nLastAleBytes      = 3;
            }
            else if (_nCommandRegister == NAND_CMD_READ_RANDOM)
            {
                // random i/o employs open row address.            
               _nLastAleBytes       = 2;
            }
            else
            {
                _nLastAleBytes      = 5;
            }

            _nNextActivate                      = ADDRESS_LETCH_TIME(_nLastAleBytes);
            _vctAccumulatedTime[NAND_FSM_ALE]   += _nNextActivate; 


            switch(_nCommandRegister)
            {
            case NAND_CMD_PROG_MULTIPLANE_FIN_RANDOM :
            case NAND_CMD_PROG_MULTIPLANE_FIN: 
            case NAND_CMD_PROG_MULTIPLANE_CACHE_FIN :
            case NAND_CMD_PROG_MULTIPLANE: 
            case NAND_CMD_PROG_MULTIPLANE_CACHE :
            case NAND_CMD_PROG_MULTIPLANE_RANDOM :
            case NAND_CMD_PROG_PAGE :
            case NAND_CMD_PROG_CACHE :
            case NAND_CMD_PROG_RANDOM :
            case NAND_CMD_PROG_RANDOM_FIN :
            case NAND_CMD_PROG_MULTIPLANE_RANDOM_DUMMY :
                nNextStage  = NAND_STAGE_TIR;
                break;
            case NAND_CMD_PROG_INTERNAL :
            case NAND_CMD_PROG_INTERNAL_MULTIPLANE :
            case NAND_CMD_PROG_INTERNAL_MULTIPLANE_FIN :
                nNextStage  = NAND_STAGE_CLE;
                break;
            //////////////////////////////////////////////////////////////////////////
            // READ
            //////////////////////////////////////////////////////////////////////////
            case NAND_CMD_READ_PAGE :
            case NAND_CMD_READ_RANDOM :
            case NAND_CMD_READ_CACHE_ADDR_INIT :
            case NAND_CMD_READ_MULTIPLANE :
            case NAND_CMD_READ_MULTIPLANE_INIT :
            case NAND_CMD_READ_MULTIPLANE_INIT_FIN :
            case NAND_CMD_READ_INTERNAL :
            case NAND_CMD_READ_INTERNAL_MULTIPLANE_FIN :
                nNextStage  = NAND_STAGE_CLE;
                break;
            case NAND_CMD_READ_INTERNAL_MULTIPLANE :
                // internal data movement with multi-plane mode require another commands for addressing.
                nNextStage  = NAND_STAGE_IDLE;
                break;
            //////////////////////////////////////////////////////////////////////////
            // READ
            //////////////////////////////////////////////////////////////////////////
            case NAND_CMD_BLOCK_ERASE :
            case NAND_CMD_BLOCK_MULTIPLANE_ERASE_FIN :
                nNextStage  = NAND_STAGE_CLE;
                break;
            case NAND_CMD_BLOCK_MULTIPLANE_ERASE :
                nNextStage  = NAND_STAGE_IDLE;
                break;
            }

        }
        else if(nStage == NAND_STAGE_CLE)
        {
            _nCommandRegister                   = stPacket._nCommand;
            _nNextActivate                      = COMMAND_LETCH_TIME;
            _vctAccumulatedTime[NAND_FSM_CLE]   += _nNextActivate; 
            _bLeakDc                            = false;

            _bNandBusy                          = false;

            switch(_nCommandRegister)
            {

            case NAND_CMD_PROG_MULTIPLANE_FIN_RANDOM :
            case NAND_CMD_PROG_MULTIPLANE_FIN: 
            case NAND_CMD_PROG_INTERNAL_MULTIPLANE_FIN :

                if(_nNxCommandCnt == NULL_SIG(UINT8))
                {
                    _bNeedReset=true;
                    NV_ERROR("The last multiplane command is issued without any other multiplane command");
                }
                else
                {
                    _nNxCommandCnt = NULL_SIG(UINT8);
                }
                nNextStage  = NAND_STAGE_ALE;
                break;
            case NAND_CMD_PROG_MULTIPLANE: 
            case NAND_CMD_PROG_MULTIPLANE_RANDOM :
            case NAND_CMD_PROG_INTERNAL_MULTIPLANE :
            case NAND_CMD_PROG_MULTIPLANE_RANDOM_DUMMY :
                if(_nNxCommandCnt == NULL_SIG(UINT8))
                {
                    _nNxCommandCnt = 0;
                }
                else if(_nNxCommandCnt > _stDevConfig._nNumsPlane)
                {
                    _bNeedReset=true;
                    NV_ERROR("The number of access for plans("<<std::hex<<_nNxCommandCnt<<")exceeds the number of physical number of planes("<<std::hex<<_stDevConfig._nNumsPlane<<")");
                }
                _nNxCommandCnt++;
            case NAND_CMD_PROG_MULTIPLANE_CACHE :
                _nNxCommandCnt=0; // MULTI PLANE cache is right after issuing nx command, but the number of nx cache write is unlimited.
            case NAND_CMD_PROG_CACHE :
            case NAND_CMD_PROG_MULTIPLANE_CACHE_FIN :
            case NAND_CMD_PROG_PAGE :
            case NAND_CMD_PROG_RANDOM :
            case NAND_CMD_PROG_RANDOM_FIN :
            case NAND_CMD_PROG_INTERNAL :
                nNextStage      = NAND_STAGE_ALE;
                break;
            case NAND_CMD_PROG_MULTIPLANE_CONF :
            case NAND_CMD_PROG_MULTIPLANE_RANDOM_DUMMY_CONF :
            case NAND_CMD_PROG_INTERNAL_MULTIPLANE_CONF :
                nNextStage      = NAND_STAGE_TIN_DUMMY;
                break;
            case NAND_CMD_PROG_PAGE_CONF :
            case NAND_CMD_PROG_RANDOM_FIN_CONF :
            case NAND_CMD_PROG_MULTIPLANE_FIN_CONF : 
            case NAND_CMD_PROG_MULTIPLANE_FIN_RANDOM_CONF : 
            case NAND_CMD_PROG_INTERNAL_CONF :
            case NAND_CMD_PROG_INTERNAL_MULTIPLANE_FIN_CONF :
                nNextStage      = NAND_STAGE_TIN;
                break;
            case NAND_CMD_PROG_MULTIPLANE_CACHE_CONF :
            case NAND_CMD_PROG_CACHE_CONF :  // means going to continuous
                nNextStage      = NAND_STAGE_TIN_CACHE;
                break;
            case NAND_CMD_PROG_MULTIPLANE_CACHE_FIN_CONF :
                nNextStage      = NAND_STAGE_TIN_TAIL;
                break;

            //////////////////////////////////////////////////////////////////////////
            // READ
            //////////////////////////////////////////////////////////////////////////
            case NAND_CMD_READ_CACHE_ADDR_INIT:
                if(_bCacheLoadFirst == true)
                {
                    _bNeedReset = true;
                    NV_ERROR("Address init operation for read cache cannot be issued redundantly without read cache operation");
                }
                else
                {
                    // first cache operation
                    _bCacheLoadFirst    = true;     // first tDCBSYR
                }
            case NAND_CMD_READ_PAGE :
            case NAND_CMD_READ_INTERNAL :
            case NAND_CMD_READ_MULTIPLANE :
            case NAND_CMD_READ_MULTIPLANE_INIT :
            case NAND_CMD_READ_MULTIPLANE_INIT_FIN :
            case NAND_CMD_READ_INTERNAL_MULTIPLANE :
            case NAND_CMD_READ_INTERNAL_MULTIPLANE_FIN :
            case NAND_CMD_READ_RANDOM :
                nNextStage          = NAND_STAGE_ALE;
                break;
            case NAND_CMD_READ_MULTIPLANE_INIT_CONF :                
                nNextStage          = NAND_STAGE_IDLE;
                break;
                // confirmation command class
            case NAND_CMD_READ_MULTIPLANE_CONF :
            case NAND_CMD_READ_RANDOM_CONF:
                nNextStage          = NAND_STAGE_TOR;
                break;
            case NAND_CMD_READ_CACHE_ADDR_INIT_CONF :
                // this will cause tR time for cache operation.
                if(!_bCacheLoadFirst && !_bCacheNohideTon)
                {
                    _bNeedReset = true;
                    NV_ERROR("NAND command chain is massed up");
                }
                _bCacheNohideTon    = true;     // first tR
            case NAND_CMD_READ_PAGE_CONF :
            case NAND_CMD_READ_INTERNAL_CONF:
            case NAND_CMD_READ_MULTIPLANE_INIT_FIN_CONF :
            case NAND_CMD_READ_INTERNAL_MULTIPLANE_FIN_CONF:
            case NAND_CMD_READ_MULTIPLANE_CACHE_CONF :
                nNextStage                          = NAND_STAGE_TON;
                break;
            case NAND_CMD_READ_CACHE :
                if(_nCurrentStage != NAND_STAGE_TOR && _nCurrentStage != NAND_STAGE_TON && _nCurrentStage != NAND_STAGE_CLE)
                {
                    // cache read should follow normal read page operation
                    _bNeedReset = true;
                    NV_ERROR("NAND command chain is massed up");
                }
                else
                {
                    assert(_vctRowRegister[nPlane] != NULL_SIG(UINT32));
                    // cache read right after normal read doesn't have address.
                    _vctColRegister[nPlane] = 0;
                    _vctRandomBytes[nPlane] = _stDevConfig._nPgSize;
                }
                nNextStage      = NAND_STAGE_TON;
                break;
            case NAND_CMD_READ_MULTIPLANE_CACHE :
                _nNextActivate                      += nandArrayTimeParam(_vctRowRegister[nPlane], ITV_tDCBSYR1);
                _vctAccumulatedTime[NAND_FSM_CLE]   += nandArrayTimeParam(_vctRowRegister[nPlane], ITV_tDCBSYR1); 
                nNextStage                          = NAND_STAGE_TON;
                break;
            //////////////////////////////////////////////////////////////////////////
            // MISC
            //////////////////////////////////////////////////////////////////////////
            case NAND_CMD_BLOCK_ERASE:
            case NAND_CMD_BLOCK_MULTIPLANE_ERASE :
            case NAND_CMD_BLOCK_MULTIPLANE_ERASE_FIN :
                nNextStage          = NAND_STAGE_ALE;
                break;
            case NAND_CMD_BLOCK_MULTIPLANE_ERASE_FIN_CONF :

                _bStanbyDc          = false;
                nNextStage          = NAND_STAGE_READ_STATUS;
                {
                    UINT64 nEraseTime   = nandArrayTimeParam(_vctRowRegister[nPlane], ITV_tBERS);

                    _nNextActivate                      += nEraseTime;
                    _vctPowerTime[NAND_DC_ERASE]        += nEraseTime;
                    _vctAccumulatedTime[NAND_FSM_ERASE] += nEraseTime;
                    UINT8 nPlaneIdx = 0;
                    for(std::vector<UINT32>::iterator iRegister =  _vctRowRegister.begin(); iRegister != _vctRowRegister.end(); ++iRegister)
                    {
                        if(*iRegister != NULL_SIG(UINT32))
                        {
#ifndef WITHOUT_PLANE_STATS
                            _vctPlanes[nPlaneIdx].Erase(*iRegister);
#endif
                            *iRegister = NULL_SIG(UINT32);
                        }
                        nPlaneIdx++;
                    }
                }

                break;
            case NAND_CMD_BLOCK_ERASE_CONF :
                _bStanbyDc          = false;
                {
                    UINT64 nEraseTime   = nandArrayTimeParam(_vctRowRegister[nPlane], ITV_tBERS);
                    nNextStage          = NAND_STAGE_READ_STATUS;
                    _nNextActivate                  += nEraseTime;
                    _vctPowerTime[NAND_DC_ERASE]    += nEraseTime;
                    _vctAccumulatedTime[NAND_FSM_ERASE]   += nEraseTime;
#ifndef WITHOUT_PLANE_STATS
                    _vctPlanes[nPlane].Erase(_vctRowRegister[nPlane]);
#endif

                }
                break;
            case NAND_CMD_RESET :
                nNextStage                          = NAND_STAGE_RESET_DELTA;                
                _nNextActivate                      += NFS_GET_PARAM(ITV_tWB) + NFS_GET_PARAM(ITV_tRST);
                _vctAccumulatedTime[NAND_FSM_TIN]   += NFS_GET_PARAM(ITV_tWB) + NFS_GET_PARAM(ITV_tRST);
                break;
            }
        }
        else if(nStage == NAND_STAGE_TIR)
        {
            _bNandBusy      = false;

            if(_nCommandRegister > NAND_DELIMITER_CMD_READ)
            {
                assert(stPacket._nRandomBytes != NULL_SIG(UINT32));
#ifndef     NO_STORAGE
                if(stPacket._pData != NULL)
                {
                    assert(_vctColRegister[nPlane]    != NULL_SIG(UINT16));
                    UINT8   nPlaneIdx       = (0xF << BITS_NAND_PAGE_ADDR) & _vctRowRegister[nPlane];
                    UINT8   *pCacheReg      = _vctpCacheRegister[nPlaneIdx].get();
                    memcpy(pCacheReg + _vctColRegister[nPlane], stPacket._pData + _vctColRegister[nPlane], _vctRandomBytes[nPlane]);
                }
#endif

                _nNextActivate                  = DATA_IN_TIME(_vctRandomBytes[nPlane] / (_stDevConfig._nNumsIoPins / 8));
                _vctPowerTime[NAND_DC_PROG]     += _nNextActivate;
                _bStanbyDc                      = false;

                if (_nCommandRegister == NAND_CMD_PROG_RANDOM || _nCommandRegister == NAND_CMD_PROG_RANDOM_FIN)
                {
                    _nNextActivate  += NFS_GET_PARAM(ITV_tADL) - NFS_GET_PARAM(ITV_tWC);
                }
                
                _vctAccumulatedTime[NAND_FSM_TIR]    += _nNextActivate;
                _bStanbyDc                           = false;
                if(_nCommandRegister == NAND_CMD_PROG_RANDOM || _nCommandRegister == NAND_CMD_PROG_MULTIPLANE_RANDOM)
                {
                    // next random request will follow this.
                    nNextStage  = NAND_STAGE_IDLE;
                }
                else
                {
                    nNextStage  = NAND_STAGE_CLE;
                }
            }
            else
            {
                _bNeedReset = true;
                NV_ERROR("NAND command chain is massed up");
            }

        }
        else if(nStage == NAND_STAGE_TIN || 
                nStage == NAND_STAGE_TIN_CACHE || 
                nStage == NAND_STAGE_TIN_DUMMY||
                nStage == NAND_STAGE_TIN_TAIL)
        {
            _bNandBusy      = true;


            if (_nCommandRegister > NAND_DELIMITER_CMD_READ)
            {

                assert(_vctColRegister[nPlane]    != NULL_SIG(UINT16));
                assert(_vctRandomBytes[nPlane]    != NULL_SIG(UINT32));
                UINT8 *pCacheReg ;
#ifndef     NO_STORAGE
                pCacheReg   = _vctpCacheRegister[nPlane].get();
#else
                pCacheReg   = NULL;
#endif
                // program
#ifndef WITHOUT_PLANE_STATS
                _vctPlanes[nPlane].Write(_vctColRegister[nPlane], _vctRowRegister[nPlane], pCacheReg);
#endif
                _vctPowerTime[NAND_DC_PROG]     += nandArrayTimeParam(_vctRowRegister[nPlane], ITV_tPROG);

                _bStanbyDc                      = false;
                if(nStage == NAND_STAGE_TIN_CACHE)
                {
                    // NANDFlashSim doesn't use tCBSY typical value.
                    // Instead, it doesn't deduct tPROG time for the page before the last page.

                    _nNextActivate  = nandArrayTimeParam(_vctRowRegister[nPlane], ITV_tPROG);
                    //_nNextActivate  = nandArrayTimeParam(_vctRowRegister[nPlane], ITV_tCBSY);

                    // In cache mode write operation, NAND bus is enable to TIR during TIN.
                    // Thus, TIR and TIN can be overlapped.
                    UINT32 nTirandLatchTime = DATA_IN_TIME(_vctRandomBytes[nPlane] / (_stDevConfig._nNumsIoPins / 8)) + COMMAND_LETCH_TIME + ADDRESS_LETCH_TIME(5);
                    if(_nNextActivate > nTirandLatchTime)
                    {
                        _nNextActivate -= nTirandLatchTime;
                    }
                    else
                    {
                        _nNextActivate = nTirandLatchTime - _nNextActivate;
                    }
 
                    nNextStage      = NAND_STAGE_IDLE;
                }
                else if (nStage == NAND_STAGE_TIN_DUMMY)
                {
                    _nNextActivate  = nandArrayTimeParam(_vctRowRegister[nPlane], ITV_tDBSY);
                    nNextStage      = NAND_STAGE_IDLE;
                }
                else if(nStage   == NAND_STAGE_TIN_TAIL)
                {
                    assert(_vctRandomBytes[nPlane] != NULL_SIG(UINT32));
                    assert(_nLastAleBytes  != NULL_SIG(UINT8));
                    _nNextActivate  = /*nandArrayTimeParam(_vctRowRegister[nPlane]-1, ITV_tPROG) +*/ nandArrayTimeParam(_vctRowRegister[nPlane], ITV_tPROG) - COMMAND_LETCH_TIME - ADDRESS_LETCH_TIME(_nLastAleBytes) - DATA_IN_TIME(_vctRandomBytes[nPlane]);
                    _nLastAleBytes  = NULL_SIG(UINT8);
                    nNextStage      = NAND_STAGE_READ_STATUS;
                }
                else
                {
                    _nNextActivate  = nandArrayTimeParam(_vctRowRegister[nPlane], ITV_tPROG);
                    nNextStage      = NAND_STAGE_READ_STATUS;
                }

                _vctAccumulatedTime[NAND_FSM_TIN]   += _nNextActivate;
                // reset DATA IN CYCLE
                _vctRandomBytes[nPlane]             = NULL_SIG(UINT32);
            }
            else
            {
                _bNeedReset = true;
                NV_ERROR("NAND command chain is massed up");
            }
        }
        else if(nStage == NAND_STAGE_READ_STATUS)
        {
            _bNandBusy      = false;

            _nNextActivate  = READ_STATUS_TIME;
            _vctAccumulatedTime[NAND_FSM_TOR]   += _nNextActivate;
            _vctPowerTime[NAND_DC_READ]         += _nNextActivate;
            _bStanbyDc                          = false;
            if(stPacket._pStatusData != NULL)
            {
                // DATA LAYOUT FOR READ STATUS
                // I do not pack status representative bits into 1 bytes.
                // It doesn't effect to emulate NAND flash performance as well as reliability.
                // bWriteFail [17:17] , bBusy [16:16], NAND_STAGE [15:0]
                *stPacket._pStatusData = (_bNeedReset) ? 0x1 << 17: 0x0 << 17;
                *stPacket._pStatusData |= (_nNextActivate != 0) ? 0x1 << 16 : 0x0 << 16;
                *stPacket._pStatusData |= NULL_SIG(UINT16) & _nCurrentStage;
            }
            nNextStage      = NAND_STAGE_IDLE;
        }
        else if(nStage == NAND_STAGE_TON)
        {

            assert(_vctColRegister[nPlane] != NULL_SIG(UINT16));
            assert(_vctRandomBytes[nPlane] != NULL_SIG(UINT32));
            UINT8    *pCacheReg;
            _bNandBusy  = true;

            if (NAND_PGO_PARSE_REGISTER(_vctRowRegister[nPlane]) >= _stDevConfig._nNumsPgPerBlk)
            {
                _bNeedReset = true;
                NV_ERROR("cache read is allowed only if ascending order read in a block");


                // in this case, state machine should be stopped.
                _nCurrentStage     = nStage;
                _nExpectedStage = NAND_STAGE_READ_STATUS;
                return _nExpectedStage ;
            }

            // PHYSICAL READ
            if(_nCommandRegister == NAND_CMD_READ_MULTIPLANE_INIT_FIN_CONF)
            {
                // in the multi-plane mode, NANDFlashSim read data from NAND at delta time.
                for(UINT16 nPlaneIdx = 0; nPlaneIdx < _stDevConfig._nNumsPlane; nPlaneIdx++)
                {
                    if(_vctRowRegister[nPlaneIdx] != NULL_SIG(UINT32))
                    {
#ifndef     NO_STORAGE
                        pCacheReg   = _vctpCacheRegister[nPlaneIdx].get();
#else
                        pCacheReg   = NULL;
#endif            
                        // TON is page based no matter what applications issued column address. 
#ifndef WITHOUT_PLANE_STATS
                        _vctPlanes[nPlaneIdx].Read(/*_vctColRegister[nPlane]*/0 , _vctRowRegister[nPlaneIdx], pCacheReg);
#endif
                    }

                }

            }
            else if(_nCommandRegister != NAND_CMD_READ_CACHE_ADDR_INIT_CONF)
            {
                // In cache read mode, reading data from virtual blocks is executed in DCBSYR.
                // This is an effort to make timing compatibility ONFI.
#ifndef     NO_STORAGE
                pCacheReg   = _vctpCacheRegister[nPlane].get();
#else
                pCacheReg   = NULL;
#endif            
                // TON is page based no matter what applications issued column address.
#ifndef WITHOUT_PLANE_STATS
                _vctPlanes[nPlane].Read(/*_vctColRegister[nPlane]*/0 , _vctRowRegister[nPlane], pCacheReg);
#endif
            }
            
            // Even though read cycle for cache, nx can be overlapped, there is no difference on power cycles for each plane.
            _vctPowerTime[NAND_DC_READ]     += NFS_GET_PARAM(ITV_tR);
            _bStanbyDc                      = false;
            if(_nCommandRegister    == NAND_CMD_READ_CACHE )
            {

                // NANDFlashSim leverages typical stat timing param for register access latency
                if(_bCacheNohideTon == true)
                {
                    _nNextActivate      = NFS_GET_PARAM(ITV_tR);
                }
                else
                {
                    if(_bCacheLoadFirst == true)
                    {
                        _nNextActivate      = NFS_GET_PARAM(IMV_tDCBSYR1) + NFS_GET_PARAM(ITV_tRR);
                        _bCacheLoadFirst    = false;
                    }
                    else
                    {
                        _nNextActivate      = NFS_GET_PARAM(IMV_tDCBSYR2) + NFS_GET_PARAM(ITV_tRR);
                    }
                }

                
                // reassigning address for next read cache operation
                UINT16  nPpo = NAND_PGO_PARSE_REGISTER(_vctRowRegister[nPlane]);
                _vctRowRegister[nPlane]++;
            }
            else
            {
                _nNextActivate      = NFS_GET_PARAM(ITV_tR);
            }

            _vctAccumulatedTime[NAND_FSM_TON]  += _nNextActivate;

            if(_bCacheNohideTon == true)
            {
                // in cache read operation, tR time is required to copy data from NAND to register first.
                // Then, NAND flash wait for cache out command (10). 
                // After this, data can be carried out without addressing row or col
                _bCacheNohideTon    = false;
                nNextStage          = NAND_STAGE_IDLE;

            }
            else if(_nCommandRegister == NAND_CMD_READ_MULTIPLANE_INIT_FIN_CONF ||
               _nCommandRegister == NAND_CMD_READ_INTERNAL_CONF ||
               _nCommandRegister == NAND_CMD_READ_INTERNAL_MULTIPLANE_FIN_CONF)
            {
                // wait another transaction for plane selection.
                nNextStage          = NAND_STAGE_IDLE;

            }
            else
            {
                nNextStage          = NAND_STAGE_TOR;
            }

        }
        else if (nStage == NAND_STAGE_TOR)
        {
            _bNandBusy      = false;

            if (_nCommandRegister < NAND_DELIMITER_CMD_READ)
            {
                assert(_vctColRegister[nPlane] != NULL_SIG(UINT16));
                assert(_vctRandomBytes[nPlane] != NULL_SIG(UINT32));
                assert(_vctRandomBytes[nPlane] + _vctColRegister[nPlane] <= _stDevConfig._nPgSize);
#ifndef     NO_STORAGE
                if(stPacket._pData != NULL)
                {
                    UINT8   *pCacheReg  = _vctpCacheRegister[nPlane].get();
                    memcpy(stPacket._pData + _vctColRegister[nPlane], pCacheReg + _vctColRegister[nPlane], sizeof(UINT8) * _vctRandomBytes[nPlane]);
                }
#endif
                _nNextActivate = DATA_OUT_TIME(_vctRandomBytes[nPlane] / (_stDevConfig._nNumsIoPins / 8));
                _vctAccumulatedTime[NAND_FSM_TOR]   += _nNextActivate;
                _vctPowerTime[NAND_DC_READ]         += _nNextActivate;
                _bStanbyDc                           = false;

                // Assume that any other command (like CLE) can be issued under state idle.
                nNextStage      = NAND_STAGE_IDLE;
                // reset DATA IN CYCLE
                _vctRandomBytes[nPlane] = NULL_SIG(UINT32);

            }
            else
            {
                _bNeedReset = true;
                NV_ERROR("NAND command chain is massed up");
            }

        }
        _nCurrentStage     = nStage;
        _nExpectedStage = nNextStage;
    }
    else if (nStage == NAND_STAGE_RESET_DELTA)
    {
        _bNandBusy      = false;

        // reset in delta time
        SoftReset();
    }
    else
    {
        // hardware abstraction layer should read nand status soft reset nand state machine.
        (_bNeedReset    == true) ? nNextStage = NAND_STAGE_READ_STATUS : NAND_STAGE_BUSY;
    }

    if(_bNeedReset)
    {
        NV_ERROR("Erroneous command orders were issued. NAND flash needs to reset");
    }
    REPORT_NAND(NANDLOG_SNOOP_INTERNAL_STATE, _nId << " , " << stPacket._nStageId << ", " << stPacket._nCommand << ", " << nStage << ", " <<  _nCurrentTime << ", " << _nNextActivate);

    return nNextStage;
}

void Die::Update( UINT64 nTime )
{
    _nCurrentTime += nTime;
    
    // update cycle
    if(_nNextActivate >= nTime)
    {
        _nCurNandClockIdleTime = 0;
        _nNextActivate -= nTime;
    }
    else
    {
        _nCurNandClockIdleTime = nTime - _nNextActivate;
        _nNextActivate = 0;
    }

    // update current.
    if(_bLeakDc == true)
    {
        _vctPowerTime[NAND_DC_LEAKAGE] += nTime;
    }
    else if(_bStanbyDc)
    {
        _vctPowerTime[NAND_DC_STANDBY] += nTime;
    }
}

void Die::Poweron()
{
    _bPowerSupply   = true;
}

void Die::resetRegisters()
{
    for(UINT8 nIter = 0; nIter < _stDevConfig._nNumsPlane; ++nIter)
    {
        _vctColRegister[nIter]       = NULL_SIG(UINT16);
        _vctRowRegister[nIter]       = NULL_SIG(UINT32);
        _vctRandomBytes[nIter]       = NULL_SIG(UINT32);
    }
    _nCommandRegister   = NAND_CMD_NOT_DETERMINED;
}

void Die::SoftReset()
{
    _bNeedReset         = false;
    _nCurrentStage      = NAND_STAGE_NOT_DETERMINED;
    _nExpectedStage     = NAND_STAGE_IDLE;
    _nNextActivate      = 0;
    _nLastAleBytes      = 0;
    _nNxCommandCnt      = NULL_SIG(UINT8);
    resetRegisters();

    _bStanbyDc          = true;
    _bLeakDc            = true;
    _bNandBusy          = false;
    _bCacheLoadFirst    = false;
    _bCacheNohideTon    = false;
}

UINT64 Die::GetAccumulatedFSMTime(NAND_FSM_STATE nFsmState)
{
    return _vctAccumulatedTime[nFsmState];
}


UINT64 Die::GetPowerTime(NAND_DC nDcState)
{
    return _vctPowerTime[nDcState];
}


void Die::ID( UINT32 val )
{
    _nId = val;

    for(UINT8 nIdx = 0; nIdx < _stDevConfig._nNumsPlane; ++nIdx)
    {
#ifndef WITHOUT_PLANE_STATS
        _vctPlanes[nIdx].ID((val * _stDevConfig._nNumsPlane) + nIdx);
#endif
    }
}


UINT64 Die::nandArrayTimeParam(UINT32 nRow, INI_DEVICE_VALUE eName)
{
    UINT64 nTimeParam;

#if 0
	static UINT32 testStaticVar[] = {413,413,487,487,1390,2485,414,414,1750,3143,488,414,1711,3048,488,414,1731,2976,488,414,1711,3041,414,414,1731,2938,488,488,1730,3022,488,488,1731,3041,414,414,1730,3133,414,414,1711,3022,414,414,1711,3126,414,414,1731,3061,414,415,1731,3113,488,415,1731,2957,414,414,1730,3003,414,414,1711,3003,414,414,1711,3022,414,414,1711,3022,414,414,1731,3068,414,414,1665,3022,414,414,1730,3010,488,414,1685,2956,414,415,1731,3022,414,414,1750,3041,414,414,1711,3028,414,414,1711,3002,414,414,1731,2983,414,51,1480,2781,488,414,1480,2538,488,414,1646,3021,414,414,1750,3041,488,415,1711,2937,414,414,1712,2996,488,414,1665,3002,414,414,1731,3002,488,414,1646,2957,414,414,1730,2938,414,488,1665,2938,414,414,1730,2918,414,414,1646,2918,414,414,1711,3021,414,414,1646,2983,414,414,1646,3022,414,414,1646,2976,414,414,1711,3022,414,414,1665,3041,414,414,1665,3021,414,488,1646,2938,414,414,1730,2957,414,414,1665,2918,414,414,1750,3002,415,414,1665,2852,414,414,1710,2982,414,414,1646,2957,414,414,1730,3041,414,414,1666,2956,414,414,1711,3002,414,414,1685,3022,414,414,1646,3022,488,414,1711,2956,414,414,1711,2963,488,414,1666,2852,414,414,1750,3059,1463,1607};

	
	if(eName == ITV_tPROG)
	{
		UINT32 nPgo = NAND_PGO_PARSE_REGISTER(nRow);
		assert(nPgo < 128);
		return testStaticVar[nPgo] * 1000;
	}
#endif

    if(NFS_GET_ENV(IEV_PARAM_BASED_SIMULATION) == 1)
    {
        // worst case
        nTimeParam  = NFS_GET_PARAM(eName);
    }
    else if (NFS_GET_ENV(IEV_PARAM_BASED_SIMULATION) == 2)
    {
        // typical case
        switch(eName)
        {
        case ITV_tPROG:
            eName = IMV_tPROG;
            break;
        case ITV_tDCBSYR1:
            eName = IMV_tDCBSYR1;
            break;
        case ITV_tDCBSYR2:
            eName = IMV_tDCBSYR2;
            break;
        case ITV_tBERS:
            eName = IMV_tBERS;
            break;
        case ITV_tCBSY:
            eName = IMV_tCBSY;
            break;
        case ITV_tDBSY:
            eName = IMV_tDBSY;
            break;
        }
        nTimeParam = NFS_GET_PARAM(eName);
    }
    else if (NFS_GET_ENV(IEV_CMLC_STYLE_VARIATION) == 1)
    {
        UINT32 nPgo = NAND_PGO_PARSE_REGISTER(nRow);
        assert(nPgo < _stDevConfig._nNumsPgPerBlk);
        if(nPgo < 2 || nPgo >= (_stDevConfig._nNumsPgPerBlk - 2)|| nPgo % 2 == 0)
        {
            switch(eName)
            {
            case ITV_tPROG:
                eName = IMV_tPROG;
                break;
            case ITV_tDCBSYR1:
                eName = IMV_tDCBSYR1;
                break;
            case ITV_tDCBSYR2:
                eName = IMV_tDCBSYR2;
                break;
            case ITV_tBERS:
                eName = IMV_tBERS;
                break;
            case ITV_tCBSY:
                eName = IMV_tCBSY;
                break;
            case ITV_tDBSY:
                eName = IMV_tDBSY;
                break;
            }
            nTimeParam = NFS_GET_PARAM(eName);
        }
        else
        {
            nTimeParam  = NFS_GET_PARAM(eName);           
        }
    }
    else
    {
        UINT32 nPgo = NAND_PGO_PARSE_REGISTER(nRow);
        assert(nPgo < _stDevConfig._nNumsPgPerBlk);
        if(nPgo < 4 || nPgo % 4 == 0 || nPgo % 4 == 1)
        {
            switch(eName)
            {
            case ITV_tPROG:
                eName = IMV_tPROG;
                break;
            case ITV_tDCBSYR1:
                eName = IMV_tDCBSYR1;
                break;
            case ITV_tDCBSYR2:
                eName = IMV_tDCBSYR2;
                break;
            case ITV_tBERS:
                eName = IMV_tBERS;
                break;
            case ITV_tCBSY:
                eName = IMV_tCBSY;
                break;
            case ITV_tDBSY:
                eName = IMV_tDBSY;
                break;
            }
            nTimeParam = NFS_GET_PARAM(eName);
        }
        else
        {
            nTimeParam  = NFS_GET_PARAM(eName);           
        }
    }
    return nTimeParam;   
}

//////////////////////////////////////////////////////////////////////////////// 
//
// Method:    CheckRb
// FullName:  Die::CheckRb
// Access:    public 
// Returns:   bool
//
// Descriptions -
// This is the function simulating a ready and busy pin 
// Note that this is just emulate the status that system is busy doing 
// NAND activities itself not I/O bus activities.
//////////////////////////////////////////////////////////////////////////////
bool Die::CheckRb()
{
    // ready and busy pin
    return !_bNandBusy;
}

void Die::HardReset(UINT32 nSystemClock, NandDeviceConfig &stDevConfig)
{
    _stDevConfig            = stDevConfig;
    _nCurrentTime           = nSystemClock;
    _bPowerSupply           = false;
    _nCurNandClockIdleTime  = 0;

    for(UINT16 nIdx = 0; nIdx < NAND_DC_MAX; ++nIdx)
    {
        _vctPowerTime[nIdx] = 0;
    }

    for(UINT16 nIdx = 0; nIdx < NAND_FSM_MAX; ++nIdx)
    {
        _vctAccumulatedTime[nIdx] = 0;
    }

    for(UINT8 nIdx = 0; nIdx < _stDevConfig._nNumsPlane; ++nIdx)
    {
        _vctColRegister[nIdx] = NULL_SIG(UINT16);
        _vctRowRegister[nIdx] = NULL_SIG(UINT32);
        _vctRandomBytes[nIdx] = 0;
#ifndef WITHOUT_PLANE_STATS
        _vctPlanes[nIdx].HardReset(_stDevConfig);
#endif
    }

    SoftReset();

}

}