#ifndef DISK_PARTITION
#define DISK_PARTITION

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <climits> //linux编译必须

using std::unordered_map;
using std::unordered_multimap;
using std::unordered_set;
using std::vector;
using std::string;

const int MAX_DISK_NUM = (10 + 1);          // 最大硬盘数(示例中预留10个+1)
const int MAX_DISK_SIZE = (16384 + 1);      // 每块硬盘最大存储单元数量(示例中预留16384+1)
const int MAX_REQUEST_NUM = (30000000 + 1); // 最大读请求数量
const int MAX_OBJECT_NUM = (100000 + 1);    // 最大对象数量
const int REP_NUM = 3;                      // 冗余副本数量(要求3份)
const int MAX_SIZE = 5;                     // 题目中每个object的size最大值为5
const int READ_COST[8] = { 64,52,42,34,28,23,19,16 };//连续读取消耗的tooken值
const int READ_NULL_THRESHOLD[3] = { 3,5,6 };
//const int READ_NULL_THRESHOLD[7] = { 0,1,2,4,6,7,8 };
const int FRE_PER_SLICING = 1800;           // 时间片相关(示例中只演示，不深入使用)
const int RED_POSITION_THRE = 9000;         // 红色位置的请求数量阈值，注意分数*1000
const int CHECKED_TIME = 2;                 //read_most()函数检查的时间片数量
const int SMALL_PARTITION_LENGTH = 40;      // 指定小分区的长度
//const double SHARED_PARTITION_THRE = 1.0;   //共享分区写操作与删操作次数的比例阈值
const int LOWEST_LENGTH = 13;            //判断共享分区时的最小余量
const double HOT_PERCENT = 0.80;

// 请求结构体，用于保存某次读请求的信息
typedef struct Request_ {
    int object_id;  // 需要读取的对象ID
    int object_size;// 需要读取的对象的size
    bool has_read[MAX_SIZE];
    int start_time;   //该请求发起的时间片序号
    bool is_done;   // 该请求是否已完成
    void check_done(){
        for (int i = 0;i < object_size;++i) {
            if (!has_read[i]) {
                is_done = false;
                return;
            }
        }
        is_done = true;
    }
} Request;

// 对象结构体，用于保存对象信息，注意Request和Object的索引都是从1开始
typedef struct Object_ {
    int replica[REP_NUM];          // 记录对象的三个副本在哪些硬盘上
    int* unit[REP_NUM];            // 每个副本对应的存储单元编号数组
    int size;                      // 对象大小(单位: 对象块数)
    int tag;                       // 新增：对象的tag
    //int last_request_point;        // 记录最后一次发起读请求的编号(用于回溯、取消等)
    bool is_delete;                // 标记该对象是否已被删除
} Object;

//得分的函数f(x)
inline int func(int x) {
    int ans;
    if (x >= 0 && x <= 10) {
        ans = -5 * x + 1000;
    }
    else if (x > 10 && x < 105) {
        ans = -10 * x + 1050;
    }
    else
        ans = 0;
    return ans;
}

//寻找给定数组的最长连0的长度和起始索引，返回：长度
static bool max_consecutive_zero(const vector<int>& nums, int threshold) {
    int begin_idx = 0;
    //max_consecutive：最长连0的长度，current_consecutive：当前连0的长度
    int max_consecutive = 0, current_consecutive = 0;
    for (int i = 0;i < nums.size();++i) {
        if (nums[i] == 0) {
            current_consecutive++;
            if (current_consecutive > max_consecutive) {
                max_consecutive = current_consecutive;
                begin_idx = i - current_consecutive + 1;
            }
        }
        else {
            current_consecutive = 0;
        }
        if (max_consecutive >= threshold)
            return true;
    }
    return false;
}

//返回给定数组的升（降）序索引
vector<int> sort_idx(const vector<int>& nums, char c = '<') {
    vector<int> res(nums.size());
    for (int i = 0;i < nums.size();++i) {
        res[i] = i;
    }
    switch (c) {
    case '>':
        std::stable_sort(res.begin(), res.end(), [=](int i1, int i2) {return nums[i1] > nums[i2];});
        break;
    case '<':
        std::stable_sort(res.begin(), res.end(), [=](int i1, int i2) {return nums[i1] < nums[i2];});
        break;
    default:
        throw "Unexpected charactors";
        break;
    }
    return res;
}

class DISK {
private:
    const int Len;                          //硬盘大小，使用size()接口
    int remain;                             //硬盘当前剩余空闲位置数，使用remaining()接口
    vector<int> data;                          //存储的数据，使用[]运算符重载访问
    const int M;                               //tag值的种类数，该值必须在构造函数中初始化
    const int G;                               //单次操作最多消耗的tooken数，必须初始化
    int tooken_[2];                            //表示当前硬盘剩余的tooken
    int head_pos[2] = {0,0};                //表示当前硬盘的磁头位置，注意索引从0开始
    int consecutive_read[2] = {0,0};           //表示磁头连读的次数
    int d[2];                                  //硬盘d值（与最近的有请求位置的距离）
    struct partition {
        int key;                            //该分区的key
        int begin;                       //该分区在硬盘的起始位置索引
        int length;                      //该分区的长度
        int remain;                      //该分区的余量
        partition* shared;                  //共享的分区的对象地址，默认为空
        //默认构造函数（供编译器生成哈希表用，实际不调用）
        partition() : key(0), begin(0), length(0), remain(0), shared(nullptr) {}
        //构造函数
        partition(DISK& d, int k, int beg, int len) : key(k), begin(beg), length(len),remain(len), shared(nullptr) {
            //构造分区，data值写为-1
            for (int i = beg;i < beg + len;i++) {
                d.data[i] = -1;
            }
        }
        //获得分区从左到右第一个可用位置索引，已满则返回begin + length
        int get_next_avail(const DISK& d) const{
            for (int i = begin;i < begin + length;i++) {
                if (d.data[i] <= 0) {
                    return i;
                }
            }
            return begin + length;
        }
        //返回分区是否为空
        bool empty()const { return remain == length; }
        //返回该分区的尾部n个元素是不是都是空的，如果是就返回真
        bool space_too_much(const DISK& disks,int n) const {
            /*if (length <= n)
                return false;*/
            for (int i = begin + length - 1;i > begin + length - 1 - n;--i) {
                if (disks.data[i] > 0)
                    return false;
            }
            return true;
        }
        //改变余量+x，如果有共享分区需要同步改变
        void maintain_remain(int x) {
            remain += x;
            if (shared != nullptr) {
                shared->remain += x;
                //起始位置索引和长度也一起同步，防止分割小分区时产生bug
                shared->length = length;
                shared->begin = begin;
            }
        }
        //返回分区的整齐度ó
        double cal_sita(const DISK& d, int begin, int length)const {
            int last_num = 0;
            int total_son = 0, conse = 0;
            for (int i = begin;i < begin + length;++i) {
                if (d.data[i] == last_num) {
                    conse++;
                }
                else {
                    last_num = d.data[i];
                    total_son += conse * conse;
                    conse = 1;
                }
            }
            total_son += conse * conse;
            return (double)total_son / (double)(length * length);
        }
    };
    struct UNIT {
        int obj_id;//object编号
        int block_id;//block编号（0到size）
        int replica;//副本编号
    };
    unordered_multimap<int, partition> table;  //哈希表，使用tabled()接口
    string c_str[2] = { "#","#" };             //用于保存硬盘动作的字符串
    /*一个1* Len的数组，表示硬盘上每个存储单元存储的数据。如果第i个存储单元当前存储了数据，则save_pos[i] = {第i个存储单元存储的obj编号，该obj的block编号，该obj的副本编号}，如果第i个存储单元当前不存在读请求则save_pos[i] = {0,0,0}*/
    vector<UNIT> save_pos;   
    /*一个1* Len的数组，表示硬盘上第i个存储单元当前是否存在读请求，如存在则request_id[i] = vector<int>{(该位置的所有请求编号...)}，元素request_id[i][j]表示第i个存储单元当前第j个读请求的编号*/
    vector<vector<int>> request_id; 
    bool time_flag[2] = {true,true};       //时间片标志，表示当前时间片该硬盘磁头是否能继续移动

