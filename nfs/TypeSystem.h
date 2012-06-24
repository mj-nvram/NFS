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
	created:	2011/07/11
	created:	11:7:2011   14:20
	file base:	TypeSystem
	file ext:	h
	author:		Myoungsoo Jung
				Pennsylvania State University, MDL Lab
	
	purpose:	TypeSystem defines several identifier and data structure
    that is required to handle NANDFlashSim.
*********************************************************************/

#ifndef _TypeSystem_h__
#define _TypeSystem_h__

/************************************************************************/
/* primitive                                                            */
/************************************************************************/

typedef unsigned short	                UINT16;
typedef unsigned int	                UINT32;
typedef long long unsigned int          UINT64;
typedef unsigned char                   UINT8;
typedef UINT32                          NV_RET;



#define		    ADDRESS_MASK(n)				((1L << (n)) - 1)
#define		    NULL_SIG(n)					((n)(-1))							// 'n' must be a TYPE. (UINT32, UINT8, ...)

#ifndef         NULL
#define         NULL                        (0)
#endif

#ifndef         BOOL
#define         BOOL                        UINT8
#endif

#ifndef         TRUE
#define         TRUE                        (1)
#endif

#ifndef         FALSE
#define         FALSE                       (0)
#endif

/************************************************************************/
/* NAND                                                                 */
/************************************************************************/

#define		    NV_ERROR(str) std::cerr<<"[ERROR ("<<__FILE__<<":"<<__LINE__<<")]: "<<str<<std::endl;
#define         NV_RETURN_VAL(_class, _major , _minor)		(((UINT32)((_class) & 0x00000001) << 31) | \
                                                            ((UINT32)((_major) & 0x00007FFF) << 16) | \
                                                            (UINT32)((_minor) & 0x0000FFFF))


#define         NV_PARSE_RETURN_MAJOR(_ret)                 ((0x7FFFF0000 & _ret)    >> 16)
#define         NV_PARSE_RETURN_MINOR(_ret)                 (0xFFFFF & _ret)

/************************************************************************/
/* 
    In a conventional address mapping that a NAND flash memory system 
    employed, the page address consists of 7 bits, the plane address is 
    represented by 1 bit, and the block address is composed by 12 bits.

    In other words, the number of plane, pages, blocks are limited to 
    be reconfigured. For example, based on the common address layout,
    memory system can only have two planes within a packages.
    
    NANDFlashSim is designed to reconfigurable architecture, which is 
    determined by the INI file that user defines. Thus, NANDFlashSim 
    employs a little different address layout as follows:
    (8 bits for page address, 4 bits for plane address, and 
    16 bits for block address)

    |--DIE--|---------BLOCK---------------|--PLANE--|--------------PAGE-----------------|


    NOTE:
    a SystemC or pin-level designer may need to adopt this address layout to their own 
    I/O pin layout. 
                                                                        */
/************************************************************************/

// all macro tools related to the address layout will be automatically 
// changed by following bit definition:
#define         BITS_NAND_PAGE_ADDR                     (8)
#define         BITS_NAND_BLOCK_ADDR                    (16)
#define         BITS_NAND_PLANE_ADDR                    (4)
#define         BITS_NAND_DIE_ADDR                      (4)


// querying the page size from device configuration structure.
#define         NAND_PLANE_SIZE(_stDevConfig)               (_stDevConfig._nPgSize * _stDevConfig._nNumsPgPerBlk * _stDevConfig._nNumsBlk)
#define         NAND_BLOCK_SIZE(_stDevConfig)               (_stDevConfig._nPgSize * _stDevConfig._nNumsPgPerBlk)

