#define _CRT_SECURE_NO_WARNINGS
#pragma GCC optimize(2)
#include <iostream>
#include <unordered_map>
#include <vector>
#include "DISK.hpp"

using std::unordered_map;
using std::unordered_multimap;
using std::unordered_set;
using std::vector;

const int EXTRA_TIME = 105;                 // 额外的时间片数量
//const int TIME_LIMIT = 5;
const int CONCURRENCY = 500;                // 高并发模式阈值
const int TIME_INTERVAL = 20;               // 高并发模式特殊jump时间片间隔

int T, M, N, V, G;
int timestamp;                            //当前时间片序号，从1开始
vector<DISK> disk;                       //N块硬盘，每块硬盘由V个存储单元构成
// 全局数组
Request request[MAX_REQUEST_NUM];
//vector<Request> request(MAX_REQUEST_NUM);
Object object[MAX_OBJECT_NUM];           //注意对象数组object的索引在这个规则里从1开始
//全局数组：当前读请求id数组，默认从前开始读，最新请求被添加到末尾
//vector<int> pendingRequests;
unordered_set<int> pendingRequests;
//全局哈希结构：保存因为超时被清除出当前读请求id数组的id，key表示object的id，value表示请求的id
unordered_multimap<int, int> timeout_req_Set0;
//近期存储的每种tag被访问的次数
vector<int> req_tag;

// 输出当前时间片，并读取(丢弃)判题器给的"TIMESTAMP"
void timestamp_action()
{
    //int timestamp;
    // 读入格式："TIMESTAMP X"
    // %*s表示跳过一个字符串(即"TIMESTAMP")，读入后面的X
    scanf("%*s%d", &timestamp);
    // 按照题目规则，原样输出"TIMESTAMP X"
    printf("TIMESTAMP %d\n", timestamp);
    check_d_at_time_start(disk,timestamp,request);
    fflush(stdout);
}

/*
* n_write：代表这一时间片写入对象的个数。 输入数据保证总写入次数小于等于100000。
* 接下来 n_write 行，每行三个数 obj_id[i]、 obj_size[i]、 obj_tag[i]，代表当前时间片写入的对象编
号， 对象大小，对象标签编号。
* 输入数据保证 obj_id 为1开始每次递增1的整数， 且1 ≤ 𝑜𝑏𝑗_𝑠𝑖𝑧𝑒[𝑖] ≤ 5，1 ≤ 𝑜𝑏𝑗_𝑡𝑎𝑔[𝑖] ≤ 𝑀
*/
// 写入动作处理函数
void write_action(const vector<int>& wr, const vector<vector<int>>& write_num_every1800, const vector<vector<int>>& delete_num_every1800)
{
    int n_write;
    // 读取本时间片要写入的对象数量
    scanf("%d", &n_write);
    // 对每个要写入的对象依次处理
    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        // 读入对象id、大小，tag
        scanf("%d%d%d", &id, &size, &tag);
        // 初始化该对象的last_request_point等信息
        // object[id].last_request_point = 0;
        // 找到当前N个硬盘中剩余空间较大的3个
        //vector<int> choose_disk = findBest3(disk, tag, size);
        //for (int j = 0; j < REP_NUM; j++) {
        //    //当前为第j个副本;
        //    // 副本放到硬盘的编号
        //    object[id].replica[j] = choose_disk[j];
        //    // 为对象的第 j 副本分配一个int数组，用于记录写在哪些存储单元
        //    object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1)));
        //    object[id].size = size;
        //    object[id].is_delete = false;
        //    object[id].tag = tag;
        //    //对每个size都要进行一次写入
        //    for (size_t k = 0;k < size;k++) {
        //        object[id].unit[j][k] = disk[choose_disk[j]].writein(tag, wr);
        //    }
        //}
        object[id].size = size;
        object[id].is_delete = false;
        object[id].tag = tag;
        for (int j = 0; j < REP_NUM; j++) {
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * size));
        }
        writein(disk, timestamp, id, tag, size, object, wr, write_num_every1800, delete_num_every1800);
        /*
        * 输出包含4 ∗ 𝑛_𝑤𝑟𝑖𝑡𝑒行，每4行代表一个对象：
        * 第一行一个整数 obj_id[i]，表示该对象的对象编号。
        * 接下来一行，第一个整数 rep[1]表示该对象的第一个副本写入的硬盘编号，接下来对象大小(obj_size)个整数 unit[1][j]，代表第一个副本第𝑗个对象块写入的存储单元编号。
        * 第三行，第四行格式与第二行相同，为写入第二，第三个副本的结果。
        */
        printf("%d\n", id);
        for (int j = 0; j < REP_NUM; j++) {
            //这里范围从1开始变为c++索引从0开始，需要+1
            printf("%d", object[id].replica[j] + 1);
            for (int k = 0; k < size; k++) {
                printf(" %d", object[id].unit[j][k] + 1);
            }
            printf("\n");
        }
    }
    fflush(stdout);
}

