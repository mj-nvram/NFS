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

/************************************************************************/
/*  
    IMPORTANT:
    NANDFlashSim was originally designed as a shared/static library, 
    which is connected to a host simulation model, not a standalone 
    simulator. 

    Please note that this sample system doesn't have any its own system clock,
    and any access rule for NANDFlashSim, and this is not a part of NANDFlashSim.
    If you want to perform event-driven or trace-driven simulation 
    with full functionalities, you need to build device diver and flash firmware 
    on "NandFlashSystem" by your own policy.    
                                                                        */
/************************************************************************/

#include "TypeSystem.h"
#include "Tools.h"
#include "ParamManager.h"
#include "IoCompletion.h"
#include "Plane.h"
#include "Die.h"
#include "LogicalUnit.h"
#include "NandStageBuilderTool.h"
#include "NandController.h"
#include "NandFlashSystem.h"

#include <string>

#include "boost/program_options.hpp"

namespace po = boost::program_options;
using namespace NANDFlashSim;

typedef enum {
    TESTUNIT_OP_LEGACY,
    TESTUNIT_OP_CACHE,
    TESTUNIT_OP_NX,
    TESTUNIT_OP_COPYBACK, 
    TESTUNIT_OP_NX_CACHE,
    TESTUNIT_OP_MAX_OPTYPES
} TESTUNIT_OP;


typedef enum {
    DIE_RR_SCHEDULE,
    DIE_FAIR_SCHEDULE,
};

/*#define     BUILD_PHYSICAL_ADDR(_nDie, _nPlane, _nPbn, _nPpo, _stNandDevConfig) \
                                    (_nPpo) + ((_nPlane) * _stNandDevConfig._nNumsPgPerBlk) +\
                                    ((_nPbn) * _stNandDevConfig._nNumsPgPerBlk * _stNandDevConfig._nNumsPlane) +\
                                    (_nDie * _stNandDevConfig._nNumsPgPerBlk * _stNandDevConfig._nNumsPlane * _stNandDevConfig._nNumsBlk)*/

#define BUILD_PHYSICAL_ADDR(_nDie, _nPlane, _nPbn, _nPpo, _stNandDevConfig) \
                                    (_nPpo) | ((_nPlane) << _stNandDevConfig._bits._page) | \
                                    ((_nPbn) << (_stNandDevConfig._bits._page + _stNandDevConfig._bits._plane)) | \
                                    ((_nDie) << (_stNandDevConfig._bits._page + _stNandDevConfig._bits._plane + _stNandDevConfig._bits._blk))

void BasicPagebasedOpTest(TESTUNIT_OP tuOp, NANDFlashSim::NandFlashSystem &flash, UINT16 nDie, UINT32 nNumsBlk, UINT32 nNumsPage, UINT32 nTransferPageSize, bool bWrite);
void DieInterleavedOpTest(TESTUNIT_OP tuOp, NANDFlashSim::NandFlashSystem &flash, UINT16 nNumsDie, UINT32 nNumsBlk, UINT32 nNumsPageInBlk, UINT32 nTransferPageSize, bool bWrite );
void InernalDataMovement(NANDFlashSim::NandFlashSystem &flash, UINT16 nDie, UINT32 nNumsBlk, UINT32 nNumsPageInBlk, bool bMultiplane);
void EraseBlock(NANDFlashSim::NandFlashSystem &flash, UINT16 nDie, UINT32 nNumsBlk, bool bMultiplane);
bool CheckReturnValue(NV_RET nRet);

void SingleDieSequentialAccessTest(UINT32 nNumsBlocks, UINT32 nTransferSizeUnit, NandFlashSystem &flash, NandDeviceConfig &stDevConfig);
void SingleDieTest(TESTUNIT_OP nTuOp, UINT32 nNumsBlocks, UINT32 nTransferSizeUnit, NandFlashSystem &flash, NandDeviceConfig &stDevConfig, bool bWrite);
void MultiDieTest(TESTUNIT_OP nTuOp, UINT32 nNumsBlocks, UINT32 nTransferSizeUnit, NandFlashSystem &flash, NandDeviceConfig &stDevConfig);
void MultiDieSequentialAccessTest(UINT32 nNumsBlocks, UINT32 nTransferSizeUnit, NandFlashSystem &flash, NandDeviceConfig &stDevConfig);
void SingleDieTests( NandFlashSystem &flash, NandDeviceConfig stDevConfig );
void MultiDieTests( NandFlashSystem &flash, NandDeviceConfig stDevConfig );




