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
const int READ_NULL_THRESHOLD[3] = { 3,5,6 };
//const int READ_NULL_THRESHOLD[7] = { 0,1,2,4,6,7,8 };
const int FRE_PER_SLICING = 1800;           // ʱ��Ƭ���(ʾ����ֻ��ʾ��������ʹ��)
const int RED_POSITION_THRE = 9000;         // ��ɫλ�õ�����������ֵ��ע�����*1000
const int CHECKED_TIME = 2;                 //read_most()��������ʱ��Ƭ����
const int SMALL_PARTITION_LENGTH = 40;      // ָ��С�����ĳ���
//const double SHARED_PARTITION_THRE = 1.0;   //�������д������ɾ���������ı�����ֵ
const int LOWEST_LENGTH = 13;            //�жϹ������ʱ����С����
const double HOT_PERCENT = 0.80;

// ����ṹ�壬���ڱ���ĳ�ζ��������Ϣ
typedef struct Request_ {
    int object_id;  // ��Ҫ��ȡ�Ķ���ID
    int object_size;// ��Ҫ��ȡ�Ķ����size
    bool has_read[MAX_SIZE];
    int start_time;   //���������ʱ��Ƭ���
    bool is_done;   // �������Ƿ������
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
    const int Len;                          //Ӳ�̴�С��ʹ��size()�ӿ�
    int remain;                             //Ӳ�̵�ǰʣ�����λ������ʹ��remaining()�ӿ�
    vector<int> data;                          //�洢�����ݣ�ʹ��[]��������ط���
    const int M;                               //tagֵ������������ֵ�����ڹ��캯���г�ʼ��
    const int G;                               //���β���������ĵ�tooken���������ʼ��
    int tooken_[2];                            //��ʾ��ǰӲ��ʣ���tooken
    int head_pos[2] = {0,0};                //��ʾ��ǰӲ�̵Ĵ�ͷλ�ã�ע��������0��ʼ
    int consecutive_read[2] = {0,0};           //��ʾ��ͷ�����Ĵ���
    int d[2];                                  //Ӳ��dֵ���������������λ�õľ��룩
    struct partition {
        int key;                            //�÷�����key
        int begin;                       //�÷�����Ӳ�̵���ʼλ������
        int length;                      //�÷����ĳ���
        int remain;                      //�÷���������
        partition* shared;                  //����ķ����Ķ����ַ��Ĭ��Ϊ��
        //Ĭ�Ϲ��캯���������������ɹ�ϣ���ã�ʵ�ʲ����ã�
        partition() : key(0), begin(0), length(0), remain(0), shared(nullptr) {}
        //���캯��
        partition(DISK& d, int k, int beg, int len) : key(k), begin(beg), length(len),remain(len), shared(nullptr) {
            //���������dataֵдΪ-1
            for (int i = beg;i < beg + len;i++) {
                d.data[i] = -1;
            }
        }
        //��÷��������ҵ�һ������λ�������������򷵻�begin + length
        int get_next_avail(const DISK& d) const{
            for (int i = begin;i < begin + length;i++) {
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
        //���ط���������Ȩ�
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
        int obj_id;//object���
        int block_id;//block��ţ�0��size��
        int replica;//�������
    };
    unordered_multimap<int, partition> table;  //��ϣ��ʹ��tabled()�ӿ�
    string c_str[2] = { "#","#" };             //���ڱ���Ӳ�̶������ַ���
    /*һ��1* Len�����飬��ʾӲ����ÿ���洢��Ԫ�洢�����ݡ������i���洢��Ԫ��ǰ�洢�����ݣ���save_pos[i] = {��i���洢��Ԫ�洢��obj��ţ���obj��block��ţ���obj�ĸ������}�������i���洢��Ԫ��ǰ�����ڶ�������save_pos[i] = {0,0,0}*/
    vector<UNIT> save_pos;   
    /*һ��1* Len�����飬��ʾӲ���ϵ�i���洢��Ԫ��ǰ�Ƿ���ڶ������������request_id[i] = vector<int>{(��λ�õ�����������...)}��Ԫ��request_id[i][j]��ʾ��i���洢��Ԫ��ǰ��j��������ı��*/
    vector<vector<int>> request_id; 
    bool time_flag[2] = {true,true};       //ʱ��Ƭ��־����ʾ��ǰʱ��Ƭ��Ӳ�̴�ͷ�Ƿ��ܼ����ƶ�

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
    int brute_write(int tag) {
        int write_idx = Len - 1;
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
    //���ص�ǰӲ�̵Ĵ�ͷλ��������������0��ʼ��
    int head(int i)const { return head_pos[i]; }
    int& head(int i) { return head_pos[i]; }
    //����Ӳ�̴�ͷiʣ���tookenֵ
    int tooken(int i)const { return tooken_[i]; }
    //����Ӳ��ʱ���־λ�����ݵ�ַ�����ⲿ�޸����ֵ��
    bool& flag(int i) { return time_flag[i]; }
    //���ص�ǰӲ�̵�idx���洢��Ԫ�洢�Ķ����tagֵ������λ��û�д洢�����򷵻�0
    const vector<int>& dat()const { return data; }
    //����������ͷdֵ��С���Ǹ���������������0��1��
    int get_min_d_id()const {
        if (d[0] <= d[1])
            return 0;
        else
            return 1;
    }
    //����Ӳ�̴�ͷi��dֵ
    int get_d(int i)const { return d[i]; }
    //Ӳ�̽�����һ��ʱ��Ƭ������Ӳ�̲���������ÿһ��Ӳ�̵�ʣ��tookenΪG��ˢ�²���c_str��ˢ��time_flagΪtrue��
    friend void update_timesample(vector<DISK>& disks) {
        for (int i = 0;i < disks.size();++i) {
            disks[i].tooken_[0] = disks[i].G , disks[i].tooken_[1] = disks[i].G;
            disks[i].c_str[0] = "#", disks[i].c_str[1] = "#";
            disks[i].time_flag[0] = true, disks[i].time_flag[1] = true;
        }
    }
    //��ÿ��ʱ��Ƭ��ʼʱ����dֵ
    friend void check_d_at_time_start(vector<DISK>& disks, int time, const Request r[]) {
        for (int i = 0;i < disks.size();++i) {
            if (disks[i].d[0] != disks[i].Len && disks[i].get_position_scores((disks[i].head_pos[0] + disks[i].d[0]) % disks[i].Len, time, r) == 0) {
                //�����λ�õ�����ȫ�������ˣ�����dֵ
                disks[i].d[0] = disks[i].update_d(time, r, 0);
            }
            if (disks[i].d[1] != disks[i].Len && disks[i].get_position_scores((disks[i].head_pos[1] + disks[i].d[1]) % disks[i].Len, time, r) == 0) {
                disks[i].d[1] = disks[i].update_d(time, r, 1);
            }
        }
    }

    /*
    * @brief ���ص�ǰ��ͷ��Ŀ������target�ľ��루��ͷֻ�ܵ����ƶ���
    * @param target  Ŀ��λ����������0��ʼ��
    * @param i       ��ͷ��ţ�0��1��
    * @return        ��ͷi��Ŀ��λ�õľ���
    */
    int distance(int target,int i)const { 
        int temp = 0;
        // ���Ŀ��λ���ڵ�ǰ��ͷ֮��ֱ��������
        if (target >= head_pos[i]) {
            temp = target - head_pos[i];
        }
        else {
            // ����������β���ľ��룬�ټ��� target
            temp = (Len - head_pos[i]) + target;
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
        for (int i = 0;i < request_id[idx].size();i++) {
            scores += func(time - r[request_id[idx][i]].start_time);// * (r[request_id[idx][i]].object_size + 1) / 2;
        }
        return scores;
    }
    /*
    * @brief                ����������ȡn����Ҫ���ĵ�tookenֵ
    * @param  conse         ��ʼʱ����������
    * @param  n             ������Ҫ������ȡ�Ĵ���
    * @return               intֵ��������ȡn�����ĵ�tookenֵ
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
    * @brief ͨ����������Ӳ�̵�dֵ��ʱ�临�Ӷȸߣ��������ٵ��ã��������÷�Ϊ0��λ�ã�
    * @param time    ��ǰʱ��Ƭ���
    * @param r       ��ǰ����������request[]
    * @param i       ��ǰ����Ĵ�ͷ��ţ�0��1��
    * @return        ���ظ��º�Ĵ�ͷi��dֵ
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
    //ͨ����������Ӳ�̵Ĵ�dֵ��ʱ�临�Ӷȸߣ�
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
                            disks[disk_idx].save_pos[b + beg_itFind] = { obj_idx,b,i };
                        }
                        itFind->second.maintain_remain(-size);//ע��ά������������
                    }
                    else {
                        //��ɢд��
                        int write_idx = itFind->second.begin;
                        for (int b = 0;b < size;++b) {
                            while (disks[disk_idx].data[write_idx] > 0) {
                                write_idx++;//write_idx�ǵ�һ����λ��
                            }
                            disks[disk_idx].data[write_idx] = tag;
                            object[obj_idx].unit[i][b] = write_idx;
                            disks[disk_idx].save_pos[write_idx] = { obj_idx,b,i };
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
                //        int write_idx = it->second.begin;
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
                        disks[disk_idx].save_pos[b + begin_idx] = { obj_idx,b,i };
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
                        disks[disk_idx].save_pos[b + begin_idx] = { obj_idx,b,i };
                    }
                    continue;
                }
                //û���ҵ��㹻�������ռ䴴���·��������Դ��������
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
            //����С������
            int begin_idx2 = disks[disk_idx].creat_partition(SMALL_PARTITION_LENGTH, tag, size);
            if (begin_idx2 != -1) {
                for (int b = 0;b < size;b++) {
                    disks[disk_idx].data[b + begin_idx2] = tag;
                    object[obj_idx].unit[i][b] = begin_idx2 + b;
                    disks[disk_idx].save_pos[b + begin_idx2] = { obj_idx,b,i };
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
            // ����Ѱ�ҹ������
            int max_remain = LOWEST_LENGTH;//Ѱ������remain
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
                        disks[disk_idx].save_pos[b + begin_idx] = { obj_idx,b,i };
                    }
                    shared_partition_iterator->second.maintain_remain(-size);
                }
                else {
                    //���ܲ���bug remain�Ƿ�һ������size��
                    int write_idx = shared_partition_iterator->second.begin;
                    for (int b = 0;b < size;++b) {
                        while (disks[disk_idx].data[write_idx] > 0) {
                            write_idx++;//write_idx�ǵ�һ����λ��
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
                //��Ӳ�����ҵ�������size�����пռ�
                for (int b = 0;b < size;b++) {
                    disks[disk_idx].data[b + begin_idx3] = tag;
                    object[obj_idx].unit[i][b] = begin_idx3 + b;
                    disks[disk_idx].save_pos[b + begin_idx3] = { obj_idx,b,i };
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
                    int brute_write_idx = disks[disk_idx].brute_write(tag);
                    object[obj_idx].unit[i][b] = brute_write_idx;
                    disks[disk_idx].save_pos[brute_write_idx] = { obj_idx,b,i };
                }
            }
        }
    }
    
    //ɾ����Ӳ������idx������
    void delete_act(int idx, int time,const Request r[]) {
        if (d[0] == distance(idx,0)) {
            //��ɾ�����ǵ�ǰ������������λ�ã������dֵ������dֵ���ֲ���
            d[0] = update_d(time, r, 0);
        }
        if (d[1] == distance(idx, 1))
            d[1] = update_d(time, r, 1);
        for (auto it = table.begin();it != table.end();it++) {
            if (it->second.begin <= idx && idx < it->second.begin + it->second.length) {
                //it->second��ʱ�ʹ������idx�ķ���partition����λ����дΪ-1
                data[idx] = -1;
                remain++;
                //it->second.remain++;
                it->second.maintain_remain(1);
                if (it->second.empty()) {
                    //����÷����Ѿ�Ϊ�գ���ǰ���ݼ�û����������������û�и��¹����������Ϣ����
                    for (int i = it->second.begin;i < it->second.begin + it->second.length;i++) {
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
        save_pos[idx] = { 0,0,0 };
    }

    /*
    * @brief                �ж�������λ�õ���һ���ж������λ���Ƿ���
    * @param  head_pos      ��ͷλ����������0��ʼ��
    * @param  consecutive   ��ʼʱ����������
    * @param  tok           ��ʼʱ��ʣ��tooken
    * @return               ����ֵ�������������ͷ���true�����pass����ͷ���false
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

        //�ȼ���ִ������d�ζ�ȡ
        int head_pos_next = (head_pos + dx) % Len;//��ͷ����λ��
        int consecutive_a = consecutive + dx;//�������Ժ����������
        int tooken_a = tok - read_cost(consecutive, dx);//��������һ���ж�����λ��ʣ��tooken
        //read_nums_a��ʾ����ִ�������������ܶ�����������
        int read_nums_a = read_successufl_nums(head_pos_next, consecutive_a, tooken_a);
        //�ټ���ִ��pass
        int tooken_b = tok - dx;
        //read_nums_b��ʾ����ִ��pass�������ܶ�����������
        int read_nums_b = read_successufl_nums(head_pos_next, 0, tooken_b);
        return read_nums_a >= read_nums_b;
    }
    /*
    * @brief                �жϵ�ǰ��ͷ״̬����ȡ��������
    * @param  head_pos      ��ͷλ����������0��ʼ��
    * @param  consecutive   ��ʼʱ����������
    * @param  tok           ��ʼʱ��ʣ��tooken
    * @return               intֵ����ʾ��ͷ��������ܶ�ȡ����Ч������
    */
    int read_successufl_nums(int head_pos, int consecutive, int tok)const {
        int read_nums = 0;//��ȡ�ɹ���������ʼ��Ϊ0
        while (true) {
            int dx = 0;
            while (request_id[(dx + head_pos) % Len].empty()) {
                dx++;
                if (dx == Len)
                    return read_nums;
            }
            if (dx == 0) {
                //ִ��һ�ζ�
                if (tok >= read_cost(consecutive, 1)) {
                    tok -= read_cost(consecutive, 1);
                    consecutive++;
                    head_pos = (head_pos + 1) % Len;
                    read_nums++;
                }
                //��ʧ�ܣ����ص�ǰ�Ծ������ĸ���
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
                    //ִ��d�������ն�
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
                    //ִ��d��pass����
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
    * @brief                �жϵ�ǰ��ͷ״̬����ȡ�󣬺ľ�tookenʱ�߹��ľ���
    * @param  head_pos      ��ͷλ����������0��ʼ��
    * @param  consecutive   ��ʼʱ����������
    * @return               intֵ����ʾ��ͷ������۶�ȡ��Ϻ��ƶ��ľ���
    */
    int read_farest_distance(int head_pos, int consecutive)const {
        int origenal_head_pos = head_pos;//��ʼ��ͷλ��
        int tok = G;//ʣ��tooken��ʼ��ΪG
        //��������lambda����
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
                    //��ǰ��ͷ����Ӳ��û�ж����󣬷���
                    return cal_dis(origenal_head_pos,head_pos,Len);
            }
            if (dx == 0) {
                //ִ��һ�ζ�
                if (tok >= read_cost(consecutive, 1)) {
                    tok -= read_cost(consecutive, 1);
                    consecutive++;
                    head_pos = (head_pos + 1) % Len;
                }
                //��ʧ�ܣ�����
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
                    //ִ��d�������ն�
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
                    //ִ��d��pass����
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
    //�ж�������ͷ�Ƿ��г�ͻ
    bool check_conflict(int time,const Request r[]) {
        int front_head_id;//ǰ���Ĵ�ͷ���
        int back_head_id;//�󷽵Ĵ�ͷ���
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
        //�󷽴�ͷ��ǰ����ͷλ�õľ���
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
                    continue;//��������ǰ��ͷ��ǰ�ߵ�����Ȼ��ͻ
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

    //�����ڵ�ǰ��ͷi��λ�ý��ж�����������ֵ����ʵ���Ƿ��ȡ�ɹ������tooken����ʲô������������false
    bool read_act(int time,const Request r[],int i) {
        //�����ȡ��Ҫ���ĵ�tooken
        int cost = 0;
        if (consecutive_read[i] < 7)
            cost = READ_COST[consecutive_read[i]];
        else
            cost = READ_COST[7];
        if (cost <= tooken_[i]) {
            //���Զ�ȡ
            head_pos[i] = (head_pos[i] + 1) % Len;//�ƶ���ͷ����һ��λ��
            //����dֵ
            if (d[i] > 0)
                d[i]--;
            else {
                d[i] = update_d(time, r, i);
                if (d[i] == Len - 1)
                    d[i] = Len;//��Ϊ����û�и���req_pos���Ҵ�ͷ�����1��������Ҫ����������
            }
                
            //��Ҫ����d[1-i]ֵ��
            if (d[1 - i] == distance((head_pos[i] - 1) % Len, 1 - i)) {
                if (d[i] < Len)
                    d[1 - i] = d[i] + distance(head_pos[i], 1 - i);
                else
                    d[1 - i] = Len;
            }
            consecutive_read[i]++;//���²���
            tooken_[i] -= cost;//����ʣ��tookenֵ
            c_str[i].back() = 'r', c_str[i].push_back('#');
            return true;
        }
        else
            return false;//����tooken���㣬��ȡʧ��
    }

    /*
    * @brief         ���Խ���n��pass��������ͷ�ƶ���targetλ�ã������Ƿ��ƶ��ɹ���
    * @param target  Ҫpass����Ŀ��洢��Ԫ��������0��ʼ��
    * @param time    ��ǰʱ��Ƭ���
    * @param r       request[]����
    * @param head_id �����Ĵ�ͷ������0��1��
    */
    bool pass_head(int target, int time,const Request r[],int head_id) {
        //���Ŀ��λ�þ��Ǵ�ͷ����ʲô������ֱ�ӷ���true���������Ӧ�����⣩
        if (target == head_pos[head_id])
            return true;
        int dx = distance(target, head_id);//��ͷ��Ŀ��λ�õľ���
        //���Ŀ��������G��ʲô������ֱ�ӷ���false���������Ӧ��jump��
        if (dx > G)
            return false;
        if (dx == d[head_id]) {
            bool a = read_cost(consecutive_read[head_id], dx + 1) <= tooken_[head_id];
            bool b = dx + READ_COST[0] <= tooken_[head_id];
            if (a && b) {
                //�ж�������ȡ���Ƿ���
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
                    c_str[head_id].pop_back();//ȥ��β���ľ���
                    for (int i = 0;i < dx;++i) {
                        c_str[head_id].push_back('p');//����dx��p
                    }
                    c_str[head_id].push_back('#');//β���ټ��Ͼ���
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
                //���� < [��������]ʱ���������������ľ�tooken���pass
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
                    int pass_nums = std::min(tooken_[head_id], dx);//pass�Ĵ���
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
            // Ҫpass��Ŀ��λ�ò���������Ķ����󣬲����������ղ��ԣ�ֱ�����pass��ȥ
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
    //����һ��jump��Ŀ��λ�ã������Ƿ�ɹ������tooken�����ʲô������
    bool jump(int target,int time,const Request r[],int i) {
        if (tooken_[i] == G) {
            head_pos[i] = target;
            d[i] = update_d(time, r, i);
            consecutive_read[i] = 0;
            tooken_[i] = 0;
            //ע�������ʱ�������Ǵ�1��ʼ��������Ҫ+1
            c_str[i] = "j " + std::to_string(target + 1);
            return true;
        }
        else
            return false;
    }

    /*@brief �õ�ǰӲ�̾����������n��ʱ��Ƭ��������÷�scores���������е������㣬�Ż�ʱ�临�Ӷȣ�
    * @param time    ��ǰʱ��Ƭ����
    * @param head    ��ͷ��ʼλ�ã����뵱ǰ��ͷλ�ò�����򲻿���������
    * @param n       ��ͷ��ȡ��ʱ��Ƭ����
    * @param disk_id �����Ĵ�ͷ��ţ�0��1��
    * @param r       �����request����
    * @return        n��ʱ��Ƭ�ô�ͷ�ĵ÷�
    */
    int read_most(int time, int head, int n, int disk_id, const Request r[]) const {
        /*vtooken��ʣ��tooken��
        vconsecutive_read����������
        v_head����ͷλ��
        next_pos����һ���е÷ֵ�Ŀ��洢��Ԫ����
        scores���ۼƵ÷�*/
        int vtooken = tooken_[disk_id], vconsecutive_read = 0, vhead = head, next_pos = head, scores = 0;
        if (head == head_pos[0]) {
            vconsecutive_read = consecutive_read[0];
        }
        else if (head == head_pos[1]) {
            vconsecutive_read = consecutive_read[1];
        }
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
                if (do_we_read_null(vhead, vconsecutive_read, vtooken)) {
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
                else {
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
        }
        return scores;
    }
    //���������洢��Ԫ������
    void swap_two_pos(int idx0, int idx1,Object obj[]) {
        request_id[idx0].swap(request_id[idx1]);
        //����dֵ��Ҫ����
        d[0] = update_d(0), d[1] = update_d(1);
        //����λ�ö������ݣ�ע���data�Լ�save_pos�Ľ���˳���Ƚ���object[].unit[][]
        if (data[idx0] > 0 && data[idx1] > 0) {
            // ����obj[x0].unit[a0][b0]��obj[x1].unit[a1][b1]����Щ������ô��
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
        std::swap(data[idx0], data[idx1]);//����ֻ���Ƿ����ڽ��������漰0��-1������
        std::swap(save_pos[idx0], save_pos[idx1]);
    }
    //ִ�����K�ν�������
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
            int left = begin, right = begin + length - 1;   //˫ָ��
            int flag1 = 0, flag2 = 0;        //�ֱ�������ָ���Ƿ����м䣬���������Ϊ1����ôһ�ν����������������һ����������
            int fro, bac;                    //��¼ͳ�ƶ��õ�ǰ�󲿷ֽ��
            int num_1 = 0, num_2 = 0;        //��¼����
            int cnt_1 = 0, cnt_2 = 0;        //��¼��ź�
            int y1 = 0, y2 = 0;              //���ڱ���Ƿ��ҵ�
            int cnt = 0;                     //��¼�Ѿ�ʹ�õĽ�������
            int f_num, b_num;
            //vector<std::pair<int, int>> ans;

            //ͳ�ƣ������ж�˭��ǰ�벿�֣�˭�ں�벿��
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

            //����������������1��flag�ж�  2������Kֵ�ж�
            //��ʵ��˵������һ��
            while ((flag1 == 0 || flag2 == 0) && cnt < k) {
                //������ҷ���������
                if (flag1 == 0) {                //��һ��ѭ���������Ǻ��ֵ��
                    y1 = 0;
                    for (int f = left; f < begin + f_num; f++) {
                        if (data[f] == bac) {
                            left = f;
                            y1 = 1;
                            break;                     //�ҵ����������ľ��˳�
                        }
                    }
                    if (y1 == 0) {
                        flag1 += 1;                        //û�ҵ����ʵģ���˵�������ˣ���ô�ʹ��״�����һ
                        left = 0;                          //���ָ�뷵��0
                    }
                }
                if (flag1 == 1) {             //�ڶ���ѭ�����ҿյ�
                    y1 = 0;
                    for (int f = left; f < begin + f_num; f++) {
                        if (data[f] <= 0) {
                            left = f;
                            y1 = 1;
                            break;                     //�ҵ����������ľ��˳�
                        }
                    }
                    if (y1 == 0) {
                        flag1 += 1;                       //û�ҵ����ʵģ���˵�������ˣ���ô�ʹ��״�����һ
                        break;                            //�ڶ��δ��ף�ֱ���˳�ѭ��
                    }
                }

                //���ұߵ�
                if (flag2 == 0) {                //��һ��ѭ���������Ǻ��ֵ��
                    y2 = 0;
                    for (int b = right; b > b_num + begin; b--) {
                        if (data[b] == fro) {
                            right = b;
                            y2 = 1;
                            break;               //�ҵ����������ľ��˳�
                        }
                    }
                    if (y2 == 0) {
                        flag2 += 1;                  //û�ҵ����ʵģ���˵�������ˣ���ô�ʹ��״�����һ
                        right = 0;                    //���ָ�뷵��0
                    }
                }
                if (flag2 == 1) {
                    y2 = 0;
                    for (int b = right; b > b_num + begin; b--) {
                        if (data[b] <= 0) {       //�ڶ���ѭ�����ҿյ�
                            right = b;
                            y2 = 1;
                            break;               //�ҵ����������ľ��˳�
                        }
                    }
                    if (y2 == 0) {
                        flag2 += 1;                  //û�ҵ����ʵģ���˵�������ˣ���ô�ʹ��״�����һ
                        break;                       //�ڶ��δ��ף�ֱ���˳�ѭ��
                    }
                }

                //1�����Ҷ��ҵ��յģ��Ǿ�ֱ���˳�
                if (flag1 == 1 && flag2 == 1) {
                    break;
                }
                //2�����ҳɹ��ҵ�������
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

    //��һ��Requestѹ��DISK�У����´洢�ö�������������object������Ӳ�̵Ĳ���request_pos��request_id��d
    friend void push_Request_in(vector<DISK>& disks,const Request& r,const Object obj[MAX_OBJECT_NUM],int r_idx) {
        DISK* d0 = &disks[obj[r.object_id].replica[0]], * d1 = &disks[obj[r.object_id].replica[1]], * d2 = &disks[obj[r.object_id].replica[2]];
        for (int i = 0;i < r.object_size;i++) {
            d0->request_id[obj[r.object_id].unit[0][i]].push_back(r_idx);
            d1->request_id[obj[r.object_id].unit[1][i]].push_back(r_idx);
            d2->request_id[obj[r.object_id].unit[2][i]].push_back(r_idx);
            //����������λ��С��dֵ���͸���dֵ
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
    //�ڶ�ȡ�ɹ��󣬽���ȡλ�ö�Ӧ����������Ķ�Ӧblock����request_pos[target]��request_id��������Ϊ��ʼֵ
    friend void pop_Request_out(vector<DISK>& disks, DISK* chosed_disk, const Object obj[],int time,const Request req[],int tar) {
        int r_idx = chosed_disk->request_id[tar][0];
        int block_idx = chosed_disk->save_pos[tar].block_id;
        DISK* d0 = &disks[obj[req[r_idx].object_id].replica[0]], * d1 = &disks[obj[req[r_idx].object_id].replica[1]], * d2 = &disks[obj[req[r_idx].object_id].replica[2]];
        //ָ��block_idx�������object��ָ��block
        d0->request_id[obj[req[r_idx].object_id].unit[0][block_idx]].clear();
        d1->request_id[obj[req[r_idx].object_id].unit[1][block_idx]].clear();
        d2->request_id[obj[req[r_idx].object_id].unit[2][block_idx]].clear();
        //���pop��λ��ǡ��Ϊ���������Ҫ����dֵ
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
    //һ����ʱ����ṹ�壬��������
    struct choose {
        DISK* disk;//DISK����ĵ�ַ
        int head_id;//ѡ��Ĵ�ͷ��ţ�0��1��
    };
    //����DISK����time_flagΪtrue��Ԫ����dֵ��С��DISKԪ�صĵ�ַ�����time_flagȫΪfalse�򷵻�һ����ָ��
    friend choose get_nearest_disk(vector<DISK>& disks,int time,Request r[]) {
        // ���ڼ�¼�ҵ��� DISK ����ָ��
        DISK* nearest = nullptr;
        int head_id = -1;
        // ��һ����ֵ����ʼ�Ƚ�ֵ
        int min_d_value = INT_MAX;
        // ��������DISK���ҵ� time_flag Ϊ true �� update_d() ����ֵ��С��
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
        // ���ȫ�� time_flag Ϊ false���� nearest ���� nullptr
        return { nearest ,head_id };
    }

    //����һ��std::pair<int,int>�������ǰӲ�̵�idx���洢��Ԫû�д洢��Ϣ��Ĭ�Ϸ���{0,0}������洢����Ϣ�����صĵ�һ��Ԫ�ر�ʾ��Ӳ�̵�idx���洢��Ԫ�洢��object�����������ڶ�������ʾ��object���������λ�ô洢��block����
    std::pair<int, int> get_save_pos(int idx)const { 
        return { save_pos[idx].obj_id, save_pos[idx].block_id }; 
    }
    const vector<UNIT>& get_save_pos()const { return save_pos; }
    //���������ʾ��idx���洢��Ԫ���ڴ��ڵĶ�������
    const vector<int>& get_request_id(int idx)const { return request_id[idx]; }
    const vector<vector<int>>& get_request_id()const { return request_id; }

    //��ĳ��request��Ҫ�����ʱ��ĳ��ʱ��Ƭ��Ԫ�ر�ɾ�����Դ���δ��ɵ���ض����󣬻�ĳ��ʱ��Ƭĳ�������ϱ���æ�����ã�����DISK�ĳ�Ա����request_id[target]��ɾ������ֵΪr_idx��Ԫ�أ�ɾ�������request_id[target]Ϊ�ջ���Ҫ����dֵ
    friend void clear_request_id(vector<DISK>& d, const Object obj[MAX_OBJECT_NUM], int time,const Request req[],int r_idx) {
        DISK* d0 = &d[obj[req[r_idx].object_id].replica[0]], * d1 = &d[obj[req[r_idx].object_id].replica[1]], * d2 = &d[obj[req[r_idx].object_id].replica[2]];
        //���û��ָ��block����������������Ӧ��object������blockλ�õ�req_id��ָ��Ԫ��
        for (int i = 0;i < req[r_idx].object_size;i++) {
            //����DISK�ĳ�Ա����request_id[target]��ɾ������ֵΪr_idx��Ԫ��
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
                //���ɾ���Ժ������Ѿ�Ϊ�գ���Ҫ��һ������dֵ��������������Ӧ��Ӳ�̶������
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
        auto headInTagPartition = [&](int diskIdx, int tag, int head_id) {
            if (disks[diskIdx].table.count(tag) == 0) return false;
            auto range = disks[diskIdx].table.equal_range(tag);
            for (auto it = range.first; it != range.second; ++it) {
                // ������ͷλ�á��ڷ��� [begin, begin+length)
                if (disks[diskIdx].head_pos[head_id] >= it->second.begin
                    && disks[diskIdx].head_pos[head_id] < it->second.begin + it->second.length)
                {
                    return true;
                }
            }
            return false;
            };

        // (b) �����ͷ��ǰ���ڷ�����ex��ֵ
        //     ����� shared �������� ex = req_tag[primary -1] + req_tag[shared->key -1]
        auto getEx = [&](int diskIdx,int head_id) {
            int exVal = 0;
            exVal = disks[diskIdx].read_most(time, disks[diskIdx].head_pos[head_id], 2, head_id, r);
            return exVal;
            };

        // ==============================================
        // 3) Ԥ�ȶ�Ӳ�̰� ex ֵ��С��������
        //    ���ǻ��������˳������ѡ ex ��С��Ӳ�̡����ȱ�ǩ
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

        // ���ڼ�¼�������ͷ�Ƿ��Ѿ��ڱ��κ�������������� pass/jump����
        // ȷ�������ͬһ��Ӳ�̶�β�����
        vector<vector<bool>> usedDisk(disks.size(), { false,false });

        // (c) ���亯������һ����ͷȥĳ�� tag ����
        auto dispatchDiskToTag = [&](int tag) -> void {
            // ����Ѿ��д�ͷ�ڷ������ֱ�ӷ���
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

            // ���򣬴� exList ���ҵ�һ������û�ù��� �� ��table.count(tag) > 0�� ��Ӳ��
            for (auto& kv : exList) {
                int exVal = kv.exVal;
                int diskIdx = kv.diskIdx;
                int head_id = kv.head_id;
                if (usedDisk[diskIdx][head_id]) {
                    continue; // �Ѿ����� pass/jump
                }
                if (disks[diskIdx].table.count(tag) == 0) {
                    continue; // ��Ӳ��û�иñ�ǩ����
                }

                // �ҵ�����Ӳ�� => ������ѷ������(������� read_most �߼�)
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
                // ����ҵ��˸��õķ�������ȥ�ǣ���û�ҵ�(itBest==end)��Ĭ�Ͼ��޶���
                if (itBest != disks[diskIdx].table.end()) {
                    int pass_target = itBest->second.begin;
                    while (disks[diskIdx].get_save_pos(pass_target).first == 0) {
                        pass_target = (pass_target + 1)% disks[diskIdx].Len;
                        if (pass_target == itBest->second.begin)
                            break;
                    }
                    // ���� pass������ jump
                    if (!disks[diskIdx].pass_head(pass_target, time, r, head_id)) {
                        disks[diskIdx].jump(pass_target, time, r, head_id);
                        disks[diskIdx].time_flag[head_id] = false;
                    }
                }
                // ������Ӳ����ʹ�ã������ٴ� pass/jump
                usedDisk[diskIdx][head_id] = true;
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

    //��������Ӳ�̵Ĵ�ͷ״̬��c����ַ���
    friend void get_cstr(const vector<DISK>& disks) {
        for (int i = 0;i < disks.size();i++) {
            std::cout << disks[i].c_str[0] << std::endl;
            std::cout << disks[i].c_str[1] << std::endl;
        }
            
    }
};

#endif // !DISK_PARTITION