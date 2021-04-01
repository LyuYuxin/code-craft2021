#define _CRT_SECURE_NO_WARNINGS
//#pragma GCC optimize(2)
//#pragma GCC optimize(3,"Ofast","inline")
//#define SUBMIT//是否提交
//#define SIMILAR_NODE
//#define MIGRATE//原始迁移
#define EARLY_STOPPING//迁移时短路判断 
//#define DO_NODE_BALANCE
//#define DEBUG
//#define BUY_SERVER_GREEDY
#include<cstdlib>
#include<cstdio>
#include<float.h>
#include <iostream>
#include<string>
#include<vector>
#include<unordered_map>
#include<map>
#include<cmath>
#include<time.h>
#include<cassert>
#include<algorithm>
#include<queue>
enum E_Deploy_status{
	dep_No = -1, dep_A, dep_B, dep_Both
};

//可调参数列表
//最大迁出服务器比例
const float MAX_MIGRATE_OUT_SERVER_RATIO = 0.5f;

//购买服务器参数
const float TOTAL_COST_RATIO =0.65f; 
const float BUY_SERVER_MAINTAINANCE_COST_RATIO = 3.0f;
const float BUY_SERVER_PURCHASE_COST_RATIO = 40.0f;
const float cpu_mem_proportion_ratio = 100.0f;

const int RANDOM_MAX = 10;
const int RANDOM_MIN = 0;
const int CHOSE_BEST_RATIO = 10;//选择最适合的服务器的概率为0.1 * 此参数

using namespace std;

//接下来是输入
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

//server信息
typedef struct  S_Server {
	int32_t cpu_num;
	int32_t memory_num;
	int32_t purchase_cost;
	int32_t maintenance_cost;
	string type;
	S_Server() :cpu_num(0), memory_num(0), purchase_cost(0), maintenance_cost(0){}
} S_Server;

//virtual machine信息
typedef struct S_VM{
	int32_t cpu_num;
	int32_t half_cpu_num;
	int32_t memory_num;
	int32_t half_mem_num;
	bool is_double_node;
	string type;
	S_VM() :cpu_num(0), half_cpu_num(0), memory_num(0), half_mem_num(0), is_double_node(false), type("") {}
}S_VM;

//一个请求信息
typedef struct S_Request {
	bool is_add;
	string vm_type;
	uint32_t vm_id;
	S_Request():is_add(false),vm_type(""), vm_id(0) {}
}S_Request;

//一天的请求信息
typedef struct  {
	uint32_t request_num;
	vector<S_Request> day_request;
	vector<int> delete_op_idxs;//用于记录删除操作的索引
}S_DayRequest;

//操作及输出
//********************************************************************************************************

//单个虚拟机迁移信息
typedef struct {
	uint32_t vm_id;//虚拟机id
	uint32_t server_seq;//目标服务器seq
	bool is_double_node;//是否双节点部署
	string node_name;//如果单节点部署，部署的节点名称
}S_MigrationInfo;



//服务器节点信息
class C_node{
public:
	C_node(const S_Server& s):remaining_cpu_num(s.cpu_num / 2),
		remaining_memory_num(s.memory_num / 2),
		cpu_used_rate(0), mem_used_rate(0)
	{}
	C_node(const S_VM& vm, bool is_half=false){
		if(!is_half){
			remaining_memory_num = vm.memory_num;
			remaining_cpu_num = vm.cpu_num;
			cpu_used_rate = 0.0f;
			mem_used_rate = 0.0f;
		}
		else{
			remaining_memory_num = vm.memory_num /  2;
			remaining_cpu_num = vm.cpu_num / 2;
			cpu_used_rate = 0.0f;
			mem_used_rate = 0.0f;
		}
	}
	int32_t remaining_cpu_num;
	int32_t remaining_memory_num;
	float cpu_used_rate;
	float mem_used_rate;
	bool operator<(C_node& node) {
		return remaining_cpu_num + remaining_memory_num < node.remaining_cpu_num + node.remaining_memory_num;
	}
	
	bool operator=(const C_node&b)const{
		if((remaining_cpu_num == b.remaining_cpu_num) && (remaining_memory_num == b.remaining_memory_num)){
			return true;
		}
		return false;
	}
};

template<class _Ty>
struct less_BoughtServer
{
	bool operator()(const _Ty& p_Left, const _Ty& p_Right) const
	{
		int32_t l_resources = p_Left->get_double_node_avail_resources();
		int32_t r_resources = p_Right->get_double_node_avail_resources();

		//二级比较，如果值相等，就进一步比较地址值，保证不出现重复key
		if (l_resources != r_resources) {
			return l_resources < r_resources;
		}
		return p_Left < p_Right;
	}
};


template<class _Ty>
struct less_SingleNode
{
	bool operator()(const _Ty& p_Left, const _Ty& p_Right) const
	{
		//同上仿函数
		if (p_Left->remaining_cpu_num + p_Left->remaining_memory_num != p_Right->remaining_cpu_num + p_Right->remaining_memory_num) {
			return p_Left->remaining_cpu_num + p_Left->remaining_memory_num < p_Right->remaining_cpu_num + p_Right->remaining_memory_num;
		}
		return p_Left < p_Right;

	}
};




