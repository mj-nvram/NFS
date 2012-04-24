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

#ifndef _NandStageBuilderTool_h__
#define _NandStageBuilderTool_h__

namespace NANDFlashSim {

class NandStageBuilderTool {
private :
    NandDeviceConfig        _stNandDevConfig;

public :
    inline void    SetDeviceConfig(NandDeviceConfig &stDevConfig)   {_stNandDevConfig   = stDevConfig;}
    NandStageBuilderTool(NandDeviceConfig &stDevConfig);
    //////////////////////////////////////////////////////////////////////////
    // Basic Block Erase: EraseBlock
    // Multiplane Erase : EraseNxBlock -> EraseNxBlock -> EraseNxBlock(with last flag)
    //////////////////////////////////////////////////////////////////////////
    NV_RET EraseBlock               (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 *pStatusData);
    NV_RET EraseNxBlock             (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 *pStatusData, bool bLast = false);

    //////////////////////////////////////////////////////////////////////////
    // basic I/O        : ReadPage 
    // random I/O       : ReadPage -> ReadRandomColSelection -> ReadRandomColSelection
    //////////////////////////////////////////////////////////////////////////
    NV_RET ReadPage                 (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 nNumsPulse);
    NV_RET ReadRandomColSelection   (NandStagePacket &stDataPacket, UINT32 nOpenRow, UINT32 nCol, UINT8 *pData, UINT32 nNumsPulse);
    NV_RET WritePage                (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData, UINT32 nNumsPulse, bool bRandomIn = false);
    NV_RET WriteRandomColSelection (NandStagePacket &stDataPacket, UINT32 nOpenAddr, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData, UINT32 nNumsPulse, bool bRandomLast = false);

    //////////////////////////////////////////////////////////////////////////
    // cache I/O
    // READ             : ReadPageCacheAddAddr -> ReadPageCache -> ReadPageCache
    // WRITE            : WritePageCache -> WritePageCache -> WritePageCache
    //////////////////////////////////////////////////////////////////////////
    NV_RET ReadPageCacheAddAddr     (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow);
    NV_RET ReadPageCache            (NandStagePacket &stDataPacket, UINT32 nOpenRow, UINT8 *pData);
    NV_RET WritePageCache           (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData);

    //////////////////////////////////////////////////////////////////////////
    // Multiplane I/O with basic mode cache mode and random mode, 
    // Nx READ          : ReadNxPlaneAddAddr -> ReadNxPlaneAddAddr(with last flag) -> ReadNxPlaneSelection -> ReadNxPlaneSelection(with last flag) 
    // Nx Random READ   : ReadNxPlaneAddAddr -> ReadNxPlaneAddAddr(with last flag) -> 
    //                    ReadNxPlaneSelection -> ReadNxPlaneRandomColSelection -> ReadNxPlaneRandomColSelection ->
    //                    ReadNxPlaneSelection(with last flag) -> ReadNxPlaneRandomColSelection -> ReadNxPlaneRandomColSelection ->
    //
    // Nx WRITE         : WriteNxPlane -> WriteNxPlane -> WriteNxPlane(with last flag)
    // Nx WRITE(cache)  : WriteNxPlaneCache -> WriteNxPlaneCache (with last flag(plane)) -> 
    //                    WriteNxPlaneCache -> WriteNxPlaneCache (with last flag(last trans)) 
    //                    The order of plane addressing has high priority and the order of page addressing (cache) has low priority
    // Nx WRITE(RANDOM) : WriteNxPlaneForRandom -> WriteNxPlaneRandomColSelection -> WriteNxPlaneRandomColSelection (with last flag(plane)) ->
    //                    WriteNxPlaneForRandom -> WriteNxPlaneRandomColSelection -> WriteNxPlaneRandomColSelection (with last flag(trans))  ->
    //////////////////////////////////////////////////////////////////////////
    NV_RET ReadNxPlaneAddAddr       (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT16 nCol = 0, bool bLast = false); // no callback
    NV_RET ReadNxPlaneSelection     (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT8 *pData, UINT32 nNumsPulse, UINT16 nCol = 0, bool bLast = false);
    NV_RET ReadNxPlaneRandomColSelection(NandStagePacket &stDataPacket, UINT32 nOpenRow, UINT32 nCol, UINT8 *pData, UINT32 nNumsPulse);
    NV_RET WriteNxPlane             (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData, bool bLastPlane);
    NV_RET WriteNxPlaneCache        (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData, bool bLastPlane = false, bool bLastStage= false);
    NV_RET WriteNxPlaneForRandom    (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 nCol, UINT8 *pData, UINT32 nNumsPulse);
    NV_RET WriteNxPlaneRandomColSelection(NandStagePacket &stDataPacket, UINT32 nOpenRow, UINT32 nCol, UINT8 *pData, UINT32 *pStatusData, UINT32 nNumsPulse, bool bLastRand = false, bool bLastStage= false );

    //////////////////////////////////////////////////////////////////////////
    // Internal I/O operation
    //////////////////////////////////////////////////////////////////////////
    NV_RET ReadInternalPage         (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow);
    NV_RET ReadInternalPageNxPlane  (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, bool bLast = false);
    NV_RET WriteInternalPage        (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 *pStatusData);
    NV_RET WriteInternalPageNxPlane (NandStagePacket &stDataPacket, UINT32 nSemiPhyRow, UINT32 *pStatusData, bool bLast = false);

    /************************************************************************/
    /* control interface                                                    */
    /************************************************************************/
    NV_RET ReadStatus               (NandStagePacket &stDataPacket, UINT8 nDie, UINT32 *pStatusData);
    NV_RET Reset                    (NandStagePacket &stDataPacket, UINT8 nDie);

    /************************************************************************/
    /* MISC                                                                 */
    /************************************************************************/
    void            breakdownSemiPhysicalAddr( UINT32 nDestAddr, UINT8 &nDie, UINT8 &nPlane, UINT16 &nPbn, UINT8 &nPpo );
};

}

#endif // _NandStageBuilderTool_h__