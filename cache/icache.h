#ifndef __ICHIBAN_CACHE_
#define __ICHIBAN_CACHE_

#include "rocksdb/slice.h"
#include "rocksdb/cache.h"
#include "rocksdb/trace_reader_writer.h"
#include "lru_cache.h"
#include "table/block_based/block_cache.h"
#include "table/block_based/block.h"
#include "table/format.h"
#include "rocksdb/status.h"

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
//     private:
//         BlockContents* next;
//         BlockContents* prev;
//         BlockContents block;
//     public:
//         ~BlockNodeHandle();
//         BlockNodeHandle();
//         BlockNodeHandle(BlockContents* f, BlockContents* n);
//         BlockNodeHandle(BlockContents t);
// }

// class key_block_node_lruhandle_table{
//     public:
//         BlockNodeHandle** list_; //块哈希表
//         int max_size;

//         BlockContents* boundary_ //ghost cache开始标志
//         uint32_t ghost_size; //ghost cache大小
//         uint32_t max_length_; //表长上限
//         ~key_block_node_lruhandle_table();
//         key_block_node_lruhandle_table();
// };

class icache : public Cache{
    //TODO: Loopup Evicted, NotFound
    //TODO: Ghost Cache
    //TODO: KP Cache hit, KP'head_->KV
    //TODO: pointer flush
};

}
}
#endif