#pragma once

#include <unordered_map>
// #include <ranges>


namespace pie {
namespace ds {

template<typename Key, typename Value>
class InsertionOrderedMap : public std::unordered_map<Key, Value> {

public:
    using std::unordered_map<Key, Value>::unordered_map;
    using std::unordered_map<Key, Value>::operator=;


    
};


}
}