class C_BoughtServer {
public:
	C_BoughtServer(const S_Server& server) :server_info(server), total_resource_used_rate(0)
	{
		seq = purchase_seq_num++;
		A = new(C_node)(server);
		B = new(C_node)(server);
	}
	
	//用于构造假节点
	C_BoughtServer(const S_VM& vm){
		A = new(C_node)(vm, true);
		B = new(C_node)(vm ,true);
	}

	~C_BoughtServer()
	{
		delete A;
		delete B;
	}

	/*//迁移用
	inline void deployVM(const S_VM& vm, int vm_id, S_MigrationInfo& one_migration_info) {
		S_DeploymentInfo one_deployment_info;
		assert(status != dep_No);
		switch (status) {
		case dep_Both:
			A->remaining_cpu_num -= vm.half_cpu_num;
			A->remaining_memory_num -= vm.half_mem_num;
			B->remaining_memory_num -= vm.half_mem_num;
			B->remaining_cpu_num -= vm.half_cpu_num;
			break;
		case dep_A:
			one_deployment_info.node_name = "A";
			one_migration_info.node_name = "A";
			A->remaining_cpu_num -= vm.cpu_num;
			A->remaining_memory_num -= vm.memory_num;
			break;
		case dep_B:
			one_deployment_info.node_name = "B";
			one_migration_info.node_name = "B";
			B->remaining_memory_num -= vm.memory_num;
			B->remaining_cpu_num -= vm.cpu_num;
		default:break;
		}
		assert(A->remaining_cpu_num >= 0);
		assert(A->remaining_memory_num >= 0);
		assert(B->remaining_cpu_num >= 0);
		assert(B->remaining_memory_num >= 0);

		//输出用迁移记录
		one_migration_info.vm_id = vm_id;
		one_migration_info.server_seq = seq;
		one_migration_info.is_double_node = vm.is_double_node;

		//更新全局虚拟机部署表
		GlobalVMDeployTable.insert(pair<uint32_t, uint32_t>(vm_id, seq));
		//维护服务器部署信息表

		one_deployment_info.is_double_node = vm.is_double_node;
		one_deployment_info.server_seq = seq;
		one_deployment_info.vm_type = vm.type;
		deployed_vms.insert(pair<uint32_t, S_DeploymentInfo>(vm_id, one_deployment_info));
	}
*/

	static float cal_node_similarity(int m1, int c1, int m2, int c2);

	bool operator<(C_BoughtServer& bought_server);

	//返回A、B节点中可用cpu和内存较小值之和
	int32_t get_double_node_avail_resources()const;

	float cal_total_resource_used_rate() {
		float cpu_used_rate = ((float)(server_info.cpu_num - A->remaining_cpu_num - B->remaining_cpu_num)) / server_info.cpu_num;
		float mem_used_rate = ((float)(server_info.memory_num - A->remaining_memory_num - B->remaining_memory_num)) / server_info.memory_num;
		float total_rate = cpu_used_rate + mem_used_rate;
		total_resource_used_rate = total_rate;
		return total_rate;
	}

	S_Server server_info;//此服务器的基本参数
	C_node* A, * B;//两个节点的信息
	uint32_t seq;//此服务器序列号，唯一标识
	float total_resource_used_rate;//cpu和mem总使用率.针对服务器
	static int32_t purchase_seq_num;//静态成员，用于存储当前已购买服务器总数，也用于给新买的服务器赋予序列号
};
int32_t C_BoughtServer::purchase_seq_num = 0;//初始时，没有任何服务器，序列号从0开始，第一台服务器序列号为0

inline  float C_BoughtServer::cal_node_similarity(int m1, int c1, int m2, int c2) {
		return float(pow((m1 - m2), 2) + pow((c1 - c2), 2));
	}

bool C_BoughtServer::operator<(C_BoughtServer& bought_server) {
		return this->get_double_node_avail_resources() < bought_server.get_double_node_avail_resources();
	}

inline int32_t C_BoughtServer:: get_double_node_avail_resources()const {
		int32_t remaining_cpu = A->remaining_cpu_num > B->remaining_cpu_num ? B->remaining_cpu_num : A->remaining_cpu_num;
		int32_t remaining_mem = A->remaining_memory_num > B->remaining_memory_num ? B->remaining_memory_num : A->remaining_memory_num;
		return remaining_cpu + remaining_mem;
	}



//单个虚拟机部署信息
typedef struct {
	uint32_t server_seq;//部署的服务器seq
	C_BoughtServer* server;//部署的server
	string node_name; //部署的节点名称
	const S_VM* vm_info;//部署的虚拟机参数
}S_DeploymentInfo;

//一天的决策信息, 用于记录所有操作并输出
typedef struct DayTotalDecisionInfo{
	int32_t Q;//扩容购买的服务器种类数
	map<string, vector<uint32_t>> server_bought_kind;//购买的不同服务器的个数

	int32_t W;//需要迁移的虚拟机的数量
	vector<S_MigrationInfo> VM_migrate_vm_record;//虚拟机迁移记录

	vector<S_DeploymentInfo> request_deployment_info;//按序对请求的处理记录
	DayTotalDecisionInfo(){
		Q = 0;
		W = 0;
	};
}S_DayTotalDecisionInfo;