// Parsing an address residing in the row address register fully depends on the bit configuration macro (BITS_XXX).
// There is nothing for you to change this macro for parsing address registers.
#define         NAND_PBN_PARSE_REGISTER(_nRow)              ((UINT16)(((ADDRESS_MASK(BITS_NAND_BLOCK_ADDR) << (BITS_NAND_PAGE_ADDR +  BITS_NAND_PLANE_ADDR)) & _nRow) >> (BITS_NAND_PAGE_ADDR + BITS_NAND_PLANE_ADDR)))
#define         NAND_PGO_PARSE_REGISTER(_nRow)              ((UINT8)(ADDRESS_MASK(BITS_NAND_PAGE_ADDR) & _nRow))
#define         NAND_PLN_PARSE_REGISTER(_nRow)              ((UINT8)(((ADDRESS_MASK(BITS_NAND_PLANE_ADDR) << BITS_NAND_PAGE_ADDR) & _nRow) >> BITS_NAND_PAGE_ADDR))
#define         NAND_DIE_PARSE_REGISTER(_nRow)              (UINT8)(((~(ADDRESS_MASK(BITS_NAND_PAGE_ADDR +  BITS_NAND_PLANE_ADDR + BITS_NAND_BLOCK_ADDR))) & _nRow) >> (BITS_NAND_PAGE_ADDR + BITS_NAND_PLANE_ADDR + BITS_NAND_BLOCK_ADDR))

// tools for manipulating row registers.
#define         NAND_SET_PLANE_REGISTER(_nRow, _nPlane)     ((_nRow & ((ADDRESS_MASK(BITS_NAND_BLOCK_ADDR + BITS_NAND_DIE_ADDR) << (BITS_NAND_PAGE_ADDR + BITS_NAND_PLANE_ADDR)) | ADDRESS_MASK(BITS_NAND_PAGE_ADDR))) |\
                                                            (_nPlane << BITS_NAND_PAGE_ADDR))

// this will be obsoleted soon
#define         NAND_COMPOSE_ROW_RIGSTER(_nDie, _nPlane, _nPbn, _nPpo)  (((ADDRESS_MASK(BITS_NAND_DIE_ADDR) & _nDie) << (BITS_NAND_PAGE_ADDR + BITS_NAND_PLANE_ADDR + BITS_NAND_BLOCK_ADDR)) |\
                                                                        ((ADDRESS_MASK(BITS_NAND_BLOCK_ADDR) & _nPbn) << (BITS_NAND_PAGE_ADDR + BITS_NAND_PLANE_ADDR)) |\
                                                                        ((ADDRESS_MASK(BITS_NAND_PLANE_ADDR) & _nPlane) << BITS_NAND_PAGE_ADDR) |\
                                                                        ((1 << BITS_NAND_PAGE_ADDR) - 1) & _nPpo)



/************************************************************************/
/* return code                                                          */
/************************************************************************/

#define         NAND_ERROR                          (1)
#define         NAND_SUCCESS                        (0)
#define         NAND_SYS                            (0x0)
#define         NAND_SYS_ERROR                      NV_RETURN_VAL(NAND_ERROR, NAND_SYS,0x0)
#define         NAND_SYS_ERROR_ADDRESS              NV_RETURN_VAL(NAND_ERROR, NAND_SYS,0x1)
#define         NAND_SYS_ERROR_INCOMPLETE           NV_RETURN_VAL(NAND_ERROR, NAND_SYS,0x4)
#define         NAND_SYS_ERROR_UNDEFINED_ORDER      NV_RETURN_VAL(NAND_ERROR, NAND_SYS,0x8)

#define         NAND_DIE                            (0x1)
#define         NAND_DIE_ERROR                      NV_RETURN_VAL(NAND_ERROR, NAND_DIE,0x0)
#define         NAND_DIE_ERROR_BUSY                 NV_RETURN_VAL(NAND_ERROR, NAND_DIE,0x1)

#define         NAND_CTRL                           (0x2)
#define         NAND_CTRL_ERROR                     NV_RETURN_VAL(NAND_ERROR, NAND_CTRL,0x0)
#define         NAND_CTRL_ERROR_ADDRESS             NV_RETURN_VAL(NAND_ERROR, NAND_CTRL,0x1)
#define         NAND_CTRL_ERROR_INVALID_PARAM       NV_RETURN_VAL(NAND_ERROR, NAND_CTRL,0x2)
#define         NAND_CTRL_ERROR_UNDEFINED_ORDER     NV_RETURN_VAL(NAND_ERROR, NAND_CTRL,0x8)
#define         NAND_CTRL_ERROR_IOLENGTH            NV_RETURN_VAL(NAND_ERROR, NAND_CTRL,0x4)