// 删除动作处理函数
void delete_action()
{
    int n_delete;
    int abort_num = 0; // 需要被取消的读请求计数

    // 读入本时间片删除的对象数量n_delete
    scanf("%d", &n_delete);
    if (n_delete == 0) {
        std::cout << 0 << std::endl;
        return;
    }

    // 依次读入要删除的对象id
    vector<int> delete_idx(n_delete);
    for (int i = 0; i < n_delete; i++) {
        scanf("%d", &delete_idx[i]);
    }

    //构建现删除请求的哈希结构
    unordered_set<int> delete_Set(delete_idx.begin(), delete_idx.end());
    //需要取消的读请求编号
    vector<int> cancel_req;
    for (auto it = pendingRequests.begin();it != pendingRequests.end();) {
        if (delete_Set.count(request[*it].object_id)) {
            cancel_req.push_back(*it);
            clear_request_id(disk, request[*it], object,timestamp,request, *it);
            it = pendingRequests.erase(it);
        }
        else
            it++;
    }

    // 输出总的取消请求数量
    printf("%d\n", (int)cancel_req.size());
    for (int i = 0;i < cancel_req.size();i++) {
        printf("%d\n", cancel_req[i]);
        req_tag[object[request[cancel_req[i]].object_id].tag - 1]--;
    }

    // 删除硬盘上的数据
    for (int i = 0; i < n_delete; i++) {
        // 真正删除对应硬盘上的内容，这里范围从1开始变为c++索引从0开始
        int id = delete_idx[i];
        for (int j = 0; j < REP_NUM; j++) {
                //_id[]表示要删除的id数组，object[id]表示本次循环要删除的object对象，要删除的是其第j个副本，disk_idx是对应硬盘索引，disk_unit是对应存储单元索引
            int disk_idx = object[id].replica[j];
            //对每个block进行一次删除
            for (int k = 0;k < object[id].size;k++) {
                int disk_unit = object[id].unit[j][k];
                disk[disk_idx].delete_act(disk_unit,timestamp,request);
            }
        }
        // 标记该对象已删除
        object[id].is_delete = true;
    }
    fflush(stdout);
}

// N个磁头开始执行读操作，从d值最小的开始动，直到所有磁头都无法继续时结束
void Ndisk_read(vector<int>& finishedThisTurn,int time) {
    DISK* chosed_disk = get_nearest_disk(disk,time,request);
    while (chosed_disk != nullptr) {
        int target = (chosed_disk->head() + chosed_disk->update_d(time,request)) % V;
        std::pair<int, int> tem = chosed_disk->get_request_pos(target);
        int obj_idx = tem.first, block_idx = tem.second;
        bool read_successful;
        if (chosed_disk->head() == target) {
            read_successful = chosed_disk->read_act(timestamp,request);
        }
        else {
            if (chosed_disk->pass_head(target, time, request)) {
                read_successful = chosed_disk->read_act(timestamp, request);
            }
            else {
                read_successful = false;
                chosed_disk->jump(target, time, request);
            }
        }
        if (read_successful) {
            //如果读成功了，需要更新参数
            for (int req_id : chosed_disk->get_request_id(target)) {
                request[req_id].has_read[block_idx] = true;
                //将该请求已读的对象块从DISK中弹出
                pop_Request_out(disk, request[req_id], object,timestamp,request,req_id, block_idx);
                clear_request_id(disk, request[req_id], object, timestamp, request, req_id, block_idx);
                request[req_id].is_done = true;
                for (int i = 0;i < request[req_id].object_size;i++) {
                    if (!request[req_id].has_read[i]) {
                        request[req_id].is_done = false;
                        break;
                    }
                }
                if (request[req_id].is_done) {
                    finishedThisTurn.push_back(req_id);
                    //pendingRequests中清除值为req_id的元素
                    pendingRequests.erase(req_id);
                }
            }
        }
        else {
            //没成功，说明这个选择的硬盘已经没法儿读了，可以进入下一个循环了
            chosed_disk->flag() = false;
        }
        chosed_disk = get_nearest_disk(disk,time,request);
    }
}

static int get_hot_tag(const vector<int>& nums) {
    int idx = 0, max_val = INT_MIN;
    for (int i = 0;i < nums.size();++i) {
        if (nums[i] > max_val) {
            idx = i;
            max_val = nums[i];
        }
    }
    return idx + 1;
}