    // 返回下一个能连续c个连续的存储单元的起始位置索引，供分区使用,（已预分配的不可用于分区），如果找不到则返回-1
    int next_partition_begin_idx(int c)const {
        int left = 0, right = 0;
        while (true) {
            if (right == Len)
                return -1;
            while (data[right] != 0) {
                right++;
                left = right;
                if (right == Len)
                    return -1;
            }
            if (right - left + 1 == c) {
                break;
            }
            right++;
        }
        return left;
    }
    // 在硬盘中从末尾往前找 size 个连续的空闲存储单元（data[i] == 0或-1），并返回其起始位置索引；找不到返回-1
    int find_consecutive_from_end(int size) const {
        // left、right 都从末尾开始
        int right = Len - 1, left = Len - 1;
        while (true) {
            if (left < 0) {
                return -1; // 已到最前，依然找不到足够的连续空闲
            }
            // 遇到非空单元就把 right、left 同时往前挪动
            while (data[left] > 0) {
                --left;
                right = left;
                if (left < 0) {
                    return -1;
                }
            }
            // 现在 data[left] == 0，检查 [left..right] 是否已经有 size 个连续的空闲
            if (right - left + 1 == size) 
                return left;// 找到，返回起始索引
            // 否则继续缩小区间，尝试往前找
            --left;
        }
    }
    //尝试创建一个长度为length的新分区，如果成功就返回其起始存储单元位置索引，失败就返回-1，size表示第一次即将写入的数据长度，用于维护remain
    int creat_partition(int length, int tag, int size = 0) {
        int begin_idx = next_partition_begin_idx(length);//连续length个连续的空闲的存储单元的起始位置索引
        if (begin_idx == -1)
            return -1;//创建失败，返回-1
        partition p(*this, tag, begin_idx, length);
        p.remain -= size;//注意维护新的分区的size值
        table.insert({ tag,p });
        return begin_idx;
    }
    //判断分区是否具有连续size个空闲空间，如果有则返回连续size个空闲空间的起始索引，没有则返回-1
    int enough_consecutive_space(int size, const partition& part)const {
        int left = part.begin, right = part.begin;
        while (true) {
            if (right == part.begin + part.length) {
                return -1;
            }
            while (data[right] > 0) {
                right++;
                left = right;
                if (right == part.begin + part.length) {
                    return -1;
                }
            }
            if (right - left + 1 == size) {
                break;
            }
            right++;
        }
        return left;
    }
    //共享一个分区
    void dumplicated(int tag) {
        auto range = table.equal_range(1);
        for (auto it = range.first;it != range.second;it++) {
            //注意共享分区的前提条件，仍需要改变
            if (it->second.remain > it->second.length / 2 && it->second.shared == nullptr) {
                partition p = it->second;
                p.key = tag, p.shared = &it->second;
                //保存哈希表中新插入的partition对象的地址（原来的对象p即将被销毁，&p无效）
                auto p_iter_in_hash_map = table.insert({ tag,p });
                it->second.shared = &p_iter_in_hash_map->second;
                break;//跳出for，结束
            }
        }
    }
    //暴力写入（从右往左找到一个空位置就写入），返回写入位置的索引
    int brute_write(int tag) {
        int write_idx = Len - 1;
        while (data[write_idx] > 0) {
            write_idx--;//write_idx是第一个空位置
            if (write_idx == -1) {
                throw std::out_of_range("磁盘空间已满，写入操作失败");
                break;
            }
        }
        data[write_idx] = tag;
        remain--;
        for (auto it = table.begin();it != table.end();it++) {
            if (it->second.begin <= write_idx && write_idx < it->second.begin + it->second.length) {
                //it->second.remain--;//对应位置分区余量-1
                it->second.maintain_remain(-1);
                break;
            }
        }
        return write_idx;
    }
public:
    DISK() :Len(1), remain(1), M(1), G(64) {
        data = vector<int>(Len, 0);
        save_pos = vector<UNIT>(Len);
        request_id = vector<vector<int>>(Len);
        tooken_[0] = G, tooken_[1] = G;
        d[0] = Len, d[1] = Len;
    }
    DISK(int v, int m, int g) :Len(v), remain(v), M(m),G(g) {
        data = vector<int>(Len, 0);
        save_pos = vector<UNIT>(Len);
        request_id = vector<vector<int>>(Len);
        tooken_[0] = G, tooken_[1] = G;
        d[0] = Len, d[1] = Len;
    }
    //返回当前硬盘的磁头位置索引（索引从0开始）
    int head(int i)const { return head_pos[i]; }
    int& head(int i) { return head_pos[i]; }
    //返回硬盘磁头i剩余的tooken值
    int tooken(int i)const { return tooken_[i]; }
    //返回硬盘时间标志位（传递地址方便外部修改这个值）
    bool& flag(int i) { return time_flag[i]; }
    //返回当前硬盘第idx个存储单元存储的对象的tag值，若该位置没有存储对象则返回0
    const vector<int>& dat()const { return data; }
    //返回两个磁头d值较小的那个的索引（即返回0或1）
    int get_min_d_id()const {
        if (d[0] <= d[1])
            return 0;
        else
            return 1;
    }
    //返回硬盘磁头i的d值
    int get_d(int i)const { return d[i]; }
    //硬盘进入下一个时间片，更新硬盘参数（更新每一个硬盘的剩余tooken为G，刷新参数c_str，刷新time_flag为true）
    friend void update_timesample(vector<DISK>& disks) {
        for (int i = 0;i < disks.size();++i) {
            disks[i].tooken_[0] = disks[i].G , disks[i].tooken_[1] = disks[i].G;
            disks[i].c_str[0] = "#", disks[i].c_str[1] = "#";
            disks[i].time_flag[0] = true, disks[i].time_flag[1] = true;
        }
    }
    //在每个时间片开始时更新d值
    friend void check_d_at_time_start(vector<DISK>& disks, int time, const Request r[]) {
        for (int i = 0;i < disks.size();++i) {
            if (disks[i].d[0] != disks[i].Len && disks[i].get_position_scores((disks[i].head_pos[0] + disks[i].d[0]) % disks[i].Len, time, r) == 0) {
                //最近的位置的请求全部过期了，更新d值
                disks[i].d[0] = disks[i].update_d(time, r, 0);
            }
            if (disks[i].d[1] != disks[i].Len && disks[i].get_position_scores((disks[i].head_pos[1] + disks[i].d[1]) % disks[i].Len, time, r) == 0) {
                disks[i].d[1] = disks[i].update_d(time, r, 1);
            }
        }
    }

    /*
    * @brief 返回当前磁头到目标索引target的距离（磁头只能单向移动）
    * @param target  目标位置索引（从0开始）
    * @param i       磁头序号（0或1）
    * @return        磁头i与目标位置的距离
    */
    int distance(int target,int i)const { 
        int temp = 0;
        // 如果目标位置在当前磁头之后，直接做减法
        if (target >= head_pos[i]) {
            temp = target - head_pos[i];
        }
        else {
            // 否则先算与尾部的距离，再加上 target
            temp = (Len - head_pos[i]) + target;
        }
        return temp;
    }
    /*@brief 估算某时间片读取索引idx的存储单元的得分
    * @param idx     该硬盘的存储单元索引
    * @param time    当前时间片序列
    * @param r       请求的request数组
    */
    int get_position_scores(int idx, int time, const Request r[])const {
        int scores = 0;//分数
        for (int i = 0;i < request_id[idx].size();i++) {
            scores += func(time - r[request_id[idx][i]].start_time);// * (r[request_id[idx][i]].object_size + 1) / 2;
        }
        return scores;
    }
    /*
    * @brief                计算连续读取n次需要消耗的tooken值
    * @param  conse         起始时的连读次数
    * @param  n             接下来要连续读取的次数
    * @return               int值，连续读取n次消耗的tooken值
    */
    int read_cost(int conse, int n) const {
        int cost = 0;
        for (int i = 0;i < n;++i) {
            if (conse + i < 7)
                cost += READ_COST[conse + i];
            else
                cost += READ_COST[7];
        }
        return cost;
    }
    /*
    * @brief 通过遍历计算硬盘的d值（时间复杂度高，尽量减少调用）（跳过得分为0的位置）
    * @param time    当前时间片序号
    * @param r       当前读请求数列request[]
    * @param i       当前计算的磁头序号（0或1）
    * @return        返回更新后的磁头i的d值
    */
    int update_d(int time, const Request r[],int i) const {
        int ans = 0;
        while (request_id[(ans + head_pos[i]) % Len].empty()) {
            ans++;
            if (ans == Len) {
                break;
            }
        }
        return ans;
    }
    int update_d(int i) const {
        int ans = 0;
        while (request_id[(ans + head_pos[i]) % Len].empty()) {
            ans++;
            if (ans == Len) {
                break;
            }
        }
        return ans;
    }
    //通过遍历计算硬盘的大d值（时间复杂度高）
    //int update_big_d(int time, const Request r[])const {
    //    int ans = 0;
    //    while (request_pos[(ans + head_pos) % Len].first == 0 || get_position_scores((ans + head_pos) % Len, time, r) < RED_POSITION_THRE) {
    //        ans++;
    //        if (ans == Len) {
    //            break;
    //        }
    //    }
    //    return ans;
    //}
    //给出初始分区，每块盘按照预处理信息分为M个分区
    friend void pre_alloc(vector<DISK>& disks, const vector<int>& wr) {
        for (int i = 0;i < disks.size();++i) {
            //int begin = 0;
            for (int j = 0;j < disks[i].M;++j) {
                //partition p(disks[i], j + 1, begin, wr[j]);
                //disks[i].table.insert({ j + 1,p });
                //begin += wr[j];
                disks[i].creat_partition(wr[j], j + 1);
            }
        }
    }

