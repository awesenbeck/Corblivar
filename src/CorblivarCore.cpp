/*
 * =====================================================================================
 *
 *    Description:  Corblivar core file (data structures, layout operations)
 *
 *         Author:  Johann Knechtel, johann.knechtel@ifte.de
 *        Company:  Institute of Electromechanical and Electronic Design, www.ifte.de
 *
 * =====================================================================================
 */
#include "Corblivar.hpp"

bool CorblivarFP::SA(CorblivarLayoutRep &chip) {
	int i;
	bool annealed, stabilized;

	// outer loop: annealing -- temperature steps
	i = 0;
	annealed = false;
	while (!annealed) {
		if (this->logMed()) {
			cout << "SA> Optimization step: " << i << endl;
		}

		// inner loop: layout operations (until cost convergence for particular temp)
		stabilized = false;
		while (!stabilized) {
			chip.generateLayout(*this);

			stabilized = true;
		}

		// TODO logMed: current cost, current temp
		if (this->logMed()) {
			cout << endl;
		}

		annealed = true;
	}

	return true;
}

void CorblivarLayoutRep::initCorblivar(CorblivarFP &corb) {
	map<int, Block*>::iterator b;
	Block *cur_block;
	Direction cur_dir;
	int i, rand, cur_t;

	if (corb.logMed()) {
		cout << "Initializing Corblivar data for chip on " << corb.conf_layer << " layers..." << endl;
	}

	// init separate data structures for dies
	for (i = 0; i < corb.conf_layer; i++) {
		this->dies.push_back(new CorblivarDie(i));
	}

	// assign each block randomly to one die, generate L and T randomly as well
	for (b = corb.blocks.begin(); b != corb.blocks.end(); ++b) {
		cur_block = (*b).second;

		// consider random die
		rand = CorblivarFP::randI(0, corb.conf_layer);

		// generate direction L
		if (CorblivarFP::randB()) {
			cur_dir = DIRECTION_HOR;
		}
		else {
			cur_dir = DIRECTION_VERT;
		}
		// generate T-junction to be overlapped
		// consider block count as upper bound
		cur_t = CorblivarFP::randI(0, this->dies[rand]->CBL.size());

		// store CBL item
		this->dies[rand]->CBL.push_back(new CBLitem(cur_block, cur_dir, cur_t));
	}

#ifdef DBG_CORB_FP
	unsigned d;
	list<CBLitem*>::iterator CBLi;

	for (d = 0; d < this->dies.size(); d++) {
		cout << "DBG_CORB_FP> ";
		cout << "Init CBL tuples for die " << d << "; " << this->dies[d]->CBL.size() << " tuples:" << endl;
		for (CBLi = this->dies[d]->CBL.begin(); CBLi != this->dies[d]->CBL.end(); ++CBLi) {
			cout << "DBG_CORB_FP> ";
			cout << (* CBLi)->itemString() << endl;
		}
		cout << "DBG_CORB_FP> ";
		cout << endl;
	}
#endif

	if (corb.logMed()) {
		cout << "Done" << endl << endl;
	}
}

void CorblivarLayoutRep::generateLayout(CorblivarFP &corb) {
	unsigned i;
	Block *cur_block;
	bool loop;

	if (corb.logMax()) {
		cout << "Performing layout generation..." << endl;
	}

	// init die pointer
	this->p = this->dies[0];

	// reset die data
	for (i = 0; i < this->dies.size(); i++) {
		// reset progress pointer
		this->dies[i]->pi = this->dies[i]->CBL.begin();
		// reset placement stacks
		while (!this->dies[i]->Hi.empty()) {
			this->dies[i]->Hi.pop();
		}
		while (!this->dies[i]->Vi.empty()) {
			this->dies[i]->Vi.pop();
		}
	}

	// perform layout generation in loop (until all blocks are placed)
	loop = true;
	while (loop) {
		// handle stalled die / resolve open alignment process by placing current block
		if (this->p->stalled) {
			// place block
			cur_block = this->p->placeCurrentBlock();
			// TODO mark current block as placed in AS
			//
			// mark die as not stalled anymore
			this->p->stalled = false;
			// increment progress pointer, next block
			this->p->incrementProgressPointer();
		}
		// die is not stalled
		else {
			// TODO check for alignment tuples for current block
			//
			if (false) {
			}
			// no alignment tuple assigned for current block
			else {
				// place block, consider next block
				this->p->placeCurrentBlock();
				this->p->incrementProgressPointer();
			}
		}

		// die done
		if (this->p->done) {
			// continue loop on yet unfinished die
			for (i = 0; i < this->dies.size(); i++) {
				if (!this->dies[i]->done) {
					this->p = this->dies[i];
					break;
				}
			}
			// all dies handled, stop loop
			if (this->p->done) {
				loop = false;
			}
		}
	}

	if (corb.logMax()) {
		cout << "Done" << endl << endl;
	}
}