/*
* n_read：代表这一时间片读取对象的个数。 输入数据保证总读取次数小于等于30000000。
* 接下来 n_read 行，每行两个数 req_id[i]、 obj_id[i]，代表当前时间片读取的请求编号和请求的对象
编号。 输入数据保证读请求编号为 1 开始每次递增 1 的整数， 读取的对象在请求到来的时刻一定在存储系
统中。
*/
// 读取动作处理函数，timestamp表示时间片序号
void read_action(int timestamp)
{
    int n_read;
    int request_id, object_id;

    // 本时间片有多少个读请求
    scanf("%d", &n_read);
    // 读取所有读请求
    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        // 记录该读请求对应的对象、上一个请求ID
        request[request_id].object_id = object_id;
        //request[request_id].prev_id = object[object_id].last_request_point;
        //该请求的下一个该读取的块，初始为0
        request[request_id].read_size_phase = 0;
        //该请求的起始时间片序号
        request[request_id].start_time = timestamp;
        // 刚到来的请求标记未完成
        request[request_id].is_done = false;
        request[request_id].object_size = object[object_id].size;
        //request[request_id].has_read = vector<bool>(object[object_id].size, false);
        req_tag[object[request[request_id].object_id].tag - 1]++;
        // 放入待处理队列
        pendingRequests.insert(request_id);
        //将请求压入
        push_Request_in(disk, request[request_id], object, request_id);
    }
    // 用来记录本时间片完成的所有请求ID
    vector<int> finishedThisTurn;
    //先让disk进入新时间片待命：（这一步是为了更新DISK参数）
    update_timesample(disk);
    if (pendingRequests.empty()) {
        //输出所有磁头不动
        get_cstr(disk);
        std::cout << 0 << std::endl;
        return;
    }
    //if (pendingRequests.size() > CONCURRENCY && timestamp % TIME_INTERVAL == 0) {
    //    special_jump(disk, timestamp, request);
    //}

    /*std::pair<int, int> top2 = findTop2Indices(req_tag);
    int hot_tag = top2.first + 1, hot_tag2 = top2.second + 1;
    check_at_hot_tag(disk, hot_tag, hot_tag2, timestamp, request, req_tag);*/

    check_at_hot_tag(disk,timestamp, request, req_tag);//（新前三热分区逻辑）

    //check_scores(disk, timestamp, request);

    Ndisk_read(finishedThisTurn,timestamp);
    //输出N个磁头的动作
    get_cstr(disk);
    //输出完成的请求个数
    std::cout << finishedThisTurn.size() << std::endl;
    //分别输出完成的请求的编号
    for (size_t i = 0;i < finishedThisTurn.size();i++) {
        std::cout << finishedThisTurn[i] << std::endl;
        req_tag[object[request[finishedThisTurn[i]].object_id].tag - 1]--;
    }
    fflush(stdout);
}



int main()
{
    // 读入T, M, N, V, G
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);

    //数组[i][j]表示tag i+1 在第 j*1800+1 ~ (j+1)*1800 时间片的删除的size总和
    vector<vector<int>> delete_num_every1800(M, vector<int>((T - 1) / FRE_PER_SLICING + 1, 0));
    // 删除预处理信息
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < (T - 1) / FRE_PER_SLICING + 1; j++) {
            int temp = 0;
            scanf("%d",&temp);
            delete_num_every1800[i][j] = temp;
        }
    }
    //int* write_num = new int[M * ((T - 1) / FRE_PER_SLICING + 1)];
    
    //每个tag总共写入的次数
    vector<int> write_num(M, 0);
    //各个tag值的分区长度
    vector<int> write_freq(M, 0);
    //数组[i][j]表示tag i+1 在第 j*1800+1 ~ (j+1)*1800 时间片的写入的size总和
    vector<vector<int>> write_num_every1800(M, vector<int>((T - 1) / FRE_PER_SLICING + 1, 0));
    int total_write = 0;//总共写操作次数
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < (T - 1) / FRE_PER_SLICING + 1; j++) {
            int temp = 0;
            scanf("%d",&temp);
            write_num[i] += temp;
            total_write += temp;
            write_num_every1800[i][j] = temp;
        }
    }
    for (int i = 0;i < M;i++) {
        write_freq[i] = (double)write_num[i] / (double)total_write * V;
    }
    //读取预处理信息
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }
    disk = vector<DISK>(N, DISK(V, M, G));           //N块硬盘，每块硬盘由V个存储单元构成
    //pre_alloc(disk, write_freq);
    req_tag = vector<int>(M, 0);
    // 输出"OK"表示预处理阶段完成
    printf("OK\n");
    fflush(stdout);


    // 主循环：从时间片1到T + EXTRA_TIME
    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        timestamp_action(); // 1. 时间片对齐
        delete_action();    // 2. 删除操作
        write_action(write_freq, write_num_every1800, delete_num_every1800);// 3. 写入操作
        read_action(t);      // 4. 读取操作
    }
    //delete[] write_num;
    return 0;
}
