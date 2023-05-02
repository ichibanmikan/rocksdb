#ifndef __ICHIBAN_CACHE_
#define __ICHIBAN_CACHE_

#include "rocksdb/slice.h"
#include "rocksdb/cache.h"
#include "rocksdb/trace_reader_writer.h"
#include "lru_cache.h"
#include "table/block_based/block_cache.h"
#include "table/block_based/block.h"
#include "table/block_based/block_builder.h"
#include "table/format.h"
#include "rocksdb/status.h"
#include "db/column_family.h"
#include "debug/version_set.h"
#include "rocksdb/options.h"

#include <unordered_map>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace ROCKSDB_NAMESPACE {

namespace icache {

class key_value_node_lruhandle_table;
class key_pointer_node_lruhandle_table;

// class KPVHandle : public enable_shared_from_this<KPVHandle> {
//     friend class key_value_node_lruhandle_table;
//     friend class key_pointer_node_lruhandle_table;
//     private:
//         Slice value_;
//     public:
//         shared_ptr<KPVHandle> next_;
//         KPVHandle* prev_;
// };

class KPVHandle{
    friend class key_value_node_lruhandle_table;
    friend class key_pointer_node_lruhandle_table;
    private:
        Slice value_;
    public:
        KPVHandle* next_;
        KPVHandle* prev_;

        uint32_t size_;

        bool p_flag; //KV KP标志

        bool ghost_flag; //ghost cache标志

        KPVHandle(bool pf);
        KPVHandle(bool gf, bool pf);
        KPVHandle(Slice& v, bool pf);
        KPVHandle(Slice& v, KPVHandle* p, KPVHandle* n, bool pf);

        inline void setNewValue(Slice v);

        ~KPVHandle(){}

        // void set_bf(KPVHandle* p, KPVHandle* n);
};
    
class key_value_node_lruhandle_table {
    private:
        std::unordered_map<std::string, KPVHandle*>::iterator Find(KPVHandle* k);
    public:
        ~key_value_node_lruhandle_table(){}

        key_value_node_lruhandle_table();
        key_value_node_lruhandle_table(Slice& key, Slice& value);
        key_value_node_lruhandle_table(uint32_t m_len);
        std::unordered_map<std::string, KPVHandle*> hash_table; //hash表
        KPVHandle* head_; //表头
        KPVHandle* tail_; //表尾

        KPVHandle* boundary_; //ghost cache开始标志，指向ghost cache链的第一个节点
        uint32_t now_ghost_length; //当前ghost cache size
        uint32_t ghost_size; //ghost cache大小
        uint32_t now_size; //当前表长
        uint32_t max_length_; //表长上限

        uint32_t step_length;

        Status Lookup(Slice& key, KPVHandle* &ret_ptr);
        Status Insert(Slice& key, Slice& value);
        Status Remove(Slice& key);
        Status Adjust(KPVHandle* k);

        // void 
        // KPVHandle* Adjust(std::unordered_map<Slice, KPVHandle*>::iterator iter);
        Status Evict(uint32_t len);

        Status DeleteSize(uint32_t len);

        inline void set_head_value(Slice v);
        inline uint32_t a_func();
        // inline uint32_t

        bool Empty();

        void Clean();
};

class key_pointer_node_lruhandle_table {
    public:
        ~key_pointer_node_lruhandle_table(){}


        key_pointer_node_lruhandle_table();
        key_pointer_node_lruhandle_table(Slice& key, Slice& value);
        key_pointer_node_lruhandle_table(uint32_t m_len);
        
        std::unordered_map<std::string, KPVHandle*> hash_table;

        inline uint32_t a_func();
        inline void set_head_value(Slice v);
        

        
        KPVHandle* head_; //表头
        KPVHandle* tail_; //表尾

        KPVHandle* boundary_; //ghost cache开始标志，指向ghost cache链的第一个节点
        uint32_t now_ghost_length; //当前ghost cache size
        uint32_t ghost_size; //ghost cache大小
        uint32_t now_size; //当前表长
        uint32_t max_length_; //表长上限

        uint32_t step_length;

        // Status Dump(std::unordered_map<std::string, KPVHandle*>::iterator iter, key_value_node_lruhandle_table& kvt);

        Status Lookup(Slice& key, KPVHandle* &ret_ptr);
        Status Insert(Slice& key, Slice& value);
        Status Remove(Slice& key);
        Status Adjust(KPVHandle* k);