#define         NAND_FLASH                          (0x4)
#define         NAND_FLASH_ERROR                    NV_RETURN_VAL(NAND_ERROR, NAND_FLASH,0x0)
#define         NAND_FLASH_ERROR_BUSY               NV_RETURN_VAL(NAND_ERROR, NAND_FLASH,0x1)
#define         NAND_FLASH_ERROR_UNSUPPORTED        NV_RETURN_VAL(NAND_ERROR, NAND_FLASH,0x2) 

#define         NAND_PLANE                          (0x8)
#define         NAND_PLANE_ERROR                    NV_RETURN_VAL(NAND_ERROR, NAND_PLANE,0x0)
#define         NAND_PLANE_ERROR_ADDRESS            NV_RETURN_VAL(NAND_ERROR, NAND_PLANE,0x1)
#define         NAND_PLANE_ERROR_NOP_VIOLOATION     NV_RETURN_VAL(NAND_ERROR, NAND_PLANE,0x2)
#define         NAND_PLANE_ERROR_WEAROUT            NV_RETURN_VAL(NAND_ERROR, NAND_PLANE,0x4)
#define         NAND_PLANE_ERROR_INODRDER_VIOLATION NV_RETURN_VAL(NAND_ERROR, NAND_PLANE,0x8)



namespace NANDFlashSim {

/************************************************************************/
/* IO TYPE                                                              */
/************************************************************************/

typedef enum {
    NAND_OP_READ,
    NAND_OP_READ_RANDOM,
    NAND_OP_READ_CACHE,
    NAND_OP_READ_MULTIPLANE,
    NAND_OP_READ_MULTIPLANE_RANDOM,
    NAND_OP_PROG,
    NAND_OP_PROG_RANDOM,
    NAND_OP_PROG_CACHE,
    NAND_OP_PROG_MULTIPLANE,
    NAND_OP_PROG_MULTIPLANE_RANDOM,
    NAND_OP_PROG_MULTIPLANE_CACHE,
    NAND_OP_INTERNAL_DATAMOVEMENT,
    NAND_OP_INTERNAL_DATAMOVEMENT_MULTIPLANE,
    NAND_OP_BLOCK_ERASE,
    NAND_OP_BLOCK_ERASE_MULTIPLANE,
    NAND_OP_NOT_DETERMINED
} NAND_TRANS_OP;

/************************************************************************/
/* NAND COMMAND SET ARCTHIECTURE                                        */
/************************************************************************/

// identifiers for different NAND command
typedef enum {
    NAND_CMD_READ_PAGE,                  
    NAND_CMD_READ_PAGE_CONF,             
    NAND_CMD_READ_MULTIPLANE_INIT,       
    NAND_CMD_READ_MULTIPLANE_INIT_CONF,  
    NAND_CMD_READ_MULTIPLANE_INIT_FIN,          // Informing final command
    NAND_CMD_READ_MULTIPLANE_INIT_FIN_CONF,
    NAND_CMD_READ_MULTIPLANE,            
    NAND_CMD_READ_MULTIPLANE_CONF,       
    NAND_CMD_READ_CACHE_ADDR_INIT,       
    NAND_CMD_READ_CACHE_ADDR_INIT_CONF,
    NAND_CMD_READ_CACHE,                 
    NAND_CMD_READ_MULTIPLANE_CACHE,      
    NAND_CMD_READ_MULTIPLANE_CACHE_DUMMY,
    NAND_CMD_READ_MULTIPLANE_CACHE_CONF,        // change plane
    NAND_CMD_READ_INTERNAL,
    NAND_CMD_READ_INTERNAL_CONF,         
    NAND_CMD_READ_INTERNAL_MULTIPLANE,
    NAND_CMD_READ_INTERNAL_MULTIPLANE_FIN,
    NAND_CMD_READ_INTERNAL_MULTIPLANE_FIN_CONF,
    NAND_CMD_READ_RANDOM,                
    NAND_CMD_READ_RANDOM_CONF,           
    NAND_CMD_READ_STATUS,                

    NAND_DELIMITER_CMD_READ,                    // delimiter

    NAND_CMD_PROG_PAGE,                  
    NAND_CMD_PROG_PAGE_CONF,             
    NAND_CMD_PROG_MULTIPLANE,
    NAND_CMD_PROG_MULTIPLANE_CONF,
    NAND_CMD_PROG_MULTIPLANE_RANDOM,
    NAND_CMD_PROG_MULTIPLANE_RANDOM_DUMMY,
    NAND_CMD_PROG_MULTIPLANE_RANDOM_DUMMY_CONF,
    NAND_CMD_PROG_MULTIPLANE_FIN,
    NAND_CMD_PROG_MULTIPLANE_FIN_CONF,
    NAND_CMD_PROG_MULTIPLANE_FIN_RANDOM,
    NAND_CMD_PROG_MULTIPLANE_FIN_RANDOM_CONF,
    NAND_CMD_PROG_MULTIPLANE_CACHE,
    NAND_CMD_PROG_MULTIPLANE_CACHE_CONF,
    NAND_CMD_PROG_MULTIPLANE_CACHE_FIN,
    NAND_CMD_PROG_MULTIPLANE_CACHE_FIN_CONF,
    NAND_CMD_PROG_CACHE,
    NAND_CMD_PROG_CACHE_CONF,
    NAND_CMD_PROG_RANDOM,
    NAND_CMD_PROG_RANDOM_FIN,                
    NAND_CMD_PROG_RANDOM_FIN_CONF,                
    NAND_CMD_PROG_INTERNAL,
    NAND_CMD_PROG_INTERNAL_CONF,
    NAND_CMD_PROG_INTERNAL_MULTIPLANE,
    NAND_CMD_PROG_INTERNAL_MULTIPLANE_CONF,
    NAND_CMD_PROG_INTERNAL_MULTIPLANE_FIN,
    NAND_CMD_PROG_INTERNAL_MULTIPLANE_FIN_CONF,
    NAND_CMD_BLOCK_ERASE,                
    NAND_CMD_BLOCK_ERASE_CONF,           
    NAND_CMD_BLOCK_MULTIPLANE_ERASE,
    NAND_CMD_BLOCK_MULTIPLANE_ERASE_FIN,
    NAND_CMD_BLOCK_MULTIPLANE_ERASE_FIN_CONF,
    NAND_CMD_RESET,
    NAND_CMD_NOT_DETERMINED
} NAND_COMMAND ;


// Identifiers for multi-stage operations.
typedef enum {
    NAND_STAGE_ALE,
    NAND_STAGE_CLE,
    NAND_STAGE_TOR,
    NAND_STAGE_TIR,
    NAND_STAGE_READ_STATUS,
    NAND_STAGE_RESET_DELTA,
    NAND_STAGE_IDLE,
    NAND_STAGE_BUSY,

    NAND_DELIMITER_STAGE_FOR_NAND_ARRAY_TASK,   // delimiter

    NAND_STAGE_TON,
    NAND_STAGE_TIN,
    NAND_STAGE_TIN_CACHE,
    NAND_STAGE_TIN_DUMMY,
    NAND_STAGE_TIN_TAIL,
    NAND_STAGE_NOT_DETERMINED
} NAND_STAGE;


typedef enum {
    NAND_FSM_ALE,
    NAND_FSM_CLE,
    NAND_FSM_TIR,
    NAND_FSM_TOR,
    NAND_FSM_TIN,
    NAND_FSM_TON,
    NAND_FSM_ERASE,
    NAND_FSM_MAX
} NAND_FSM_STATE;

typedef enum {
    NAND_DC_READ,
    NAND_DC_PROG,
    NAND_DC_ERASE,
    NAND_DC_STANDBY,
    NAND_DC_LEAKAGE,
    NAND_DC_MAX
} NAND_DC;

typedef enum {
    NAND_ISR_COMPLETE_TRANS
} NAND_ISR_TYPE;

struct Transaction {
    NAND_TRANS_OP   _nTransOp;
    UINT32          _nHostTransId;          
    UINT8           *_pData;
    UINT32          *_pStatusData;
    UINT32          _nAddr;                // physical address. 
    UINT32          _nDestAddr;            // for internal data move data model
    UINT32          _nByteOff;
    UINT32          _nNumsByte;
    bool            _bAutoPlaneAddressing; // whether or not NANDFlashSim internally generates plane address by respecting to plane address rule.
    bool            _bLastNxSubTrans;      // for cache or random mode with multi-plane
    bool            _bLastPlane;           // for manual plane addressing mode.


