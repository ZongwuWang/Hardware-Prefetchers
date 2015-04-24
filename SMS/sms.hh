/*
 * sms.hh
 *
 *  Created on: 23-Apr-2015
 *      Author: biswa
 */

#ifndef _MEM_CACHE_PREFETCH_SMS_HH_
#define _MEM_CACHE_PREFETCH_SMS_HH_
#define REGION_SIZE 2048                  // 2KB
#define SHIFT_KEY 11                      // Shift 11 bits to get the region tag
#define BLOCK_SIZE 64                     // 64 bytes
#define NO_OF_BLOCKS REGION_SIZE/BLOCK_SIZE
#define AGT_HEIGHT 64                     // Number of entries in the AGT table
#define PHT_HEIGHT 4096                   // Number of entries in the PHT table
#include <climits>
#include "mem/cache/prefetch/queued.hh"
#include "params/SMSPrefetcher.hh"
class SMSPrefetcher : public QueuedPrefetcher
{
  protected:
    static const int Max_Contexts = 64;
    class PHTEntry {
      public:
        uint32_t region_tag;
        uint32_t region_base;
        uint32_t region_offset;
        int count;
        bool bitVector[NO_OF_BLOCKS];      // Assuming a cache line size of 64 bytes with region size of 2KB
        int bitCounter[NO_OF_BLOCKS];      // A 2-bit counter per entry for the feedback purpose.
        bool trained;                      // Is the entry trained ?
        int numOnes;                       // Number of 1's in the bit vector
        unsigned long long PC;             // PC of the demand request
    };
    class ATEntry {
      public:
        uint32_t region_tag;
        uint32_t region_offset;
        bool bitVector[NO_OF_BLOCKS];
        bool start;
        int numOnes;
        unsigned long long PC;
    };
    std::list<ATEntry*> at_table[Max_Contexts];
    std::list<PHTEntry*> pht_table[Max_Contexts];

  public:

    SMSPrefetcher(const SMSPrefetcherParams *p);
    ~SMSPrefetcher();
    void UpdateRegion(Addr address, int core_id, unsigned long long PC);
    void calculatePrefetch(const PacketPtr &pkt, std::vector<Addr> &addresses);
    unsigned long long inthash(unsigned long long key);
};

#endif // __MEM_CACHE_PREFETCH_SMS_PREFETCHER_HH__
