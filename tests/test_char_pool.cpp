#include <vector>
#include <iostream>
using std::vector;

#include "../src/char_pool.hpp"

const int POOL_MIN_SIZE = 32;
const int POOL_MAX_SIZE = 1024 * 4;
typedef char_pool_t<POOL_MIN_SIZE, POOL_MAX_SIZE> char_pool;
typedef vector<char*> char_array;

int main(int argc, char* argv[])
{
    vector<int> int_arr_ = { 4, 26, 30, 45, 60,150, 500, 32, 49,3056,4096,4099,128, 582, 1024,1025 };

    char_pool pool_;
    std::cout << "first pool size: " << pool_.size() << std::endl;

    char_array arr_;
    for(auto it=int_arr_.begin();it!=int_arr_.end();++it)
    {
        char* ptr_ = pool_.alloc(*it);
        if(ptr_ == nullptr)
        {
            std::cout << "alloc memory failed, size: " << *it << std::endl;
            continue;
        }
        arr_.push_back(ptr_);
    }
    std::cout << "alloced pool size: " << pool_.size() << std::endl;

    for(auto it=arr_.begin();it!=arr_.end();)
    {
        char* ptr_ = *it;
        if(!pool_.contain(ptr_))
        {
            std::cout << " find ptr not in pool. " << std::endl;
            ++it;
            continue;
        }
        pool_.free(ptr_);
    }
    std::cout << "freed array size: " << arr_.size() << std::endl;

    pool_.clear();
    std::cout << "cleared pool size: " << pool_.size() << std::endl;

    getchar();
    return 0;
}