//////////////////////////////////////////////////////////////////////////////////////////////
// NANDFlashSim was originally designed as a shared/static library, 
// which is connected to a host simulation model, not a standalone 
// simulator. 
// 
// Please note that this sample system doesn't have any its own system clock,
// and any access rule for NANDFlashSim, and this is not a part of NANDFlashSim.
// If you want to perform event-driven or trace-driven simulation 
// with full functionalities, you need to build device diver and flash firmware 
// on "NandFlashSystem" by your own policy.
//
///////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    po::options_description odIni("Ini file settings");
    odIni.add_options()
        ("devini", po::value<std::string>(), "Device ini file name. This parameter must be set.")
        ("envini", po::value<std::string>(), "Environment ini file name. This parameter must be set.")
        ;

    po::options_description odTime("Time options");
    odTime.add_options()
        ("wtprog,w", po::value<UINT32>(), "The worst page programming time (ns)")
        ("ttprog,p", po::value<UINT32>(), "The typical page programming time (ns)")
        ("tterase,e", po::value<UINT32>(), "The typical block erase time (ns)")
        ;

    po::options_description odSys("System options");
    odSys.add_options()
        ("nop,n", po::value<UINT32>(), "The number of programming, which means the maximum number being able to write without erase.")
        ("plane,l", po::value<UINT32>(), "The number of Planes")
        ("die,d", po::value<UINT32>(), "The number of Dies")
        ("blocks,b", po::value<UINT32>(), "The total number of Blocks")
        ("pages,g", po::value<UINT32>(), "The number of Pages")
        ("pagesize,u", po::value<UINT32>(), "The page unit size (byte)")
        ("spare,s", po::value<UINT32>(), "The size of spare space (byte)")
        ("pins", po::value<UINT32>(), "The number of pins, NAND I/O interface")
        ("erasecnt,c", po::value<UINT32>(), "the number of allowed block erase, that can be erased without data corruption.")
        ("cp", po::value<UINT32>(), "Clock Periods")
        ("tblocks", po::value<UINT32>()->default_value(8), "The total number of blocks for testing")
        ("tpages", po::value<UINT32>()->default_value(8), "The number of of pages for testing")        
        ;

    po::options_description odEnv("Environment options (Values must be boolean - 1 or 0.)");
    odEnv.add_options()
        ("readhistory", po::value<UINT32>(), "Print read request history")
        ("writehistory", po::value<UINT32>(), "Print write request history")
        ("interstate", po::value<UINT32>(), "Snoop multi-stage operations and print internal state changes")
        ("bustrans", po::value<UINT32>(), "Print bus-level NAND flash transaction")
        ("iocomp", po::value<UINT32>(), "Print history of I/O completions")
        ("cycleanaly", po::value<UINT32>(), "Print cycles for each state")
        ("analypower", po::value<UINT32>(), "PowerCyclesForEachDcParam")
        ("fastmode", po::value<UINT32>(), "skip common minimum cycles")
        ("variation", po::value<UINT32>(), "Choose variation generator model")
        ;

    po::options_description desc("NANDFlashSim v1.0 Parameter description");
    desc.add(odIni).add(odTime).add(odSys).add(odEnv);

    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch( const po::error &e )
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return -1;
    }

    if(!vm.count("devini") || !vm.count("envini"))
    {
        std::cout << desc << std::endl;
        return 0;
    }
    
    // Checks if existing the dev ini file and the env ini file
    FILE *fp = fopen(vm["devini"].as<std::string>().c_str(), "rt");
    if(fp == NULL)
    {
        std::cout << "ERROR : Can't open the device ini file - " << vm["devini"].as<std::string>() << std::endl;
        return -1;
    }
    else
        fclose(fp);

    fp = fopen(vm["envini"].as<std::string>().c_str(), "rt");
    if(fp == NULL)
    {
        std::cout << "ERROR : Can't open the environment ini file - " << vm["envini"].as<std::string>() << std::endl;
        return -1;
    }
    else
        fclose(fp);

    // Set and over-set values
    ParamManager::SetIniInfo(vm["devini"].as<std::string>().c_str(), vm["envini"].as<std::string>().c_str());

    char *pParamName;
    for(int i = 0; gParamTypes[i].szTypeIniName[0] != '\0'; i++)
    {
        pParamName = gParamTypes[i].szTypeParamName;
        if(pParamName[0] == '\0') continue;

        if(vm.count(pParamName))
        {
            if(gParamTypes[i].bIsEnv == TRUE)
                ParamManager::SetEnv(gParamTypes[i].eEnvValue, i, vm[pParamName].as<UINT32>());
            else
                ParamManager::SetParam(gParamTypes[i].eDeviceValue, i, vm[pParamName].as<UINT32>());

            gParamTypes[i].bValueExist = TRUE;
        }
    }

    // Check if there are all must-have parameters.
    pParamName = ParamManager::HasAllParameterValues();
    if(pParamName != NULL)
    {
        std::cout << "ERROR: The setting of '" << pParamName << "' parameter must be needed." << std::endl;
        return -1;
    }
    
    NandDeviceConfig stDevConfig;
    NANDFlashSim::tool::LoadDeviceConfig(stDevConfig);
    NandFlashSystem     flash(0, stDevConfig);
    
    flash.ReportConfiguration();
    UINT32 nTestBlockNums       = vm["tblocks"].as<UINT32>();
    UINT32 nTestPageNums        = vm["tpages"].as<UINT32>();
    try
    {

        if((nTestPageNums & (nTestPageNums - 1)) != 0)
        {
            throw "# of page is not a 2 to n";
        }

        if((nTestBlockNums & (nTestBlockNums - 1)) != 0)
        {
            throw "# of block is not a 2 to n";
        }

        SingleDieTests(flash, stDevConfig);
        SingleDieSequentialAccessTest(16,16, flash, stDevConfig);
        MultiDieTests(flash, stDevConfig);
        MultiDieSequentialAccessTest(8, 64, flash, stDevConfig);

    }
    catch (std::string strExption)
    {
        std::cout << strExption << std::endl;
    }

    return 0;
}