vector<C_BoughtServer*> My_servers;//已购买的服务器列表
map<C_BoughtServer*, uint32_t, less_BoughtServer<C_BoughtServer *> > DoubleNodeTable;//将所有服务器组织成一个双节点表，值为服务器seq
map<C_node*, uint32_t, less_SingleNode<C_node*> > SingleNodeTable;//将所有服务器的节点组织成一个单节点表， 值为服务器seq
unordered_map<uint32_t, S_DeploymentInfo> GlobalVMDeployTable;//全局虚拟机部署表，记录虚拟机id和相应的部署信息
unordered_map<uint32_t, uint32_t> GlobalServerSeq2IdMapTable;//全局服务器id表，用于从购买序列号到输出id的映射
unordered_map<uint32_t,  S_VM> GlobalVMRequestInfo;


//全局变量定义
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

int32_t N;//可以采购的服务器类型
int32_t M;//虚拟机类型数量
int32_t T;//总共T天
int32_t K;//先给K天

vector<S_Server> ServerList;//用于存储所有可买的服务器种类信息
unordered_map<string, S_VM> VMList;//用于存储所有虚拟机种类信息
queue<S_DayRequest> Requests;//用于存储用户所有的请求信息

//utils
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

 void write_standard_output(const S_DayTotalDecisionInfo& day_decision);

void read_one_request();

//生成每一天购买服务器的id，以及序列号跟id之间的映射表
void server_seq_to_id(const S_DayTotalDecisionInfo& day_decision) {
	static uint32_t idx = 0;
	map<string, vector<uint32_t>>::const_iterator it = day_decision.server_bought_kind.begin();
	map<string, vector<uint32_t>>::const_iterator end = day_decision.server_bought_kind.end();
	for (; it != end; ++it) {
		for (uint32_t j = 0; j != it->second.size(); ++j) {
			GlobalServerSeq2IdMapTable.insert(pair<uint32_t, uint32_t>(it->second[j], idx));
			++idx;
		}
	}
}

//部署虚拟机用
void deployVM(int vm_id, uint32_t server_seq, S_DeploymentInfo& one_deployment_info,C_node* node = nullptr) {
		const S_VM &vm = GlobalVMRequestInfo[vm_id];
		C_BoughtServer* s = My_servers[server_seq];
		if(node == nullptr){
			s->A->remaining_cpu_num -= vm.half_cpu_num;
			s->A->remaining_memory_num -= vm.half_mem_num;
			s->B->remaining_memory_num -= vm.half_mem_num;
			s->B->remaining_cpu_num -= vm.half_cpu_num;

			//更新map
			SingleNodeTable.insert(pair<C_node*, uint32_t>(s->A, server_seq));
			SingleNodeTable.insert(pair<C_node*, uint32_t>(s->B, server_seq));
			DoubleNodeTable.insert(pair<C_BoughtServer *, uint32_t>(s, server_seq));

		}

		else if(node == s->A){
			one_deployment_info.node_name = "A";
			s->A->remaining_cpu_num -= vm.cpu_num;
			s->A->remaining_memory_num -= vm.memory_num;

			SingleNodeTable.insert(pair<C_node*, uint32_t>(s->A, server_seq));
			DoubleNodeTable.insert(pair<C_BoughtServer *, uint32_t>(s, server_seq));
		}
		else if(node == s->B){
			one_deployment_info.node_name = "B";
			s->B->remaining_memory_num -= vm.memory_num;
			s->B->remaining_cpu_num -= vm.cpu_num;

			SingleNodeTable.insert(pair<C_node*, uint32_t>(s->B, server_seq));
			DoubleNodeTable.insert(pair<C_BoughtServer *, uint32_t>(s, server_seq));
		}

		assert(s->A->remaining_cpu_num >= 0);
		assert(s->A->remaining_memory_num >= 0);
		assert(s->B->remaining_cpu_num >= 0);
		assert(s->B->remaining_memory_num >= 0);
		one_deployment_info.server = s;
		one_deployment_info.vm_info = &vm;
		one_deployment_info.server_seq = server_seq;
		//更新全局虚拟机部署表
		GlobalVMDeployTable.insert(pair<uint32_t, S_DeploymentInfo>(vm_id, one_deployment_info));
		
	}

