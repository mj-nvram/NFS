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
	created:	2011/07/27
	created:	27:7:2011   17:51
	file base:	Tools
	file ext:	h
	author:		Myoungsoo Jung
				Pennsylvania State University, MDL Lab
	
	purpose:	
*********************************************************************/

#include <string>
#include <map>
#include <vector>
#include <list>
#include <iostream>
#include <fstream>


#define   NFS_PARSE_DIE_ADDR(_addr, _bits)		(( _addr >> (_bits._blk + _bits._page + _bits._plane)) & ((1<<_bits._die)-1))

namespace NANDFlashSim{
    namespace tool {
        unsigned short GetBits(unsigned int nNums);
        void LoadDeviceConfig(NandDeviceConfig  &stDevConfig);
    }
}