    //向磁盘写入一个tag的数据，返回写入位置的索引（新逻辑）
    friend void writein(vector<DISK>& disks,int time,int obj_idx, int tag, int size, Object object[], const vector<int>& wr,const vector<vector<int>>& write_every1800, const vector<vector<int>>& delete_every1800) {
        vector<int> chosed_disks_idx = findBest3(disks, tag, size);
        for (int i = 0;i < REP_NUM;++i) {
            int disk_idx = chosed_disks_idx[i];
            disks[disk_idx].remain -= size; //注意维护remain值
            object[obj_idx].replica[i] = disk_idx;
            if (disks[disk_idx].table.count(tag) > 0) {
                auto range = disks[disk_idx].table.equal_range(tag);
                auto itFind = disks[disk_idx].table.end();//选中的写入分区的迭代器
                //选中写入分区的连续写入起始位置索引，如果选中写入分区无连续size空间则为-1
                int beg_itFind = -1;
                //bool has_written_successful = false;//是否成功向已有分区中写入
                for (auto it = range.first;it != range.second;it++) {
                    if (it->second.remain < size)
                        continue;//跳过不满足写入必要条件的分区
                    if (itFind == disks[disk_idx].table.end()) {
                        itFind = it;//首次初始化
                        beg_itFind = disks[disk_idx].enough_consecutive_space(size, itFind->second);
                        continue;
                    }
                    if (it->second.shared == nullptr && itFind->second.shared != nullptr) {
                        // 1.未共享的分区优先于已共享的分区
                        itFind = it;
                        beg_itFind = disks[disk_idx].enough_consecutive_space(size, itFind->second);
                        continue;
                    }
                    int beg_it = disks[disk_idx].enough_consecutive_space(size, it->second);
                    //beg_itFind = disks[disk_idx].enough_consecutive_space(size, itFind->second);
                    if (beg_it != -1 && beg_itFind == -1) {
                        // 2.有连续size个空闲空间的分区优先于无连续size个空闲空间的分区
                        itFind = it;          //更新所选分区
                        beg_itFind = beg_it;  //更新起始位置（debug）
                        continue;
                    }
                    if (itFind->second.shared == nullptr && beg_itFind != -1) {
                        //说明找到了合适的分区
                        break;
                    }
                }
                if (itFind != disks[disk_idx].table.end()) {
                    if (beg_itFind != -1) {
                        //可以连续写入
                        for (int b = 0;b < size;b++) {
                            disks[disk_idx].data[b + beg_itFind] = tag;
                            object[obj_idx].unit[i][b] = beg_itFind + b;
                            disks[disk_idx].save_pos[b + beg_itFind] = { obj_idx,b,i };
                        }
                        itFind->second.maintain_remain(-size);//注意维护分区的余量
                    }
                    else {
                        //拆散写入
                        int write_idx = itFind->second.begin;
                        for (int b = 0;b < size;++b) {
                            while (disks[disk_idx].data[write_idx] > 0) {
                                write_idx++;//write_idx是第一个空位置
                            }
                            disks[disk_idx].data[write_idx] = tag;
                            object[obj_idx].unit[i][b] = write_idx;
                            disks[disk_idx].save_pos[write_idx] = { obj_idx,b,i };
                        }
                        itFind->second.maintain_remain(-size);//维护分区的余量
                    }
                    continue;
                }
                //for (auto it = range.first;it != range.second;it++) {
                //    int begin_idx = disks[disk_idx].enough_consecutive_space(size, it->second);
                //    if (begin_idx != -1) {
                //        //如果找到连续size个空闲位置了，执行具体的写入操作
                //        for (int b = 0;b < size;b++) {
                //            disks[disk_idx].data[b + begin_idx] = tag;
                //            object[obj_idx].unit[i][b] = begin_idx + b;
                //        }
                //        //it->second.remain -= size;//注意维护分区的余量
                //        it->second.maintain_remain(-size);
                //        has_written_successful = true;
                //        break;
                //    }
                //    //分区找不到连续的空间，但如果剩余空间足够就把object拆开写入在这个分区中
                //    else if (it->second.remain >= size) {
                //        int write_idx = it->second.begin;
                //        for (int b = 0;b < size;++b) {
                //            while (disks[disk_idx].data[write_idx] > 0) {
                //                write_idx++;//write_idx是第一个空位置
                //            }
                //            disks[disk_idx].data[write_idx] = tag;
                //            object[obj_idx].unit[i][b] = write_idx;
                //        }
                //        //it->second.remain -= size;//注意维护分区的余量
                //        it->second.maintain_remain(-size);
                //        has_written_successful = true;
                //        break;
                //    }
                //}
                //if (has_written_successful) continue;
                
                //没能找到含有连续size个空闲位置的分区，尝试创立半分区
                int begin_idx = disks[disk_idx].creat_partition(wr[tag - 1] / 2, tag, size);//创建新半分区
                if (begin_idx != -1) {
                    for (int b = 0;b < size;b++) {
                        disks[disk_idx].data[b + begin_idx] = tag;
                        object[obj_idx].unit[i][b] = begin_idx + b;
                        disks[disk_idx].save_pos[b + begin_idx] = { obj_idx,b,i };
                    }
                    continue;
                }
            }
            else {
                int begin_idx = disks[disk_idx].creat_partition(wr[tag - 1], tag, size);//创建新分区
                if (begin_idx != -1) {
                    for (int b = 0;b <  size;b++) {
                        disks[disk_idx].data[b + begin_idx] = tag;
                        object[obj_idx].unit[i][b] = begin_idx + b;
                        disks[disk_idx].save_pos[b + begin_idx] = { obj_idx,b,i };
                    }
                    continue;
                }
                //没能找到足够的连续空间创建新分区，尝试创建半分区
                begin_idx = disks[disk_idx].creat_partition(wr[tag - 1] / 2, tag, size);
                if (begin_idx != -1) {
                    for (int b = 0;b < size;b++) {
                        disks[disk_idx].data[b + begin_idx] = tag;
                        object[obj_idx].unit[i][b] = begin_idx + b;
                        disks[disk_idx].save_pos[b + begin_idx] = { obj_idx,b,i };
                    }
                    continue;
                }
            }
            //创建小分区：
            int begin_idx2 = disks[disk_idx].creat_partition(SMALL_PARTITION_LENGTH, tag, size);
            if (begin_idx2 != -1) {
                for (int b = 0;b < size;b++) {
                    disks[disk_idx].data[b + begin_idx2] = tag;
                    object[obj_idx].unit[i][b] = begin_idx2 + b;
                    disks[disk_idx].save_pos[b + begin_idx2] = { obj_idx,b,i };
                }
                continue;
            }
            //创建小分区也失败了
            //遍历一遍table，能否找到一个分区右侧连续小分区长度的空间均为空闲
            bool find_a_part = false;//表示是否成功找到满足要求的分区
            //unordered_set<int> has_begin_idx;//分区起始位置哈希表，用于过滤重复遍历共享分区
            for (auto it = disks[disk_idx].table.begin();it != disks[disk_idx].table.end();it++) {
                //if (has_begin_idx.count(it->second.begin))
                //    continue;
                //has_begin_idx.insert(it->second.begin);
                if (it->second.space_too_much(disks[disk_idx], SMALL_PARTITION_LENGTH)) {
                    find_a_part = true;
                    //it->second表示右侧有连续SMALL_PARTITION_LENGTH个空闲位置的分区，这时候把这部分位置分割出it->second，分给本tag的小分区
                    int begin_idx = it->second.begin + it->second.length - SMALL_PARTITION_LENGTH;//新分区的起始位置
                    //手动创建新分区，省去data从-1变0再变-1的过程
                    partition p;
                    p.key = tag, p.begin = begin_idx, p.length = SMALL_PARTITION_LENGTH, p.remain = SMALL_PARTITION_LENGTH - size;
                    disks[disk_idx].table.insert({ tag,p });
                    for (int b = 0;b < size;b++) {
                        disks[disk_idx].data[b + begin_idx] = tag;
                        object[obj_idx].unit[i][b] = begin_idx + b;
                        disks[disk_idx].save_pos[b + begin_idx] = { obj_idx,b,i };
                    }
                    it->second.length -= SMALL_PARTITION_LENGTH;
                    //it->second.remain -= SMALL_PARTITION_LENGTH;
                    it->second.maintain_remain(-SMALL_PARTITION_LENGTH);
                    break;
                }
            }
            if (find_a_part)
                continue;
            // 尝试寻找共享分区
            int max_remain = LOWEST_LENGTH;//寻找最多的remain
            //共享的partition对象的迭代器
            unordered_multimap<int, DISK::partition>::iterator shared_partition_iterator = disks[disk_idx].table.end();
            for (auto it = disks[disk_idx].table.begin();it != disks[disk_idx].table.end();it++) {
                int replace = it->first;//要被共享的tag
                // 被共享的tag的写-删
                int x1 = write_every1800[replace - 1][time / FRE_PER_SLICING] -delete_every1800[replace - 1][time / FRE_PER_SLICING];
                // 发起共享的tag的写-删
                int x2 = write_every1800[tag - 1][time / FRE_PER_SLICING] - delete_every1800[tag - 1][time / FRE_PER_SLICING];
                if (it->second.shared == nullptr && x2 > x1) {
                    if (it->second.remain > max_remain) {
                        max_remain = it->second.remain;
                        shared_partition_iterator = it;
                    }
                }
            }
            //double max_spare_ratio = 0.0;//最大空闲比例
            //for (auto it = disks[disk_idx].table.begin();it != disks[disk_idx].table.end();it++) {
            //    if (it->second.remain < LOWEST_LENGTH || it->second.shared != nullptr)
            //        continue;//不满足必要条件
            //    int replace = it->first;//要被共享的tag
            //    // 被共享的tag的写-删
            //    int x1 = write_every1800[replace - 1][time / FRE_PER_SLICING] - delete_every1800[replace - 1][time / FRE_PER_SLICING];
            //    // 发起共享的tag的写-删
            //    int x2 = write_every1800[tag - 1][time / FRE_PER_SLICING] - delete_every1800[tag - 1][time / FRE_PER_SLICING];
            //    //当前分区的空闲比例
            //    double spare_ratio = (double)it->second.remain / (double)it->second.length;
            //    if (x2 > x1 && spare_ratio > max_spare_ratio) {
            //        max_spare_ratio = spare_ratio;
            //        shared_partition_iterator = it;
            //    }
            //}
            if (shared_partition_iterator != disks[disk_idx].table.end()) {
                //说明找到了符合条件的共享分区，两个partition对象互相指向对方
                partition p = shared_partition_iterator->second;//p是新分区的临时对象
                p.key = tag, p.shared = &shared_partition_iterator->second;
                //保存哈希表中新插入的partition对象的地址（原来的对象p即将被销毁，&p无效）
                auto p_iter_in_hash_map = disks[disk_idx].table.insert({ tag,p });
                shared_partition_iterator->second.shared = &p_iter_in_hash_map->second;
                //进行写入操作
                int begin_idx = disks[disk_idx].enough_consecutive_space(size, shared_partition_iterator->second);
                if (begin_idx != -1) {
                    for (int b = 0;b < size;b++) {
                        disks[disk_idx].data[b + begin_idx] = tag;
                        object[obj_idx].unit[i][b] = begin_idx + b;
                        disks[disk_idx].save_pos[b + begin_idx] = { obj_idx,b,i };
                    }
                    shared_partition_iterator->second.maintain_remain(-size);
                }
                else {
                    //可能产生bug remain是否一定大于size？
                    int write_idx = shared_partition_iterator->second.begin;
                    for (int b = 0;b < size;++b) {
                        while (disks[disk_idx].data[write_idx] > 0) {
                            write_idx++;//write_idx是第一个空位置
                        }
                        disks[disk_idx].data[write_idx] = tag;
                        object[obj_idx].unit[i][b] = write_idx;
                        disks[disk_idx].save_pos[write_idx] = { obj_idx,b,i };
                    }
                    shared_partition_iterator->second.maintain_remain(-size);
                }
                continue;
            }
            int begin_idx3 = disks[disk_idx].find_consecutive_from_end(size);
            if (begin_idx3 != -1) {
                //在硬盘中找到连续的size个空闲空间
                for (int b = 0;b < size;b++) {
                    disks[disk_idx].data[b + begin_idx3] = tag;
                    object[obj_idx].unit[i][b] = begin_idx3 + b;
                    disks[disk_idx].save_pos[b + begin_idx3] = { obj_idx,b,i };
                    for (auto it = disks[disk_idx].table.begin();it != disks[disk_idx].table.end();it++) {
                        if (b + begin_idx3 >= it->second.begin && b + begin_idx3 < it->second.begin + it->second.length) {
                            //如果当前位置在某个分区里，该分区余量-1
                            //it->second.remain--;
                            it->second.maintain_remain(-1);
                            break;
                        }
                    }
                }
            }
            else {
                //退化为最原始的暴力写入
                disks[disk_idx].remain += size;//因为在brute_write和本函数for循环开头中对remain重复减size，因此需要抵消一个size
                for (int b = 0;b < size;b++) {
                    int brute_write_idx = disks[disk_idx].brute_write(tag);
                    object[obj_idx].unit[i][b] = brute_write_idx;
                    disks[disk_idx].save_pos[brute_write_idx] = { obj_idx,b,i };
                }
            }
        }
    }
    
