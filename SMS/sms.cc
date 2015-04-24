/**
 * Author: Biswabandan Panda
 */

/**
 * @file
 * SMS Prefetcher template instantiations.
 */

#include "debug/HWPrefetch.hh"
#include "mem/cache/prefetch/sms.hh"


void
SMSPrefetcher::calculatePrefetch(PacketPtr &pkt, std::vector<Addr> &addresses) {
	if (!pkt->req->hasPC()) {
		return;
	}
	Addr blk_addr = (pkt->getAddr() & ~(Addr)(blkSize-1));
	MasterID master_id = useMasterId ? pkt->req->masterId() : 0;
	uint32_t core_id = pkt->req->hasContextId() ? pkt->req->contextId() : -1;
	if (core_id < 0) {
		DPRINTF(HWPrefetch, "ignoring request with no core ID");
		return;
	}
	int region_tag = blk_addr >> SHIFT_KEY;
	int region_base = region_tag << SHIFT_KEY;
	int region_offset = ((blk_addr - region_base)/BLK_SIZE) % NO_OF_BLOCKS;
	unsigned long long pc = (unsigned long long) pkt->req->getPC();
	assert(core_id < Max_Contexts);
	std::list<ATEntry*> &tab_at = at_table[core_id];
	std::list<ATEntry*>::iterator at_iter;
	for (at_iter = tab_at.begin(); at_iter != tab_at.end(); at_iter++) {
		if((*at_iter)->region_tag == region_tag)
			break;
	}
	if(at_iter != tab_at.end()) {
		if ((*at_iter)->start == true){
			(*at_iter)->bitVector[region_offset] = true;
		}
	}
	assert(tab_at.size() <= AGT_HEIGHT);
	if ((tab_at.size() >= AGT_HEIGHT)) {
		std::list<ATEntry*>::iterator min_pos = tab_at.begin();
		int min_conf = (*min_pos)->numOnes;
		for (at_iter = min_pos, ++at_iter; at_iter != tab_at.end(); ++at_iter) {
			if ((*at_iter)->numOnes < min_conf){
				min_pos = at_iter;
				min_conf = (*at_iter)->numOnes;
			}
		}
		delete *min_pos;
		tab_at.erase(min_pos);
	}
	if(at_iter == tab_at.end()) {
		ATEntry *At_entry = new ATEntry;
		At_entry->start = true;
		At_entry->region_tag = region_tag;
		At_entry->PC = pc;
		At_entry->region_offset = region_offset;
		At_entry->bitVector[region_offset] = true;
		tab_at.push_back(At_entry);
	}
	std::list<PHTEntry*> &tab_pht = pht_table[core_id];
	std::list<PHTEntry*>::iterator pht_iter;
	for(pht_iter = tab_pht.begin(); pht_iter != tab_pht.end(); pht_iter++){
		if (((*pht_iter)->PC == pc) && ((*pht_iter)->region_offset == region_offset))
			break;
	}
	if (pht_iter != tab_pht.end()) {
		DPRINTF(HWPrefetch, "hit: in region %d\n",
				region_tag);
		int count = 0;
		for (int d = 0; d < NO_OF_BLOCKS; d++) {
			if((*pht_iter)->bitVector[d] == true){
				Addr pf_addr = region_base + d * BLOCK_SIZE;
				if (pageStop && !samePage(blk_addr, pf_addr)) {
					pfSpanPage += degree - d + 1;
					return;
				} else {
					count++;
					DPRINTF(HWPrefetch, "  queuing prefetch to %x \n",
							pf_addr);
					addresses.push_back(pf_addr);
				}
			}
		}
	}
}

void SMSPrefetcher::UpdateRegion(Addr address, int core_id, unsigned long long PC)
{
	int repl_addr = (int)(address & ~(Addr)(blkSize-1));
	int region_tag = repl_addr >> SHIFT_KEY;
	std::list<PHTEntry*> &tab_pht = pht_table[core_id];
	std::list<ATEntry*>  &tab_at  = at_table[core_id];
	std::list<PHTEntry*>::iterator pht_iter;
	std::list<ATEntry*>::iterator at_iter;
	for (at_iter = tab_at.begin(); at_iter != tab_at.end(); at_iter++) {
		if((*at_iter)->region_tag == region_tag) {
			// Transferring bit vector from AGT to PHT
			int count = 0;
			for(pht_iter = tab_pht.begin(); pht_iter != tab_pht.end(); pht_iter++) {
				if(((*at_iter)->PC == (*pht_iter)->PC) && ((*at_iter)->region_offset == (*pht_iter)->region_offset)) {
					// Doing a set intersection operation between the old and the present bit vectors
					for(int i = 0; i < NO_OF_BLOCKS; i++)
						(*pht_iter)->bitVector[i] &= (*at_iter)->bitVector[i];
					break;
				}
			}
			if(pht_iter != tab_pht.end())
				break;
			if(pht_table[core_id].size() < PHT_HEIGHT) {
				PHTEntry *new_entry = new PHTEntry;
				new_entry->trained = true;
				new_entry->PC = (*at_iter)->PC;
				new_entry->region_offset = (*at_iter)->region_offset;
				for(int i = 0; i < NO_OF_BLOCKS; i++){
					new_entry->bitVector[i] = (*at_iter)->bitVector[i];
					count++;
				}
				new_entry->numOnes = count;
				(*at_iter)->start = false;
				tab_pht.push_back(new_entry);
			}
			assert(pht_table[core_id].size() <= PHT_HEIGHT);
			if (pht_table[core_id].size() >= PHT_HEIGHT) { //set default table size is 2048
				std::list<PHTEntry*>::iterator min_pos = tab_pht.begin();
				int min_conf = (*min_pos)->numOnes;
				for (pht_iter = min_pos, ++pht_iter; pht_iter != tab_pht.end(); ++pht_iter) {
					if ((*pht_iter)->numOnes < min_conf){
						min_pos = pht_iter;
						min_conf = (*pht_iter)->numOnes;
					}
				}
				delete *min_pos;
				tab_pht.erase(min_pos);
				PHTEntry *new_entry = new PHTEntry;
				new_entry->trained = true;
				new_entry->PC = PC;
				new_entry->region_offset = (*at_iter)->region_offset;
				for(int i = 0; i < NO_OF_BLOCKS; i++)
					new_entry->bitVector[i] = (*at_iter)->bitVector[i];
				tab_pht.push_back(new_entry);
			}
			break;
		}
	}
}

SMSPrefetcher*
SMSPrefetcherParams::create()
{
	return new SMSPrefetcher(this);
}