//迁移虚拟机用
void deployVM(int vm_id, uint32_t server_seq, S_MigrationInfo& one_migration_info,C_node* node = nullptr) {
		const S_VM& vm = GlobalVMRequestInfo[vm_id];
		S_DeploymentInfo one_deployment_info;
		C_BoughtServer* s = My_servers[server_seq];

		one_migration_info.is_double_node = vm.is_double_node;
		one_migration_info.server_seq = server_seq;
		one_migration_info.vm_id = vm_id;
		if(node == nullptr){
			s->A->remaining_cpu_num -= vm.half_cpu_num;
			s->A->remaining_memory_num -= vm.half_mem_num;
			s->B->remaining_memory_num -= vm.half_mem_num;
			s->B->remaining_cpu_num -= vm.half_cpu_num;
			
			//更新map
			SingleNodeTable.erase(s->A);
			SingleNodeTable.erase(s->B);
			SingleNodeTable.insert(pair<C_node*, uint32_t>(s->A, server_seq));
			SingleNodeTable.insert(pair<C_node*, uint32_t>(s->B, server_seq));

			DoubleNodeTable.erase(s);
			DoubleNodeTable.insert(pair<C_BoughtServer *, uint32_t>(s, server_seq));

		}

		else if(node == s->A){
			one_deployment_info.node_name = "A";
			one_migration_info.node_name = "A";
			s->A->remaining_cpu_num -= vm.cpu_num;
			s->A->remaining_memory_num -= vm.memory_num;

			SingleNodeTable.erase(s->A);
			SingleNodeTable.insert(pair<C_node*, uint32_t>(s->A, server_seq));

			DoubleNodeTable.erase(s);
			DoubleNodeTable.insert(pair<C_BoughtServer *, uint32_t>(s, server_seq));
		}
		else if(node == s->B){
			one_deployment_info.node_name = "B";
			one_migration_info.node_name = "B";

			s->B->remaining_memory_num -= vm.memory_num;
			s->B->remaining_cpu_num -= vm.cpu_num;

			SingleNodeTable.erase(s->B);
			SingleNodeTable.insert(pair<C_node*, uint32_t>(s->B, server_seq));

			DoubleNodeTable.erase(s);
			DoubleNodeTable.insert(pair<C_BoughtServer *, uint32_t>(s, server_seq));
		}

		assert(s->A->remaining_cpu_num >= 0);
		assert(s->A->remaining_memory_num >= 0);
		assert(s->B->remaining_cpu_num >= 0);
		assert(s->B->remaining_memory_num >= 0);
		one_deployment_info.server = s;
		one_deployment_info.vm_info = &vm;
		one_deployment_info.server_seq = server_seq;
		//更新全局虚拟机部署表
		GlobalVMDeployTable.insert(pair<uint32_t, S_DeploymentInfo>(vm_id, one_deployment_info));
		
	}


void removeVM(uint32_t vm_id, uint32_t server_seq) {
		const S_DeploymentInfo& deployment_info = GlobalVMDeployTable[vm_id];
		const S_VM *vm_info = deployment_info.vm_info;
		C_BoughtServer * s = My_servers[server_seq];
		if (vm_info->is_double_node) {
			s->A->remaining_cpu_num += vm_info->half_cpu_num;
			s->B->remaining_cpu_num += vm_info->half_cpu_num;
			s->A->remaining_memory_num += vm_info->half_mem_num;
			s->B->remaining_memory_num += vm_info->half_mem_num;

			SingleNodeTable.insert(pair<C_node*, uint32_t>(s->A, server_seq));
			SingleNodeTable.insert(pair<C_node*, uint32_t>(s->B, server_seq));
			DoubleNodeTable.insert(pair<C_BoughtServer *, uint32_t>(s, server_seq));

		}
		else {
			if (deployment_info.node_name == "A") {
				s->A->remaining_cpu_num += vm_info->cpu_num;
				s->A->remaining_memory_num += vm_info->memory_num;

				SingleNodeTable.insert(pair<C_node*, uint32_t>(s->A, server_seq));
				DoubleNodeTable.insert(pair<C_BoughtServer *, uint32_t>(s, server_seq));
			}
			else {
				s->B->remaining_memory_num += vm_info->memory_num;
				s->B->remaining_cpu_num += vm_info->cpu_num;
				SingleNodeTable.insert(pair<C_node*, uint32_t>(s->B, server_seq));
				DoubleNodeTable.insert(pair<C_BoughtServer *, uint32_t>(s, server_seq));
			}
		}
		assert(s->A->remaining_cpu_num <= s->server_info.cpu_num / 2);
		assert(s->A->remaining_memory_num <= s->server_info.memory_num / 2);
		assert(s->B->remaining_cpu_num <= s->server_info.cpu_num / 2);
		assert(s->B->remaining_memory_num <= s->server_info.memory_num / 2);

		

		GlobalVMDeployTable.erase(vm_id);
	}



inline void migrate_vm(E_Deploy_status status ,uint32_t vm_id, uint32_t in_server_seq, S_MigrationInfo& one_migration_info);

//购买服务器
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************