    //删除该硬盘索引idx的数据
    void delete_act(int idx, int time,const Request r[]) {
        if (d[0] == distance(idx,0)) {
            //被删除的是当前最近的有请求的位置，则更新d值，否则d值保持不变
            d[0] = update_d(time, r, 0);
        }
        if (d[1] == distance(idx, 1))
            d[1] = update_d(time, r, 1);
        for (auto it = table.begin();it != table.end();it++) {
            if (it->second.begin <= idx && idx < it->second.begin + it->second.length) {
                //it->second此时就代表包含idx的分盘partition，该位置先写为-1
                data[idx] = -1;
                remain++;
                //it->second.remain++;
                it->second.maintain_remain(1);
                if (it->second.empty()) {
                    //如果该分区已经为空（当前数据集没遇到过，所以这里没有更新共享分区的信息！）
                    for (int i = it->second.begin;i < it->second.begin + it->second.length;i++) {
                        data[i] = 0;//删除该分区，所有位置写为0
                    }
                    table.erase(it);//删除哈希表中的此分区
                }
                return;
            }
        }
        //没有找到idx匹配的分区，说明该idx不在分区里，直接写0
        data[idx] = 0;
        remain++;
        save_pos[idx] = { 0,0,0 };
    }

    /*
    * @brief                判断连读空位置到下一个有读请求的位置是否划算
    * @param  head_pos      磁头位置索引（从0开始）
    * @param  consecutive   起始时的连读次数
    * @param  tok           起始时的剩余tooken
    * @return               布尔值，如果连读划算就返回true；如果pass划算就返回false
    */
    bool do_we_read_null(int head_pos, int consecutive, int tok)const {
        int dx = 0;
        while (request_id[(dx + head_pos) % Len].empty()) {
            dx++;
            if (dx == Len)
                return false;
        }
        if (dx == 0)
            return true;
        if (dx < 4 && consecutive >= READ_NULL_THRESHOLD[dx - 1])
            return true;

        //先假设执行连续d次读取
        int head_pos_next = (head_pos + dx) % Len;//磁头的新位置
        int consecutive_a = consecutive + dx;//连读空以后的连读次数
        int tooken_a = tok - read_cost(consecutive, dx);//连读到下一个有读请求位置剩余tooken
        //read_nums_a表示本次执行连读，后续能读到的最大个数
        int read_nums_a = read_successufl_nums(head_pos_next, consecutive_a, tooken_a);
        //再假设执行pass
        int tooken_b = tok - dx;
        //read_nums_b表示本次执行pass，后续能读到的最大个数
        int read_nums_b = read_successufl_nums(head_pos_next, 0, tooken_b);
        return read_nums_a >= read_nums_b;
    }
    /*
    * @brief                判断当前磁头状态向后读取的最多个数
    * @param  head_pos      磁头位置索引（从0开始）
    * @param  consecutive   起始时的连读次数
    * @param  tok           起始时的剩余tooken
    * @return               int值，表示磁头向后理论能读取的有效最多个数
    */
    int read_successufl_nums(int head_pos, int consecutive, int tok)const {
        int read_nums = 0;//读取成功的量，初始化为0
        while (true) {
            int dx = 0;
            while (request_id[(dx + head_pos) % Len].empty()) {
                dx++;
                if (dx == Len)
                    return read_nums;
            }
            if (dx == 0) {
                //执行一次读
                if (tok >= read_cost(consecutive, 1)) {
                    tok -= read_cost(consecutive, 1);
                    consecutive++;
                    head_pos = (head_pos + 1) % Len;
                    read_nums++;
                }
                //读失败，返回当前以经读出的个数
                else {
                    return read_nums;
                }
            }
            else if (dx > tok) {
                return read_nums;
            }
            else {
                bool implement_read_null = do_we_read_null(head_pos, consecutive, tok);
                if (implement_read_null) {
                    //执行d次连续空读
                    for (int i = 0;i < dx;++i) {
                        if (tok >= read_cost(consecutive, 1)) {
                            head_pos = (head_pos + 1) % Len;
                            tok -= read_cost(consecutive, 1);
                            consecutive += 1;
                            read_nums++;
                        }
                        else
                            return read_nums;
                    }
                }
                else {
                    //执行d次pass操作
                    if (tok < dx) {
                        return read_nums;
                    }
                    tok -= dx;
                    head_pos = (head_pos + dx) % Len;
                    consecutive = 0;
                }
            }
        }
    }

