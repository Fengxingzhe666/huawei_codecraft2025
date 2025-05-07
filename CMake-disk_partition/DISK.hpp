#ifndef DISK_PARTITION
#define DISK_PARTITION

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <climits> //linux�������

using std::unordered_map;
using std::unordered_multimap;
using std::unordered_set;
using std::vector;
using std::string;

const int MAX_DISK_NUM = (10 + 1);          // ���Ӳ����(ʾ����Ԥ��10��+1)
const int MAX_DISK_SIZE = (16384 + 1);      // ÿ��Ӳ�����洢��Ԫ����(ʾ����Ԥ��16384+1)
const int MAX_REQUEST_NUM = (30000000 + 1); // ������������
const int MAX_OBJECT_NUM = (100000 + 1);    // ����������
const int REP_NUM = 3;                      // ���ั������(Ҫ��3��)
const int MAX_SIZE = 5;                     // ��Ŀ��ÿ��object��size���ֵΪ5
const int READ_COST[8] = { 64,52,42,34,28,23,19,16 };//������ȡ���ĵ�tookenֵ
//const int READ_NULL_THRESHOLD[4] = { 3,5,5,6 };
const int READ_NULL_THRESHOLD[7] = { 0,1,2,4,6,7,8 };
const int FRE_PER_SLICING = 1800;           // ʱ��Ƭ���(ʾ����ֻ��ʾ��������ʹ��)
const int RED_POSITION_THRE = 9000;         // ��ɫλ�õ�����������ֵ��ע�����*1000
const int CHECKED_TIME = 25;                 //read_most()��������ʱ��Ƭ����
const int SMALL_PARTITION_LENGTH = 40;      // ָ��С�����ĳ���
//const double SHARED_PARTITION_THRE = 1.0;   //�������д������ɾ���������ı�����ֵ
const size_t LOWEST_LENGTH = 13;            //�жϹ������ʱ����С����
const double HOT_PERCENT = 0.80;

// ����ṹ�壬���ڱ���ĳ�ζ��������Ϣ
typedef struct Request_ {
    int object_id;  // ��Ҫ��ȡ�Ķ���ID
    int object_size;// ��Ҫ��ȡ�Ķ����size
    //int prev_id;    // ����ʽָ����һ�������ID(���ڻ���)
    int read_size_phase;//������Ŀǰ������blockλ�ã���ʼΪ0
    bool has_read[MAX_SIZE];
    //int time_flag;  //��������뷢��ʱ����ʱ����ʼ��Ϊ0�����������ǰ���ָ���
    int start_time;   //���������ʱ��Ƭ���
    bool is_done;   // �������Ƿ������
} Request;

// ����ṹ�壬���ڱ��������Ϣ��ע��Request��Object���������Ǵ�1��ʼ
typedef struct Object_ {
    int replica[REP_NUM];          // ��¼�����������������ЩӲ����
    int* unit[REP_NUM];            // ÿ��������Ӧ�Ĵ洢��Ԫ�������
    int size;                      // �����С(��λ: �������)
    int tag;                       // �����������tag
    //int last_request_point;        // ��¼���һ�η��������ı��(���ڻ��ݡ�ȡ����)
    bool is_delete;                // ��Ǹö����Ƿ��ѱ�ɾ��
} Object;

//�÷ֵĺ���f(x)
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