void SingleDieSequentialAccessTest(UINT32 nNumsBlocks, UINT32 nTransferSizeUnit, NandFlashSystem &flash, NandDeviceConfig &stDevConfig)
{
    

    for(UINT32 nTuOp = 0; nTuOp < TESTUNIT_OP_MAX_OPTYPES; ++nTuOp)
    {
        if(nTuOp >= TESTUNIT_OP_NX && stDevConfig._nNumsPlane <= 1) continue;
        if(nTuOp != TESTUNIT_OP_COPYBACK)
        {
            std::cout << "********************************************************" << std::endl;
            if(nTuOp != TESTUNIT_OP_NX_CACHE)
            {
                // read case
                std::cout << "Unit Test Operation Type [READ]: " << nTuOp << std::endl;
                BasicPagebasedOpTest((TESTUNIT_OP)nTuOp , flash, 0, nNumsBlocks, stDevConfig._nNumsPgPerBlk, nTransferSizeUnit, false);
                flash.ReportStatistics();
                flash.HardReset(0, stDevConfig);
            }

            std::cout << "Unit Test Operation Type [WRITE]: " << nTuOp << std::endl;
            BasicPagebasedOpTest((TESTUNIT_OP)nTuOp , flash, 0, nNumsBlocks, stDevConfig._nNumsPgPerBlk, nTransferSizeUnit, true);
            flash.ReportStatistics();
            flash.HardReset(0, stDevConfig);
        }
    }
}


void SingleDieTest(TESTUNIT_OP  nTuOp,  UINT32 nNumsBlocks, UINT32 nTransferSizeUnit, NandFlashSystem &flash, NandDeviceConfig &stDevConfig, bool bWrite)
{
    

    std::cout << "Unit Test Operation Type [" << (bWrite ? "WRITE] :" : "READ] : " )<< nTuOp << std::endl;
    BasicPagebasedOpTest((TESTUNIT_OP)nTuOp , flash, 0, nNumsBlocks, stDevConfig._nNumsPgPerBlk, nTransferSizeUnit, bWrite);
    flash.ReportStatistics();

}