    /*
    * @brief                判断当前磁头状态向后读取后，耗尽tooken时走过的距离
    * @param  head_pos      磁头位置索引（从0开始）
    * @param  consecutive   起始时的连读次数
    * @return               int值，表示磁头向后理论读取完毕后移动的距离
    */
    int read_farest_distance(int head_pos, int consecutive)const {
        int origenal_head_pos = head_pos;//初始磁头位置
        int tok = G;//剩余tooken初始化为G
        //计算距离的lambda函数
        auto cal_dis = [](int a, int b,int len)->int {
            if (b >= a)
                return b - a;
            else
                return len - a + b;
        };
        while (true) {
            int dx = 0;
            while (request_id[(dx + head_pos) % Len].empty()) {
                dx++;
                if (dx == Len)
                    //当前磁头所在硬盘没有读请求，返回
                    return cal_dis(origenal_head_pos,head_pos,Len);
            }
            if (dx == 0) {
                //执行一次读
                if (tok >= read_cost(consecutive, 1)) {
                    tok -= read_cost(consecutive, 1);
                    consecutive++;
                    head_pos = (head_pos + 1) % Len;
                }
                //读失败，返回
                else {
                    return cal_dis(origenal_head_pos, head_pos, Len);
                }
            }
            else if (dx > tok) {
                return cal_dis(origenal_head_pos, head_pos, Len);
            }
            else {
                bool implement_read_null = do_we_read_null(head_pos, consecutive, tok);
                if (implement_read_null) {
                    //执行d次连续空读
                    for (int i = 0;i < dx;++i) {
                        if (tok >= read_cost(consecutive, 1)) {
                            head_pos = (head_pos + 1) % Len;
                            tok -= read_cost(consecutive, 1);
                            consecutive += 1;
                        }
                        else
                            return cal_dis(origenal_head_pos, head_pos, Len);
                    }
                }
                else {
                    //执行d次pass操作
                    if (tok < dx) {
                        return cal_dis(origenal_head_pos, head_pos, Len);
                    }
                    tok -= dx;
                    head_pos = (head_pos + dx) % Len;
                    consecutive = 0;
                }
            }
        }
    }
    //判断两个磁头是否有冲突
    bool check_conflict(int time,const Request r[]) {
        int front_head_id;//前方的磁头序号
        int back_head_id;//后方的磁头序号
        if (head_pos[0] <= head_pos[1]) {
            if (head_pos[1] - head_pos[0] >= Len / 2)
                front_head_id = 1, back_head_id = 0;
            else
                front_head_id = 0, back_head_id = 1;
        }
        else {
            if (head_pos[0] - head_pos[1] >= Len / 2)
                front_head_id = 0, back_head_id = 1;
            else
                front_head_id = 1, back_head_id = 0;
        }
        //后方磁头与前方磁头位置的距离
        int distance_two_head = distance(head_pos[front_head_id], back_head_id);
        int back_farest = read_farest_distance(head_pos[back_head_id], consecutive_read[back_head_id]);
        if (back_farest > distance_two_head) {
            int front_farest = read_farest_distance(head_pos[front_head_id], consecutive_read[front_head_id]);
            auto itFind = table.end();
            //int max_scores = read_most(time, head_pos[back_head_id], CHECKED_TIME + 1, back_head_id, r);
            int max_scores = 0;
            for (auto it = table.begin();it != table.end();it++) {
                int target = it->second.begin;
                if (distance(target, front_farest) <= front_farest) {
                    continue;//不能跳到前磁头的前边导致仍然冲突
                }
                int read_scores = read_most(time, target, CHECKED_TIME, back_head_id, r);
                if (read_scores > max_scores) {
                    itFind = it;
                    max_scores = read_scores;
                }
            }
            if (itFind != table.end()) {
                jump(itFind->second.begin, time, r, back_head_id);
                time_flag[back_head_id] = false;
                return true;
            }
            return false;
        }
        else
            return false;
    }

    //尝试在当前磁头i的位置进行读操作，返回值代表实际是否读取成功，如果tooken不足什么都不做并返回false
    bool read_act(int time,const Request r[],int i) {
        //计算读取需要消耗的tooken
        int cost = 0;
        if (consecutive_read[i] < 7)
            cost = READ_COST[consecutive_read[i]];
        else
            cost = READ_COST[7];
        if (cost <= tooken_[i]) {
            //可以读取
            head_pos[i] = (head_pos[i] + 1) % Len;//移动磁头到下一个位置
            //更新d值
            if (d[i] > 0)
                d[i]--;
            else {
                d[i] = update_d(time, r, i);
                if (d[i] == Len - 1)
                    d[i] = Len;//因为这里没有更新req_pos，且磁头向后移1，所以需要矫正这个误差
            }
                
            //需要更新d[1-i]值：
            if (d[1 - i] == distance((head_pos[i] - 1) % Len, 1 - i)) {
                if (d[i] < Len)
                    d[1 - i] = d[i] + distance(head_pos[i], 1 - i);
                else
                    d[1 - i] = Len;
            }
            consecutive_read[i]++;//更新参数
            tooken_[i] -= cost;//更新剩余tooken值
            c_str[i].back() = 'r', c_str[i].push_back('#');
            return true;
        }
        else
            return false;//由于tooken不足，读取失败
    }

    /*
    * @brief         尝试进行n次pass操作将磁头移动到target位置，返回是否移动成功。
    * @param target  要pass到的目标存储单元索引（从0开始）
    * @param time    当前时间片序号
    * @param r       request[]数组
    * @param head_id 操作的磁头索引（0或1）
    */
    bool pass_head(int target, int time,const Request r[],int head_id) {
        //如果目标位置就是磁头本身，什么都不做直接返回true（这种情况应当避免）
        if (target == head_pos[head_id])
            return true;
        int dx = distance(target, head_id);//磁头与目标位置的距离
        //如果目标距离大于G，什么都不做直接返回false（这种情况应当jump）
        if (dx > G)
            return false;
        if (dx == d[head_id]) {
            bool a = read_cost(consecutive_read[head_id], dx + 1) <= tooken_[head_id];
            bool b = dx + READ_COST[0] <= tooken_[head_id];
            if (a && b) {
                //判定连续读取空是否划算
                if (do_we_read_null(head_pos[head_id], consecutive_read[head_id], tooken_[head_id])) {
                    for (int i = 0;i < dx;++i) {
                        read_act(time, r, head_id);
                    }
                }
                else {
                    head_pos[head_id] = target;
                    tooken_[head_id] -= dx;
                    consecutive_read[head_id] = 0;
                    d[head_id] = 0;
                    c_str[head_id].pop_back();//去掉尾部的井号
                    for (int i = 0;i < dx;++i) {
                        c_str[head_id].push_back('p');//加上dx个p
                    }
                    c_str[head_id].push_back('#');//尾部再加上井号
                }
                return true;
            }
            else if (a && !b) {
                for (int i = 0;i < dx;++i) {
                    read_act(time, r, head_id);
                }
                return true;
            }
            else {
                //距离 < [连读次数]时，向后连读，否则耗尽tooken向后pass
                const static int TIME_END_MOVE_THRE[8] = { 0,4,6,9,10,11,11,11 };
                int conse = consecutive_read[head_id] < 7 ? consecutive_read[head_id] : 7;
                if (dx < TIME_END_MOVE_THRE[conse]) {
                    for (int i = 0;i < dx;++i) {
                        if (!read_act(time, r, head_id)) {
                            time_flag[head_id] = false;
                            return false;
                        }
                    }
                    return true;
                }
                else {
                    int pass_nums = std::min(tooken_[head_id], dx);//pass的次数
                    if (pass_nums == 0)
                        return false;
                    for (int i = 0;i < dx;++i) {
                        if (tooken_[head_id] == 0) {
                            time_flag[head_id] = false;
                            return false;
                        }
                        head_pos[head_id] = (head_pos[head_id] + 1) % Len;
                        tooken_[head_id] -= 1;
                        consecutive_read[head_id] = 0;
                        d[head_id] -= 1;
                        c_str[head_id].back() = 'p';
                        c_str[head_id].push_back('#');
                    }
                    return true;
                }
            }
        }
        else {
            // 要pass的目标位置不属于最近的读请求，不考虑连读空策略，直接逐个pass过去
            if (tooken_[head_id] < dx) {
                return false;
            }
            else {
                tooken_[head_id] -= dx;
                head_pos[head_id] = target;
                consecutive_read[head_id] = 0;
                d[head_id] = update_d(time, r, head_id);
                c_str[head_id].pop_back();
                for (int i = 0;i < dx;++i) {
                    c_str[head_id].push_back('p');
                }
                c_str[head_id].push_back('#');
                return true;
            }
        }
        return true;
    }
    //尝试一次jump到目标位置，返回是否成功，如果tooken不足就什么都不做
    bool jump(int target,int time,const Request r[],int i) {
        if (tooken_[i] == G) {
            head_pos[i] = target;
            d[i] = update_d(time, r, i);
            consecutive_read[i] = 0;
            tooken_[i] = 0;
            //注意输出的时候，索引是从1开始，所以需要+1
            c_str[i] = "j " + std::to_string(target + 1);
            return true;
        }
        else
            return false;
    }