void CorblivarDie::incrementProgressPointer() {

	if ((* this->pi) == this->CBL.back()) {
		this->done = true;
	}
	else {
		++(this->pi);
	}
}

Block* CorblivarDie::currentBlock() {
	return (* this->pi)->Si;
}

Block* CorblivarDie::placeCurrentBlock() {
	Block *cur_block;
	CBLitem *cur_CBLi;
	vector<Block*> relevBlocks;
	unsigned relevBlocksCount, b;
	double x, y;

	cur_CBLi = (* this->pi);
	cur_block = cur_CBLi->Si;

	// assign layer to block
	cur_block->layer = this->id;

	// horizontal placement
	if (cur_CBLi->Li == DIRECTION_HOR) {
		// pop relevant blocks from stack
		relevBlocksCount = min(cur_CBLi->Ti + 1, this->Hi.size());
//cout << "relevBlocksCount=" << relevBlocksCount << endl;
		while (relevBlocksCount > relevBlocks.size()) {
			relevBlocks.push_back(this->Hi.top());
			this->Hi.pop();
		}

		// determine x-coordinate for lower left corner of current block
		x = 0;
		for (b = 0; b < relevBlocks.size(); b++) {
			// determine right front
			x = max(x, relevBlocks[b]->bb.ur.x);
		}

		// determine y-coordinate for lower left corner of current block
		if (this->Hi.empty()) {
			y = 0;
//cout << "Hi empty" << endl;
		}
		else {
			// init min value (lower front)
			if (relevBlocks.empty()) {
				y = 0;
//cout << "relevBlocks empty" << endl;
			}
			else {
//cout << "OK" << endl;
				y = relevBlocks[0]->bb.ll.y;

				// determine min value
				for (b = 0; b < relevBlocks.size(); b++) {
					if (relevBlocks[b]->bb.ll.y != Point::UNDEF) {
						y = min(y, relevBlocks[b]->bb.ll.y);
					}
				}
			}
		}
	}
	// vertical placement
	else {
		// pop relevant blocks from stack
		relevBlocksCount = min(cur_CBLi->Ti + 1, this->Vi.size());
		while (relevBlocksCount > relevBlocks.size()) {
			relevBlocks.push_back(this->Vi.top());
			this->Vi.pop();
		}

		// determine y-coordinate for lower left corner of current block
		y = 0;
		for (b = 0; b < relevBlocks.size(); b++) {
			// determine upper front
			y = max(y, relevBlocks[b]->bb.ur.y);
		}

		// determine x-coordinate for lower left corner of current block
		if (this->Vi.empty()) {
			x = 0;
		}
		else {
			// init min value (left front)
			if (relevBlocks.empty()) {
				x = 0;
			}
			else {
				x = relevBlocks[0]->bb.ll.x;

				// determine min value
				for (b = 0; b < relevBlocks.size(); b++) {
					if (relevBlocks[b]->bb.ll.x != Point::UNDEF) {
						x = min(x, relevBlocks[b]->bb.ll.x);
					}
				}
			}
		}
	}

	// update block coordinates
	cur_block->bb.ll.x = x;
	cur_block->bb.ll.y = y;
	cur_block->bb.ur.x = cur_block->bb.w + x;
	cur_block->bb.ur.y = cur_block->bb.h + y;

	// remember placed block on stacks
	this->Hi.push(cur_block);
	this->Vi.push(cur_block);

#ifdef DBG_CORB_FP
	cout << "DBG_CORB_FP> ";
	cout << "Processing CBL tuple " << cur_CBLi->itemString() << " on die " << this->id << ": ";
	cout << "LL=(" << cur_block->bb.ll.x << ", " << cur_block->bb.ll.y << "), ";
	cout << "UR=(" << cur_block->bb.ur.x << ", " << cur_block->bb.ur.y << ")" << endl;
#endif

	return cur_block;
}