//综合容量、价格购买
inline void buy_server(int32_t required_cpu, int32_t required_mem, map<string, vector<uint32_t>>& bought_server_kind) {

		//找到一台服务器
		size_t size = ServerList.size();
		int min_idx = 0;
		float min_dis = FLT_MAX;
		vector<int> accomadatable_seqs;
		
		
		//先找到所有可容纳当前请求的服务器型号
		for(size_t i = 0; i != size; ++i){
			if((ServerList[i].cpu_num / 2 >= required_cpu) and (ServerList[i].memory_num / 2 >= required_mem)){
				accomadatable_seqs.emplace_back(i);
			}
		}
		//综合考虑价格和容量差，选择一台服务器
		size = accomadatable_seqs.size();
		for(size_t i = 0; i != size; ++i){
			//容量差距
			float vol_dis = pow(ServerList[i].cpu_num - required_cpu, 2) + pow(ServerList[i].memory_num - required_mem, 2);
			float purchase_cost = BUY_SERVER_MAINTAINANCE_COST_RATIO * ServerList[accomadatable_seqs[i]].maintenance_cost + BUY_SERVER_PURCHASE_COST_RATIO * ServerList[accomadatable_seqs[i]].purchase_cost;
			float total_dis = vol_dis + TOTAL_COST_RATIO * purchase_cost;

			if(total_dis < min_dis){
				min_dis = total_dis;
				min_idx = i;
			}
		}

		const S_Server& server =  ServerList[accomadatable_seqs[min_idx]];
		C_BoughtServer *p_bought_server = new C_BoughtServer(server);

		//更新三个表
		My_servers.emplace_back(p_bought_server);
		SingleNodeTable.emplace(p_bought_server->A, p_bought_server->seq);
		SingleNodeTable.emplace(p_bought_server->B, p_bought_server->seq);
		DoubleNodeTable.emplace(p_bought_server, p_bought_server->seq);
	
		//记录购买了哪种服务器，并更新相应决策记录
		if (bought_server_kind.find(server.type) != bought_server_kind.end()) {
			bought_server_kind[server.type].emplace_back(p_bought_server->seq);
		}
		else {
			vector<uint32_t> bought_server_seq_nums;
			bought_server_seq_nums.emplace_back(p_bought_server->seq);
			bought_server_kind.insert(pair<string, vector<uint32_t>>(server.type, bought_server_seq_nums));
		}

}


//部署算法
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

inline bool best_fit(const S_Request & request, S_DeploymentInfo & one_deployment_info){
	assert(request.is_add);
	const S_VM& vm = VMList[request.vm_type];
	if(!vm.is_double_node){
		C_node * fake_node = new C_node(vm);

		//将这个假节点插入到单节点表中，然后以其位置为基准，向右(剩余节点容量升序)寻找最接近的节点
		SingleNodeTable.insert(pair<C_node*, uint32_t>(fake_node, 0));
		map<C_node*, uint32_t>::iterator fake_it = SingleNodeTable.find(fake_node);
		map<C_node*, uint32_t>::iterator right_it = fake_it;

		while(++right_it != SingleNodeTable.end()){
			if(right_it->first->remaining_cpu_num >= vm.cpu_num && right_it->first->remaining_memory_num >= vm.memory_num){
				break;
			}	
		}
		//如果当前无节点可以容纳此虚拟机
		if(right_it == SingleNodeTable.end()){
			SingleNodeTable.erase(fake_it);
			delete fake_node;
			return false;
		}
		else{
			uint32_t server_seq = right_it->second;
			C_node* node = right_it->first;

			DoubleNodeTable.erase(My_servers[right_it->second]);
			SingleNodeTable.erase(right_it);
			SingleNodeTable.erase(fake_it);
			delete fake_node;

			deployVM(request.vm_id,server_seq, one_deployment_info, node);

			return true;
		}
	}
	else{
		C_BoughtServer* fake_server = new C_BoughtServer(vm);
		DoubleNodeTable.insert(pair<C_BoughtServer*, uint32_t>(fake_server, 0));
		map<C_BoughtServer*, uint32_t>::iterator fake_it = DoubleNodeTable.find(fake_server);
		map<C_BoughtServer*, uint32_t>::iterator right_it = fake_it;
		while(++right_it != DoubleNodeTable.end()){
			if(right_it->first->A->remaining_cpu_num >= vm.half_cpu_num &&
			right_it->first->B->remaining_cpu_num >= vm.half_cpu_num&&
			right_it->first->A->remaining_memory_num >= vm.half_mem_num&&
			right_it->first->B->remaining_memory_num >= vm.half_mem_num){
				break;
			}
		}
		if(right_it == DoubleNodeTable.end()){
			DoubleNodeTable.erase(fake_it);
			delete fake_server;
			return false;
		}
		else{
			uint32_t server_seq = right_it->second;

			SingleNodeTable.erase(right_it->first->A);
			SingleNodeTable.erase(right_it->first->B);
			DoubleNodeTable.erase(right_it);
			DoubleNodeTable.erase(fake_it);
			delete fake_server;

			deployVM(request.vm_id, server_seq, one_deployment_info);
			return true;
		}
	}
}


//迁移操作
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

//返回当天最大可用迁移次数
inline int32_t get_max_migrate_num(){
	size_t num = GlobalVMDeployTable.size();
	return (int32_t)(num * 0.03);
}

//基本迁移操作
inline void migrate_vm(uint32_t vm_id, uint32_t in_server_seq, S_MigrationInfo& one_migration_info, C_node* p_node = nullptr){
	// assert(GlobalVMDeployTable.find(vm_id) != GlobalVMDeployTable.end());
	// assert(GlobalVMDeployTable[vm_id] < My_servers.size());
	// assert(0 <= in_server_seq < My_servers.size());

	//原服务器删除此虚拟机
	removeVM(vm_id, GlobalVMDeployTable[vm_id].server_seq);
	//新服务器增加此虚拟机
	deployVM(vm_id, in_server_seq, one_migration_info, p_node);

}

//对服务器按利用率升序排序
bool com_used_rate(C_BoughtServer* p_A, C_BoughtServer* p_B){
	return *p_A < *p_B;
}