    /*@brief 让当前硬盘尽可能往后读n个时间片，返回其得分scores（本函数中单独计算，优化时间复杂度）
    * @param time    当前时间片序列
    * @param head    磁头开始位置（若与当前磁头位置不相等则不考虑连读）
    * @param n       磁头读取的时间片个数
    * @param disk_id 操作的磁头编号（0或1）
    * @param r       请求的request数组
    * @return        n个时间片该磁头的得分
    */
    int read_most(int time, int head, int n, int disk_id, const Request r[]) const {
        /*vtooken：剩余tooken数
        vconsecutive_read：连读次数
        v_head：磁头位置
        next_pos：下一个有得分的目标存储单元索引
        scores：累计得分*/
        int vtooken = tooken_[disk_id], vconsecutive_read = 0, vhead = head, next_pos = head, scores = 0;
        if (head == head_pos[0]) {
            vconsecutive_read = consecutive_read[0];
        }
        else if (head == head_pos[1]) {
            vconsecutive_read = consecutive_read[1];
        }
        int current_time = time;//当前时间片序号
        while (current_time < time + n) {
            // 计算下一个next_pos的位置
            int vd = 0;//d值
            next_pos = vhead;
            while (request_id[next_pos].empty() || get_position_scores(next_pos, current_time, r) == 0) {
                next_pos = (next_pos + 1) % Len;
                vd++;
                if (vd == Len)
                    break;//vd == Len 表示遍历了整个硬盘的存储单元没有找到有得分的位置
            }
            //当前位置有得分请求，尝试读取
            if (vd == 0) {
                //读取所需花费tooken数
                int cost = vconsecutive_read < 7 ? READ_COST[vconsecutive_read] : READ_COST[7];
                if (cost <= vtooken) {
                    vtooken -= cost;
                    vconsecutive_read++;
                    vhead = (vhead + 1) % Len;
                    scores += get_position_scores(vhead, time, r);
                }
                else {
                    //读取失败，进入下一个时间片
                    vtooken = G;
                    current_time++;
                }
            }
            else if (vd == Len) {
                //硬盘上没有读请求了，直接返回累计得分
                return scores;
            }
            else {
                //尝试连读
                if (do_we_read_null(vhead, vconsecutive_read, vtooken)) {
                    while (vhead != next_pos) {
                        int cost = vconsecutive_read < 7 ? READ_COST[vconsecutive_read] : READ_COST[7];
                        //尝试一次空读
                        if (vtooken >= cost) {
                            vtooken -= cost;
                            vconsecutive_read++;
                            vhead = (vhead + 1) % Len;
                        }
                        else {
                            break;
                        }
                    }
                    if (vhead != next_pos) {
                        vtooken = G;
                        current_time++;
                    }
                    continue;
                }
                else {
                    if (vtooken >= vd) {
                        vtooken -= vd;//每次pass消耗1tooken
                        vconsecutive_read = 0;//连读清0
                        vhead = (vhead + vd) % Len;
                    }
                    else {
                        vtooken = G;
                        current_time++;
                    }
                }
            }
        }
        return scores;
    }
    //交换两个存储单元的数据
    void swap_two_pos(int idx0, int idx1,Object obj[]) {
        request_id[idx0].swap(request_id[idx1]);
        //两个d值需要更新
        d[0] = update_d(0), d[1] = update_d(1);
        //两个位置都有数据，注意和data以及save_pos的交换顺序，先交换object[].unit[][]
        if (data[idx0] > 0 && data[idx1] > 0) {
            // 交换obj[x0].unit[a0][b0]和obj[x1].unit[a1][b1]，这些参数怎么求？
            int x0 = save_pos[idx0].obj_id, x1 = save_pos[idx1].obj_id;
            int a0 = save_pos[idx0].replica, a1 = save_pos[idx1].replica;
            int b0 = save_pos[idx0].block_id, b1 = save_pos[idx1].block_id;
            std::swap(obj[x0].unit[a0][b0], obj[x1].unit[a1][b1]);
        }
        else if (data[idx0] > 0 && data[idx1] <= 0) {
            int x0 = save_pos[idx0].obj_id;
            int a0 = save_pos[idx0].replica;
            int b0 = save_pos[idx0].block_id;
            obj[x0].unit[a0][b0] = idx1;
        }
        else if (data[idx0] <= 0 && data[idx1] > 0) {
            int x1 = save_pos[idx1].obj_id;
            int a1 = save_pos[idx1].replica;
            int b1 = save_pos[idx1].block_id;
            obj[x1].unit[a1][b1] = idx0;
        }
        std::swap(data[idx0], data[idx1]);//这里只考虑分区内交换，不涉及0和-1的区别
        std::swap(save_pos[idx0], save_pos[idx1]);
    }
    //执行最多K次交换操作
    vector<std::pair<int, int>> swap_K(int k,Object obj[]) {
        double min_sita = 0.3;
        vector<std::pair<int, int>> ans;
        vector<std::pair<partition*, double>> part;
        for (auto it = table.begin();it != table.end();it++) {
            double sita = it->second.cal_sita(*this, it->second.begin, it->second.length);
            part.push_back({ &it->second, sita });
        }
        std::sort(part.begin(), part.end(), [&](auto a, auto b) {return a.second < b.second;});
        for (std::pair<partition*, double> itFind : part) {
            if (itFind.second >= 0.3)
                break;
            int tag1 = itFind.first->key;
            int tag2;
            if (itFind.first->shared == nullptr)
                tag2 = -1;
            else
                tag2 = itFind.first->shared->key;
            int begin = itFind.first->begin, length = itFind.first->length;

            //000
            int left = begin, right = begin + length - 1;   //双指针
            int flag1 = 0, flag2 = 0;        //分别标记左右指针是否到了中间，如果两个都为1，那么一次交换结束，亦或是有一个触底两次
            int fro, bac;                    //记录统计而得的前后部分结果
            int num_1 = 0, num_2 = 0;        //记录数量
            int cnt_1 = 0, cnt_2 = 0;        //记录序号和
            int y1 = 0, y2 = 0;              //用于标记是否找到
            int cnt = 0;                     //记录已经使用的交换次数
            int f_num, b_num;
            //vector<std::pair<int, int>> ans;

            //统计，用于判断谁在前半部分，谁在后半部分
            for (int i = left; i <= right; i++) {
                if (data[i] == tag1) {
                    num_1 += 1;
                    cnt_1 += i + 1;
                }
                else if (data[i] == tag2) {
                    num_2 += 1;
                    cnt_2 += i + 1;
                }
            }

            if ((double)cnt_1 / num_1 > (double)cnt_2 / num_2) {
                fro = tag2, bac = tag1;
                f_num = num_2;
                b_num = num_1;
            }
            else {
                fro = tag1, bac = tag2;
                f_num = num_1;
                b_num = num_2;
            }

            //结束条件有两个，1、flag判断  2、根据K值判断
            //其实来说，当有一个
            while ((flag1 == 0 || flag2 == 0) && cnt < k) {
                //左边先找符合条件的
                if (flag1 == 0) {                //第一次循环，先找是后边值的
                    y1 = 0;
                    for (int f = left; f < begin + f_num; f++) {
                        if (data[f] == bac) {
                            left = f;
                            y1 = 1;
                            break;                     //找到符合条件的就退出
                        }
                    }
                    if (y1 == 0) {
                        flag1 += 1;                        //没找到合适的，就说明触底了，那么就触底次数加一
                        left = 0;                          //左边指针返回0
                    }
                }
                if (flag1 == 1) {             //第二次循环，找空的
                    y1 = 0;
                    for (int f = left; f < begin + f_num; f++) {
                        if (data[f] <= 0) {
                            left = f;
                            y1 = 1;
                            break;                     //找到符合条件的就退出
                        }
                    }
                    if (y1 == 0) {
                        flag1 += 1;                       //没找到合适的，就说明触底了，那么就触底次数加一
                        break;                            //第二次触底，直接退出循环
                    }
                }

                //找右边的
                if (flag2 == 0) {                //第一次循环，先找是后边值的
                    y2 = 0;
                    for (int b = right; b > b_num + begin; b--) {
                        if (data[b] == fro) {
                            right = b;
                            y2 = 1;
                            break;               //找到符合条件的就退出
                        }
                    }
                    if (y2 == 0) {
                        flag2 += 1;                  //没找到合适的，就说明触底了，那么就触底次数加一
                        right = 0;                    //左边指针返回0
                    }
                }
                if (flag2 == 1) {
                    y2 = 0;
                    for (int b = right; b > b_num + begin; b--) {
                        if (data[b] <= 0) {       //第二次循环，找空的
                            right = b;
                            y2 = 1;
                            break;               //找到符合条件的就退出
                        }
                    }
                    if (y2 == 0) {
                        flag2 += 1;                  //没找到合适的，就说明触底了，那么就触底次数加一
                        break;                       //第二次触底，直接退出循环
                    }
                }

                //1、左右都找到空的，那就直接退出
                if (flag1 == 1 && flag2 == 1) {
                    break;
                }
                //2、左右成功找到，交换
                else {
                    swap_two_pos(left, right, obj);
                    ans.push_back({ left,right });
                    cnt += 1;
                }
            }
            k -= cnt;
            //if (k == 0)
                break;
        }
        return ans;
    }