void BlockDataMigrationTest(UINT32 nNumsBlocks, UINT32 nDie, NandFlashSystem &flash, NandDeviceConfig &stDevConfig )
{
    

    // single plane test
    EraseBlock(flash, nDie, nNumsBlocks, false);
    for(UINT32 nBlockIdx = 0; nBlockIdx < nNumsBlocks; nBlockIdx++)
    {
        InernalDataMovement(flash, nDie, nNumsBlocks, stDevConfig._nNumsPgPerBlk, false);
        flash.ReportStatistics();
        flash.HardReset(0, stDevConfig);
    }

    // mutlplane test
    EraseBlock(flash, nDie, nNumsBlocks, true);
    for(UINT32 nBlockIdx = 0; nBlockIdx < nNumsBlocks; nBlockIdx++)
    {
        InernalDataMovement(flash, nDie, nNumsBlocks, stDevConfig._nNumsPgPerBlk, true);
        flash.ReportStatistics();
        flash.HardReset(0, stDevConfig);
    }
}

void MultiDieTest(TESTUNIT_OP  nTuOp,  UINT32 nNumsBlocks, UINT32 nTransferSizeUnit, NandFlashSystem &flash, NandDeviceConfig &stDevConfig)
{
    
    std::cout << "Unit Test Operation Type [WRITE]: " << nTuOp << std::endl;
    DieInterleavedOpTest((TESTUNIT_OP)nTuOp , flash, stDevConfig._nNumsDie, nNumsBlocks, stDevConfig._nNumsPgPerBlk, nTransferSizeUnit, true);
    flash.ReportStatistics();
    flash.HardReset(0, stDevConfig);
}

void MultiDieSequentialAccessTest(UINT32 nNumsBlocks, UINT32 nTransferSizeUnit, NandFlashSystem &flash, NandDeviceConfig &stDevConfig)
{
    

    for(UINT32 nTuOp = 0; nTuOp < TESTUNIT_OP_MAX_OPTYPES; ++nTuOp)
    {
        if(nTuOp >= TESTUNIT_OP_NX && stDevConfig._nNumsPlane <= 1) continue;
        if(nTuOp != TESTUNIT_OP_COPYBACK)
        {
            std::cout << "********************************************************" << std::endl;
            if(nTuOp != TESTUNIT_OP_NX_CACHE)
            {
                // read case
                std::cout << "Unit Test Operation Type [READ]: " << nTuOp << std::endl;
                DieInterleavedOpTest((TESTUNIT_OP)nTuOp , flash, stDevConfig._nNumsDie, nNumsBlocks, stDevConfig._nNumsPgPerBlk, nTransferSizeUnit, false);
                flash.ReportStatistics();
                flash.HardReset(0, stDevConfig);
            }

            std::cout << "Unit Test Operation Type [WRITE]: " << nTuOp << std::endl;
            DieInterleavedOpTest((TESTUNIT_OP)nTuOp , flash, stDevConfig._nNumsDie, nNumsBlocks, stDevConfig._nNumsPgPerBlk, nTransferSizeUnit, true);
            flash.ReportStatistics();
            flash.HardReset(0, stDevConfig);
        }
    }
}

void SingleDieTests( NandFlashSystem &flash, NandDeviceConfig stDevConfig ) 
{
    bool bWrite = true;
    //BlockDataMigrationTest(1, 0, flash, stDevConfig);
    SingleDieTest(TESTUNIT_OP_LEGACY, 2, 16, flash, stDevConfig, bWrite );
    flash.HardReset(0, stDevConfig);

    SingleDieTest(TESTUNIT_OP_CACHE, 2, 8, flash, stDevConfig, bWrite );
    flash.HardReset(0, stDevConfig);

    if(stDevConfig._nNumsPlane > 1)
    {
        SingleDieTest(TESTUNIT_OP_NX, 2, 8, flash, stDevConfig, bWrite );
        flash.HardReset(0, stDevConfig);

        SingleDieTest(TESTUNIT_OP_NX_CACHE, 2, 8, flash, stDevConfig, bWrite );
        flash.HardReset(0, stDevConfig);
    }
}

void MultiDieTests( NandFlashSystem &flash, NandDeviceConfig stDevConfig ) 
{
    MultiDieTest(TESTUNIT_OP_CACHE, 2, 2, flash, stDevConfig);
    if(stDevConfig._nNumsPlane > 1)
    {
        MultiDieTest(TESTUNIT_OP_NX_CACHE, 2, 64, flash, stDevConfig);
        MultiDieTest(TESTUNIT_OP_NX, 2, 64, flash, stDevConfig);
    }
}



