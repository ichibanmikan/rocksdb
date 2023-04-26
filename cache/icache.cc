#include "cache/icache.h"

namespace ROCKSDB_NAMESPACE {

namespace icache {

KPVHandle::KPVHandle(bool pf=false){
    value_();
    next_=prev_=nullptr;
    p_flag=pf;
    size_=0;
    ghost_flag=false;
}

KPVHandle::KPVHandle(const Slice& v, bool pf=false){
    value_(v.data());
    next_=prev_=nullptr;
    p_flag=pf;
    size_=v.size()+24;
    ghost_flag=false;
}

KPVHandle::KPVHandle(const Slice& v, KPVHandle* p, KPVHandle* n, bool pf=false){
    value(v.data());
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

key_value_node_lruhandle_table::key_value_node_lruhandle_table(const Slice& key, Slice& value){
    max_length_=67108864; //64MB
    ghost_size=65536; //64KB

    now_ghost_length=0;

    KPVHandle* k1=new KPVHandle(value);
    hash_table.emplace(key, k1);

    now_size=key.size()+k1->size_;

    tail_=head_=k1;
    boundary_=tail_;
}

key_value_node_lruhandle_table::key_value_node_lruhandle_table(){
    max_length_=67108864;
    ghost_size=65536;

    now_ghost_length=0;

    now_size=0;
    boundary_=head_=tail_=nullptr;
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

inline uint32_t a_func(){
    return (uint32_t)log(2000);
}

Status key_value_node_lruhandle_table::Lookup(const Slice& key, KPVHandle* &ret_ptr){
    auto iter=hash_table.find(key);
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
        return Status::OK();
    }
}

Status key_value_node_lruhandle_table::Insert(const Slice& key, const Slice& value){
    auto iter=hash_table.find(key);
    if(key->size_+value->size_>max_length_){
        return Status::MemoryLimit("Insert failed due to LRU cache being full.");
    }
    if(iter==hash_table.end()||iter->second==nullptr){
        /*原来的表中没有或是表是空的，需要插入*/
        if(iter->second==nullptr){
            hash_table.erase(iter);
        }
        KPVHandle* tmp_kh=new KPVHandle(value);
        if(((now_size-now_ghost_length)+key.size()+tmp_kh->size_)>(max_length_-ghost_size)){
            // delete tmp_kh;
            Evict(key.size()+tmp_kh->size_);
        }
        tmp_kh->next_=head_;
        head_->prev_=tmp_kh;
        head_=prev_;
        now_size+=(key.size()+tmp_kh->size_);
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
        if(ptr->ghost_flag){
            /*****************************/
            //TODO: 如果命中的是ghost cache中的内容
        } else {
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
        }
        return Status::OK();
    } //如果命中链表尾

    if(iter!=hash_table.end()){
        if(ptr->ghost_flag){
            /*****************************/
        } else {
            ptr->next_->prev_=ptr->prev_;
            ptr->prev_->next_=ptr->next_;
            ptr->prev_=nullptr;
            ptr->next_=head_;
            head_=ptr;

            set_head_value(value);

            // now_size-=ptr->size_;
            // ptr->size_-=ptr->value_.size();
            // ptr->value_=value;
        }
        return Status::OK();
    } //如果命中链表内元素但并非表头或表尾

    return Status::Aborted();
}

// Status key_value_node_lruhandle_table::Remove(const Slice& key){
//     if(empty()){
//         return Status::NotFound();
//     }
//     KPVHandle* kh;
//     Status s=key_value_node_lruhandle_table::Lookup(key, kh);
//     if(s.ok()){
//         if(kh==head_){
//             head_=head_->next
//         }
//         return Status::OK();
//     }
//     if(s.IsEvicted()){

//     }
// }

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

    // for(int i=v_tmp.size()-1; i>=0; i--){
    for(int i=v_tmp.size(); i>=0; i--){
        for(int j=v_tmp[i].size(); j>=0; j--){
            len-=v_tmp[i][j]->size_;
            if(now_ghost_length+v_tmp[i][j]->size_<ghost_size){
                now_ghost_length+=v_tmp[i][j]->size_;
                //TODO: ghost cache保存metadata

                if(boundary_==tail_){
                    v_tmp[i][j]->prev_->next_=v_tmp[i][j]->next_;
                    v_tmp[i][j]->next_->prev_=v_tmp[i][j]->prev_;
                    v_tmp[i][j]->next_=nullptr;
                    v_tmp[i][j]->prev_=tail_;
                    tail_=v_tmp[i][j];
                    continue;
                }

                if(v_tmp[i][j]==head_){
                    // boundary->next=head_;
                    if(boundary_->next_){
                        boundary_->next_->prev_=head_;
                    }
                    head_->next_->prev_=nullptr;
                    head_=head_->next_;
                    v_tmp[i][j]->prev_=boundary_;
                    v_tmp[i][j]->next_=boundary_->next_;
                    boundary_->next=v_tmp[i][j];
                    // head_=v_tmp[i][j]->next;
                    continue;
                }

                boundary_->next_->prev_=v_tmp[i][j];
                v_tmp[i][j]->prev_->next_=v_tmp[i][j]->next_;
                v_tmp[i][j]->next_->prev_=v_tmp[i][j]->prev_;
                v_tmp[i][j]->next_=boundary_->next_;
                v_tmp[i][j]->prev_=boundary_;
                boundary_->next_=v_tmp[i][j];
            } else {
                delete v_tmp[i][j];
            }
        }
    }

    return Status::OK();
}



}
}