    //将一个Request压入DISK中，更新存储该读请求的请求对象object的三个硬盘的参数request_pos、request_id、d
    friend void push_Request_in(vector<DISK>& disks,const Request& r,const Object obj[MAX_OBJECT_NUM],int r_idx) {
        DISK* d0 = &disks[obj[r.object_id].replica[0]], * d1 = &disks[obj[r.object_id].replica[1]], * d2 = &disks[obj[r.object_id].replica[2]];
        for (int i = 0;i < r.object_size;i++) {
            d0->request_id[obj[r.object_id].unit[0][i]].push_back(r_idx);
            d1->request_id[obj[r.object_id].unit[1][i]].push_back(r_idx);
            d2->request_id[obj[r.object_id].unit[2][i]].push_back(r_idx);
            //如果新请求的位置小于d值，就更新d值
            if (d0->distance(obj[r.object_id].unit[0][i], 0) < d0->d[0])
                d0->d[0] = d0->distance(obj[r.object_id].unit[0][i], 0);
            if (d0->distance(obj[r.object_id].unit[0][i], 1) < d0->d[1])
                d0->d[1] = d0->distance(obj[r.object_id].unit[0][i], 1);
            if (d1->distance(obj[r.object_id].unit[1][i], 0) < d1->d[0])
                d1->d[0] = d1->distance(obj[r.object_id].unit[1][i], 0);
            if (d1->distance(obj[r.object_id].unit[1][i], 1) < d1->d[1])
                d1->d[1] = d1->distance(obj[r.object_id].unit[1][i], 1);
            if (d2->distance(obj[r.object_id].unit[2][i], 0) < d2->d[0])
                d2->d[0] = d2->distance(obj[r.object_id].unit[2][i], 0);
            if (d2->distance(obj[r.object_id].unit[2][i], 1) < d2->d[1])
                d2->d[1] = d2->distance(obj[r.object_id].unit[2][i], 1);
        }
    }
    //在读取成功后，将读取位置对应的所有请求的对应block处的request_pos[target]和request_id重新设置为初始值
    friend void pop_Request_out(vector<DISK>& disks, DISK* chosed_disk, const Object obj[],int time,const Request req[],int tar) {
        int r_idx = chosed_disk->request_id[tar][0];
        int block_idx = chosed_disk->save_pos[tar].block_id;
        DISK* d0 = &disks[obj[req[r_idx].object_id].replica[0]], * d1 = &disks[obj[req[r_idx].object_id].replica[1]], * d2 = &disks[obj[req[r_idx].object_id].replica[2]];
        //指定block_idx，仅清除object的指定block
        d0->request_id[obj[req[r_idx].object_id].unit[0][block_idx]].clear();
        d1->request_id[obj[req[r_idx].object_id].unit[1][block_idx]].clear();
        d2->request_id[obj[req[r_idx].object_id].unit[2][block_idx]].clear();
        //如果pop的位置恰好为最近处，需要更新d值
        if (d0->distance(obj[req[r_idx].object_id].unit[0][block_idx], 0) == d0->d[0])
            d0->d[0] = d0->update_d(time, req, 0);
        if (d0->distance(obj[req[r_idx].object_id].unit[0][block_idx], 1) == d0->d[1])
            d0->d[1] = d0->update_d(time, req, 1);
        if (d1->distance(obj[req[r_idx].object_id].unit[1][block_idx], 0) == d1->d[0])
            d1->d[0] = d1->update_d(time, req, 0);
        if (d1->distance(obj[req[r_idx].object_id].unit[1][block_idx], 1) == d1->d[1])
            d1->d[1] = d1->update_d(time, req, 1);
        if (d2->distance(obj[req[r_idx].object_id].unit[2][block_idx], 0) == d2->d[0])
            d2->d[0] = d2->update_d(time, req, 0);
        if (d2->distance(obj[req[r_idx].object_id].unit[2][block_idx], 1) == d2->d[1])
            d2->d[1] = d2->update_d(time, req, 1);
    }
    //一个临时定义结构体，用作返回
    struct choose {
        DISK* disk;//DISK对象的地址
        int head_id;//选择的磁头序号（0或1）
    };
    //返回DISK数组time_flag为true的元素中d值最小的DISK元素的地址，如果time_flag全为false则返回一个空指针
    friend choose get_nearest_disk(vector<DISK>& disks,int time,Request r[]) {
        // 用于记录找到的 DISK 对象指针
        DISK* nearest = nullptr;
        int head_id = -1;
        // 用一个大值做初始比较值
        int min_d_value = INT_MAX;
        // 遍历所有DISK，找到 time_flag 为 true 且 update_d() 返回值最小者
        for (int i = 0; i < disks.size(); ++i) {
            if (disks[i].time_flag[0] && disks[i].time_flag[1]) {
                int cur_d = std::min(disks[i].d[0],disks[i].d[1]);
                if (cur_d == disks[0].Len)
                    continue;
                if (cur_d < min_d_value) {
                    min_d_value = cur_d;
                    nearest = &disks[i];
                    head_id = disks[i].d[0] <= disks[i].d[1] ? 0 : 1;
                }
            }
            else if (disks[i].time_flag[0] && !disks[i].time_flag[1]) {
                int cur_d = disks[i].d[0];
                if (cur_d == disks[0].Len)
                    continue;
                if (cur_d < min_d_value) {
                    min_d_value = cur_d;
                    nearest = &disks[i];
                    head_id = 0;
                }
            }
            else if (!disks[i].time_flag[0] && disks[i].time_flag[1]) {
                int cur_d = disks[i].d[1];
                if (cur_d == disks[0].Len)
                    continue;
                if (cur_d < min_d_value) {
                    min_d_value = cur_d;
                    nearest = &disks[i];
                    head_id = 1;
                }
            }
        }
        // 如果全部 time_flag 为 false，则 nearest 保持 nullptr
        return { nearest ,head_id };
    }

    //返回一个std::pair<int,int>，如果当前硬盘第idx个存储单元没有存储信息，默认返回{0,0}；如果存储了信息，返回的第一个元素表示该硬盘第idx个存储单元存储的object对象索引，第二个数表示该object对象在这个位置存储的block索引
    std::pair<int, int> get_save_pos(int idx)const { 
        return { save_pos[idx].obj_id, save_pos[idx].block_id }; 
    }
    const vector<UNIT>& get_save_pos()const { return save_pos; }
    //返回数组表示第idx个存储单元现在存在的读请求编号
    const vector<int>& get_request_id(int idx)const { return request_id[idx]; }
    const vector<vector<int>>& get_request_id()const { return request_id; }

    //当某个request需要被清除时（某个时间片该元素被删除但仍存在未完成的相关读请求，或某个时间片某读请求上报繁忙）调用，更新DISK的成员变量request_id[target]，删除其中值为r_idx的元素，删除后如果request_id[target]为空还需要更新d值
    friend void clear_request_id(vector<DISK>& d, const Object obj[MAX_OBJECT_NUM], int time,const Request req[],int r_idx) {
        DISK* d0 = &d[obj[req[r_idx].object_id].replica[0]], * d1 = &d[obj[req[r_idx].object_id].replica[1]], * d2 = &d[obj[req[r_idx].object_id].replica[2]];
        //如果没有指定block索引，清除该请求对应的object的所有block位置的req_id的指定元素
        for (int i = 0;i < req[r_idx].object_size;i++) {
            //更新DISK的成员变量request_id[target]，删除其中值为r_idx的元素
            d0->request_id[obj[req[r_idx].object_id].unit[0][i]].erase(
                std::remove(d0->request_id[obj[req[r_idx].object_id].unit[0][i]].begin(), d0->request_id[obj[req[r_idx].object_id].unit[0][i]].end(), r_idx)
                , d0->request_id[obj[req[r_idx].object_id].unit[0][i]].end());
            d1->request_id[obj[req[r_idx].object_id].unit[1][i]].erase(
                std::remove(d1->request_id[obj[req[r_idx].object_id].unit[1][i]].begin(), d1->request_id[obj[req[r_idx].object_id].unit[1][i]].end(), r_idx)
                , d1->request_id[obj[req[r_idx].object_id].unit[1][i]].end());
            d2->request_id[obj[req[r_idx].object_id].unit[2][i]].erase(
                std::remove(d2->request_id[obj[req[r_idx].object_id].unit[2][i]].begin(), d2->request_id[obj[req[r_idx].object_id].unit[2][i]].end(), r_idx)
                , d2->request_id[obj[req[r_idx].object_id].unit[2][i]].end());
            if (d0->request_id[obj[req[r_idx].object_id].unit[0][i]].empty()) {
                //如果删除以后这里已经为空，需要进一步更新d值，对三个副本对应的硬盘都是如此
                if (d0->distance(obj[req[r_idx].object_id].unit[0][i], 0) == d0->d[0])
                    d0->d[0] = d0->update_d(time, req, 0);
                if (d0->distance(obj[req[r_idx].object_id].unit[0][i], 1) == d0->d[1])
                    d0->d[1] = d0->update_d(time, req, 1);
            }
            if (d1->request_id[obj[req[r_idx].object_id].unit[1][i]].empty()) {
                if (d1->distance(obj[req[r_idx].object_id].unit[1][i], 0) == d1->d[0])
                    d1->d[0] = d1->update_d(time, req, 0);
                if (d1->distance(obj[req[r_idx].object_id].unit[1][i], 1) == d1->d[1])
                    d1->d[1] = d1->update_d(time, req, 1);
            }
            if (d2->request_id[obj[req[r_idx].object_id].unit[2][i]].empty()) {
                if (d2->distance(obj[req[r_idx].object_id].unit[2][i], 0) == d2->d[0])
                    d2->d[0] = d2->update_d(time, req, 0);
                if (d2->distance(obj[req[r_idx].object_id].unit[2][i], 1) == d2->d[1])
                    d2->d[1] = d2->update_d(time, req, 1);
            }  
        }
    }