void InernalDataMovement(NandFlashSystem &flash, UINT16 nDie, UINT32 nNumsBlk, UINT32 nNumsPageInBlk, bool bMultiplane)
{
    NandDeviceConfig stDevConfig = flash.GetDeviceConfig();
    NV_RET nRet;

    if(bMultiplane)
    {
        // lsb of block address classifies plane address.
        nNumsBlk = nNumsBlk / stDevConfig._nNumsPlane;
    }

    for(UINT32 nIterBlk =0; nIterBlk < nNumsBlk; nIterBlk++)
    {
        for(UINT32 nIterPage =0; nIterPage < nNumsPageInBlk;)
        {
            nRet = flash.AddTransaction(NULL_SIG(UINT32), (bMultiplane) ? NAND_OP_INTERNAL_DATAMOVEMENT_MULTIPLANE : NAND_OP_INTERNAL_DATAMOVEMENT , BUILD_PHYSICAL_ADDR(nDie,0, nIterBlk, nIterPage, stDevConfig));
            if(!CheckReturnValue(nRet)) continue;
            nIterPage++;

            while(flash.IsActiveMode())
            {
                flash.UpdateWithoutIdleCycles();
            }
        }
    }

}

void BasicPagebasedOpTest( TESTUNIT_OP tuOp, NandFlashSystem &flash, UINT16 nDie, UINT32 nNumsBlk, UINT32 nNumsPageInBlk, UINT32 nTransferPageSize, bool bWrite )
{
    NandDeviceConfig stDevConfig = flash.GetDeviceConfig();
    NV_RET           nRet;

    if(tuOp == TESTUNIT_OP_NX_CACHE)
    {
        // lsb of block address classifies plane address.
        if(nNumsBlk < stDevConfig._nNumsPlane)
            tuOp = TESTUNIT_OP_CACHE;
        else 
            nNumsBlk = nNumsBlk / stDevConfig._nNumsPlane;
    }
    else if (tuOp == TESTUNIT_OP_NX )
    {
        if(nNumsBlk < stDevConfig._nNumsPlane)
            tuOp = TESTUNIT_OP_LEGACY;
        else 
            nNumsBlk = nNumsBlk / stDevConfig._nNumsPlane;
    }
    else if(tuOp == TESTUNIT_OP_COPYBACK)
    {
        return;
    }


    NAND_TRANS_OP   nTransOp;
    for(UINT32 nIterBlk =0; nIterBlk < nNumsBlk; nIterBlk++)
    {
        for(UINT32 nIterPage =0; nIterPage < nNumsPageInBlk;)
        {
            switch(tuOp)
            {
            case TESTUNIT_OP_LEGACY :
                nTransOp = (bWrite) ? NAND_OP_PROG : NAND_OP_READ;
                nRet = flash.AddTransaction(NULL_SIG(UINT32), nTransOp, BUILD_PHYSICAL_ADDR(nDie, 0, nIterBlk, nIterPage, stDevConfig));
                if(!CheckReturnValue(nRet)) continue;
                nIterPage++;
                break;

            case TESTUNIT_OP_CACHE :
                nTransOp = (bWrite) ? NAND_OP_PROG_CACHE: NAND_OP_READ_CACHE;
                for(UINT32 nIdx = 0; nIdx < nTransferPageSize; nIdx++)
                {
                    nRet = flash.AddTransaction(NULL_SIG(UINT32), nTransOp, BUILD_PHYSICAL_ADDR(nDie, 0, nIterBlk, nIterPage + nIdx, stDevConfig));
                    if(!CheckReturnValue(nRet)) continue;
                }
                nIterPage += nTransferPageSize;

                break;

            case TESTUNIT_OP_NX_CACHE :
                // use auto plane addressing mode
                if(!bWrite)
                {
                    throw "No support for read cache mode";
                }
                else
                {
                    for(UINT32 nIdx = 0; nIdx < nTransferPageSize; nIdx++)
                    {
                        // build translations for each planes
                        for(UINT16 nPlane = 0; nPlane < stDevConfig._nNumsPlane; nPlane++)
                        {
                            if(nIdx == nTransferPageSize -1 && nPlane == stDevConfig._nNumsPlane - 1)
                            {
                                // According to the protocol compatible with ONFI,
                                // this routine informs that this command is the last command.
                                nRet = flash.AddTransaction(NULL_SIG(UINT32), NAND_OP_PROG_MULTIPLANE_CACHE, BUILD_PHYSICAL_ADDR(nDie, 0, nIterBlk, nIterPage + (nTransferPageSize -1), stDevConfig), true);
                            }
                            else
                            {
                                nRet = flash.AddTransaction(NULL_SIG(UINT32), NAND_OP_PROG_MULTIPLANE_CACHE, BUILD_PHYSICAL_ADDR(nDie, 0, nIterBlk, nIterPage + nIdx, stDevConfig), false);
                            }
                            if(!CheckReturnValue(nRet)) continue;
                        }
                    }

                }
                nIterPage += nTransferPageSize;
                break;

            case TESTUNIT_OP_NX :
                // use plane auto addressing mode
                nTransOp = (bWrite) ? NAND_OP_PROG_MULTIPLANE :NAND_OP_READ_MULTIPLANE;
                for(UINT32 nIdx = 0; nIdx < nTransferPageSize; nIdx++)
                {
                    // build translations for each planes
                    for(UINT16 nPlane = 0; nPlane < stDevConfig._nNumsPlane; nPlane++)
                    {
                        nRet = flash.AddTransaction(NULL_SIG(UINT32), nTransOp, BUILD_PHYSICAL_ADDR(nDie,0, nIterBlk, nIterPage + nIdx , stDevConfig));
                        if(!CheckReturnValue(nRet)) continue;
                    }

                }
                nIterPage += nTransferPageSize;
                break;
            }

            while(flash.IsActiveMode())
            {
                flash.UpdateWithoutIdleCycles();
            }
        }
    }
}

