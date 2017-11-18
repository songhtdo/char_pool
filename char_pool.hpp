#ifndef SPEED_CHAR_POOL_H_
#define SPEED_CHAR_POOL_H_

#include <mutex>
#include <deque>
#include <unordered_map>
#include <cassert>
#include <string.h>

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
class char_pool_t
{
  typedef std::mutex this_mutex;
  typedef std::unique_lock<std::mutex> this_guard;
  //数组节点
  typedef struct tag_char_node
  {
    unsigned int size;
    char* data;
    //下一节点
    struct tag_char_node* next;
  } char_node;
  //空闲头部数据
  typedef struct tag_char_node_head
  {
    unsigned int count;
    char_node* head;
  } char_node_head;
  typedef typename std::unordered_map<const char*, char_node*> char_node_map;
  //缓存最小尺寸、最大尺寸、每个空闲队列限制长度 //SIZE_RATE=128, LIMIT_MAX_SIZE = 4096, 
  enum { ONCE_DEQUE_LIMIT_SIZE = 10, SIZE_ALIGN = 8 };
public:
  char_pool_t(int once_limit_size = ONCE_DEQUE_LIMIT_SIZE);
  ~char_pool_t();

public:
  void set_once_limit_size(unsigned int size) { this->once_limit_size_ = size; }
  const unsigned int get_size_rate(void) const { return SIZE_RATE; }
  const unsigned int get_limit_max_size(void) const { return LIMIT_MAX_SIZE; }
  char* alloc(unsigned int size);
  void free(char*& ptr);
  bool contain(const char* ptr);
  void clear(void);
  size_t size(void) const;
  bool empty(void) const;

private:
  void init_pool(void);
  bool find_size_range(unsigned int size, unsigned int& roundup_size);
  unsigned int find_size_index(unsigned int size);
  char_node* alloc_char_node(unsigned int size);
  void free_char_node(char_node*& node);

private:
  unsigned int once_limit_size_;
  char_node_map node_map_;
  char_node_head* idle_list_;
  unsigned int idle_list_count_;
  mutable this_mutex mutex_;
};

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::char_pool_t(int once_limit_size)
  :idle_list_(0), idle_list_count_(0), once_limit_size_(once_limit_size)
{
  static_assert((LIMIT_MAX_SIZE % SIZE_RATE == 0), "LIMIT_MAX_SIZE must be SIZE_RATE rate.");
  static_assert((LIMIT_MAX_SIZE % SIZE_ALIGN == 0), "LIMIT_MAX_SIZE must be SIZE_ALIGN rate.");
  this->init_pool();
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::~char_pool_t()
{
  this->clear();

  if (this->idle_list_)
  {
    delete[] this->idle_list_;
    this->idle_list_ = 0;
  }
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
char * char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::alloc(unsigned int size)
{
  if (size > 0)
  {
    this_guard guard_(this->mutex_);
    //查找所属的节点位置及尺寸范围
    unsigned int index_ = 0;
    unsigned int roundup_size_ = 0;
    char_node* node_ = 0;
    if (find_size_range(size, roundup_size_))
    {
      index_ = this->find_size_index(roundup_size_);
      char_node_head& head_ = this->idle_list_[index_];
      if (!head_.head)
      {//如果此节点上面无对象，临时创建
        node_ = this->alloc_char_node(roundup_size_);
        this->node_map_.insert(std::make_pair(node_->data, node_));
      }
      else
      {//移除空闲头部位置节点
        node_ = head_.head;
        char_node* next_ = node_->next;
        head_.head = next_;
        head_.count--;
      }
      //缓存数据置0，将下一节点指针置空
      node_->next = 0;
      memset(node_->data, 0, node_->size);
      return node_->data;
    }
    else
    {//大于最大长度4096，临时创建
      node_ = this->alloc_char_node(size);
      this->node_map_.insert(std::make_pair(node_->data, node_));
      memset(node_->data, 0, node_->size);
      return node_->data;
    }
  }
  return nullptr;
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
void char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::free(char *& ptr)
{
  assert(ptr);
  if (!ptr)
    return;
  this_guard guard_(this->mutex_);
  //查找当前指针是否属于缓存
  typedef typename char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::char_node_map::iterator char_node_map_iter;
  char_node_map_iter it_ = this->node_map_.find(ptr);
  if (it_ == this->node_map_.end())
    return;
  //判断是否大于最高限值，高于最高限值释放空间
  char_node* node_ = it_->second;
  if (node_->size > LIMIT_MAX_SIZE)
  {
    free_char_node(node_);
    this->node_map_.erase(it_);
    ptr = 0;
    return;
  }
  //从首部插入空闲节点数据
  unsigned int index_ = this->find_size_index(node_->size);
  char_node_head& head_ = this->idle_list_[index_];
  //如果当前队列的总数量小于许可长度
  if ((this->once_limit_size_ > 0) && (this->once_limit_size_ > head_.count))
  {
    node_->next = head_.head;
    head_.head = node_;
    head_.count++;
  }
  else
  {//大于等于许可长度，则直接释放当前空间
    free_char_node(node_);
    this->node_map_.erase(it_);
  }
  ptr = 0;
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
void char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::clear(void)
{
  this_guard guard_(this->mutex_);
  for (auto it_ = this->node_map_.begin(); it_ != this->node_map_.end();)
  {
    char_node* node_ = it_->second;
    free_char_node(node_);
    it_ = this->node_map_.erase(it_);
  }
  memset(this->idle_list_, 0, sizeof(char_node_head)*this->idle_list_count_);
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
size_t char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::size(void) const
{
  this_guard guard_(this->mutex_);
  return this->node_map_.size();
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
bool char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::empty(void) const
{
  this_guard guard_(this->mutex_);
  return this->node_map_.empty();
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
bool char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::contain(const char * ptr)
{
  this_guard guard_(this->mutex_);
  //查找当前指针是否属于缓存
  typedef typename char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::char_node_map::iterator char_node_map_iter;
  char_node_map_iter it_ = this->node_map_.find(ptr);
  return (it_ != this->node_map_.end());
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
void char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::init_pool()
{
  assert(!this->idle_list_);

  this_guard guard_(this->mutex_);
  this->idle_list_count_ = LIMIT_MAX_SIZE / SIZE_RATE + ((LIMIT_MAX_SIZE % SIZE_RATE) > 0 ? 1 : 0);
  this->idle_list_ = new char_node_head[this->idle_list_count_];
  memset(this->idle_list_, 0, sizeof(char_node_head) * this->idle_list_count_);
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
bool char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::find_size_range(unsigned int size, unsigned int & roundup_size)
{
  if (size == 0)
    return false;
  //如果大于上限字节
  if (size > LIMIT_MAX_SIZE)
  {
    roundup_size = (size + ((size % SIZE_ALIGN) > 0) ? (SIZE_ALIGN - (size % SIZE_ALIGN)) : 0);
    return false;
  }
  //向上取整数
  unsigned int remaind_ = (size % SIZE_RATE);
  roundup_size = (size + ((remaind_ > 0) ? (SIZE_RATE - remaind_) : 0));
  return true;
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
unsigned int char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::find_size_index(unsigned int size)
{
  return ((size / SIZE_RATE) - 1 + ((size % SIZE_RATE) > 0 ? 1 : 0));
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
typename char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::char_node* char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::alloc_char_node(unsigned int size)
{
  assert(size > 0);
  char* new_ptr_ = new(std::nothrow)char[size];
  //创建数组失败
  if (!new_ptr_) return nullptr;
  //返回对象
  char_node* node = new char_node;
  node->size = size;
  node->data = new_ptr_;
  node->next = NULL;
  return node;
}

template<unsigned int SIZE_RATE, unsigned int LIMIT_MAX_SIZE>
void char_pool_t<SIZE_RATE, LIMIT_MAX_SIZE>::free_char_node(char_node*& node)
{
  if (node->data && node->size > 0)
  {//转换为char*，释放空间
    delete[] node->data;
    node->data = 0;
    delete node;
    node = 0;
  }
}

#endif