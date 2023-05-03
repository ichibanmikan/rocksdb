#include "cache/icache.h"

namespace ROCKSDB_NAMESPACE {

namespace icache {

/****************Key Value Handle*****************/

KPVHandle::KPVHandle(bool pf=false){
    next_=prev_=nullptr;
    p_flag=pf;
    size_=24;
    ghost_flag=false;
}

KPVHandle::KPVHandle(bool gf, bool pf=false){
    next_=prev_=nullptr;
    p_flag=pf;
    size_=24;
    ghost_flag=gf;
}

KPVHandle::KPVHandle(Slice& v, bool pf=false){
    value_.data_=v.data_;
    value_.size_=v.size_;
    next_=prev_=nullptr;
    p_flag=pf;
    size_=v.size()+24;
    ghost_flag=false;
}

KPVHandle::KPVHandle(Slice& v, KPVHandle* p, KPVHandle* n, bool pf=false){
    value_.data_=v.data_;
    value_.size_=v.size_;
    next_=n;
    prev_=p;
    p_flag=pf;
    ghost_flag=false;
    size_=v.size()+24;
}

// void KPVHandle::set_bf(KPVHandle* p, KPVHandle* n){
//     if(this->pre)
// }

inline void KPVHandle::setNewValue(Slice v){
    size_-=value_.size();
    this->value_=v;    
    size_+=value_.size();
}

// void KPVHandle::printHelp(){
//     std::cout << value_->data_ << ' ';
//     if(next_){
//         std::cout << "-> ";
//     }
// }

/****************Key-Value Cache*****************/

key_value_node_lruhandle_table::key_value_node_lruhandle_table(){
    max_length_=67108864;
    ghost_size=65536;

    now_ghost_length=0;

    now_size=0;
    boundary_=head_=tail_=nullptr;
}

key_value_node_lruhandle_table::key_value_node_lruhandle_table(Slice& key, Slice& value){
    max_length_=67108864; //64MB
    ghost_size=65536; //64KB

    now_ghost_length=0;

    KPVHandle* k1=new KPVHandle(value);
    hash_table.emplace(key.ToString(), k1);

    now_size=key.size()+k1->size_;

    tail_=head_=k1;
    boundary_=tail_;
}

key_value_node_lruhandle_table::key_value_node_lruhandle_table(uint32_t m_len){
    max_length_=m_len;

    ghost_size=m_len/1000;

    now_ghost_length=0;
    now_size=0;
    boundary_=head_=tail_=nullptr;
}

inline void key_value_node_lruhandle_table::set_head_value(Slice v){
    now_size-=head_->size_;
    head_->setNewValue(v);
    now_size+=head_->size_;
}

inline uint32_t key_value_node_lruhandle_table::a_func(){
    return (uint32_t)log(2000);
}

Status key_value_node_lruhandle_table::Lookup(Slice& key, KPVHandle* &ret_ptr){
    auto iter=hash_table.find(key.ToString());
    // Status s;
    if(iter==hash_table.end()){
        ret_ptr = new KPVHandle();
        return Status::NotFound();
    } else {
        if(!iter->second){
            return Status::Evicted();
        }
        ret_ptr = iter->second;
        if(ret_ptr->ghost_flag){
            return Status::Ghost();
        }
        if(ret_ptr==head_){
            return Status::OK();
        }
        if(ret_ptr==tail_){
            tail_->prev_->next_=nullptr;
            tail_=tail_->prev_;
            ret_ptr->prev_=nullptr;
            ret_ptr->next_=head_;
            head_->prev_=ret_ptr;
            head_=ret_ptr;
            return Status::OK();
        }
        ret_ptr->prev_->next_=ret_ptr->next_;
        ret_ptr->next_->prev_=ret_ptr->prev_;
        ret_ptr->prev_=nullptr;
        ret_ptr->next_=head_;
        head_->prev_=ret_ptr;
        head_=ret_ptr;
        return Status::OK();
        
    }
}

Status key_value_node_lruhandle_table::Insert(Slice& key, Slice& value){
    auto iter=hash_table.find(key.ToString());
    if(key.size_+value.size_>max_length_){
        return Status::MemoryLimit("Insert failed due to LRU cache being full.");
    } //K-V本体就大于整个缓存
    if(iter==hash_table.end()||iter->second==nullptr){
        /*原来的表中没有或是表是空的，需要插入*/
        if(iter->second==nullptr){
            hash_table.erase(iter);
        }
        KPVHandle* tmp_kh=new KPVHandle(value);
        if(((now_size-now_ghost_length)+key.size()+tmp_kh->size_)>(max_length_-ghost_size)){
            // delete tmp_kh;
            Evict((uint32_t)key.size()+tmp_kh->size_);
        }
        tmp_kh->next_=head_;
        head_->prev_=tmp_kh;
        head_=head_->prev_;
        now_size+=(key.size()+tmp_kh->size_);
        hash_table.emplace(key.ToString(), tmp_kh);
        // if((now_size+key.size()+tmp_kh->size_)>(now_size-ghost_size)){
        //     evict(key.size()+tmp_kh->size_);
        //     tmp_kh->next_=head_;
        //     head_->prev_=tmp_kh;
        //     head_=tmp_kh;
        // }
    }

    KPVHandle* ptr=iter->second;

    if(ptr==head_){
        // now_size-=head_->size_;
        // head_->size_-=head_->value_.size();
        // head_->size_+=value->size();
        // head_->value_=value;
        // now_size+=head_->size_;
        set_head_value(value);
        return Status::OK();
    } //如果命中链表头

    if(ptr==tail_){
        // if(ptr->ghost_flag){
        //     tail_->prev_->next_=nullptr;
        //     tail_->next_=head_;
        //     tail_=tail_->prev_;
        //     ptr->prev_=nullptr;
        //     head_=ptr;
        //     ptr->next_->prev_=ptr;            
        //     set_head_value()
        // } else {

        tail_->prev_->next_=nullptr;
        tail_->next_=head_;
        tail_=tail_->prev_;
        ptr->prev_=nullptr;
        head_=ptr;
        ptr->next_->prev_=ptr;

        set_head_value(value);

        // now_size-=ptr->size_;
        // ptr->size_-=ptr->value_.size();
        // ptr->value_=value;
        // ptr->size_+=value.size();
        // now_size+=ptr->size_;
        // }
        return Status::OK();
    } //如果命中链表尾

    if(iter!=hash_table.end()){
        // if(ptr->ghost_flag){
        //     /*****************************/
        // } else {
        ptr->next_->prev_=ptr->prev_;
        ptr->prev_->next_=ptr->next_;
        ptr->prev_=nullptr;
        ptr->next_=head_;
        head_=ptr;

        set_head_value(value);

            // now_size-=ptr->size_;
            // ptr->size_-=ptr->value_.size();
            // ptr->value_=value;
        // }
        return Status::OK();
    } //如果命中链表内元素但并非表头或表尾

    return Status::Aborted();
}

Status key_value_node_lruhandle_table::Remove(Slice& key){
    if(Empty()){
        return Status::NotFound();
    }
    KPVHandle* kh;
    Status s=Lookup(key, kh);
    if(s.ok()||s.IsGhostCache()){
        if(head_==tail_){
            delete head_;
        } else {
            if(head_==kh){
                head_=head_->next_;
                delete head_->prev_;
                head_->prev_=nullptr;
            } else if(tail_==kh){
                tail_=tail_->prev_;
                delete tail_->next_;
                tail_->next_=nullptr;
            } else {
                kh->prev_->next_=kh->next_;
                kh->next_->prev_=kh->prev_;
                delete kh;
            }
        }
        return Status::OK();
    }
    if(s.IsEvicted()){
        return Status::Evicted();
    }
    return Status::NotFound();
}

bool key_value_node_lruhandle_table::Empty(){
    return head_==tail_&&tail_==nullptr;
}

Status key_value_node_lruhandle_table::DeleteSize(uint32_t len){
    if(len>now_size){
        return Status::MemoryLimit();
    }
    Status s=Evict(len);
    if(s.ok()){
        return Status::OK();
    }
    return Status::Aborted();
}

bool cmp_kpv(KPVHandle* k1, KPVHandle* k2){
    return k1->size_>k2->size_;
}

//驱逐算法
Status key_value_node_lruhandle_table::Evict(uint32_t len){
    if(len>(max_length_ - ghost_size)){
        return Status::MemoryLimit();
    }

    uint32_t tmp_size = max_length_ - ghost_size - now_size + now_ghost_length; //当前表中的剩余空间

    if(len<tmp_size){
        return Status::OK();
    }

    
    std::vector<std::vector<KPVHandle*>> evict_kh_real; //备选的替换Cache对
    KPVHandle* iter=boundary_; //从real Cache的末端开始

    while(tmp_size<len){
        uint32_t sum_size=0;
        std::vector<KPVHandle*> v_tmp;
        uint32_t i=a_func(); //循环a次 ，找到a个备选的cache对
        while(i>0){
            v_tmp.push_back(iter);
            i--;
            sum_size+=iter->size_;
            iter=iter->prev_;
        }
        tmp_size+=sum_size;
        evict_kh_real.push_back(v_tmp);
    }
    /*从real cache中选出等待替换的k-v对，每当尺寸不够就选出a个*/

    for(int i=evict_kh_real.size()-2; i>=0; i--){
    // for(int i=evict_kh_real.size(); i>=0; i--){
        for(int j=evict_kh_real[i].size()-1; j>=0; j--){
            len-=evict_kh_real[i][j]->size_;
            if(now_ghost_length+evict_kh_real[i][j]->size_<ghost_size){
                now_ghost_length+=evict_kh_real[i][j]->size_;
                //TODO: ghost cache保存metadata

                Adjust(evict_kh_real[i][j]);

                if(boundary_==tail_){
                    evict_kh_real[i][j]->prev_->next_=evict_kh_real[i][j]->next_;
                    evict_kh_real[i][j]->next_->prev_=evict_kh_real[i][j]->prev_;
                    evict_kh_real[i][j]->next_=nullptr;
                    evict_kh_real[i][j]->prev_=tail_;
                    tail_=evict_kh_real[i][j];
                    continue;
                }

                if(evict_kh_real[i][j]==head_){
                    // boundary_->next_=head_;
                    if(boundary_->next_){
                        boundary_->next_->prev_=head_;
                    }
                    head_->next_->prev_=nullptr;
                    head_=head_->next_;
                    evict_kh_real[i][j]->prev_=boundary_;
                    evict_kh_real[i][j]->next_=boundary_->next_;
                    boundary_->next_=evict_kh_real[i][j];
                    // head_=evict_kh_real[i][j]->next_;
                    continue;
                }

                boundary_->next_->prev_=evict_kh_real[i][j];
                evict_kh_real[i][j]->prev_->next_=evict_kh_real[i][j]->next_;
                evict_kh_real[i][j]->next_->prev_=evict_kh_real[i][j]->prev_;
                evict_kh_real[i][j]->next_=boundary_->next_;
                evict_kh_real[i][j]->prev_=boundary_;
                boundary_->next_=evict_kh_real[i][j];
            } else {
                delete evict_kh_real[i][j];
            }
        }
    }

    std::sort(evict_kh_real[evict_kh_real.size()-1].begin(), evict_kh_real[evict_kh_real.size()-1].end(), cmp_kpv);

    for(int i=0; i<evict_kh_real[evict_kh_real.size()-1].size(); i++){
        if(len<=0){
            return Status::OK();
        }
        len-=evict_kh_real[evict_kh_real.size()-1][i]->size_;
        if(now_ghost_length+evict_kh_real[evict_kh_real.size()-1][i]->size_<ghost_size){
            now_ghost_length+=evict_kh_real[evict_kh_real.size()-1][i]->size_;
            //TODO: ghost cache保存metadata

            Adjust(evict_kh_real[evict_kh_real.size()-1][i]);

            if(boundary_==tail_){
                evict_kh_real[evict_kh_real.size()-1][i]->prev_->next_=evict_kh_real[evict_kh_real.size()-1][i]->next_;
                evict_kh_real[evict_kh_real.size()-1][i]->next_->prev_=evict_kh_real[evict_kh_real.size()-1][i]->prev_;
                evict_kh_real[evict_kh_real.size()-1][i]->next_=nullptr;
                evict_kh_real[evict_kh_real.size()-1][i]->prev_=tail_;
                tail_=evict_kh_real[evict_kh_real.size()-1][i];
                continue;
            }

            if(evict_kh_real[evict_kh_real.size()-1][i]==head_){
                // boundary_->next_=head_;
                if(boundary_->next_){
                    boundary_->next_->prev_=head_;
                }
                head_->next_->prev_=nullptr;
                head_=head_->next_;
                evict_kh_real[evict_kh_real.size()-1][i]->prev_=boundary_;
                evict_kh_real[evict_kh_real.size()-1][i]->next_=boundary_->next_;
                boundary_->next_=evict_kh_real[evict_kh_real.size()-1][i];
                // head_=evict_kh_real[i][j]->next_;
                continue;
            }

            boundary_->next_->prev_=evict_kh_real[evict_kh_real.size()-1][i];
            evict_kh_real[evict_kh_real.size()-1][i]->prev_->next_=evict_kh_real[evict_kh_real.size()-1][i]->next_;
            evict_kh_real[evict_kh_real.size()-1][i]->next_->prev_=evict_kh_real[evict_kh_real.size()-1][i]->prev_;
            evict_kh_real[evict_kh_real.size()-1][i]->next_=boundary_->next_;
            evict_kh_real[evict_kh_real.size()-1][i]->prev_=boundary_;
            boundary_->next_=evict_kh_real[evict_kh_real.size()-1][i];
        } else {
            delete evict_kh_real[evict_kh_real.size()-1][i];
        }
    }

    return Status::OK();
}

std::unordered_map<std::string, KPVHandle*>::iterator key_value_node_lruhandle_table::Find(KPVHandle* k){
    for(auto iter=hash_table.begin(); iter!=hash_table.end(); iter++){
        if(iter->second==k){
            return iter;
        }
    }
    return hash_table.end();
}

void key_value_node_lruhandle_table::Clean(){
    std::vector<std::unordered_map<std::string, KPVHandle*>::iterator>  v_iter;
    for(auto iter=hash_table.begin(); iter!=hash_table.end(); iter++){
        if(iter->second==nullptr){
            v_iter.push_back(iter);
        }
    }

    for(auto i : v_iter){
        hash_table.erase(i);
    }
}

Status key_value_node_lruhandle_table::Adjust(KPVHandle* k){
    auto iter=Find(k);
    now_size-=iter->second->size_;
    iter->second=new KPVHandle(true, false);
    now_size-=iter->second->size_;
    return Status::OK();
}

// void key_value_node_lruhandle_table::printHelp(){
//     for(auto iter=hash_table.begin(); iter!=hash_table.end(); iter++){
//         if(iter->second){
//             std::cout << iter->first << ' ';
//         }
//     }
//     std::cout << std::endl;
//     for(auto iter=hash_table.begin(); iter!=hash_table.end(); iter++){
//         if(iter->second){
//             iter->second->printHelp();
//         }
//     }
// }

/************************Key Pointer Cache*************************/
/* @kpcache: */

key_pointer_node_lruhandle_table::key_pointer_node_lruhandle_table(){
    max_length_=67108864;
    ghost_size=65536;

    now_ghost_length=0;

    now_size=0;
    boundary_=head_=tail_=nullptr;
}

key_pointer_node_lruhandle_table::key_pointer_node_lruhandle_table(Slice& key, Slice& value){
    max_length_=67108864; //64MB
    ghost_size=65536; //64KB

    now_ghost_length=0;

    KPVHandle* k1=new KPVHandle(value);
    hash_table.emplace(key.ToString(), k1);

    now_size=key.size()+k1->size_;

    tail_=head_=k1;
    boundary_=tail_;
}

key_pointer_node_lruhandle_table::key_pointer_node_lruhandle_table(uint32_t m_len){
    max_length_=m_len;

    ghost_size=m_len/1000;

    now_ghost_length=0;
    now_size=0;
    boundary_=head_=tail_=nullptr;
}

inline void key_pointer_node_lruhandle_table::set_head_value(Slice v){
    now_size-=head_->size_;
    head_->setNewValue(v);
    now_size+=head_->size_;
}

inline uint32_t key_pointer_node_lruhandle_table::a_func(){
    return (uint32_t)log(2000);
}

Status key_pointer_node_lruhandle_table::DeleteSize(uint32_t len){
    if(len>now_size){
        return Status::MemoryLimit();
    }
    Status s=Evict(len);
    if(s.ok()){
        return Status::OK();
    }
    return Status::Aborted();    
}

Status key_pointer_node_lruhandle_table::Lookup(Slice& key, KPVHandle* &ret_ptr){
    auto iter=hash_table.find(key.ToString());
    if(iter==hash_table.end()){
        ret_ptr = new KPVHandle();
        return Status::NotFound();
    } else {
        if(!iter->second){
            return Status::Evicted();
        }
        ret_ptr = iter->second;
        if(ret_ptr->ghost_flag){
            return Status::Ghost();
        }
        return Status::OK();
    }
}

Status key_pointer_node_lruhandle_table::Insert(Slice& key, Slice& value){
    auto iter=hash_table.find(key.ToString());
    if(key.size_+value.size_>max_length_){
        return Status::MemoryLimit("Insert failed due to LRU cache being full.");
    } //K-V本体就大于整个缓存
    if(iter==hash_table.end()||iter->second==nullptr){
        /*原来的表中没有或是表是空的，需要插入*/
        if(iter->second==nullptr){
            hash_table.erase(iter);
        }
        KPVHandle* tmp_kh;
        if(value.size_<24){
            tmp_kh=new KPVHandle(value); //value小于24B, 不存储地址
        } else {
            tmp_kh=new KPVHandle(value, true);
        }
        if(((now_size-now_ghost_length)+key.size()+tmp_kh->size_)>(max_length_-ghost_size)){
            Evict((uint32_t)key.size()+tmp_kh->size_);
        }
        tmp_kh->next_=head_;
        head_->prev_=tmp_kh;
        head_=head_->prev_;
        now_size+=(key.size()+tmp_kh->size_);
        hash_table.emplace(key.ToString(), tmp_kh);
    }

    KPVHandle* ptr=iter->second;

    if(ptr==head_){
        set_head_value(value);
        return Status::OK();
    } //如果命中链表头

    if(ptr==tail_){
        tail_->prev_->next_=nullptr;
        tail_->next_=head_;
        tail_=tail_->prev_;
        ptr->prev_=nullptr;
        head_=ptr;
        ptr->next_->prev_=ptr;
        set_head_value(value);
        return Status::OK();
    } //如果命中链表尾

    if(iter!=hash_table.end()){
        ptr->next_->prev_=ptr->prev_;
        ptr->prev_->next_=ptr->next_;
        ptr->prev_=nullptr;
        ptr->next_=head_;
        head_=ptr;

        set_head_value(value);
        return Status::OK();
    } //如果命中链表内元素但并非表头或表尾

    return Status::Aborted();
}

Status key_pointer_node_lruhandle_table::Remove(Slice& key){
    if(Empty()){
        return Status::NotFound();
    }
    KPVHandle* kh;
    Status s=Lookup(key, kh);
    if(s.ok()||s.IsGhostCache()){
        if(head_==tail_){
            delete head_;
        } else {
            if(head_==kh){
                head_=head_->next_;
                delete head_->prev_;
                head_->prev_=nullptr;
            } else if(tail_==kh){
                tail_=tail_->prev_;
                delete tail_->next_;
                tail_->next_=nullptr;
            } else {
                kh->prev_->next_=kh->next_;
                kh->next_->prev_=kh->prev_;
                delete kh;
            }
        }
        return Status::OK();
    }
    if(s.IsEvicted()){
        return Status::Evicted();
    }
    return Status::NotFound();
}

Status key_pointer_node_lruhandle_table::Adjust(KPVHandle* k){
    auto iter=Find(k);
    now_size-=iter->second->size_;
    iter->second=new KPVHandle(true, false);
    now_size-=iter->second->size_;
    return Status::OK();
}

bool key_pointer_node_lruhandle_table::Empty(){
    return head_==tail_&&tail_==nullptr;
}

//驱逐算法
Status key_pointer_node_lruhandle_table::Evict(uint32_t len){
    if(len>(max_length_ - ghost_size)){
        return Status::MemoryLimit();
    }

    uint32_t tmp_size = max_length_ - ghost_size - now_size + now_ghost_length; //当前表中的剩余空间

    if(len<tmp_size){
        return Status::OK();
    }

    
    std::vector<std::vector<KPVHandle*>> evict_kh_real; //备选的替换Cache对
    KPVHandle* iter=boundary_; //从real Cache的末端开始

    while(tmp_size<len){
        uint32_t sum_size=0;
        std::vector<KPVHandle*> v_tmp;
        uint32_t i=a_func(); //循环a次 ，找到a个备选的cache对
        while(i>0){
            v_tmp.push_back(iter);
            i--;
            sum_size+=iter->size_;
            iter=iter->prev_;
        }
        tmp_size+=sum_size;
        evict_kh_real.push_back(v_tmp);
    }
    for(int i=evict_kh_real.size()-2; i>=0; i--){
        for(int j=evict_kh_real[i].size()-1; j>=0; j--){
            len-=evict_kh_real[i][j]->size_;
            if(now_ghost_length+evict_kh_real[i][j]->size_<ghost_size){
                now_ghost_length+=evict_kh_real[i][j]->size_;
                //TODO: ghost cache保存metadata

                Adjust(evict_kh_real[i][j]);

                if(boundary_==tail_){
                    evict_kh_real[i][j]->prev_->next_=evict_kh_real[i][j]->next_;
                    evict_kh_real[i][j]->next_->prev_=evict_kh_real[i][j]->prev_;
                    evict_kh_real[i][j]->next_=nullptr;
                    evict_kh_real[i][j]->prev_=tail_;
                    tail_=evict_kh_real[i][j];
                    continue;
                }

                if(evict_kh_real[i][j]==head_){
                    // boundary_->next_=head_;
                    if(boundary_->next_){
                        boundary_->next_->prev_=head_;
                    }
                    head_->next_->prev_=nullptr;
                    head_=head_->next_;
                    evict_kh_real[i][j]->prev_=boundary_;
                    evict_kh_real[i][j]->next_=boundary_->next_;
                    boundary_->next_=evict_kh_real[i][j];
                    // head_=evict_kh_real[i][j]->next_;
                    continue;
                }

                boundary_->next_->prev_=evict_kh_real[i][j];
                evict_kh_real[i][j]->prev_->next_=evict_kh_real[i][j]->next_;
                evict_kh_real[i][j]->next_->prev_=evict_kh_real[i][j]->prev_;
                evict_kh_real[i][j]->next_=boundary_->next_;
                evict_kh_real[i][j]->prev_=boundary_;
                boundary_->next_=evict_kh_real[i][j];
            } else {
                delete evict_kh_real[i][j];
            }
        }
    }

    std::sort(evict_kh_real[evict_kh_real.size()-1].begin(), evict_kh_real[evict_kh_real.size()-1].end(), cmp_kpv);

    for(int i=0; i<evict_kh_real[evict_kh_real.size()-1].size(); i++){
        if(len<=0){
            return Status::OK();
        }
        len-=evict_kh_real[evict_kh_real.size()-1][i]->size_;
        if(now_ghost_length+evict_kh_real[evict_kh_real.size()-1][i]->size_<ghost_size){
            now_ghost_length+=evict_kh_real[evict_kh_real.size()-1][i]->size_;
            //TODO: ghost cache保存metadata

            Adjust(evict_kh_real[evict_kh_real.size()-1][i]);

            if(boundary_==tail_){
                evict_kh_real[evict_kh_real.size()-1][i]->prev_->next_=evict_kh_real[evict_kh_real.size()-1][i]->next_;
                evict_kh_real[evict_kh_real.size()-1][i]->next_->prev_=evict_kh_real[evict_kh_real.size()-1][i]->prev_;
                evict_kh_real[evict_kh_real.size()-1][i]->next_=nullptr;
                evict_kh_real[evict_kh_real.size()-1][i]->prev_=tail_;
                tail_=evict_kh_real[evict_kh_real.size()-1][i];
                continue;
            }

            if(evict_kh_real[evict_kh_real.size()-1][i]==head_){
                // boundary_->next_=head_;
                if(boundary_->next_){
                    boundary_->next_->prev_=head_;
                }
                head_->next_->prev_=nullptr;
                head_=head_->next_;
                evict_kh_real[evict_kh_real.size()-1][i]->prev_=boundary_;
                evict_kh_real[evict_kh_real.size()-1][i]->next_=boundary_->next_;
                boundary_->next_=evict_kh_real[evict_kh_real.size()-1][i];
                // head_=evict_kh_real[i][j]->next_;
                continue;
            }

            boundary_->next_->prev_=evict_kh_real[evict_kh_real.size()-1][i];
            evict_kh_real[evict_kh_real.size()-1][i]->prev_->next_=evict_kh_real[evict_kh_real.size()-1][i]->next_;
            evict_kh_real[evict_kh_real.size()-1][i]->next_->prev_=evict_kh_real[evict_kh_real.size()-1][i]->prev_;
            evict_kh_real[evict_kh_real.size()-1][i]->next_=boundary_->next_;
            evict_kh_real[evict_kh_real.size()-1][i]->prev_=boundary_;
            boundary_->next_=evict_kh_real[evict_kh_real.size()-1][i];
        } else {
            delete evict_kh_real[evict_kh_real.size()-1][i];
        }
    }

    return Status::OK();
}

std::unordered_map<std::string, KPVHandle*>::iterator key_pointer_node_lruhandle_table::Find(KPVHandle* k){
    for(auto iter=hash_table.begin(); iter!=hash_table.end(); iter++){
        if(iter->second==k){
            return iter;
        }
    }
    return hash_table.end();
}

void key_pointer_node_lruhandle_table::Clean(){
    std::vector<std::unordered_map<std::string, KPVHandle*>::iterator>  v_iter;
    for(auto iter=hash_table.begin(); iter!=hash_table.end(); iter++){
        if(iter->second==nullptr){
            v_iter.push_back(iter);
        }
    }

    for(auto i : v_iter){
        hash_table.erase(i);
    }
}

// void key_printer_node_lruhandle_table::printHelp(){
//     for(auto iter=hash_table.begin(); iter!=hash_table.end(); iter++){
//         if(iter->second){
//             std::cout << iter->first << ' ';
//         }
//     }
//     std::cout << std::endl;
//     for(auto iter=hash_table.begin(); iter!=hash_table.end(); iter++){
//         if(iter->second){
//             iter->second->printHelp();
//         }
//     }
// }

/********************iCache**********************/

icache::icache(){
    std::cout << "this is iCache!" << std::endl;
    kvt=new key_value_node_lruhandle_table();
    kpt=new key_pointer_node_lruhandle_table();
    sv=new SuperVersion();
}

icache::icache(SuperVersion s){
    std::cout << "this is iCache!" << std::endl;
    kvt=new key_value_node_lruhandle_table();
    kpt=new key_pointer_node_lruhandle_table();
    sv=&s;
}

icache::icache(Slice& key, Slice& value){
    std::cout << "this is iCache!" << std::endl;
    kvt=new key_value_node_lruhandle_table();
    kpt=new key_pointer_node_lruhandle_table(key, value);
    sv=new SuperVersion();
}

icache::icache(uint32_t m_len_kv, uint32_t m_len_kp){
    std::cout << "this is iCache!" << std::endl;
    kvt=new key_value_node_lruhandle_table(m_len_kv);
    kpt=new key_value_node_lruhandle_table(m_len_kp);
    sv=new SuperVersion();
}

icache::~icache(){
    delete kvt;
    delete kpt;
    delete sv;
}

icache::icache(key_value_node_lruhandle_table t1, key_pointer_node_lruhandle_table t2){
    kvt=&t1;
    kpt=&t2;
    sv=new SuperVersion();
}

Status icache::Insert(Slice& key, Slice& value){
    Status s=kpt->Remove(key);
    if(s.ok()){
        s=kvt->Insert(key, value);
        return s;
    }
    if(s.IsNotFound()||s.IsEvicted()){
        s=kvt->Lookup(key);
        if(s.ok()){
            s=kvt->Insert(key, value);
            return s;
        }
        if(s.IsGhostCache()){
            s=kpt->DeleteSize(value.size_);
            kvt->max_length_+=value.size_;
            kvt->Insert(key, value);
            return s; 
        }
        if(s.IsEvicted()){
            s=kpt->Insert(key, value);
            return s;
        }
    }
    return Status::Aborted();
}

Status icache::Lookup(Slice& key, KPVHandle* &ret_ptr){
    Status s=kpt->Lookup(key, ret_ptr);
    if(s.IsNotFound()||s.IsEvicted()){
        s=kvt->Lookup(key, ret_ptr);
        if(s.ok()){
            return Status::OK();
        } else {
            ReadOptions ro();
            const Slice ck=key;
            LookupKey* lkey=new LookupKey (ck, 0);
            PinnableSlice* ps;
            PinnableWideColumns* pwc;
            Status* s_ptr;
            sv->current->Get(ro, lkey, ps, pwc, nullptr, s_ptr, nullptr, nullptr, nullptr);

            if(s.IsGhostCache()){
                kpt->DeleteSize((uint32_t)ps->size_);
            }

            if(s_ptr->OK()){
                kvt->Insert(key, ps->toSlice());
                return OK();
            }

            return NotFound();
        }
    }

    if(s.IsGhostCache()){
        ReadOptions ro(true, false);
        // const Slice ck=key;
        // // kvt->DeleteSize();
        LookupKey* lkey=new LookupKey (ck, 0);
        // Slice* ps;
        PinnableWideColumns* pwc;
        Status* s_ptr;
        std::string* value_str;
        sv->cfd->mem()->Get(lkey, value_str, pwc, nullptr, s_ptr, nullptr, nullptr, nullptr, ro, true, nullptr, nullptr, true);
        kvt->DeleteSize((uint32_t)value_str->size());
        const std::string str=*value_str;
        s=kpt->Insert(key, Slice(str));
        return s;
    }

    if(s.ok()){
        if(ret_ptr->p_flag){
            ReadOptions ro();
            const Slice ck=key;
            LookupKey* lkey=new LookupKey (ck, 0);
            PinnableSlice* ps;
            PinnableWideColumns* pwc;
            Status* s_ptr;
            sv->current->Get(ro, lkey, ps, pwc, nullptr, s_ptr, nullptr, nullptr, nullptr);
            kpt->Remove(key);
            kvt->Insert(key, ps->toSlice());
        } else {
            kvt->Insert(key, ret_ptr->value_);
            kpt->Remove(key);
        }
    }
    return Status::Aborted();
} //MergeContext 是查找内容的上下文，类似于局部性的原理，它只和memtable中的内容相关
  //SequenceNumber 不清楚具体作用，但是它只与WAL日志文件的查找有关
  //timestamp 时间戳，版本新加入的，具体内容不理解
  //columns 所在列族，因为是单列族可用，因此固定值为默认

  // readoptions: cksnum:verify_checksums，就是在遍历时对所有数据进行校验，默认是true, cache默认也是true, 所以可以直接默认构造。

Status iCache::Erase(Slice& key){
    Status s_1=kpt->Remove(key);
    Status s_2=kvt->Remove(key);
    if(s_1.ok()||s_1.IsGhostCache()||s_2.ok()||s_2.IsGhostCache()){
        return Status::OK();
    }
    return Status::NotFound();
}

Status iCache::Flush(Slice& key){
    ReadOptions ro(true, false);
    // const Slice ck=key;
    // // kvt->DeleteSize();
    LookupKey* lkey=new LookupKey (ck, 0);
    // Slice* ps;
    PinnableWideColumns* pwc;
    Status* s_ptr;
    std::string* value_str;
    sv->cfd->mem()->Get(lkey, value_str, pwc, nullptr, s_ptr, nullptr, nullptr, nullptr, ro, true, nullptr, nullptr, true);

    const std::String str=*value_str;

    Slice value_new=Slice(str);

    return kpt->Insert(key, value_new);
}

}
}