//根据两个节点使用率均衡程度对服务器排序，差值越大越不均衡
bool com_node_used_balance_rate(C_BoughtServer& s1,  C_BoughtServer& s2){
	return abs(s1.A->cpu_used_rate + s1.A->mem_used_rate - (s1.B->cpu_used_rate + s1.B->mem_used_rate)) < abs(s2.A->cpu_used_rate + s2.A->mem_used_rate - (s2.B->cpu_used_rate + s2.B->mem_used_rate));
}

/*
//迁移主流程，只进行服务器间迁移，适配最佳适应算法，将服务器资源利用率拉满
void full_loaded_migrate_vm(S_DayTotalDecisionInfo & day_decision, bool do_balance){

	register int32_t remaining_migrate_vm_num = get_max_migrate_num();//当天可用迁移量
	int32_t max_out = (int32_t)(My_servers.size() * MAX_MIGRATE_OUT_SERVER_RATIO);	//查看的迁出服务器窗口大小
	if(max_out < 1)return;

	if(remaining_migrate_vm_num == 0) return;

	size_t size = My_servers.size();
	 C_BoughtServer * p_server;

	
	register int32_t out = 0;
	uint32_t least_used_server_seq;
	const C_BoughtServer *p_out_server = nullptr;

	vector<pair<uint32_t, const S_VM*> >:: const_iterator vm_it;
	vector<pair<uint32_t, const S_VM*> >:: const_iterator vm_end;

	map<C_BoughtServer*, uint32_t>::const_iterator server_it = tmp_my_servers.begin();
	//迁移主循环
	do{	
		//迁出服务器信息
		
		p_out_server = server_it->first;
		
		
		#ifdef EARLY_STOPPING
		//用于排序
		vector<pair<uint32_t, const S_VM*> > cur_server_vm_info;
		//判断需求最小的虚拟机是否可以迁移，若不能则直接看下一个服务器

		//构造有序虚拟机信息表
		server_it = p_out_server->deployed_vms.begin();		
		server_end = p_out_server->deployed_vms.end();
			for(;server_it != server_end; ++server_it){
			cur_server_vm_info->emplace_back(pair<uint32_t,const S_VM*>(server_it->first, &VMList[server_it->second.vm_type]));
		}
		sort(cur_server_vm_info->begin(), cur_server_vm_info->end(), com_VM);

		//开始遍历当前迁出服务器所有已有的虚拟机
		vm_it = cur_server_vm_info->begin();
		vm_end = cur_server_vm_info->end();

		

		for(;vm_it != vm_end;){
			
			E_Deploy_status stat = dep_No;
			
			//遍历所有迁入服务器
			//i只是当前迁入服务器遍历序号，并不是服务器序列号
			const C_BoughtServer *in_server = nullptr;
			size_t size = tmp_my_servers.size();
			for(int32_t in = (int32_t)(size - 1); in > out; --in){
				//把当前虚拟机迁移到当前服务器上
				uint32_t most_used_server_seq = tmp_my_servers[in]->seq;
				in_server = &My_servers[most_used_server_seq];
				
				stat = in_server->is_deployable(*vm_it->second);
				
				if(stat == dep_No){
					continue;
				}

				else{
				//一次成功迁移
					vector<pair<uint32_t, const S_VM*> >:: const_iterator tmp_it = vm_it;
					++vm_it;
					//迁移信息记录
					S_MigrationInfo one_migration_info;
					migrate_vm(stat, tmp_it->first, most_used_server_seq, one_migration_info);
					day_decision.VM_migrate_vm_record.emplace_back(one_migration_info);
					if(--remaining_migrate_vm_num == 0)return;
					break;
				}


			}
			
			//如果扫描完一遍迁入服务器，stat不为no，说明此虚拟机已被迁入新服务器
			if(stat != dep_No)continue;
			
			//当前虚拟机不可迁出，直接换下一台迁出服务器
			break;
		}

		#endif
		// //初始化使用率高的服务器剩余容量与当前迁出虚拟机需要容量的距离
		// uint32_t min_dis = UINT32_MAX;
		// int32_t min_idx = 0;
		#ifndef EARLY_STOPPING
		unordered_map<uint32_t, S_DeploymentInfo>::const_iterator it = out_server.deployed_vms.begin();		
		unordered_map<uint32_t, S_DeploymentInfo>::const_iterator end = out_server.deployed_vms.end();

		for(; it != end;){
			const S_VM & vm =  VMList[it->second.vm_type];
		
			E_Deploy_status stat = dep_No;
			//i只是当前迁入服务器遍历序号，并不是服务器序列号
			for(int32_t in = tmp_my_servers.size() - 1; in > out; --in){
				
				//把当前虚拟机迁移到当前服务器上
				uint32_t most_used_server_seq = tmp_my_servers[in].seq;
				const C_BoughtServer &in_server = My_servers[most_used_server_seq];
				stat = in_server.is_deployable(vm);
				
				if(stat == dep_No){
					continue;
				}

				else{
				//一次成功迁移
					unordered_map<uint32_t, S_DeploymentInfo>::const_iterator tmp_it = it;
					++it;
					//迁移信息记录
					S_MigrationInfo one_migration_info;
					migrate_vm(stat, tmp_it->first, most_used_server_seq, one_migration_info);
					day_decision.VM_migrate_vm_record.emplace_back(one_migration_info);
					if(--remaining_migrate_vm_num == 0)return;
					break;
					
				}


			}
			if(stat != dep_No)continue;
			//最好是能把使用率低的服务器腾出来，如果腾不出来，就要考虑换次低的服务器进行尝试
			++it;
		}
		#endif
		++out;
	}while(out != max_out);
	
	#ifdef DO_NODE_BALANCE
	//对所有服务器做节点均衡
	assert(remaining_migrate_vm_num > 0);
	size_t server_size = My_servers.size();
	S_MigrationInfo migration_info;
	for(size_t i = 0; i != server_size; ++i){
		if(My_servers[i].make_node_balance(migration_info, do_balance)){
			day_decision.VM_migrate_vm_record.emplace_back(migration_info);
			if(--remaining_migrate_vm_num == 0)return;
		}
	}
	#endif
}
*/
//主流程
void process() {
	int32_t t = 0;
	do{
	S_DayTotalDecisionInfo day_decision;
		#ifndef SUBMIT
		std::cout<<"process"<<t<<" day"<<endl;
		#endif
		
		const S_DayRequest& day_request = Requests.front();
		#ifdef MIGRATE
		//根据当天的请求是单节点多还是双节点多来判断是要做节点均衡还是不均衡
		int32_t double_node_add_num = 0;
		int32_t add_request_num = 0;
		bool do_balance = false;

		#ifdef DO_NODE_BALANCE
		for(uint32_t i = 0; i != day_request.request_num; ++i){
			if(!day_request.day_request[i].is_add)continue;
			double_node_add_num = VMList[day_request.day_request[i].vm_type].is_double_node ? double_node_add_num + 1 : double_node_add_num;
			++add_request_num;
		}
		do_balance = double_node_add_num > add_request_num / 2 ? true :false;
		#endif

		//对存量虚拟机进行迁移
		full_loaded_migrate_vm(day_decision, do_balance);
		day_decision.W = day_decision.VM_migrate_vm_record.size();
		#endif
		for (uint32_t i = 0; i != day_request.request_num; ++i) {

		//不断处理请求，直至已有服务器无法满足
		S_DeploymentInfo one_request_deployment_info;
			const S_Request & one_request = day_request.day_request[i];
			//删除虚拟机
			if (!one_request.is_add) {
				assert(GlobalVMDeployTable.find(one_request.vm_id) != GlobalVMDeployTable.end());
				int32_t cur_server_seq = GlobalVMDeployTable[one_request.vm_id].server_seq;
				removeVM(one_request.vm_id, cur_server_seq);
				continue;
			}

			//如果是增加虚拟机
			if (best_fit(one_request, one_request_deployment_info)) {
				day_decision.request_deployment_info.emplace_back(one_request_deployment_info);
				continue;
			};

			const S_VM& vm = VMList[one_request.vm_type];

			//根据所需cpu and mem,购买服务器
			buy_server(vm.cpu_num, vm.memory_num, day_decision.server_bought_kind);
			
			best_fit(one_request, one_request_deployment_info);//购买服务器后重新处理当前请求
			day_decision.request_deployment_info.emplace_back(one_request_deployment_info);
	}
		Requests.pop();

		day_decision.Q = day_decision.server_bought_kind.size();
		server_seq_to_id(day_decision);
		write_standard_output(day_decision);

		if(t != T - K){
		read_one_request();
		++t;
		}
		
	}while(Requests.size());
	/*
	freopen("bought_server_ids.txt", "w", stdout);
	
	for (int32_t i = 0; i != T; ++i) {
		std::cout << "(purchase, " << day_decision.Q << ")" << endl;
		for (map<string, vector<uint32_t>>::iterator iter = day_decision.server_bought_kind.begin(); iter != day_decision.server_bought_kind.end(); ++iter) {
			std::cout << "(" << iter->first << ", " << iter->second.size() << ")" << endl;
			for (int j = 0; j != iter->second.size(); ++j) {
				std::cout << "server type:" << iter->first << "	" << "server seq:" << iter->second[j] << "   " << "server id:" << GlobalServerSeq2IdMapTable[iter->second[j]] << endl;
			}
		}
	}*/

}

