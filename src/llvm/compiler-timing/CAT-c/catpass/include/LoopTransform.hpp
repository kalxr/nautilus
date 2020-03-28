/*
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2019, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2019, Simone Campanoni <simonec@eecs.northwestern.edu>
 * Copyright (c) 2019, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2019, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Souradip Ghosh <sgh@u.northwestern.edu>
 *          Simone Campanoni <simonec@eecs.northwestern.edu>
 *          Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/*
 * LoopTransform.hpp
 * ----------------------------------------
 * 
 * The main driver for custom loop analysis, loop transformations, and
 * implementations for heuristics required to implement the overall 
 * compiler-timing mechanism.
 * 
 */ 

#include "LatencyDFA.hpp"

using namespace std;
using namespace llvm;

const double ExpansionFactor = 2.4; // Weight to account for LLSs that are small due to 
                                    // possible vectorization in the middle-end or clever 
                                    // instruction selection at the backend
const uint64_t MaxExtensionCount = 12;
const uint64_t MaxExtensionSize = 0;
const uint64_t MaxMargin = 50; // Number of cycles maximum to miss (by compile-time analysis)

// Options to transform the loop
// - EXTEND --- unroll the loop --- based on a calculated factor
// - BRANCH --- inject a biased branch into the loop --- based on a calculated factor
// - MANUAL --- determine callback locations manually via LatencyDFA traversal, 
//              occurs when the LLS is large
enum TransformOption {EXTEND, BRANCH, MANUAL};

class LoopTransform
{
public:
    LoopTransform(Loop *L, Function *F,  LoopInfo *LI, DominatorTree *DT,
                  ScalarEvolution *SE, AssumptionCache *AC, 
                  OptimizationRemarkEmitter *ORE, uint64_t Gran);

    // ------- User transformation methods -------
    void Transform();
    void BuildBiasedBranch(Instruction *InsertionPoint, uint64_t ExtensionCount);
    void ExtendLoop(uint64_t ExtensionCount);
    
    // ------- Analysis/transformation query methods -------    
    set<Instruction *> *GetCallbackLocations() { return &CallbackLocations; }
    TransformOption GetTransformationTy() { return this->Extend; }

private:
    Loop *L;
    Function *F;
    uint64_t Granularity;
    LatencyDFA *LoopLDFA; // DFA --- full analysis
    LatencyDFA *LoopIDFA; // Interval analysis --- top-level/same-depth

    // ------- Analysis state -------

    // Wrapper pass state
    LoopInfo *LI;
    DominatorTree *DT;
    ScalarEvolution *SE;
    AssumptionCache *AC;
    OptimizationRemarkEmitter *ORE;

    // Initialization state
    bool CorrectForm=false;
    MDNode *CBNode;

    // Transform info, statistics
    TransformOption Extend=TransformOption::BRANCH; // default
    set<Instruction *> CallbackLocations; // We want to record the important points in the bitcode 
                                          // at which the new loop which are eligible for future
                                          // callback injections, transformations, etc. --- if (Extend),
                                          // the callback location is the last instruction in the loop,
                                          // else, the callback location is the branch instruction of
                                          // the new basic block inserted (biased branch)

    // ------- Transformation methods -------

    // Pre-transformation methods
    void _transformSubLoops();
    bool _canVectorizeLoop() { return true; } // TODO 
    uint64_t _calculateLoopExtensionStats(uint64_t LLS, uint64_t *MarginOffset);

    // Loop iteration schemes
    void _buildIterator(PHINode *&NewPHI, Instruction *&Iterator);
    void _setIteratorPHI(PHINode *ThePHI, Value *Init, Value *Iterator);

    // Callback blocks
    Instruction *_buildCallbackBlock(CmpInst *CI, Instruction *InsertionPoint, const string MD);

    // Top guard
    void _designateTopGuardViaPredecessors();

    // Bottom guard
    void _buildBottomGuard(BasicBlock *Source, BasicBlock *Exit, PHINode *IteratorPHI);
    void _designateBottomGuardViaExits();

    // Post-transformation methods
    void _collectUnrolledCallbackLocations();
};