//Ѱ�Ҹ�����������0�ĳ��Ⱥ���ʼ���������أ�����
static bool max_consecutive_zero(const vector<int>& nums, int threshold) {
    int begin_idx = 0;
    //max_consecutive�����0�ĳ��ȣ�current_consecutive����ǰ��0�ĳ���
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
//���ظ������飨����2��Ԫ�أ����ֵ�����͵ڶ����ֵ������
std::pair<int, int> findTop2Indices(const vector<int>& nums) {
    int max1 = INT_MIN, max2 = INT_MIN;
    int idx1 = -1, idx2 = -1;
    for (int i = 0; i < (int)nums.size(); ++i) {
        int val = nums[i];
        if (val > max1) {
            // �������ֵ��ԭ���ֵ��Ϊ�ڶ���
            max2 = max1;   idx2 = idx1;
            max1 = val;    idx1 = i;
        }
        else if (val > max2) {
            // �����µڶ���
            max2 = val;
            idx2 = i;
        }
    }
    return { idx1, idx2 };
}
//���ظ����������������������
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
    const size_t Len;                          //Ӳ�̴�С��ʹ��size()�ӿ�
    size_t remain;                             //Ӳ�̵�ǰʣ�����λ������ʹ��remaining()�ӿ�
    vector<int> data;                          //�洢�����ݣ�ʹ��[]��������ط���
    const int M;                               //tagֵ������������ֵ�����ڹ��캯���г�ʼ��
    const int G;                               //���β���������ĵ�tooken���������ʼ��
    int tooken_;                               //��ʾ��ǰӲ��ʣ���tooken
    size_t head_idx;                           //��ʾ��ǰӲ�̵Ĵ�ͷλ�ã�ע��������0��ʼ
    int consecutive_read;                      //��ʾ��ͷ�����Ĵ���
    int d;                                     //Ӳ��dֵ���������������λ�õľ��룩
    struct partition {
        int key;                            //�÷�����key
        size_t begin;                       //�÷�����Ӳ�̵���ʼλ������
        size_t length;                      //�÷����ĳ���
        size_t remain;                      //�÷���������
        partition* shared;                  //����ķ����Ķ����ַ��Ĭ��Ϊ��
        //Ĭ�Ϲ��캯���������������ɹ�ϣ���ã�ʵ�ʲ����ã�
        partition() : key(0), begin(0), length(0), remain(0), shared(nullptr) {}
        //���캯��
        partition(DISK& d, int k, size_t beg, int len) : key(k), begin(beg), length(len),remain(len), shared(nullptr) {
            //���������dataֵдΪ-1
            for (size_t i = beg;i < beg + len;i++) {
                d.data[i] = -1;
            }
        }
        //��÷��������ҵ�һ������λ�������������򷵻�begin + length
        size_t get_next_avail(const DISK& d) const{
            for (size_t i = begin;i < begin + length;i++) {
                if (d.data[i] <= 0) {
                    return i;
                }
            }
            return begin + length;
        }
        //���ط����Ƿ�Ϊ��
        bool empty()const { return remain == length; }
        //���ظ÷�����β��n��Ԫ���ǲ��Ƕ��ǿյģ�����Ǿͷ�����
        bool space_too_much(const DISK& disks,int n) const {
            /*if (length <= n)
                return false;*/
            for (int i = begin + length - 1;i > begin + length - 1 - n;--i) {
                if (disks.data[i] > 0)
                    return false;
            }
            return true;
        }
        //�ı�����+x������й��������Ҫͬ���ı�
        void maintain_remain(int x) {
            remain += x;
            if (shared != nullptr) {
                shared->remain += x;
                //��ʼλ�������ͳ���Ҳһ��ͬ������ֹ�ָ�С����ʱ����bug
                shared->length = length;
                shared->begin = begin;
            }
        }
    };
    unordered_multimap<int, partition> table;  //��ϣ��ʹ��tabled()�ӿ�
    string c_str = "#";                        //���ڱ���Ӳ�̶������ַ���
    /*һ��1* Len�����飬��ʾӲ����ÿ���洢��Ԫ��ǰ�Ƿ���ڶ����������i���洢��Ԫ��ǰ���ڶ�������request_pos[i] = {��i���洢��Ԫ�洢��obj��ţ���obj��block���}�������i���洢��Ԫ��ǰ�����ڶ�������request_pos[i] = {0,1}*/
    vector<std::pair<int, int>> request_pos;   
    /*һ��1* Len�����飬��ʾӲ���ϵ�i���洢��Ԫ��ǰ�Ƿ���ڶ������������request_id[i] = vector<int>{(��λ�õ�����������...)}��Ԫ��request_id[i][j]��ʾ��i���洢��Ԫ��ǰ��j��������ı��*/
    vector<vector<int>> request_id; 
    bool time_flag;                      //ʱ��Ƭ��־����ʾ��ǰʱ��Ƭ��Ӳ�̴�ͷ�Ƿ��ܼ����ƶ�

    // ������һ��������c�������Ĵ洢��Ԫ����ʼλ��������������ʹ��,����Ԥ����Ĳ������ڷ�����������Ҳ����򷵻�-1
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
    // ��Ӳ���д�ĩβ��ǰ�� size �������Ŀ��д洢��Ԫ��data[i] == 0��-1��������������ʼλ���������Ҳ�������-1
    int find_consecutive_from_end(int size) const {
        // left��right ����ĩβ��ʼ
        int right = Len - 1, left = Len - 1;
        while (true) {
            if (left < 0) {
                return -1; // �ѵ���ǰ����Ȼ�Ҳ����㹻����������
            }
            // �����ǿյ�Ԫ�Ͱ� right��left ͬʱ��ǰŲ��
            while (data[left] > 0) {
                --left;
                right = left;
                if (left < 0) {
                    return -1;
                }
            }
            // ���� data[left] == 0����� [left..right] �Ƿ��Ѿ��� size �������Ŀ���
            if (right - left + 1 == size) 
                return left;// �ҵ���������ʼ����
            // ���������С���䣬������ǰ��
            --left;
        }
    }
    //���Դ���һ������Ϊlength���·���������ɹ��ͷ�������ʼ�洢��Ԫλ��������ʧ�ܾͷ���-1��size��ʾ��һ�μ���д������ݳ��ȣ�����ά��remain
    int creat_partition(int length, int tag, int size = 0) {
        int begin_idx = next_partition_begin_idx(length);//����length�������Ŀ��еĴ洢��Ԫ����ʼλ������
        if (begin_idx == -1)
            return -1;//����ʧ�ܣ�����-1
        partition p(*this, tag, begin_idx, length);
        p.remain -= size;//ע��ά���µķ�����sizeֵ
        table.insert({ tag,p });
        return begin_idx;
    }
    //�жϷ����Ƿ��������size�����пռ䣬������򷵻�����size�����пռ����ʼ������û���򷵻�-1
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
    //����һ������
    void dumplicated(int tag) {
        auto range = table.equal_range(1);
        for (auto it = range.first;it != range.second;it++) {
            //ע�⹲�������ǰ������������Ҫ�ı�
            if (it->second.remain > it->second.length / 2 && it->second.shared == nullptr) {
                partition p = it->second;
                p.key = tag, p.shared = &it->second;
                //�����ϣ�����²����partition����ĵ�ַ��ԭ���Ķ���p���������٣�&p��Ч��
                auto p_iter_in_hash_map = table.insert({ tag,p });
                it->second.shared = &p_iter_in_hash_map->second;
                break;//����for������
            }
        }
    }
    //����д�루���������ҵ�һ����λ�þ�д�룩������д��λ�õ�����
    size_t brute_write(int tag) {
        size_t write_idx = Len - 1;
        while (data[write_idx] > 0) {
            write_idx--;//write_idx�ǵ�һ����λ��
            if (write_idx == -1) {
                throw std::out_of_range("���̿ռ�������д�����ʧ��");
                break;
            }
        }
        data[write_idx] = tag;
        remain--;
        for (auto it = table.begin();it != table.end();it++) {
            if (it->second.begin <= write_idx && write_idx < it->second.begin + it->second.length) {
                //it->second.remain--;//��Ӧλ�÷�������-1
                it->second.maintain_remain(-1);
                break;
            }
        }
        return write_idx;
    }
public:
    DISK() :Len(1), remain(1), M(1), head_idx(0), consecutive_read(0), G(64),tooken_(64), time_flag(true),d(1){
        data = vector<int>(Len, 0);
        request_pos = vector<std::pair<int, int>>(Len, { 0,-1 });
        request_id = vector<vector<int>>(Len);
    }
    DISK(size_t v, int m, int g) :Len(v), remain(v), M(m), head_idx(0), consecutive_read(0), G(g), tooken_(g), time_flag(true), d(v) {
        data = vector<int>(Len, 0);
        request_pos = vector<std::pair<int, int>>(Len, { 0,-1 });
        request_id = vector<vector<int>>(Len);
    }
    //���ص�ǰӲ�̵Ĵ�ͷλ��������������0��ʼ��
    size_t head()const { return head_idx; }
    size_t& head() { return head_idx; }
    int tooken()const { return tooken_; }
    //����Ӳ��ʱ���־λ�����ݵ�ַ�����ⲿ�޸����ֵ
    bool& flag() { return time_flag; }
    //Ӳ�̽�����һ��ʱ��Ƭ������Ӳ�̲���������ÿһ��Ӳ�̵�ʣ��tookenΪG��ˢ�²���c_str��ˢ��time_flagΪtrue��
    friend void update_timesample(vector<DISK>& disks) {
        for (int i = 0;i < disks.size();++i) {
            disks[i].tooken_ = disks[i].G;
            disks[i].c_str = "#";
            disks[i].time_flag = true;
        }
    }
    friend void check_d_at_time_start(vector<DISK>& disks, int time, const Request r[]) {
        for (int i = 0;i < disks.size();++i) {
            if (disks[i].get_position_scores((disks[i].head_idx + disks[i].d) % disks[i].Len, time, r) == 0) {
                //�����λ�õ�����ȫ�������ˣ�����dֵ
                disks[i].d = disks[i].update_d(time, r);
            }
        }
    }
    //���ص�ǰ��ͷ��Ŀ������target�ľ��루��ͷֻ�ܵ����ƶ���
    size_t distance(size_t target)const { 
        size_t temp = 0;
        // ���Ŀ��λ���ڵ�ǰ��ͷ֮��ֱ��������
        if (target >= head_idx) {
            temp = target - head_idx;
        }
        else {
            // ����������β���ľ��룬�ټ��� target
            temp = (Len - head_idx) + target;
        }
        return temp;
    }
    /*@brief ����ĳʱ��Ƭ��ȡ����idx�Ĵ洢��Ԫ�ĵ÷�
    * @param idx     ��Ӳ�̵Ĵ洢��Ԫ����
    * @param time    ��ǰʱ��Ƭ����
    * @param r       �����request����
    */
    int get_position_scores(int idx, int time, const Request r[])const {
        int scores = 0;//����
        for (size_t i = 0;i < request_id[idx].size();i++) {
            scores += func(time - r[request_id[idx][i]].start_time);// * (r[request_id[idx][i]].object_size + 1) / 2;
        }
        return scores;
    }
    //ͨ����������Ӳ�̵�dֵ��ʱ�临�Ӷȸߣ��������ٵ��ã��������÷�Ϊ0��λ�ã�
    int update_d(int time, const Request r[]) const {
        int ans = 0;
        while (request_pos[(ans + head_idx) % Len].first == 0 || get_position_scores((ans + head_idx) % Len, time, r) == 0) {
            ans++;
            if (ans == Len) {
                break;
            }
        }
        return ans;
    }
    //ͨ����������Ӳ�̵Ĵ�dֵ��ʱ�临�Ӷȸߣ�
    int update_big_d(int time, const Request r[])const {
        int ans = 0;
        while (request_pos[(ans + head_idx) % Len].first == 0 || get_position_scores((ans + head_idx) % Len, time, r) < RED_POSITION_THRE) {
            ans++;
            if (ans == Len) {
                break;
            }
        }
        return ans;
    }
    //������ʼ������ÿ���̰���Ԥ������Ϣ��ΪM������
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
    //�����д��һ��tag�����ݣ�����д��λ�õ�����
    size_t writein(int tag,const vector<int>& wr) {
        if (table.count(tag)) {
            //�Ѿ�����������
            auto range = table.equal_range(tag);
            for (auto it = range.first;it != range.second;it++) {
                size_t flag = it->second.get_next_avail(*this);
                if (flag < it->second.begin + it->second.length) {
                    //�����������ʣ��ռ䣬ֱ��д������
                    data[flag] = tag;
                    remain--;
                    it->second.remain--;
                    return flag;
                }
            }
            //�����Ѿ�û�пռ�
            int disk_next_available = next_partition_begin_idx(wr[tag - 1]/3);
            if (disk_next_available >= 0) {
                //ʣ�µĿռ仹���ٷ����������²�һ��������
                partition p(*this, tag, disk_next_available, wr[tag - 1]/3);
                data[disk_next_available] = tag;
                remain--;
                p.remain--;
                table.insert({ tag,p });//�����·���
                return disk_next_available;
            }
            else {
                //ʣ�µĿռ��Ѿ����ܼ���������
                disk_next_available = next_partition_begin_idx(Len / 280);
                if (disk_next_available != -1) {
                    partition p(*this, tag, disk_next_available, Len / 280);
                    data[disk_next_available] = tag;
                    remain--;
                    p.remain--;
                    table.insert({ tag,p });
                    return disk_next_available;
                }
                else
                    return brute_write(tag);
            }
        }
        else {
            //��û�з���
            int disk_next_available = next_partition_begin_idx(wr[tag - 1]);
            if (disk_next_available >= 0) {
                //ʣ��ռ��㹻�����·���
                partition p(*this, tag, disk_next_available, wr[tag - 1]);
                data[disk_next_available] = tag;//д������
                remain--;
                p.remain--;
                table.insert({ tag,p });//�����·���
                return disk_next_available;
            }
            else {
                //ʣ��ռ䲻�㴴���·���
                disk_next_available = next_partition_begin_idx(Len / 280);
                if (disk_next_available != -1) {
                    partition p(*this, tag, disk_next_available, Len / 280);
                    data[disk_next_available] = tag;
                    remain--;
                    p.remain--;
                    table.insert({ tag,p });
                    return disk_next_available;
                }
                else
                    return brute_write(tag);
            }
        }
    }
    //�����д��һ��tag�����ݣ�����д��λ�õ����������߼���
    friend void writein(vector<DISK>& disks,int time,int obj_idx, int tag, int size, Object object[], const vector<int>& wr,const vector<vector<int>>& write_every1800, const vector<vector<int>>& delete_every1800) {
        vector<int> chosed_disks_idx = findBest3(disks, tag, size);
        for (int i = 0;i < REP_NUM;++i) {
            int disk_idx = chosed_disks_idx[i];
            disks[disk_idx].remain -= size; //ע��ά��remainֵ
            object[obj_idx].replica[i] = disk_idx;
            if (disks[disk_idx].table.count(tag) > 0) {
                auto range = disks[disk_idx].table.equal_range(tag);
                auto itFind = disks[disk_idx].table.end();//ѡ�е�д������ĵ�����
                //ѡ��д�����������д����ʼλ�����������ѡ��д�����������size�ռ���Ϊ-1
                int beg_itFind = -1;
                //bool has_written_successful = false;//�Ƿ�ɹ������з�����д��
                for (auto it = range.first;it != range.second;it++) {
                    if (it->second.remain < size)
                        continue;//����������д���Ҫ�����ķ���
                    if (itFind == disks[disk_idx].table.end()) {
                        itFind = it;//�״γ�ʼ��
                        beg_itFind = disks[disk_idx].enough_consecutive_space(size, itFind->second);
                        continue;
                    }
                    if (it->second.shared == nullptr && itFind->second.shared != nullptr) {
                        // 1.δ����ķ����������ѹ���ķ���
                        itFind = it;
                        beg_itFind = disks[disk_idx].enough_consecutive_space(size, itFind->second);
                        continue;
                    }
                    int beg_it = disks[disk_idx].enough_consecutive_space(size, it->second);
                    //beg_itFind = disks[disk_idx].enough_consecutive_space(size, itFind->second);
                    if (beg_it != -1 && beg_itFind == -1) {
                        // 2.������size�����пռ�ķ���������������size�����пռ�ķ���
                        itFind = it;          //������ѡ����
                        beg_itFind = beg_it;  //������ʼλ�ã�debug��
                        continue;
                    }
                    if (itFind->second.shared == nullptr && beg_itFind != -1) {
                        //˵���ҵ��˺��ʵķ���
                        break;
                    }
                }
                if (itFind != disks[disk_idx].table.end()) {
                    if (beg_itFind != -1) {
                        //��������д��
                        for (int b = 0;b < size;b++) {
                            disks[disk_idx].data[b + beg_itFind] = tag;
                            object[obj_idx].unit[i][b] = beg_itFind + b;
                        }
                        itFind->second.maintain_remain(-size);//ע��ά������������
                    }
                    else {
                        //��ɢд��
                        size_t write_idx = itFind->second.begin;
                        for (int b = 0;b < size;++b) {
                            while (disks[disk_idx].data[write_idx] > 0) {
                                write_idx++;//write_idx�ǵ�һ����λ��
                            }
                            disks[disk_idx].data[write_idx] = tag;
                            object[obj_idx].unit[i][b] = write_idx;
                        }
                        itFind->second.maintain_remain(-size);//ά������������
                    }
                    continue;
                }
                //for (auto it = range.first;it != range.second;it++) {
                //    int begin_idx = disks[disk_idx].enough_consecutive_space(size, it->second);
                //    if (begin_idx != -1) {
                //        //����ҵ�����size������λ���ˣ�ִ�о����д�����
                //        for (int b = 0;b < size;b++) {
                //            disks[disk_idx].data[b + begin_idx] = tag;
                //            object[obj_idx].unit[i][b] = begin_idx + b;
                //        }
                //        //it->second.remain -= size;//ע��ά������������
                //        it->second.maintain_remain(-size);
                //        has_written_successful = true;
                //        break;
                //    }
                //    //�����Ҳ��������Ŀռ䣬�����ʣ��ռ��㹻�Ͱ�object��д�������������
                //    else if (it->second.remain >= size) {
                //        size_t write_idx = it->second.begin;
                //        for (int b = 0;b < size;++b) {
                //            while (disks[disk_idx].data[write_idx] > 0) {
                //                write_idx++;//write_idx�ǵ�һ����λ��
                //            }
                //            disks[disk_idx].data[write_idx] = tag;
                //            object[obj_idx].unit[i][b] = write_idx;
                //        }
                //        //it->second.remain -= size;//ע��ά������������
                //        it->second.maintain_remain(-size);
                //        has_written_successful = true;
                //        break;
                //    }
                //}
                //if (has_written_successful) continue;
                
                //û���ҵ���������size������λ�õķ��������Դ��������
                int begin_idx = disks[disk_idx].creat_partition(wr[tag - 1] / 2, tag, size);//�����°����
                if (begin_idx != -1) {
                    for (int b = 0;b < size;b++) {
                        disks[disk_idx].data[b + begin_idx] = tag;
                        object[obj_idx].unit[i][b] = begin_idx + b;
                    }
                    continue;
                }
            }
            else {
                int begin_idx = disks[disk_idx].creat_partition(wr[tag - 1], tag, size);//�����·���
                if (begin_idx != -1) {
                    for (int b = 0;b <  size;b++) {
                        disks[disk_idx].data[b + begin_idx] = tag;
                        object[obj_idx].unit[i][b] = begin_idx + b;
                    }
                    continue;
                }
                //û���ҵ��㹻�������ռ䴴���·��������Դ��������
                begin_idx = disks[disk_idx].creat_partition(wr[tag - 1] / 2, tag, size);
                if (begin_idx != -1) {
                    for (int b = 0;b < size;b++) {
                        disks[disk_idx].data[b + begin_idx] = tag;
                        object[obj_idx].unit[i][b] = begin_idx + b;
                    }
                    continue;
                }
            }
            //����С������
            int begin_idx2 = disks[disk_idx].creat_partition(SMALL_PARTITION_LENGTH, tag, size);
            if (begin_idx2 != -1) {
                for (int b = 0;b < size;b++) {
                    disks[disk_idx].data[b + begin_idx2] = tag;
                    object[obj_idx].unit[i][b] = begin_idx2 + b;
                }
                continue;
            }
            //����С����Ҳʧ����
            //����һ��table���ܷ��ҵ�һ�������Ҳ�����С�������ȵĿռ��Ϊ����
            bool find_a_part = false;//��ʾ�Ƿ�ɹ��ҵ�����Ҫ��ķ���
            //unordered_set<int> has_begin_idx;//������ʼλ�ù�ϣ�����ڹ����ظ������������
            for (auto it = disks[disk_idx].table.begin();it != disks[disk_idx].table.end();it++) {
                //if (has_begin_idx.count(it->second.begin))
                //    continue;
                //has_begin_idx.insert(it->second.begin);
                if (it->second.space_too_much(disks[disk_idx], SMALL_PARTITION_LENGTH)) {
                    find_a_part = true;
                    //it->second��ʾ�Ҳ�������SMALL_PARTITION_LENGTH������λ�õķ�������ʱ����ⲿ��λ�÷ָ��it->second���ָ���tag��С����
                    int begin_idx = it->second.begin + it->second.length - SMALL_PARTITION_LENGTH;//�·�������ʼλ��
                    //�ֶ������·�����ʡȥdata��-1��0�ٱ�-1�Ĺ���
                    partition p;
                    p.key = tag, p.begin = begin_idx, p.length = SMALL_PARTITION_LENGTH, p.remain = SMALL_PARTITION_LENGTH - size;
                    disks[disk_idx].table.insert({ tag,p });
                    for (int b = 0;b < size;b++) {
                        disks[disk_idx].data[b + begin_idx] = tag;
                        object[obj_idx].unit[i][b] = begin_idx + b;
                    }
                    it->second.length -= SMALL_PARTITION_LENGTH;
                    //it->second.remain -= SMALL_PARTITION_LENGTH;
                    it->second.maintain_remain(-SMALL_PARTITION_LENGTH);
                    break;
                }
            }
            if (find_a_part)
                continue;
            // ����Ѱ�ҹ������
            size_t max_remain = LOWEST_LENGTH;//Ѱ������remain
            //�����partition����ĵ�����
            unordered_multimap<int, DISK::partition>::iterator shared_partition_iterator = disks[disk_idx].table.end();
            for (auto it = disks[disk_idx].table.begin();it != disks[disk_idx].table.end();it++) {
                int replace = it->first;//Ҫ�������tag
                // �������tag��д-ɾ
                int x1 = write_every1800[replace - 1][time / FRE_PER_SLICING] -delete_every1800[replace - 1][time / FRE_PER_SLICING];
                // �������tag��д-ɾ
                int x2 = write_every1800[tag - 1][time / FRE_PER_SLICING] - delete_every1800[tag - 1][time / FRE_PER_SLICING];
                if (it->second.shared == nullptr && x2 > x1) {
                    if (it->second.remain > max_remain) {
                        max_remain = it->second.remain;
                        shared_partition_iterator = it;
                    }
                }
            }
            //double max_spare_ratio = 0.0;//�����б���
            //for (auto it = disks[disk_idx].table.begin();it != disks[disk_idx].table.end();it++) {
            //    if (it->second.remain < LOWEST_LENGTH || it->second.shared != nullptr)
            //        continue;//�������Ҫ����
            //    int replace = it->first;//Ҫ�������tag
            //    // �������tag��д-ɾ
            //    int x1 = write_every1800[replace - 1][time / FRE_PER_SLICING] - delete_every1800[replace - 1][time / FRE_PER_SLICING];
            //    // �������tag��д-ɾ
            //    int x2 = write_every1800[tag - 1][time / FRE_PER_SLICING] - delete_every1800[tag - 1][time / FRE_PER_SLICING];
            //    //��ǰ�����Ŀ��б���
            //    double spare_ratio = (double)it->second.remain / (double)it->second.length;
            //    if (x2 > x1 && spare_ratio > max_spare_ratio) {
            //        max_spare_ratio = spare_ratio;
            //        shared_partition_iterator = it;
            //    }
            //}
            if (shared_partition_iterator != disks[disk_idx].table.end()) {
                //˵���ҵ��˷��������Ĺ������������partition������ָ��Է�
                partition p = shared_partition_iterator->second;//p���·�������ʱ����
                p.key = tag, p.shared = &shared_partition_iterator->second;
                //�����ϣ�����²����partition����ĵ�ַ��ԭ���Ķ���p���������٣�&p��Ч��
                auto p_iter_in_hash_map = disks[disk_idx].table.insert({ tag,p });
                shared_partition_iterator->second.shared = &p_iter_in_hash_map->second;
                //����д�����
                int begin_idx = disks[disk_idx].enough_consecutive_space(size, shared_partition_iterator->second);
                if (begin_idx != -1) {
                    for (int b = 0;b < size;b++) {
                        disks[disk_idx].data[b + begin_idx] = tag;
                        object[obj_idx].unit[i][b] = begin_idx + b;
                    }
                    shared_partition_iterator->second.maintain_remain(-size);
                }
                else {
                    //���ܲ���bug remain�Ƿ�һ������size��
                    size_t write_idx = shared_partition_iterator->second.begin;
                    for (int b = 0;b < size;++b) {
                        while (disks[disk_idx].data[write_idx] > 0) {
                            write_idx++;//write_idx�ǵ�һ����λ��
                        }
                        disks[disk_idx].data[write_idx] = tag;
                        object[obj_idx].unit[i][b] = write_idx;
                    }
                    shared_partition_iterator->second.maintain_remain(-size);
                }
                continue;
            }
            int begin_idx3 = disks[disk_idx].find_consecutive_from_end(size);
            if (begin_idx3 != -1) {
                //��Ӳ�����ҵ�������size�����пռ�
                for (int b = 0;b < size;b++) {
                    disks[disk_idx].data[b + begin_idx3] = tag;
                    object[obj_idx].unit[i][b] = begin_idx3 + b;
                    for (auto it = disks[disk_idx].table.begin();it != disks[disk_idx].table.end();it++) {
                        if (b + begin_idx3 >= it->second.begin && b + begin_idx3 < it->second.begin + it->second.length) {
                            //�����ǰλ����ĳ��������÷�������-1
                            //it->second.remain--;
                            it->second.maintain_remain(-1);
                            break;
                        }
                    }
                }
            }
            else {
                //�˻�Ϊ��ԭʼ�ı���д��
                disks[disk_idx].remain += size;//��Ϊ��brute_write�ͱ�����forѭ����ͷ�ж�remain�ظ���size�������Ҫ����һ��size
                for (int b = 0;b < size;b++) {
                    object[obj_idx].unit[i][b] =  disks[disk_idx].brute_write(tag);
                }
            }
        }
    }
    
    //ɾ����Ӳ������idx������
    void delete_act(size_t idx, int time,const Request r[]) {
        if (d == distance(idx)) {
            //��ɾ�����ǵ�ǰ������������λ�ã������dֵ������dֵ���ֲ���
            d = update_d(time, r);
        }
        for (auto it = table.begin();it != table.end();it++) {
            if (it->second.begin <= idx && idx < it->second.begin + it->second.length) {
                //it->second��ʱ�ʹ������idx�ķ���partition����λ����дΪ-1
                data[idx] = -1;
                remain++;
                //it->second.remain++;
                it->second.maintain_remain(1);
                if (it->second.empty()) {
                    //����÷����Ѿ�Ϊ�գ���ǰ���ݼ�û����������������û�и��¹����������Ϣ����
                    for (size_t i = it->second.begin;i < it->second.begin + it->second.length;i++) {
                        data[i] = 0;//ɾ���÷���������λ��дΪ0
                    }
                    table.erase(it);//ɾ����ϣ���еĴ˷���
                }
                return;
            }
        }
        //û���ҵ�idxƥ��ķ�����˵����idx���ڷ����ֱ��д0
        data[idx] = 0;
        remain++;
    }
    //�����ڵ�ǰ��ͷλ�ý��ж�����������ֵ����ʵ���Ƿ��ȡ�ɹ������tooken����ʲô������������false
    bool read_act(int time,const Request r[]) {
        //�����ȡ��Ҫ���ĵ�tooken
        int cost = 0;
        if (consecutive_read < 7)
            cost = READ_COST[consecutive_read];
        else
            cost = READ_COST[7];
        if (cost <= tooken_) {
            //���Զ�ȡ
            head_idx = (head_idx + 1) % Len;//�ƶ���ͷ����һ��λ��
            //����dֵ
            if (d > 0)
                d--;
            else
                d = update_d(time, r);
            consecutive_read++;//���²���
            //head_last_move = true;//���²���
            tooken_ -= cost;//����ʣ��tookenֵ
            c_str.back() = 'r', c_str.push_back('#');
            return true;
        }
        else
            return false;//����tooken���㣬��ȡʧ��
    }

    //���Խ���n��pass��������ͷ�ƶ���targetλ�ã����tooken�����ʲô�������������Ƿ��ƶ��ɹ����ھ���С�ڵ���4ʱ�᳢���������ղ�������ʱ��ʹ����falseҲ���ܽ��й����ղ���
    bool pass_head(int target, int time,const Request r[]) {
        int n = distance(target);//��Ҫn��pass����
        if (n <= tooken_) {
            //sizeof(READ_NULL_THRESHOLD)/sizeof(READ_NULL_THRESHOLD[0])
            if (n > 7) {
                head_idx = target;
                consecutive_read = 0;
                tooken_ -= n;//ÿ������ 1 tooken
                if (d >= n)
                    d -= n;
                else
                    d = update_d(time, r);
                c_str.pop_back();
                for (int i = 0;i < n;i++)
                    c_str.push_back('p');
                c_str.push_back('#');
                return true;
            }
            else {
                //���ھ���Ϊ1~4��Ŀ�꣬���Կ�������
                if (consecutive_read >= READ_NULL_THRESHOLD[n - 1]) {
                    //�����������ԣ��ȼ�¼Ӳ��״̬
                    /*size_t origenal_head = head_idx;
                    int origenal_consecutive_read = consecutive_read;
                    int origenal_tooken = tooken_;
                    string origenal_c_str = c_str;*/
                    for (int i = 0;i < n;i++) {
                        //����n������������λ�þ�Ϊ�����ݣ������е�ʧ�ܵ���һ��
                        if (!read_act(time,r)) {
                            //��ʧ�ܣ�Ӳ��״̬��ԭ�����ؼ�
                            /*head_idx = origenal_head;
                            consecutive_read = origenal_consecutive_read;
                            tooken_ = origenal_tooken;
                            c_str = origenal_c_str;*/
                            return false;
                        }
                    }
                    return true;
                }
                else {
                    head_idx = target;
                    consecutive_read = 0;
                    tooken_ -= n;//ÿ������ 1 tooken
                    if (d >= n)
                        d -= n;
                    else
                        d = update_d(time, r);
                    c_str.pop_back();
                    for (int i = 0;i < n;i++)
                        c_str.push_back('p');
                    c_str.push_back('#');
                    return true;
                }
            }
        }
        else {
            return false;
        }
        
    }
    //����һ��jump��Ŀ��λ�ã������Ƿ�ɹ������tooken�����ʲô������
    bool jump(int target,int time,const Request r[]) {
        if (tooken_ == G) {
            head_idx = target;
            d = update_d(time, r);
            //head_last_move = false;
            consecutive_read = 0;
            tooken_ = 0;
            //ע�������ʱ�������Ǵ�1��ʼ��������Ҫ+1
            c_str = "j " + std::to_string(target + 1);
        }
        else
            return false;
    }
    //�õ�ǰӲ�̾����������n��ʱ��Ƭ��������÷�scores���汾1�����ø��ƹ��캯������DISK������ʱ�临�Ӷȸߣ�
    //int read_most(int time,int head,int n,const Request r[]) const {
    //    DISK temp = *this;//��ʱDISK����
    //    if (head != head_idx) {
    //        temp.head_idx = head;//ָ����ͷλ��
    //        temp.d = temp.update_d(time, r);
    //    }
    //    int scores = 0;//�÷�
    //    int current_time = time;
    //    while (current_time < time + n) {
    //        if (temp.request_pos[temp.head_idx].first != 0) {
    //            //�����ͷλ�ô��ڶ�����
    //            if (temp.read_act(current_time, r)) {
    //                //���ɹ��ˣ�����÷�
    //                scores += get_position_scores(temp.head_idx, current_time, r);
    //                continue;
    //            }
    //            else
    //                current_time++;
    //        }
    //        else {
    //            //��ǰλ�ò����ڶ�����
    //            if (temp.d == temp.Len) {
    //                return 0;
    //            }
    //            if (temp.pass_head((temp.head_idx + temp.d) % temp.Len, current_time, r)) {
    //                continue;
    //            }
    //            else {
    //                current_time++;
    //            }
    //        }
    //        temp.tooken_ = temp.G;
    //    }
    //    return scores;
    //}

    /*@brief �õ�ǰӲ�̾����������n��ʱ��Ƭ��������÷�scores���������е������㣬�Ż�ʱ�临�Ӷȣ�
    * @param time    ��ǰʱ��Ƭ����
    * @param head    ��ͷ��ʼλ�ã����뵱ǰ��ͷλ�ò�����򲻿���������
    * @param n       ��ͷ��ȡ��ʱ��Ƭ����
    * @param r       �����request����
    */
    int read_most(int time, int head, int n, const Request r[]) const {
        /*vtooken��ʣ��tooken��
        vconsecutive_read����������
        v_head����ͷλ��
        next_pos����һ���е÷ֵ�Ŀ��洢��Ԫ����
        scores���ۼƵ÷�*/
        int vtooken = tooken_, vconsecutive_read = 0, vhead = head, next_pos = head, scores =0;
        if (head == head_idx)
            vconsecutive_read = consecutive_read;
        int current_time = time;//��ǰʱ��Ƭ���
        while (current_time < time + n) {
            // ������һ��next_pos��λ��
            int vd = 0;//dֵ
            next_pos = vhead;
            while (request_id[next_pos].empty() || get_position_scores(next_pos, current_time, r) == 0) {
                next_pos = (next_pos + 1) % Len;
                vd++;
                if (vd == Len)
                    break;//vd == Len ��ʾ����������Ӳ�̵Ĵ洢��Ԫû���ҵ��е÷ֵ�λ��
            }
            //��ǰλ���е÷����󣬳��Զ�ȡ
            if (vd == 0) {
                //��ȡ���軨��tooken��
                int cost = vconsecutive_read < 7 ? READ_COST[vconsecutive_read] : READ_COST[7];
                if (cost <= vtooken) {
                    vtooken -= cost;
                    vconsecutive_read++;
                    vhead = (vhead + 1) % Len;
                    scores += get_position_scores(vhead, time, r);
                }
                else {
                    //��ȡʧ�ܣ�������һ��ʱ��Ƭ
                    vtooken = G;
                    current_time++;
                }
            }
            else if (vd == Len) {
                //Ӳ����û�ж������ˣ�ֱ�ӷ����ۼƵ÷�
                return scores;
            }
            else {
                //��������
                if (vd <= 7 && vconsecutive_read >= READ_NULL_THRESHOLD[vd - 1]) {
                    while (vhead != next_pos) {
                        int cost = vconsecutive_read < 7 ? READ_COST[vconsecutive_read] : READ_COST[7];
                        //����һ�οն�
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
                if (vtooken >= vd) {
                    vtooken -= vd;//ÿ��pass����1tooken
                    vconsecutive_read = 0;//������0
                    vhead = (vhead + vd) % Len;
                }
                else {
                    vtooken = G;
                    current_time++;
                }
            }
        }
        return scores;
    }

    //��һ��Requestѹ��DISK�У����´洢�ö�������������object������Ӳ�̵Ĳ���request_pos��request_id��d
    friend void push_Request_in(vector<DISK>& disks,const Request& r,const Object obj[MAX_OBJECT_NUM],int r_idx) {
        DISK* d0 = &disks[obj[r.object_id].replica[0]], * d1 = &disks[obj[r.object_id].replica[1]], * d2 = &disks[obj[r.object_id].replica[2]];
        for (size_t i = 0;i < r.object_size;i++) {
            d0->request_pos[obj[r.object_id].unit[0][i]] = { r.object_id,i };
            d0->request_id[obj[r.object_id].unit[0][i]].push_back(r_idx);
            d1->request_pos[obj[r.object_id].unit[1][i]] = { r.object_id,i };
            d1->request_id[obj[r.object_id].unit[1][i]].push_back(r_idx);
            d2->request_pos[obj[r.object_id].unit[2][i]] = { r.object_id,i };
            d2->request_id[obj[r.object_id].unit[2][i]].push_back(r_idx);
            //����������λ��С��dֵ���͸���dֵ
            if (d0->distance(obj[r.object_id].unit[0][i]) < d0->d)
                d0->d = d0->distance(obj[r.object_id].unit[0][i]);
            if (d1->distance(obj[r.object_id].unit[1][i]) < d1->d)
                d1->d = d1->distance(obj[r.object_id].unit[1][i]);
            if (d2->distance(obj[r.object_id].unit[2][i]) < d2->d)
                d2->d = d2->distance(obj[r.object_id].unit[2][i]);
        }
    }
    //�ڶ�ȡ�ɹ��󣬽���ȡλ�ö�Ӧ����������Ķ�Ӧblock����request_pos[target]��������Ϊ��ʼֵ��ע�Ȿ���������object��ָ��block����request_pos[]
    friend void pop_Request_out(vector<DISK>& disks, const Request& r, const Object obj[MAX_OBJECT_NUM],int time,const Request req[],int r_idx,int block_idx) {
        DISK* d0 = &disks[obj[r.object_id].replica[0]], * d1 = &disks[obj[r.object_id].replica[1]], * d2 = &disks[obj[r.object_id].replica[2]];
        //ָ��block_idx�������object��ָ��block
        d0->request_pos[obj[r.object_id].unit[0][block_idx]] = { 0,-1 };
        d1->request_pos[obj[r.object_id].unit[1][block_idx]] = { 0,-1 };
        d2->request_pos[obj[r.object_id].unit[2][block_idx]] = { 0,-1 };
        //���pop��λ��ǡ��Ϊ���������Ҫ����dֵ
        if (d0->distance(obj[r.object_id].unit[0][block_idx]) == d0->d)
            d0->d = d0->update_d(time,req);
        if (d1->distance(obj[r.object_id].unit[1][block_idx]) == d1->d)
            d1->d = d1->update_d(time, req);
        if (d2->distance(obj[r.object_id].unit[2][block_idx]) == d2->d)
            d2->d = d2->update_d(time, req);
    }
    
    //����DISK����time_flagΪtrue��Ԫ����dֵ��С��DISKԪ�صĵ�ַ�����time_flagȫΪfalse�򷵻�һ����ָ��
    friend DISK* get_nearest_disk(vector<DISK>& disks,int time,Request r[]) {
        // ���ڼ�¼�ҵ��� DISK ����ָ��
        DISK* nearest = nullptr;
        // ��һ����ֵ����ʼ�Ƚ�ֵ
        int min_d_value = INT_MAX;
        // ��������DISK���ҵ� time_flag Ϊ true �� update_d() ����ֵ��С��
        for (size_t i = 0; i < disks.size(); ++i) {
            if (disks[i].time_flag) {
                int cur_d = disks[i].d;
                if (cur_d == disks[0].Len)
                    continue;
                if (cur_d < min_d_value) {
                    min_d_value = cur_d;
                    nearest = &disks[i];
                }
            }
        }
        // ���ȫ�� time_flag Ϊ false���� nearest ���� nullptr
        return nearest;
    }
    friend DISK* get_nearest_disk_bigd(vector<DISK>& disks, int time, Request r[]) {
        // ���ڼ�¼�ҵ��� DISK ����ָ��
        DISK* nearest = nullptr;
        // ��һ����ֵ����ʼ�Ƚ�ֵ
        int min_d_value = INT_MAX;
        // ��������DISK���ҵ� time_flag Ϊ true �� update_d() ����ֵ��С��
        for (size_t i = 0; i < disks.size(); ++i) {
            if (disks[i].time_flag) {
                int cur_d = disks[i].update_big_d(time, r);
                if (cur_d == disks[0].Len)
                    continue;
                if (cur_d < min_d_value) {
                    min_d_value = cur_d;
                    nearest = &disks[i];
                }
            }
        }
        // ���ȫ�� time_flag Ϊ false���� nearest ���� nullptr
        return nearest;
    }
    //ȫ��Ӳ��ִ������jump
    //friend void special_jump(vector<DISK>& disk, int time,const Request r[], int hot_tag) {
    //    vector<int> temp(disk.size());
    //    for (int i = 0;i < disk.size();++i) {
    //        temp[i] = i;
    //    }
    //    // ����һ�������������
    //    std::mt19937 generator(std::random_device{}());
    //    std::shuffle(temp.begin(), temp.end(), generator);
    //    for (size_t i = 0;i < disk.size();++i) {
    //        int idx = temp[i];
    //        if (disk[idx].table.count(hot_tag) == 0)
    //            continue;
    //        auto range = disk[idx].table.equal_range(hot_tag);
    //        int hot_start = -1;//d��ʾ��tag������ʼλ��
    //        for (auto it = range.first;it != range.second;it++) {
    //            //�쵽��tag�ķ�����ʼ�������Ѿ�����tag�������Ӳ�̴�ͷ������⶯��
    //            if (disk[idx].distance(it->second.begin) < disk[idx].G || disk[idx].distance(it->second.begin) > disk[idx].Len - it->second.length) {
    //                hot_start = -1;
    //                break;
    //            }
    //            hot_start = it->second.begin;
    //        }
    //        if (hot_start == -1) continue;
    //        disk[idx].jump(hot_start,time,r);
    //        disk[idx].time_flag = false;
    //        break;
    //    }
    //}

    //ȫ��Ӳ��ִ������jump���汾1��
    //friend void special_jump(vector<DISK>& disk, int time, const Request r[]) {
    //    for (size_t i = 0;i < disk.size();i++) {
    //        int big_d = disk[i].update_big_d(time, r); //����Ӳ�̵Ĵ�dֵ
    //        int scores_a = disk[i].read_most(time, disk[i].head_idx, 2, r);

    //        if (big_d != disk[i].Len) {
    //            int scores_b = disk[i].read_most(time, (disk[i].head_idx + big_d) % disk[i].Len, 1, r);
    //            if (scores_b > scores_a) {
    //                if (big_d > disk[i].G) {
    //                    disk[i].jump((disk[i].head_idx + big_d) % disk[i].Len, time, r);
    //                    disk[i].time_flag = false;
    //                }
    //                else {
    //                    disk[i].pass_head((disk[i].head_idx + big_d) % disk[i].Len, time, r);
    //                }
    //            }
    //        }
    //    }
    //}
    //ȫ��Ӳ��ִ������jump���汾2��
    friend void special_jump(vector<DISK>& disks, int time, const Request r[]) {
        for (size_t i = 0;i < disks.size();i++) {
            auto itFind = disks[i].table.end();
            int scores = disks[i].read_most(time, disks[i].head_idx, CHECKED_TIME + 1, r);
            for (auto it = disks[i].table.begin();it != disks[i].table.end();it++) {
                int current_scores = disks[i].read_most(time, it->second.begin, CHECKED_TIME, r);
                if (current_scores > scores) {
                    scores = current_scores;
                    itFind = it;
                }
            }
            if (itFind != disks[i].table.end()) {
                size_t pass_target = itFind->second.begin;
                // ���� pass������ jump
                if (!disks[i].pass_head(pass_target, time, r)) {
                    disks[i].jump(pass_target, time, r);
                    disks[i].time_flag = false;
                }
            }
        }
    }

    //����һ��std::pair<int,int>�������ǰӲ�̵�idx���洢��Ԫ�����ڶ�����Ĭ�Ϸ���{0,-1}��������ڶ����󣬷��صĵ�һ��Ԫ�ر�ʾ��Ӳ�̵�idx���洢��Ԫ�洢��object�����������ڶ�������ʾ��object���������λ�ô洢��block����
    std::pair<int, int> get_request_pos(int idx)const { return request_pos[idx]; }

    //���������ʾ��idx���洢��Ԫ���ڴ��ڵĶ�������
    const vector<int>& get_request_id(int idx)const { return request_id[idx]; }

    //��ĳ��request��Ҫ�����ʱ��ĳ��ʱ��Ƭ��Ԫ�ر�ɾ�����Դ���δ��ɵ���ض����󣩵��ã�����DISK�ĳ�Ա����request_id[target]��ɾ������ֵΪr_idx��Ԫ�أ�ɾ�������request_id[target]Ϊ�ջ���Ҫ����dֵ
    friend void clear_request_id(vector<DISK>& d, const Request& r, const Object obj[MAX_OBJECT_NUM], int time,const Request req[],int r_idx, int block_idx = -1) {
        DISK* d0 = &d[obj[r.object_id].replica[0]], * d1 = &d[obj[r.object_id].replica[1]], * d2 = &d[obj[r.object_id].replica[2]];
        if (block_idx == -1) {
            //���û��ָ��block����������������Ӧ��object������blockλ�õ�req_id��ָ��Ԫ��
            for (size_t i = 0;i < r.object_size;i++) {
                //����DISK�ĳ�Ա����request_id[target]��ɾ������ֵΪr_idx��Ԫ��
                d0->request_id[obj[r.object_id].unit[0][i]].erase(
                    std::remove(d0->request_id[obj[r.object_id].unit[0][i]].begin(), d0->request_id[obj[r.object_id].unit[0][i]].end(), r_idx)
                    , d0->request_id[obj[r.object_id].unit[0][i]].end());
                d1->request_id[obj[r.object_id].unit[1][i]].erase(
                    std::remove(d1->request_id[obj[r.object_id].unit[1][i]].begin(), d1->request_id[obj[r.object_id].unit[1][i]].end(), r_idx)
                    , d1->request_id[obj[r.object_id].unit[1][i]].end());
                d2->request_id[obj[r.object_id].unit[2][i]].erase(
                    std::remove(d2->request_id[obj[r.object_id].unit[2][i]].begin(), d2->request_id[obj[r.object_id].unit[2][i]].end(), r_idx)
                    , d2->request_id[obj[r.object_id].unit[2][i]].end());
                if (d0->request_id[obj[r.object_id].unit[0][i]].empty()) {
                    //���ɾ���Ժ������Ѿ�Ϊ�գ���Ҫ��һ�����request_pos��������������Ӧ��Ӳ�̶������
                    d0->request_pos[obj[r.object_id].unit[0][i]] = { 0,-1 };
                    if (d0->distance(obj[r.object_id].unit[0][i]) == d0->d)
                        d0->d = d0->update_d(time, req);
                }
                if (d1->request_id[obj[r.object_id].unit[1][i]].empty()) {
                    d1->request_pos[obj[r.object_id].unit[1][i]] = { 0,-1 };
                    if (d1->distance(obj[r.object_id].unit[1][i]) == d1->d)
                        d1->d = d1->update_d(time, req);
                }
                if (d2->request_id[obj[r.object_id].unit[2][i]].empty()) {
                    d2->request_pos[obj[r.object_id].unit[2][i]] = { 0,-1 };
                    if (d2->distance(obj[r.object_id].unit[2][i]) == d2->d)
                        d2->d = d2->update_d(time, req);
                }  
            }
        }
        else {
            //���ָ����block������ֻ���object��Ӧ��block
            d0->request_id[obj[r.object_id].unit[0][block_idx]].clear();
            d1->request_id[obj[r.object_id].unit[1][block_idx]].clear();
            d2->request_id[obj[r.object_id].unit[2][block_idx]].clear();
            if (d0->distance(obj[r.object_id].unit[0][block_idx]) == d0->d)
                d0->d = d0->update_d(time, req);
            if (d1->distance(obj[r.object_id].unit[1][block_idx]) == d1->d)
                d1->d = d1->update_d(time, req);
            if (d2->distance(obj[r.object_id].unit[2][block_idx]) == d2->d)
                d2->d = d2->update_d(time, req);
        }
    }

    //��������Ӳ���У�ʣ��ռ�����3�������ص�������
    friend vector<int> findTop3(const vector<DISK>& disks) {
        // ���� disks.size() >= 3
        
        // �ֱ�洢ǰ��������ֵ������
        int max1 = INT_MIN, max2 = INT_MIN, max3 = INT_MIN;
        int idx1 = -1, idx2 = -1, idx3 = -1;

        for (int i = 0; i < (int)disks.size(); ++i) {
            int val = disks[i].remain;

            // �����ǰֵ�ȵ�һ��ֵ���󣬾����Ρ�������֮ǰ�ļ�¼
            if (val > max1) {
                max3 = max2;    idx3 = idx2;
                max2 = max1;    idx2 = idx1;
                max1 = val;     idx1 = i;
            }
            // ���������ǰֵ�ȵڶ���ֵ����
            else if (val > max2) {
                max3 = max2;    idx3 = idx2;
                max2 = val;     idx2 = i;
            }
            // ���������ǰֵ�ȵ�����ֵ����
            else if (val > max3) {
                max3 = val;     idx3 = i;
            }
        }
        // �����򷵻�ǰ������������
        return { idx1, idx2, idx3 };
    }
    //��������Ӳ���У���ѵ�3��д���λ�ã����ص�������
    friend vector<int> findBest3(const vector<DISK>& disks,int tag,int size) {
        vector<int> coefficient(disks.size(),0);//Ȩ��
        // coefficient[i]��ʾ��i��Ӳ���ϵ�ѡ��Ȩ�ء�Ȩ������3��Ӳ�̽���ѡ��
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
        // �ֱ�洢ǰ��������ֵ������
        int max1 = INT_MIN, max2 = INT_MIN, max3 = INT_MIN;
        int idx1 = -1, idx2 = -1, idx3 = -1;
        for (int i = 0; i < (int)disks.size(); ++i) {
            int val = coefficient[i];
            // �����ǰֵ�ȵ�һ��ֵ���󣬾����Ρ�������֮ǰ�ļ�¼
            if (val > max1) {
                max3 = max2;    idx3 = idx2;
                max2 = max1;    idx2 = idx1;
                max1 = val;     idx1 = i;
            }
            // ���������ǰֵ�ȵڶ���ֵ����
            else if (val > max2) {
                max3 = max2;    idx3 = idx2;
                max2 = val;     idx2 = i;
            }
            // ���������ǰֵ�ȵ�����ֵ����
            else if (val > max3) {
                max3 = val;     idx3 = i;
            }
        }
        return { idx1,idx2,idx3 };
    }
    //����N��Ӳ�����Ƿ�������һ��λ����tag����
    //friend void check_at_hot_tag(vector<DISK>& disks,int hot_tag,int hot_tag2,int time,const Request r[],const vector<int>& req_tag) {
    //    int modify_disk_idx = -1;//��Ҫ������Ӳ�����������У�
    //    int pass_target = -1;//��Ҫ������Ӳ�̴�ͷӦ����������Ŀ��λ�ã����У�
    //    int min_ex = INT_MAX;//��ǰ����Ӳ����ex����Сֵ
    //    
    //    for (int i = 0;i < disks.size();++i) {
    //        if (disks[i].table.count(hot_tag) > 0) {
    //            auto range = disks[i].table.equal_range(hot_tag);
    //            for (auto it = range.first;it != range.second;it++) {
    //                if (disks[i].distance(it->second.begin + it->second.length - 1) < it->second.length) {
    //                    //�������һ����ͷλ���ȷ����У�ʲô��������
    //                    return;
    //                }
    //            }//end for it
    //            //ex��ʾ��ǰ��ͷ����tag�������req_tag[tag]��ֵ�����Ӳ�̴�ͷ�����κη����exֵĬ��Ϊ0�����Ӧʹex��С�Ĵ�ͷִ��jump
    //            int ex = 0;
    //            // �����Ӳ�̵�exֵ
    //            for (auto it0 = disks[i].table.begin();it0 != disks[i].table.end();it0++) {
    //                if (disks[i].head_idx >= it0->second.begin && disks[i].head_idx < it0->second.begin + it0->second.length) {
    //                    //�����ǰӲ�̵Ĵ�ͷ��ĳ�������
    //                    ex = req_tag[it0->first - 1];
    //                    if (ex < min_ex) {
    //                        //����ҵ���С��ex���͸������²���
    //                        min_ex = ex;
    //                        modify_disk_idx = i;
    //                        //pass_target = it0->second.begin; //�����⣿
    //                    }
    //                    break;
    //                }
    //            }//end for it0
    //            if (ex < min_ex) {
    //                //����ҵ���С��ex���͸������²���
    //                min_ex = ex;
    //                modify_disk_idx = i;
    //                //pass_target = it0->second.begin; //�����⣿
    //            }
    //        }
    //    }// end for i
    //    if (modify_disk_idx != -1) {
    //        pass_target = disks[modify_disk_idx].table.find(hot_tag)->second.begin;
    //        if (!disks[modify_disk_idx].pass_head(pass_target, time, r)) {
    //            //���pass����Ŀ�꣬�ͳ��Խ���jump
    //            disks[modify_disk_idx].jump(pass_target, time, r);
    //            disks[modify_disk_idx].time_flag = false;
    //        }
    //    }
    //}



    friend void check_at_hot_tag(
        std::vector<DISK>& disks,
        int time,
        const Request r[],
        const std::vector<int>& req_tag
    )
    {
        // ==============================================
        // 1) �ҵ� sum(req_tag) �� 70% ��ֵ
        //    �ڱ�����Ҫѡ�����ɡ��ȱ�ǩ����ʹ���ǵ��������� >= 70% * sum_all
        // ==============================================
        long long sumAll = 0;
        for (auto x : req_tag) sumAll += x;
        double threshold = HOT_PERCENT * sumAll;  // 70%��Ŀ��ֵ

        // ��Ҫһ������� (freq, tag) ���� freq ��������
        std::vector<std::pair<long long, int>> freqTag; // (������, tag_id)
        freqTag.reserve(req_tag.size());
        for (int tag = 1; tag <= (int)req_tag.size(); ++tag) {
            freqTag.push_back({ req_tag[tag - 1], tag });
        }
        std::sort(freqTag.begin(), freqTag.end(),
            [](auto& a, auto& b) { return a.first > b.first; });

        // �����ʼ�ۼӣ�ֱ���ﵽ��ֵ
        std::vector<int> hotTags;
        long long accum = 0;
        for (auto& kv : freqTag) {
            if (kv.first <= 0) break;  // û�б�Ҫ�� 0 Ƶ�ȼ���
            accum += kv.first;
            hotTags.push_back(kv.second);
            if (accum >= (long long)threshold) {
                break;
            }
        }
        // ��� hotTags Ϊ�վͲ������κβ���
        if (hotTags.empty()) {
            return;
        }
        //if (hotTags.size() > 7) {
        //    hotTags.erase(hotTags.begin() + 7, hotTags.end());
        //}

        // ==============================================
        // 2) ����һЩ��������
        // ==============================================

        // (a) �жϡ���ͷ�Ƿ���ĳ��ǩ tag �ķ����ڡ�
        auto headInTagPartition = [&](int diskIdx, int tag) {
            if (disks[diskIdx].table.count(tag) == 0) return false;
            auto range = disks[diskIdx].table.equal_range(tag);
            for (auto it = range.first; it != range.second; ++it) {
                // ������ͷλ�á��ڷ��� [begin, begin+length)
                if (disks[diskIdx].head_idx >= it->second.begin
                    && disks[diskIdx].head_idx < it->second.begin + it->second.length)
                {
                    return true;
                }
            }
            return false;
            };

        // (b) �����ͷ��ǰ���ڷ�����ex��ֵ
        //     ����� shared �������� ex = req_tag[primary -1] + req_tag[shared->key -1]
        auto getEx = [&](int diskIdx) {
            int exVal = 0;
            /*for (auto it = disks[diskIdx].table.begin();
                it != disks[diskIdx].table.end();
                ++it)
            {
                if (disks[diskIdx].head_idx >= it->second.begin
                    && disks[diskIdx].head_idx < it->second.begin + it->second.length)
                {
                    if (it->second.shared == nullptr) {
                        exVal = req_tag[it->first - 1];
                    }
                    else {
                        exVal = req_tag[it->first - 1] + req_tag[it->second.shared->key - 1];
                    }
                    break;
                }
            }*/
            exVal = disks[diskIdx].read_most(time, disks[diskIdx].head_idx, 2, r);
            return exVal;
            };

        // ==============================================
        // 3) Ԥ�ȶ�Ӳ�̰� ex ֵ��С��������
        //    ���ǻ��������˳������ѡ ex ��С��Ӳ�̡����ȱ�ǩ
        // ==============================================
        std::vector<std::pair<int, int>> exList; // (exVal, diskIdx)
        exList.reserve(disks.size());
        for (int i = 0; i < (int)disks.size(); ++i) {
            exList.push_back({ getEx(i), i });
        }
        std::sort(exList.begin(), exList.end(),
            [](auto& a, auto& b) { return a.first < b.first; });

        // ���ڼ�¼�������ͷ�Ƿ��Ѿ��ڱ��κ�������������� pass/jump����
        // ȷ�������ͬһ��Ӳ�̶�β�����
        std::vector<bool> usedDisk(disks.size(), false);

        // (c) ���亯������һ����ͷȥĳ�� tag ����
        auto dispatchDiskToTag = [&](int tag) -> void {
            // ����Ѿ��д�ͷ�ڷ������ֱ�ӷ���
            bool anyInThisTag = false;
            for (int i = 0; i < (int)disks.size(); ++i) {
                if (headInTagPartition(i, tag)) {
                    anyInThisTag = true;
                    break;
                }
            }
            if (anyInThisTag) {
                return;
            }

            // ���򣬴� exList ���ҵ�һ������û�ù��� �� ��table.count(tag) > 0�� ��Ӳ��
            for (auto& kv : exList) {
                int exVal = kv.first;
                int diskIdx = kv.second;
                if (usedDisk[diskIdx]) {
                    continue; // �Ѿ����� pass/jump
                }
                if (disks[diskIdx].table.count(tag) == 0) {
                    continue; // ��Ӳ��û�иñ�ǩ����
                }

                // �ҵ�����Ӳ�� => ������ѷ������(������� read_most �߼�)
                auto range = disks[diskIdx].table.equal_range(tag);
                auto itBest = disks[diskIdx].table.end();
                int bestScore = disks[diskIdx].read_most(time,
                    disks[diskIdx].head_idx,
                    CHECKED_TIME + 1,
                    r);
                for (auto it = range.first; it != range.second; ++it) {
                    size_t pass_target = it->second.begin;
                    int sc = disks[diskIdx].read_most(time, pass_target,
                        CHECKED_TIME,
                        r);
                    if (sc > bestScore) {
                        bestScore = sc;
                        itBest = it;
                    }
                }
                // ����ҵ��˸��õķ�������ȥ�ǣ���û�ҵ�(itBest==end)��Ĭ�Ͼ��޶���
                if (itBest != disks[diskIdx].table.end()) {
                    size_t pass_target = itBest->second.begin;
                    // ���� pass������ jump
                    if (!disks[diskIdx].pass_head(pass_target, time, r)) {
                        disks[diskIdx].jump(pass_target, time, r);
                        disks[diskIdx].time_flag = false;
                    }
                }
                // ������Ӳ����ʹ�ã������ٴ� pass/jump
                usedDisk[diskIdx] = true;
                // ֻ��Ҫ���˱�ǩ����һ����ͷ����
                break;
            }
            };

        // ==============================================
        // 4) �����ȶȡ��Ӵ�С����Ϊ��Щ��ǩ��Ӳ��
        //    (hotTags �����ǰ��Ӵ�С��˳����Ϊ freqTag �����ǽ��� sorted)
        // ==============================================
        for (int tag : hotTags) {
            dispatchDiskToTag(tag);
        }

        // done
    }
    //ÿ��ʱ��Ƭ�������㣬ÿ����ͷ��Ծ����ߵ÷ֵķ������
    friend void check_scores(vector<DISK>& disks,int time,const Request r[]) {
        if (time % 3 != 1)
            return;
        int i = time / 3 % disks.size();
        auto itFind = disks[i].table.end();//ѡ������ĵ�����
        unordered_set<int> has_begin_idx;
        int scores = disks[i].read_most(time, disks[i].head_idx, CHECKED_TIME +1, r);
        for (auto it = disks[i].table.begin();it != disks[i].table.end();it++) {
            if (has_begin_idx.count(it->second.begin)) {
                continue;//��ֹ�ظ������������
            }
            has_begin_idx.insert(it->second.begin);
            int current_scores = disks[i].read_most(time, it->second.begin, CHECKED_TIME,r);
            if (current_scores > scores) {
                scores = current_scores;
                itFind = it;
            }
        }
        if (itFind != disks[i].table.end()) {
            size_t pass_target = itFind->second.begin;
            // ���� pass������ jump
            if (!disks[i].pass_head(pass_target, time, r)) {
                disks[i].jump(pass_target, time, r);
                disks[i].time_flag = false;
            }
        }
    }


    //��������Ӳ�̵Ĵ�ͷ״̬��c����ַ���
    friend void get_cstr(const vector<DISK>& disks) {
        for (size_t i = 0;i < disks.size();i++)
            std::cout << disks[i].c_str << std::endl;
    }
};

#endif // !DISK_PARTITION