    Transaction() {
        _nTransOp               = NAND_OP_NOT_DETERMINED;
        _pData                  = NULL;
        _pStatusData            = NULL;
        _nHostTransId           = NULL_SIG(UINT32);
        _nAddr                  = NULL_SIG(UINT32);
        _nDestAddr              = NULL_SIG(UINT32);
        _nByteOff               = NULL_SIG(UINT32);
        _bAutoPlaneAddressing   = true;
        _bLastNxSubTrans        = false;
        _bLastPlane             = false;
    }
};

/************************************************************************/
/*                                                                      
    NandStagePacket is the structure to specify information, which is
    required to handle NAND commands and data movements. 
    
    One transaction are composed by multiple NAND operation stages. Each
    stage should be build with appropriate information based on the sequence
    that flash datasheet specifies. For example, cache mode or multi-plane
    mode operations have different command sequence and data movement sequences
    based on the number of pages in a block or the number of plane that you 
    specifies in your own INI file. When you handle the last command (for the
    last page for the cache mode operation or the last plane), you should inform
    such thing to memory system using the _bLastCmdForFgTrans flag.

    Arrival cycle value need to be initiated by users start to initiate a transaction
    (not stage).
    If you want to directly handle NAND commands and data movements, please
    check NandStageBuilderTool class as an example. 
                                                                        */
/************************************************************************/
struct NandStagePacket
{
    UINT32              _nStageId;
    NAND_COMMAND        _nCommand;
    UINT8               *_pData;
    UINT32              *_pStatusData;          // pointer for 32bit storage
    UINT32              _nRow;
    UINT16              _nCol;
    UINT32              _nRandomBytes;
    UINT64              _nArrivalCycle;
    bool                _bLastCmdForFgTrans;

