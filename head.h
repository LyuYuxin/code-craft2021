#pragma once
#include<cstdlib>
#include<cstdio>
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
#include<set>
#include<random>
#define _CRT_SECURE_NO_WARNINGS

#define SUBMIT//是否提交
#define MIGRATE//是否迁移
//#define EARLY_STOPPING//是否迁移时短路判断 
//#define DO_NODE_BALANCE


using namespace std;

//server信息
typedef struct  S_Server {
	int32_t cpu_num;
	int32_t mem_num;
	int32_t purchase_cost;
	int32_t maintenance_cost;
	string type;
	S_Server() :cpu_num(0), mem_num(0), purchase_cost(0), maintenance_cost(0) {}
} S_Server;

//virtual machine参数
typedef struct S_VM {
	int32_t cpu_num;
	int32_t half_cpu_num;
	int32_t mem_num;
	int32_t half_mem_num;
	bool is_double_node;
	string type;
	S_VM() :cpu_num(0), half_cpu_num(0), mem_num(0), half_mem_num(0), is_double_node(false), type("") {}
}S_VM;

//虚拟机实体
class C_VM_Entity{
public:
	C_VM_Entity(){}
	C_VM_Entity(const S_VM*vm, uint32_t vm_id):vm(vm), vm_id(vm_id){};

	const S_VM * vm;
	uint32_t vm_id;
};

//一个请求信息
typedef struct S_Request {
	bool is_add;
	string vm_type;
	uint32_t vm_id;
	S_Request() :is_add(false), vm_type(""), vm_id(0) {}
}S_Request;

//一天的请求信息
typedef struct {
	uint32_t request_num;
	vector<S_Request> day_request;
	vector<int> delete_op_idxs;//用于记录删除操作的索引
}S_DayRequest;


//单个虚拟机迁移信息
typedef struct {
	uint32_t vm_id;//虚拟机id
	uint32_t server_seq;//目标服务器seq
	bool is_double_node;//是否双节点部署
	string node_name;//如果单节点部署，部署的节点名称
}S_MigrationInfo;

template<class _Ty>
struct less_VM
{
	bool operator()(const _Ty& p_Left, const _Ty& p_Right) const
	{
		float l = sqrt(p_Left.vm->cpu_num) + sqrt(p_Left.vm->mem_num);
		float r = sqrt(p_Right.vm->cpu_num) + sqrt(p_Right.vm->mem_num);

		if (abs(l - r) > 1e-7) {
			return l < r;
		}
		return p_Left.vm_id < p_Right.vm_id;

	}
};


//服务器节点信息
class C_node {
public:
	C_node(const S_Server& s) :remaining_cpu_num(s.cpu_num / 2),
		remaining_mem_num(s.mem_num / 2),
		total_cpu_num(s.cpu_num / 2),
		total_mem_num(s.mem_num / 2),
		cpu_used_rate(0), mem_used_rate(0)
	{}
	C_node(const S_VM& vm, bool is_half = false) {
		if (!is_half) {
			remaining_mem_num = vm.mem_num;
			remaining_cpu_num = vm.cpu_num;
			total_mem_num = vm.mem_num;
			total_cpu_num = vm.cpu_num;
			cpu_used_rate = 0.0f;
			mem_used_rate = 0.0f;
		}
		else {
			remaining_mem_num = vm.mem_num / 2;
			remaining_cpu_num = vm.cpu_num / 2;
			total_cpu_num = remaining_cpu_num;
			total_mem_num = remaining_mem_num;
			cpu_used_rate = 0.0f;
			mem_used_rate = 0.0f;
		}
	}

	bool operator<(C_node& node) {
		return remaining_cpu_num + remaining_mem_num < node.remaining_cpu_num + node.remaining_mem_num;
	}

	bool operator=(const C_node& b)const {

		if ((remaining_cpu_num == b.remaining_cpu_num) && (remaining_mem_num == b.remaining_mem_num)) {
			return true;
		}
		return false;
	}

	float get_mem_used_rate(){
		return static_cast<float>(total_mem_num - remaining_mem_num) / total_mem_num;
	}
	float get_cpu_used_rate(){
		return static_cast<float>(total_cpu_num - remaining_cpu_num) / total_cpu_num;
	}

	float get_total_used_rate(){
		return static_cast<float>(total_cpu_num + total_mem_num - remaining_mem_num - remaining_cpu_num) / (total_mem_num + total_cpu_num);
	}

	set<C_VM_Entity, less_VM<C_VM_Entity>>single_node_deploy_table;//用于记录此节点上部署的单节点虚拟机信息
	set<C_VM_Entity, less_VM<C_VM_Entity> >double_node_deploy_table;//用于记录此节点上部署的双节点部署信息
	int32_t total_cpu_num, total_mem_num;
	int32_t remaining_cpu_num;
	int32_t remaining_mem_num;
	float cpu_used_rate;
	float mem_used_rate;
};

