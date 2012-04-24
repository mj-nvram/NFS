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

namespace NANDFlashSim {
    namespace tool 
    {
        unsigned short GetBits(unsigned int nNums)
        {
            unsigned short nBits = 0;

            if((nNums & (nNums - 1)) != 0)
            {
                throw "This number is not a 2 to n";
            }

            
            while(nNums > 1)
            {
                nNums = nNums >> 1;
                nBits++;
            }


            return nBits;
        }

        void LoadDeviceConfig(NandDeviceConfig  &stDevConfig)
        {
            // NANDFlashSim beta version does not use _nCacheDepth, nNumsLun (Multiple LUN will be support in the next version),
            // _nTransBusDepth options.
            stDevConfig._nCacheDepth    = 1;
            stDevConfig._nNumsLun       = 1;
            stDevConfig._nTransBusDepth = 1;
            stDevConfig._nDeviceId      = 0xbeefdead;

            stDevConfig._nNumsBlk       = NFS_GET_PARAM(ISV_NUMS_BLOCKS);
            stDevConfig._nEc            = NFS_GET_PARAM(ISV_MAX_ERASE_CNT);
            stDevConfig._nNop           = NFS_GET_PARAM(ISV_NOP);
            stDevConfig._nNumsDie       = NFS_GET_PARAM(ISV_NUMS_DIE);
            stDevConfig._nNumsIoPins    = NFS_GET_PARAM(ISV_NUMS_IOPINS);
            stDevConfig._nNumsPgPerBlk  = NFS_GET_PARAM(ISV_NUMS_PAGES);
            stDevConfig._nNumsPlane     = NFS_GET_PARAM(ISV_NUMS_PLANE);
            stDevConfig._nPgSize        = NFS_GET_PARAM(ISV_NUMS_PGSIZE);

            stDevConfig._bits._pgsize    = GetBits(stDevConfig._nPgSize);
            stDevConfig._bits._blk       = GetBits(stDevConfig._nNumsBlk);
            stDevConfig._bits._die       = GetBits(stDevConfig._nNumsDie);
            stDevConfig._bits._page      = GetBits(stDevConfig._nNumsPgPerBlk);
            stDevConfig._bits._plane     = GetBits(stDevConfig._nNumsPlane);
            stDevConfig._bits._lun       = GetBits(stDevConfig._nNumsLun);
        }
    }
}