    NandStagePacket(UINT32 nId, UINT64 nArrivalCycle) 
    {
        _nStageId           = nId; 
        _nCommand           = NAND_CMD_NOT_DETERMINED;
        _pData              = NULL;
        _pStatusData        = NULL;
        _nRow               = NULL_SIG(UINT32);
        _nCol               = 0;
        _nRandomBytes       = NULL_SIG(UINT32);
        _nArrivalCycle      = nArrivalCycle;
        _bLastCmdForFgTrans = false;
    }
};

typedef struct {
    UINT32  _nPgSize;
    UINT32  _nNumsPgPerBlk;
    UINT32  _nNumsBlk;
    UINT32  _nNumsPlane;
    UINT32  _nNumsDie;
    UINT8   _nNumsLun;
    UINT16  _nCacheDepth;
    UINT32  _nTransBusDepth;

    UINT32  _nNumsIoPins;
    UINT32  _nDeviceId;
    UINT32  _nNop;
    UINT32  _nEc;

    struct {
        UINT16  _pgsize;
        UINT16  _page;
        UINT16  _plane;
        UINT16  _blk;
        UINT16  _die;
        UINT16  _lun;
    } _bits;
}NandDeviceConfig;

}   // the end of NANDFlashSim namespace

#define         ZERO_TIME                                   (0)
#define         NAND_VIRTUAL_BLOCK_IDX_RESOLUTION           (8)           

#endif // _TypeSystem_h__
