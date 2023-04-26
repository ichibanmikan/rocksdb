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

        KPVHandle(bool pf=false);
        KPVHandle(Slice& v, bool pf=false);
        KPVHandle(Slice& v, KPVHandle* p, KPVHandle* n, bool pf=false);

        inline void setNewValue(Slice v);

        ~KPVHandle(){}

        // void set_bf(KPVHandle* p, KPVHandle* n);
};
    
class key_value_node_lruhandle_table {
    public:
        ~key_value_node_lruhandle_table(){}

        key_value_node_lruhandle_table();
        key_value_node_lruhandle_table(const Slice& key, Slice& value);
        key_value_node_lruhandle_table(uint32_t m_len);
        std::unordered_map<Slice, KPVHandle*> hash_table; //hash表
        KPVHandle* head_; //表头
        KPVHandle* tail_; //表尾

        KPVHandle* boundary_ //ghost cache开始标志，指向ghost cache链的第一个节点
        uint32_t now_ghost_length; //当前ghost cache size
        const uint32_t ghost_size; //ghost cache大小
        uint32_t now_size; //当前表长
        const uint32_t max_length_; //表长上限

        uint32_t step_length;

        Status Lookup(const Slice& key, KPVHandle* &ret_ptr);
        Status Insert(const Slice& key, const Slice& value);
        Status Remove(const Slice& key);

        // void 
        // KPVHandle* Adjust(std::unordered_map<Slice, KPVHandle*>::iterator iter);
        Status Evict(uint32_t len);

        inline void set_head_value(Slice v);
        inline uint32_t a_func();
        // inline uint32_t

        bool Empty();
};

class key_pointer_node_lruhandle_table {
    public:
        ~key_pointer_node_lruhandle_table(){}

        key_pointer_node_lruhandle_table();
        key_pointer_node_lruhandle_table(const Slice& key, Slice pointer);
        key_pointer_node_lruhandle_table(uint32_t m_len, uint32_t ghost_size);
        std::unordered_map<Slice, KPVHandle*> hash_table; //hash表
        KPVHandle* head_; //表头
        KPVHandle* tail_; //表尾
        // bool* pv_flag; //KV和KP标志

        KPVHandle* boundary_ //ghost cache开始标志
        uint32_t ghost_size; //ghost cache大小
        uint32_t max_length_; //表长上限

        KPVHandle* Lookup(const Slice& key, Status& ghost_adjust);
        KPVHandle* Insert(KPVHandle* h);
        KPVHandle* Remove(const Slice& key);
};

class BlockNodeHandle{
    private:
        BlockContents* next;
        BlockContents* prev;
        BlockContents block;
    public:
        ~BlockNodeHandle();
        BlockNodeHandle();
        BlockNodeHandle(BlockContents* f, BlockContents* n);
        BlockNodeHandle(BlockContents t);
}

class key_block_node_lruhandle_table{
    public:
        BlockNodeHandle** list_; //块哈希表
        int max_size;

        BlockContents* boundary_ //ghost cache开始标志
        uint32_t ghost_size; //ghost cache大小
        uint32_t max_length_; //表长上限
        ~key_block_node_lruhandle_table();
        key_block_node_lruhandle_table();
};

class icache : public Cache{
    
};

}
}
#endif