class C_BoughtServer {
public:
	C_BoughtServer(const S_Server& server);

	//用于构造假节点
	C_BoughtServer(const S_VM& vm);
	~C_BoughtServer();

	static float cal_node_similarity(int m1, int c1, int m2, int c2);

	bool operator<(C_BoughtServer& bought_server);

	//返回A、B节点中可用cpu和内存较小值
	float get_double_node_avail_resources()const;

	//计算总cpu内存资源利用率
	float cal_total_resource_used_rate();

	S_Server server_info;//此服务器的基本参数
	C_node* A, * B;//两个节点的信息
	uint32_t seq;//此服务器序列号，唯一标识
	float total_resource_used_rate;//cpu和mem总使用率.针对服务器
	static int32_t purchase_seq_num;//静态成员，用于存储当前已购买服务器总数，也用于给新买的服务器赋予序列号
};

template<class _Ty>
struct less_BoughtServer
{
	bool operator()(const _Ty& p_Left, const _Ty& p_Right) const
	{
		float l_resources = p_Left->get_double_node_avail_resources();
		float r_resources = p_Right->get_double_node_avail_resources();

		//二级比较，如果值相等，就进一步比较地址值，保证不出现重复key
		if (abs(l_resources - r_resources) > 1e-7) {
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
		float l = sqrt(p_Left->remaining_cpu_num) + sqrt(p_Left->remaining_mem_num) + 0.001 * (1 / (1 + p_Left->get_total_used_rate()));
		float r = sqrt(p_Right->remaining_cpu_num) + sqrt(p_Right->remaining_mem_num)  + 0.001 * (1 / (1 + p_Right->get_total_used_rate()));

		if (abs(l - r) > 1e-7) {
			return l < r;
		}
		return p_Left < p_Right;

	}
};



// template<class _Ty>
// struct less_SingleNode
// {
// 	bool operator()(const _Ty& p_Left, const _Ty& p_Right) const
// 	{
// 		float l = sqrt(p_Left->remaining_cpu_num) + sqrt(p_Left->remaining_mem_num);
// 		float r = sqrt(p_Right->remaining_cpu_num) + sqrt(p_Right->remaining_mem_num);

// 		if (abs(l - r) > 1e-7) {
// 			return l < r;
// 		}
// 		return p_Left < p_Right;

// 	}
// };

//单个虚拟机部署信息
typedef struct {
	uint32_t server_seq;//部署的服务器seq
	C_BoughtServer* server;//部署的server
	string node_name; //部署的节点名称
	const S_VM* vm_info;//部署的虚拟机参数
}S_DeploymentInfo;

//一天的决策信息, 用于记录所有操作并输出
typedef struct DayTotalDecisionInfo {
	int32_t Q;//扩容购买的服务器种类数
	map<string, vector<uint32_t>> server_bought_kind;//购买的不同服务器的个数
	int32_t W;//需要迁移的虚拟机的数量
	vector<S_MigrationInfo> VM_migrate_vm_record;//虚拟机迁移记录

	vector<S_DeploymentInfo> request_deployment_info;//按序对请求的处理记录
	DayTotalDecisionInfo() {
		Q = 0;
		W = 0;
	};
}S_DayTotalDecisionInfo;

extern int32_t N;//可以采购的服务器类型
extern int32_t M;//虚拟机类型数量
extern int32_t T;//总共T天
extern int32_t K;//先给K天

extern vector<S_Server> ServerList;//用于存储所有可买的服务器种类信息
extern unordered_map<string, S_VM> VMList;//用于存储所有虚拟机种类信息
extern vector<S_DayRequest> Requests;//用于存储用户所有的请求信息



extern vector<C_BoughtServer*> My_servers;//已购买的服务器列表
extern map<C_BoughtServer*, uint32_t, less_BoughtServer<C_BoughtServer*> > DoubleNodeTable;//将所有服务器组织成一个双节点表，值为服务器seq
extern map<C_node*, uint32_t, less_SingleNode<C_node*> > SingleNodeTable;//将所有服务器的节点组织成一个单节点表， 值为服务器seq
extern unordered_map<uint32_t, S_DeploymentInfo> GlobalVMDeployTable;//全局虚拟机部署表，记录虚拟机id和相应的部署信息
extern unordered_map<uint32_t, uint32_t> GlobalServerSeq2IdMapTable;//全局服务器id表，用于从购买序列号到输出id的映射
extern unordered_map<uint32_t, S_VM> GlobalVMRequestInfo;//全局VMadd请求表，用于从虚拟机id映射到虚拟机信息



template<class _Ty>
struct less_Request
{
	bool operator()(const _Ty& p_Left, const _Ty& p_Right) const
	{
		const S_VM & l = VMList[p_Left->vm_type];
		const S_VM & r = VMList[p_Right->vm_type];

		if((l.is_double_node) && (!r.is_double_node)){
			return true;
		}
		else if((!l.is_double_node) && (r.is_double_node)){
			return false;
		}
		else{
			float l_val = pow(l.cpu_num, 2) + pow(l.mem_num, 2);
			float r_val = pow(r.cpu_num, 2) + pow(r.mem_num, 2);
			if(abs(l_val - r_val) > 1e-7){
				return l_val < r_val;
			}
			return p_Left <p_Right;
		}
	}
};


inline bool com_VM(const pair<uint32_t, const S_VM*>& A, const pair<uint32_t, const S_VM*>& B) {
	return A.second->cpu_num + A.second->mem_num < B.second->mem_num + B.second->cpu_num;
}


//对服务器按利用率升序排序
bool com_used_rate(C_BoughtServer* p_A, C_BoughtServer* p_B);


//根据两个节点使用率均衡程度对服务器排序，差值越大越不均衡
bool com_node_used_balance_rate(C_BoughtServer& s1, C_BoughtServer& s2);


//部署虚拟机用
void deployVM(int vm_id, uint32_t server_seq, S_DeploymentInfo& one_deployment_info,C_node* node = nullptr);

//迁移虚拟机用部署函数
void deployVM(int vm_id, uint32_t server_seq, S_MigrationInfo& one_migration_info, C_node* node = nullptr);

//删除虚拟机
void removeVM(uint32_t vm_id, uint32_t server_seq);

//返回当天最大可用迁移次数
inline int32_t get_max_migrate_num(){
	size_t num = GlobalVMDeployTable.size();
	return static_cast<int32_t>(num * 0.03);
}

//基本迁移操作
inline void migrate_vm(uint32_t vm_id, uint32_t in_server_seq, S_MigrationInfo& one_migration_info, C_node* p_node = nullptr){
	//原服务器删除此虚拟机
	removeVM(vm_id, GlobalVMDeployTable[vm_id].server_seq);
	//新服务器增加此虚拟机
	deployVM(vm_id, in_server_seq, one_migration_info, p_node);

}

//生成每一天购买服务器的id，以及序列号跟id之间的映射表
inline void server_seq_to_id(const S_DayTotalDecisionInfo& day_decision) {
	static uint32_t idx = 0;
	map<string, vector<uint32_t>>::const_iterator it = day_decision.server_bought_kind.begin();
	map<string, vector<uint32_t>>::const_iterator end = day_decision.server_bought_kind.end();
	for (; it != end; ++it) {
		for (uint32_t j = 0; j != it->second.size(); ++j) {
			GlobalServerSeq2IdMapTable.emplace(it->second[j], idx);
			++idx;
		}
	}
}

//用于处理输入字符串
inline vector<string> split(string& line, char pattern) {
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

inline void read_standard_input() {
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

	std::cin >> N;
	ServerList.reserve(N + 100);
	std::cin.get();
	S_Server server;
	for (int32_t i = 0; i != N; ++i) {
		getline(std::cin, line);

		vector <string> results = split(line, ',');
		server.type = results[0];
		server.cpu_num = stoi(results[1]);
		server.mem_num = stoi(results[2]);
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
		vm.mem_num = stoi(results[2]);
		vm.half_mem_num = vm.mem_num / 2;
		vm.is_double_node = stoi(results[3]);

		VMList.emplace(vm.type, vm);
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
				GlobalVMRequestInfo.emplace(one_request.vm_id, VMList[one_request.vm_type]);
			}
			else {
				one_request.is_add = false;
				one_request.vm_id = stoi(results[1]);
				one_request.vm_type = GlobalVMRequestInfo[one_request.vm_id].type;
				day_request.delete_op_idxs.emplace_back(j);
			}

			day_request.day_request.emplace_back(one_request);
		}
		day_request.delete_op_idxs.emplace_back(day_request_num);
		Requests.emplace_back(day_request);
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

inline void read_one_request() {
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
			GlobalVMRequestInfo.emplace(one_request.vm_id, VMList[one_request.vm_type]);
		}
		else {
			one_request.is_add = false;
			one_request.vm_id = stoi(results[1]);
			one_request.vm_type = GlobalVMRequestInfo[one_request.vm_id].type;
			day_request.delete_op_idxs.emplace_back(j);
		}

		day_request.day_request.emplace_back(one_request);
	}
	day_request.delete_op_idxs.emplace_back(day_request_num);

	Requests.emplace_back(day_request);
}