        Status DeleteSize(uint32_t len);

        // void 
        // KPVHandle* Adjust(std::unordered_map<Slice, KPVHandle*>::iterator iter);
        Status Evict(uint32_t len);

        // inline uint32_t

        bool Empty();

        void Clean();


    private:
        std::unordered_map<std::string, KPVHandle*>::iterator Find(KPVHandle* k);
};

// class BlockNodeHandle{
//     public:
//         friend class block_node_lruhandle_table;
//         BlockNodeHandle* next_;
//         BlockNodeHandle* prev_;

//         BlockBuilder value_{0};

//         bool ghost_flag; //ghost cache标志

//         BlockNodeHandle(bool gf);
//         // BlockNodeHandle(bool gf, int start_, int block_type);
//         BlockNodeHandle(BlockBuilder& v, bool gf);
//         BlockNodeHandle(BlockBuilder& v, BlockNodeHandle* p, BlockNodeHandle* n, bool gf);

//         ~BlockNodeHandle(){}
// };

// class block_node_lruhandle_table{
//     std::unordered_map<>
// };

// class BlockNodeHandle{
//     public:
//         BlockContents* next;
//         BlockContents* prev;
//         BlockContents block;
//     public:
//         ~BlockNodeHandle();
//         BlockNodeHandle();
//         BlockNodeHandle(BlockContents* f, BlockContents* n);
//         BlockNodeHandle(BlockContents t);
// }

// class key_block_node_lruhandle_table : public CacheWrapper {
//    public:
//     explicit key_block_node_lruhandle_table(std::shared_ptr<Cache> target)
//         : CacheWrapper(target),
//           num_lookups_(0),
//           num_found_(0),
//           num_inserts_(0) {}

//     const char* Name() const override { return "key_block_node_lruhandle_table"; }

//     Status Insert(const Slice& key, Cache::ObjectPtr value,
//                   const CacheItemHelper* helper, size_t charge,
//                   Handle** handle = nullptr,
//                   Priority priority = Priority::LOW) override {
//       num_inserts_++;
//       return target_->Insert(key, value, helper, charge, handle, priority);
//     }

//     Handle* Lookup(const Slice& key, const CacheItemHelper* helper,
//                    CreateContext* create_context,
//                    Priority priority = Priority::LOW,
//                    Statistics* stats = nullptr) override {
//       num_lookups_++;
//       Handle* handle =
//           target_->Lookup(key, helper, create_context, priority, stats);
//       if (handle != nullptr) {
//         num_found_++;
//       }
//       return handle;
//     }

//     int num_lookups() { return num_lookups_; }

//     int num_found() { return num_found_; }

//     int num_inserts() { return num_inserts_; }

//    private:
//     int num_lookups_;
//     int num_found_;
//     int num_inserts_;
//   };

//   std::shared_ptr<MyBlockCache> compressed_cache_;
//   std::shared_ptr<MyBlockCache> uncompressed_cache_;
//   Options options_;
//   bool compression_enabled_;
//   std::vector<std::string> values_;
//   std::vector<std::string> uncompressable_values_;
//   bool fill_cache_;
//   std::vector<std::string> cf_names_;
// };

//单列族模式下可以运行
class icache{
    //TODO: Loopup Evicted, NotFound
    //TODO: Ghost Cache
    //TODO: KP Cache hit, KP'head_->KV
    //TODO: pointer flush
    //TODO: block build
    public:
        key_value_node_lruhandle_table* kvt;
        key_pointer_node_lruhandle_table* kpt;

        SuperVersion* sv;
        // LRUCache<>

        icache();
        icache(SuperVersion s);
        icache(Slice& key, Slice& value);
        icache(uint32_t m_len_kv, uint32_t m_len_kp);
        icache(key_value_node_lruhandle_table t1, key_pointer_node_lruhandle_table t2);
        ~icache();

        Status Insert(Slice& key, Slice& value);
        Status Lookup(Slice& key, KPVHandle* &ret_ptr,
                      const ReadOptions&, const LookupKey& key, PinnableSlice* value,
                      PinnableWideColumns* columns, std::string* timestamp, Status* status,
                      MergeContext* merge_context,
                      SequenceNumber* max_covering_tombstone_seq,
                      PinnedIteratorsManager* pinned_iters_mgr);
        Status Erase(Slice& key);
        Status Flush(Slice& key);

};

}
}
#endif