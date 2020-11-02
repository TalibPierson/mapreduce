#include "mapreduce.hh"

#include <future>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace MapReduce;
int num_part;
// Use part[partkey][key].emplace_back(value)
using PART =
std::vector<std::unordered_map<std::string, std::vector<std::string>>>;
PART parts;
std::queue<std::future<PART>> threadQueue;

void deleteVal(const std::string &key, int part_num) {
    parts[part_num].at(key).erase(parts[part_num].at(key).begin());
}

std::string get(const std::string &key, int part_num) {
    if (!parts[part_num].at(key).empty()) {
        std::string value = parts[part_num].at(key).front();
        deleteVal(key, part_num);
        return value;
    }
    return "";
}

void MapReduce::MR_Run(int argc, char *argv[], MapReduce::mapper_t map,
                       int num_mappers, MapReduce::reducer_t reduce,
                       int num_reducers, MapReduce::partitioner_t partition) {
    num_part = num_reducers;
    parts.resize(static_cast<size_t>(num_part));

    for(int i = 0; i < argc; i++){
        if(threadQueue.size() >= num_mappers){
            threadQueue.front().get();
            threadQueue.pop();
        }
        threadQueue.emplace(std::async(std::launch::async, map, argv[i]));
    }
    while(!threadQueue.empty()){
        threadQueue.front().get();
        threadQueue.pop();
    }

    for(int i = 0; i < num_reducers; i++){
        threadQueue.emplace(std::async(std::launch::async, reduce, NEEDKEY, get, partitioner(NEEDKEY, num_part)));
    }

    while(!threadQueue.empty()){
        threadQueue.front().get();
        threadQueue.pop();
    }

}

void MapReduce::MR_Emit(const std::string &key, const std::string &value) {
    unsigned long partKey = MR_DefaultHashPartition(key, num_part);
    int exists = parts[partKey].count(key);
    if (exists > 0){
        parts[partKey][key].emplace_back(value);
    }
    else{
        auto insert = std::pair<std::string, std::vector<std::string>>(key, {value});
        parts[partKey].insert(insert);
    }
}

unsigned long MapReduce::MR_DefaultHashPartition(const std::string &key,
                                                 int num_partitions) {
    unsigned long hash = std::hash<std::string>{}(key);
    return hash % num_partitions;
}