bool CheckReturnValue( NV_RET nRet )
{
    if(nRet == NAND_SUCCESS)
    {
        return true;
    }

    std::string strException = "Error Module(s) and Error Type(s):";

    UINT32 nMajorCode = NV_PARSE_RETURN_MAJOR(nRet);
    UINT32 nMinorCode = NV_PARSE_RETURN_MINOR(nRet);
    if(nMajorCode & NAND_DIE)   
    {
        strException += " Die";
        if(NV_PARSE_RETURN_MINOR(NAND_DIE_ERROR_BUSY) & nMinorCode)
            strException += " Busy";
    }

    if(nMajorCode & NAND_CTRL)   strException += " NandController";
    if(nMajorCode & NAND_FLASH)   
    {
        strException += " NandFlashSystem";
        if(NV_PARSE_RETURN_MINOR(NAND_FLASH_ERROR_BUSY) & nMinorCode)
            strException += " Busy";
        else if (NV_PARSE_RETURN_MINOR(NAND_FLASH_ERROR_UNSUPPORTED) & nMinorCode)
            strException += " Unsupported";
    }
    if(nMajorCode & NAND_PLANE)   strException += " Plane";

    throw strException;
    return false;
}

void EraseBlock( NandFlashSystem &flash, UINT16 nDie, UINT32 nNumsBlk, bool bMultiplane )
{
    NandDeviceConfig stDevConfig = flash.GetDeviceConfig();
    NV_RET nRet;

    if(bMultiplane)
    {
        // lsb of block address classifies plane address.
        nNumsBlk = nNumsBlk / stDevConfig._nNumsPlane;
    }
    for(UINT32 nIterBlk =0; nIterBlk < nNumsBlk; nIterBlk++)
    {
        for(UINT32 nIterPage =0; nIterPage < stDevConfig._nNumsPgPerBlk;)
        {
            nRet = flash.AddTransaction(NULL_SIG(UINT32),(bMultiplane) ? NAND_OP_BLOCK_ERASE_MULTIPLANE : NAND_OP_BLOCK_ERASE, BUILD_PHYSICAL_ADDR(nDie,0, nIterBlk, nIterPage, stDevConfig));
            if(!CheckReturnValue(nRet)) continue;
            nIterPage++;

            while(flash.IsActiveMode())
            {
                flash.UpdateWithoutIdleCycles();
            }
        }
    }
}