//IO
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

//用于处理输入字符串
inline vector<string> split( string& line, char pattern) {
	string::size_type pos;
	vector<string> results;

	line[line.size() - 1] = pattern;
	string::size_type size = line.size();

	for (size_t i = 1; i < size; ++i) {
		pos = line.find(',', i);
		if (pos < size) {
			string s = line.substr(i, pos - i);
			results.emplace_back(s);
			i = pos;
		}
	}
	return results;
}

void read_standard_input() {
#ifndef SUBMIT
freopen("./training-1.txt", "r", stdin);
#endif 
	
	//优化io效率
	std::ios::sync_with_stdio(false);
    std::cin.tie(0);

	string line;
	VMList.reserve(1000);
	VMList.rehash(1000);
	GlobalVMRequestInfo.reserve(1000000);
	GlobalVMRequestInfo.rehash(1000000);
	GlobalVMDeployTable.reserve(1000000);
	GlobalVMDeployTable.rehash(1000000);
	std::cin >> N ;
	ServerList.reserve(N + 100);
	std::cin.get();
	S_Server server;
	for (int32_t i = 0; i != N; ++i) {
		getline(std::cin, line);

		vector <string> results = split(line, ',');
		server.type = results[0];
		server.cpu_num = stoi(results[1]);
		server.memory_num = stoi(results[2]);
		server.purchase_cost = stoi(results[3]);
		server.maintenance_cost = stoi(results[4]);

		ServerList.emplace_back(server);
	}

	std::cin >> M;
	std::cin.get();
	S_VM vm;
	for (int32_t i = 0; i != M; ++i) {
		getline(std::cin, line);
		vector<string> results = split(line, ',');
		vm.type = results[0];
		vm.cpu_num = stoi(results[1]);
		vm.half_cpu_num = vm.cpu_num / 2;
		vm.memory_num = stoi(results[2]);
		vm.half_mem_num = vm.memory_num / 2;
		vm.is_double_node = stoi(results[3]);

		VMList.insert(pair<string, S_VM>(vm.type, vm));
	}

	std::cin >> T;
	std::cin >> K;
	std::cin.get();
	for (int32_t i = 0; i != K; ++i) {
		int32_t day_request_num;
		std::cin >> day_request_num;
		std::cin.get();
		S_DayRequest day_request;
		day_request.request_num = day_request_num;

		for (int32_t j = 0; j != day_request_num; ++j) {
			getline(std::cin, line);
			S_Request one_request;
			vector<string>results = split(line, ',');
			if (results[0] == "add") {
				one_request.is_add = true;
				one_request.vm_type = results[1].erase(0, 1);
				one_request.vm_id = stoi(results[2]);
				GlobalVMRequestInfo.insert(pair<uint32_t,  S_VM>(one_request.vm_id, VMList[one_request.vm_type]));
			}
			else {
				one_request.is_add = false;
				one_request.vm_id = stoi(results[1]);
				one_request.vm_type = GlobalVMRequestInfo[one_request.vm_id].type;
				day_request.delete_op_idxs.emplace_back(j);
			}

			day_request.day_request.emplace_back(one_request);
		}

		Requests.push(day_request);
	}
}

