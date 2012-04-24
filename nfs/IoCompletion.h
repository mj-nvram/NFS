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
	created:	2011/07/10
	created:	10:7:2011   19:18
	file base:	IoCompletion
	file ext:	h
	author:		Myoungsoo Jung
				Pennsylvania State University, MDL Lab
	
	purpose:	
*********************************************************************/

#ifndef _IoCompletion_h__
#define _IoCompletion_h__

namespace NANDFlashSim {

template<typename RETURN, typename PARAM1, typename PARAM2, typename PARAM3>
class CallbackBase
{
public :
	virtual RETURN operator()(PARAM1 param1, PARAM2 param2, PARAM3 param3) = 0;
};

template <typename OWNER, typename RETURN, typename PARAM1, typename PARAM2, typename PARAM3>
class CallbackRepository : public CallbackBase<RETURN, PARAM1, PARAM2, PARAM3> {
private :
	typedef	RETURN (OWNER::*COMP_ROUTINE)(PARAM1, PARAM2, PARAM3);
	
	OWNER* const		_pInstance;
	const COMP_ROUTINE	_pCompIoFunc;	

public : 
	CallbackRepository(OWNER *pInstance, COMP_ROUTINE compIoFunc) : _pInstance(pInstance), _pCompIoFunc(compIoFunc){}

	virtual RETURN operator()(PARAM1 param1, PARAM2 param2, PARAM3 param3)
	{
		return (_pInstance->*_pCompIoFunc)(param1, param2, param3);
	}
};


typedef CallbackBase<void, NAND_ISR_TYPE, UINT32, UINT64> NandSystemIsr;
// Transaction ID, arrival cycle, and completion cycles;
typedef CallbackBase<void, UINT32, UINT64, UINT64> NandIoCompletion;

}


#endif // _IoCompletion_h__