    //返回若干硬盘中，最佳的3个写入的位置，返回的是索引
    friend vector<int> findBest3(const vector<DISK>& disks,int tag,int size) {
        vector<int> coefficient(disks.size(),0);//权重
        // coefficient[i]表示第i个硬盘上的选择权重。权重最大的3个硬盘将入选。
        for (int i = 0;i < disks.size();i++) {
            if (disks[i].table.count(tag)) {
                auto range = disks[i].table.equal_range(tag);
                for (auto it = range.first;it != range.second;it++) {
                    coefficient[i] += it->second.remain;
                }
                if (coefficient[i] >= size) {
                    coefficient[i] += disks[i].Len * 2;
                    continue;
                }
            }
            bool max_conse_0 = max_consecutive_zero(disks[i].data, SMALL_PARTITION_LENGTH);
            if (max_conse_0) {
                coefficient[i] = disks[i].remain + disks[i].Len;
            }
            else {
                coefficient[i] = disks[i].remain;
            }
        }
        // 分别存储前三大数的值和索引
        int max1 = INT_MIN, max2 = INT_MIN, max3 = INT_MIN;
        int idx1 = -1, idx2 = -1, idx3 = -1;
        for (int i = 0; i < (int)disks.size(); ++i) {
            int val = coefficient[i];
            // 如果当前值比第一大值还大，就依次“降级”之前的记录
            if (val > max1) {
                max3 = max2;    idx3 = idx2;
                max2 = max1;    idx2 = idx1;
                max1 = val;     idx1 = i;
            }
            // 否则如果当前值比第二大值还大
            else if (val > max2) {
                max3 = max2;    idx3 = idx2;
                max2 = val;     idx2 = i;
            }
            // 否则如果当前值比第三大值还大
            else if (val > max3) {
                max3 = val;     idx3 = i;
            }
        }
        return { idx1,idx2,idx3 };
    }

    friend void check_at_hot_tag(
        std::vector<DISK>& disks,
        int time,
        const Request r[],
        const std::vector<int>& req_tag
    )
    {
        // ==============================================
        // 1) 找到 sum(req_tag) 的 70% 阈值
        //    在本例中要选出若干“热标签”，使它们的总请求量 >= 70% * sum_all
        // ==============================================
        long long sumAll = 0;
        for (auto x : req_tag) sumAll += x;
        double threshold = HOT_PERCENT * sumAll;  // 70%的目标值

        // 需要一个数组存 (freq, tag) 并按 freq 降序排序
        std::vector<std::pair<long long, int>> freqTag; // (请求量, tag_id)
        freqTag.reserve(req_tag.size());
        for (int tag = 1; tag <= (int)req_tag.size(); ++tag) {
            freqTag.push_back({ req_tag[tag - 1], tag });
        }
        std::sort(freqTag.begin(), freqTag.end(),
            [](auto& a, auto& b) { return a.first > b.first; });

        // 从最大开始累加，直到达到阈值
        std::vector<int> hotTags;
        long long accum = 0;
        for (auto& kv : freqTag) {
            if (kv.first <= 0) break;  // 没有必要把 0 频度继续
            accum += kv.first;
            hotTags.push_back(kv.second);
            if (accum >= (long long)threshold) {
                break;
            }
        }
        // 如果 hotTags 为空就不用做任何操作
        if (hotTags.empty()) {
            return;
        }
        //if (hotTags.size() > 7) {
        //    hotTags.erase(hotTags.begin() + 7, hotTags.end());
        //}

        // ==============================================
        // 2) 定义一些辅助函数
        // ==============================================

        // (a) 判断“磁头是否在某标签 tag 的分区内”
        auto headInTagPartition = [&](int diskIdx, int tag, int head_id) {
            if (disks[diskIdx].table.count(tag) == 0) return false;
            auto range = disks[diskIdx].table.equal_range(tag);
            for (auto it = range.first; it != range.second; ++it) {
                // 若“磁头位置”在分区 [begin, begin+length)
                if (disks[diskIdx].head_pos[head_id] >= it->second.begin
                    && disks[diskIdx].head_pos[head_id] < it->second.begin + it->second.length)
                {
                    return true;
                }
            }
            return false;
            };

        // (b) 计算磁头当前所在分区“ex”值
        //     如果有 shared 分区，则 ex = req_tag[primary -1] + req_tag[shared->key -1]
        auto getEx = [&](int diskIdx,int head_id) {
            int exVal = 0;
            exVal = disks[diskIdx].read_most(time, disks[diskIdx].head_pos[head_id], 2, head_id, r);
            return exVal;
            };

        // ==============================================
        // 3) 预先对硬盘按 ex 值从小到大排序
        //    我们会依赖这个顺序“优先选 ex 最小的硬盘”给热标签
        // ==============================================
        struct HEAD_PROP{
            int exVal;
            int diskIdx;
            int head_id;
            bool has_moved;
        };
        std::vector<HEAD_PROP> exList; // (exVal, diskIdx)
        exList.reserve(disks.size());
        for (int i = 0; i < (int)disks.size(); ++i) {
            exList.push_back({ getEx(i,0), i, 0 ,false});
            exList.push_back({ getEx(i,1), i, 1 ,false});
        }
        std::sort(exList.begin(), exList.end(),
            [](HEAD_PROP& a, HEAD_PROP& b) { return a.exVal < b.exVal; });

        // 用于记录“这个磁头是否已经在本次函数调用里进行了 pass/jump”，
        // 确保不会对同一个硬盘多次操作。
        vector<vector<bool>> usedDisk(disks.size(), { false,false });

        // (c) 分配函数：让一个磁头去某个 tag 分区
        auto dispatchDiskToTag = [&](int tag) -> void {
            // 如果已经有磁头在分区里，就直接返回
            bool anyInThisTag = false;
            for (int i = 0; i < (int)disks.size(); ++i) {
                if (headInTagPartition(i, tag, 0) || headInTagPartition(i, tag, 1)) {
                    anyInThisTag = true;
                    break;
                }
            }
            if (anyInThisTag) {
                return;
            }

            // 否则，从 exList 中找第一个“还没用过” 且 “table.count(tag) > 0” 的硬盘
            for (auto& kv : exList) {
                int exVal = kv.exVal;
                int diskIdx = kv.diskIdx;
                int head_id = kv.head_id;
                if (usedDisk[diskIdx][head_id]) {
                    continue; // 已经做过 pass/jump
                }
                if (disks[diskIdx].table.count(tag) == 0) {
                    continue; // 该硬盘没有该标签分区
                }

                // 找到合适硬盘 => 先找最佳分区起点(参照你的 read_most 逻辑)
                auto range = disks[diskIdx].table.equal_range(tag);
                auto itBest = disks[diskIdx].table.end();
                int bestScore = disks[diskIdx].read_most(time,disks[diskIdx].head_pos[head_id], CHECKED_TIME + 1, head_id,r);
                for (auto it = range.first; it != range.second; ++it) {
                    int pass_target = it->second.begin;
                    int sc = disks[diskIdx].read_most(time, pass_target, CHECKED_TIME, head_id, r);
                    if (sc > bestScore) {
                        bestScore = sc;
                        itBest = it;
                    }
                }
                // 如果找到了更好的分区，就去那；若没找到(itBest==end)，默认就无动作
                if (itBest != disks[diskIdx].table.end()) {
                    int pass_target = itBest->second.begin;
                    while (disks[diskIdx].get_save_pos(pass_target).first == 0) {
                        pass_target = (pass_target + 1)% disks[diskIdx].Len;
                        if (pass_target == itBest->second.begin)
                            break;
                    }
                    // 尝试 pass，否则 jump
                    if (!disks[diskIdx].pass_head(pass_target, time, r, head_id)) {
                        disks[diskIdx].jump(pass_target, time, r, head_id);
                        disks[diskIdx].time_flag[head_id] = false;
                    }
                }
                // 标记这个硬盘已使用，避免再次 pass/jump
                usedDisk[diskIdx][head_id] = true;
                // 只需要给此标签分配一个磁头即可
                break;
            }
            };

        // ==============================================
        // 4) 按“热度”从大到小依次为这些标签找硬盘
        //    (hotTags 本身是按从大到小的顺序，因为 freqTag 那里是降序 sorted)
        // ==============================================
        for (int tag : hotTags) {
            dispatchDiskToTag(tag);
        }

        // done
    }

    //返回若干硬盘的磁头状态的c风格字符串
    friend void get_cstr(const vector<DISK>& disks) {
        for (int i = 0;i < disks.size();i++) {
            std::cout << disks[i].c_str[0] << std::endl;
            std::cout << disks[i].c_str[1] << std::endl;
        }
            
    }
};

#endif // !DISK_PARTITION