void DieInterleavedOpTest(TESTUNIT_OP tuOp, NandFlashSystem &flash, UINT16 nNumsDie, UINT32 nNumsBlk, UINT32 nNumsPageInBlk, UINT32 nTransferPageSize, bool bWrite )
{
    NandDeviceConfig stDevConfig = flash.GetDeviceConfig();
    NV_RET           nRet;

    if(tuOp == TESTUNIT_OP_NX_CACHE)
    {
        // lsb of block address classifies plane address.
        if(nNumsBlk < stDevConfig._nNumsPlane)
            tuOp = TESTUNIT_OP_CACHE;
        else
            nNumsBlk = nNumsBlk / stDevConfig._nNumsPlane;
    }
    else if (tuOp == TESTUNIT_OP_NX )
    {
        if(nNumsBlk < stDevConfig._nNumsPlane)
            tuOp = TESTUNIT_OP_LEGACY;
        else
            nNumsBlk = nNumsBlk / stDevConfig._nNumsPlane;
    }
    else if(tuOp == TESTUNIT_OP_COPYBACK)
    {
        return;
    }

    NAND_TRANS_OP   nTransOp;
    for(UINT32 nIterBlk =0; nIterBlk < nNumsBlk; nIterBlk++)
    {
        for(UINT32 nIterPage =0; nIterPage < nNumsPageInBlk;)
        {
            switch(tuOp)
            {
            case TESTUNIT_OP_LEGACY :
                nTransOp = (bWrite) ? NAND_OP_PROG : NAND_OP_READ;

                for(UINT16 nDie = 0; nDie < nNumsDie; nDie++)
                {
                    nRet = flash.AddTransaction(NULL_SIG(UINT32),nTransOp, BUILD_PHYSICAL_ADDR(nDie, 0, nIterBlk, nIterPage, stDevConfig));
                    if(!CheckReturnValue(nRet)) continue;
                }
                nIterPage++;
                break;

            case TESTUNIT_OP_CACHE :
                nTransOp = (bWrite) ? NAND_OP_PROG_CACHE: NAND_OP_READ_CACHE;


                for(UINT32 nIdx = 0; nIdx < nTransferPageSize; nIdx++)
                {

                    for(UINT16 nDie = 0; nDie < nNumsDie; nDie++)
                    {
                        nRet = flash.AddTransaction(NULL_SIG(UINT32), nTransOp, BUILD_PHYSICAL_ADDR(nDie, 0, nIterBlk, nIterPage + nIdx, stDevConfig));
                        if(!CheckReturnValue(nRet)) continue;
                    }
                }
                nIterPage += nTransferPageSize;
                break;

            case TESTUNIT_OP_NX_CACHE :
                // use plane auto addressing mode
                if(!bWrite)
                {
                    throw "No support for read cache mode";
                }
                else
                {
                    for(UINT32 nIdx = 0; nIdx < nTransferPageSize -1; nIdx++)
                    {
                        for(UINT16 nDie = 0; nDie < nNumsDie; nDie++)
                        {
                            nRet = flash.AddTransaction(NULL_SIG(UINT32), NAND_OP_PROG_MULTIPLANE_CACHE, BUILD_PHYSICAL_ADDR(nDie, 0, nIterBlk, nIterPage + nIdx, stDevConfig), false);
                            if(!CheckReturnValue(nRet)) continue;
                        }
                    }
                    for(UINT16 nDie = 0; nDie < nNumsDie; nDie++)
                    {
                        nRet = flash.AddTransaction(NULL_SIG(UINT32), NAND_OP_PROG_MULTIPLANE_CACHE, BUILD_PHYSICAL_ADDR(nDie, 0, nIterBlk, nIterPage + nTransferPageSize -1, stDevConfig), true);
                        if(!CheckReturnValue(nRet)) continue;
                    }
                }
                nIterPage += nTransferPageSize;
                break;

            case TESTUNIT_OP_NX :
                // use plane auto addressing mode
                nTransOp = (bWrite) ? NAND_OP_PROG_MULTIPLANE :NAND_OP_READ_MULTIPLANE;
                for(UINT32 nIdx = 0; nIdx < nTransferPageSize; nIdx++)
                {
                    for(UINT16 nDie = 0; nDie < nNumsDie; nDie++)
                    {
                        nRet = flash.AddTransaction(NULL_SIG(UINT32), nTransOp, BUILD_PHYSICAL_ADDR(nDie, 0, nIterBlk, nIterPage + nIdx, stDevConfig));
                        if(!CheckReturnValue(nRet)) continue;
                    }
                }
                nIterPage += nTransferPageSize;
                break;
            }

            while(flash.IsActiveMode())
            {
                flash.UpdateWithoutIdleCycles();
            }
        }
    }
}