inline void write_standard_output(const S_DayTotalDecisionInfo& day_decision) {


	std::cout << "(purchase, " << day_decision.Q << ")" << endl;
	for (map<string, vector<uint32_t>>::const_iterator iter = day_decision.server_bought_kind.begin(); iter != day_decision.server_bought_kind.end(); ++iter) {
		std::cout << "(" << iter->first << ", " << iter->second.size() << ")" << endl;
	}

	std::cout << "(migration, " << day_decision.W << ")" << endl;
	for (int32_t j = 0; j != day_decision.W; ++j) {
		if (day_decision.VM_migrate_vm_record[j].is_double_node) {
			std::cout << "(" << day_decision.VM_migrate_vm_record[j].vm_id << ", " << GlobalServerSeq2IdMapTable[day_decision.VM_migrate_vm_record[j].server_seq] << ")" << endl;
		}
		else {
			std::cout << "(" << day_decision.VM_migrate_vm_record[j].vm_id << ", " << GlobalServerSeq2IdMapTable[day_decision.VM_migrate_vm_record[j].server_seq] <<
				", " << day_decision.VM_migrate_vm_record[j].node_name << ")" << endl;
		}
	}

	for (size_t k = 0; k != day_decision.request_deployment_info.size(); ++k) {
		if (day_decision.request_deployment_info[k].vm_info->is_double_node) {
			std::cout << "(" << GlobalServerSeq2IdMapTable[day_decision.request_deployment_info[k].server_seq] << ")";
		}
		else {
			std::cout << "(" << GlobalServerSeq2IdMapTable[day_decision.request_deployment_info[k].server_seq] << ", " << day_decision.request_deployment_info[k].node_name
				<< ")";
		}
		std::cout << endl;
	}
	fflush(stdout);
}

inline void read_one_request(){
	//优化io效率
	std::ios::sync_with_stdio(false);
    std::cin.tie(0);
	string line;	
	int32_t day_request_num;
	std::cin >> day_request_num;

	std::cin.get();
	S_DayRequest day_request;
	day_request.request_num = day_request_num;

	for (int32_t j = 0; j != day_request_num; ++j) {
		getline(std::cin, line);
		S_Request one_request;
		vector<string>results = split(line, ',');
		if (results[0] == "add") {
			one_request.is_add = true;
			one_request.vm_type = results[1].erase(0, 1);
			one_request.vm_id = stoi(results[2]);
			GlobalVMRequestInfo.insert(pair<uint32_t,  S_VM>(one_request.vm_id, VMList[one_request.vm_type]));
		}
		else {
			one_request.is_add = false;
			one_request.vm_id = stoi(results[1]);
			one_request.vm_type = GlobalVMRequestInfo[one_request.vm_id].type;
			day_request.delete_op_idxs.emplace_back(j);
		}

		day_request.day_request.emplace_back(one_request);
	}

	Requests.push(day_request);
}

int main()
{	
	//int start = clock();
	//int end = 0;
	read_standard_input();
	//end = clock();
	//std::cout<<"read input cost: ";
	//std::cout<< (double)(end - start) / CLOCKS_PER_SEC << endl;
	process();
	//start = clock();
	//std::cout<<"process cost: ";
	//std::cout<< (double)(start - end) / CLOCKS_PER_SEC <<endl;
	
	return 0;
}
