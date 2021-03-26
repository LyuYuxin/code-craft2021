#define _CRT_SECURE_NO_WARNINGS
//#define SUBMIT//是否提交
//#define SIMILAR_NODE
//#define BALANCE_NODE
#define MIGRATE//原始迁移
#define EARLY_STOPPING//迁移时短路判断 todo
#define MAXIMIZE_FITNESS//pso
#define PSO
//#define BUY_SERVER_GREEDY
#include <stdlib.h>
#include <iostream>
#include<string>
#include<vector>
#include<unordered_map>
#include<map>
#include<cmath>
#include<time.h>
#include<cassert>
#include<algorithm>


enum E_Deploy_status{
	dep_No = -1, dep_A, dep_B, dep_Both
};

//可调参数列表
//最大迁出服务器比例
const float MAX_MIGRATE_OUT_SERVER_RATIO = 0.1;

//计算平衡分数用
const float BIAS = 100.0f;
          
//进行内部节点迁移的服务器比例
const float MAX_MIGRATE_INNER_SERVER_RATIO = 0.2;

using namespace std;

//接下来是输入
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

//server信息
typedef struct  {
	int32_t cpu_num;
	int32_t memory_num;
	int32_t purchase_cost;
	int32_t maintenance_cost;
	string type;
} S_Server;


//virtual machine信息
typedef struct {
	int32_t cpu_num;
	int32_t half_cpu_num;
	int32_t memory_num;
	int32_t half_mem_num;
	bool is_double_node;
	string type;
}S_VM;

//一个请求信息
typedef struct  {
	bool is_add;
	string vm_type;
	uint32_t vm_id;
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

//单个虚拟机部署信息
typedef struct {
	uint32_t server_seq;//部署的服务器seq
	bool is_double_node;//是否双节点部署
	string node_name; //部署的节点名称
	string vm_type; //部署的虚拟机类型
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

//服务器节点信息
typedef struct node{
	int32_t remaining_cpu_num;
	int32_t remaining_memory_num;
	node(){
		remaining_cpu_num = 0;
		remaining_memory_num = 0;
	}
}S_node;

//全局变量定义
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

int32_t N;//可以采购的服务器类型
int32_t M;//虚拟机类型数量
int32_t T;//T天
float GLOBAL_BALANCE_SCORE;//根据所有虚拟机请求得到的内存-核数比例

vector<S_Server> ServerList;//用于存储所有可买的服务器种类信息
unordered_map<string, S_VM> VMList;//用于存储所有虚拟机种类信息
vector<S_DayRequest> Requests;//用于存储用户所有的请求信息
vector<S_DayTotalDecisionInfo> Decisions;//所有的决策信息
unordered_map<uint32_t, uint32_t> GlobalVMDeployTable;//全局虚拟机部署表，记录虚拟机id和部署的服务器序列号
unordered_map<uint32_t, S_VM> GlobalVMRequestInfo;//全局虚拟机信息表，记录虚拟机id和对应的虚拟机其他信息
unordered_map<uint32_t, uint32_t> GlobalServerSeq2IdMapTable;//全局服务器id表，用于从购买序列号到输出id的映射

//utils
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

inline float get_global_balance_score(){
	uint32_t cpu_num = 0;
	uint32_t mem_num = 0;
	for(int t = 0; t != T; ++t){
		for(int i = 0; i != Requests[t].request_num; ++i){
			cpu_num += VMList[Requests[t].day_request[i].vm_type].cpu_num;
			mem_num += VMList[Requests[t].day_request[i].vm_type].memory_num;
		}
	}
	
	return  (float)(cpu_num) / mem_num;
}

float get_day_balance_score(int32_t day_idx){
	uint32_t cpu_num = 0;
	uint32_t mem_num = 0;
	for(int i = 0; i != Requests[day_idx].request_num; ++i){
		cpu_num += VMList[Requests[day_idx].day_request[i].vm_type].cpu_num;
		mem_num += VMList[Requests[day_idx].day_request[i].vm_type].memory_num;
	}

	return  (float)(cpu_num) / mem_num;
}

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


class C_BoughtServer {
public:
	C_BoughtServer(const S_Server& server) :server_info(server)
	{
		A.remaining_cpu_num = server.cpu_num / 2;
		A.remaining_memory_num = server.memory_num / 2;
		B = A;
		seq = purchase_seq_num++;
		total_resource_used_rate = 0.f;
	}
	
	//迁移用
	inline void deployVM(E_Deploy_status status, const S_VM& vm, int vm_id, S_MigrationInfo& one_migration_info){
		S_DeploymentInfo one_deployment_info;
		assert(status != dep_No);
		switch(status){
			case dep_Both:
				A.remaining_cpu_num -= vm.half_cpu_num;
				A.remaining_memory_num -= vm.half_mem_num;
				B.remaining_memory_num -= vm.half_mem_num;
				B.remaining_cpu_num -= vm.half_cpu_num;
				break;
			case dep_A:
				one_deployment_info.node_name = "A";
				one_migration_info.node_name = "A";
				A.remaining_cpu_num -= vm.cpu_num;
				A.remaining_memory_num -= vm.memory_num;
				break;
			case dep_B:
				one_deployment_info.node_name = "B";
				one_migration_info.node_name = "B";
				B.remaining_memory_num -= vm.memory_num;
				B.remaining_cpu_num -= vm.cpu_num;

	}
		assert(A.remaining_cpu_num >= 0);
		assert(A.remaining_memory_num >= 0);
		assert(B.remaining_cpu_num >= 0);
		assert(B.remaining_memory_num >= 0);

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
	//正常部署用
	inline void deployVM(E_Deploy_status status, const S_VM& vm, int vm_id, S_DeploymentInfo& one_deployment_info){

		switch(status){
			case dep_Both:
				A.remaining_cpu_num -= vm.half_cpu_num;
				A.remaining_memory_num -= vm.half_mem_num;
				B.remaining_memory_num -= vm.half_mem_num;
				B.remaining_cpu_num -= vm.half_cpu_num;
				break;
			case dep_A:
				one_deployment_info.node_name = "A";
				A.remaining_cpu_num -= vm.cpu_num;
				A.remaining_memory_num -= vm.memory_num;
				break;
			case dep_B:
				one_deployment_info.node_name = "B";
				B.remaining_memory_num -= vm.memory_num;
				B.remaining_cpu_num -= vm.cpu_num;
			default:break;
		}
		assert(A.remaining_cpu_num >= 0);
		assert(A.remaining_memory_num >= 0);
		assert(B.remaining_cpu_num >= 0);
		assert(B.remaining_memory_num >= 0);

		one_deployment_info.server_seq = seq;
		one_deployment_info.vm_type = vm.type;
		one_deployment_info.is_double_node = vm.is_double_node;
		//更新全局虚拟机部署表
		GlobalVMDeployTable.insert(pair<uint32_t, uint32_t>(vm_id, seq));
		deployed_vms.insert(pair<uint32_t, S_DeploymentInfo>(vm_id, one_deployment_info));
	} 
	
	inline void removeVM(uint32_t vm_id, const string & vm_type){
		assert(deployed_vms.find(vm_id) != deployed_vms.end());
		S_DeploymentInfo deployment_info = deployed_vms[vm_id];
		S_VM vm_info = VMList[vm_type];
		if(deployment_info.is_double_node){
			A.remaining_cpu_num += vm_info.half_cpu_num;
			B.remaining_cpu_num += vm_info.half_cpu_num;
			A.remaining_memory_num += vm_info.half_mem_num;
			B.remaining_memory_num += vm_info.half_mem_num;
		}
		else{
			if(deployment_info.node_name == "A"){
				A.remaining_cpu_num += vm_info.cpu_num;
				A.remaining_memory_num += vm_info.memory_num;
			}
			else{
				B.remaining_memory_num += vm_info.memory_num;
				B.remaining_cpu_num += vm_info.cpu_num;
			}
		}
		assert(A.remaining_cpu_num <= server_info.cpu_num / 2);
		assert(A.remaining_memory_num <= server_info.memory_num / 2);
		assert(B.remaining_cpu_num <= server_info.cpu_num / 2);
		assert(B.remaining_memory_num <= server_info.memory_num / 2);
		deployed_vms.erase(vm_id);
		GlobalVMDeployTable.erase(vm_id);
	}
	
	E_Deploy_status is_deployable(const S_VM& vm)const{
		if(vm.is_double_node){
			if((A.remaining_cpu_num >= vm.half_cpu_num) &&
			 (B.remaining_cpu_num >= vm.half_cpu_num) && 
			 (A.remaining_memory_num >= vm.half_mem_num) &&
			  (B.remaining_memory_num >= vm.half_mem_num)){
				  return dep_Both;
			  }
		}
		else{
			bool a = false;			bool b = false;
			if (((A.remaining_memory_num >= vm.memory_num) && (A.remaining_cpu_num >= vm.cpu_num))){
				a = true; 
			}
			if(((B.remaining_memory_num >= vm.memory_num) && (B.remaining_cpu_num >= vm.cpu_num))){
				b = true;
			}
			if(!a & !b){
				return dep_No;
			}
			if(!a & b){
				return dep_B;
			}
			if(a &!b){
				return dep_A;
			}
			if(a & b){
			//计算A若部署后的core
			#ifdef BALANCE_NODE
				float score_A = cal_node_bs(A.remaining_memory_num - vm.memory_num, A.remaining_cpu_num - vm.cpu_num);
				float score_B = cal_node_bs(B.remaining_memory_num - vm.memory_num, B.remaining_cpu_num - vm.cpu_num);
				return score_A > score_B ? dep_A : dep_B;
			#endif
			
			#ifdef SIMILAR_NODE
				float score_A = cal_node_similarity(A.remaining_memory_num - vm.memory_num, A.remaining_cpu_num - vm.cpu_num, B.remaining_memory_num, B.remaining_cpu_num);
				float score_B = cal_node_similarity(B.remaining_memory_num - vm.memory_num, B.remaining_cpu_num - vm.cpu_num, A.remaining_memory_num, A.remaining_cpu_num);
				return score_A > score_B ? dep_B :dep_A;
			#endif
				return dep_A;
			}
		}
		return dep_No;
	}	

	inline static float cal_node_bs(float m, float c){
		return abs(((m + BIAS * GLOBAL_BALANCE_SCORE) / ((c + BIAS) * GLOBAL_BALANCE_SCORE)) - 1);
	}
	
	inline 	static float cal_node_similarity(int m1, int c1, int m2, int c2){
		return float(pow((m1 - m2), 2) + pow((c1 - c2), 2));
	}
	
	bool operator<(C_BoughtServer& bought_server){
		return this->cal_total_resource_used_rate() < bought_server.cal_total_resource_used_rate();
	}

	inline float cal_total_resource_used_rate(){
		float cpu_used_rate = ((float)(server_info.cpu_num - A.remaining_cpu_num - B.remaining_cpu_num)) / server_info.cpu_num;
		float mem_used_rate = ((float)(server_info.memory_num - A.remaining_memory_num - B.remaining_memory_num)) / server_info.memory_num;
		float total_rate = cpu_used_rate + mem_used_rate;
		total_resource_used_rate = total_rate;
		return total_rate;
	}

	inline void cal_AB_resource_used_rate(){
		int32_t half_server_cpu = server_info.cpu_num / 2;
		int32_t half_server_mem = server_info.memory_num / 2;

		float cpu_a = (float)(half_server_cpu - A.remaining_cpu_num) / half_server_cpu;
		float mem_a = (float)(half_server_mem - A.remaining_memory_num) / half_server_mem;
		A_used_rate = cpu_a + mem_a;

		float cpu_b = (float)(half_server_cpu - B.remaining_cpu_num) / half_server_cpu;
		float mem_b = (float)(half_server_mem - B.remaining_memory_num) / half_server_mem;
		B_used_rate = cpu_b + mem_b;
	}

	S_Server server_info;//此服务器的基本参数
	S_node A, B;//两个节点的信息
	uint32_t seq;//此服务器序列号，唯一标识
	float total_resource_used_rate;//cpu和mem总使用率.针对服务器
	float A_used_rate, B_used_rate;//A、B节点各自cpu和mem利用率，针对各个节点

	unordered_map<uint32_t, S_DeploymentInfo> deployed_vms;//部署在本服务器上的虚拟机id和对应的部署信息
	static int32_t purchase_seq_num;//静态成员，用于存储当前已购买服务器总数，也用于给新买的服务器赋予序列号

	//暂时没用
	static uint32_t total_remaining_cpus;//静态成员，用于存储当前总剩余cpu
	static uint32_t total_remaining_mems;//静态成员，用于存储当前总剩余内存
};


int32_t C_BoughtServer::purchase_seq_num = 0;//初始时，没有任何服务器，序列号从0开始，第一台服务器序列号为0

vector<C_BoughtServer> My_servers;//已购买的服务器列表


//购买服务器
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************


//计算当天最多需要的单节点cpu和mem数量
//暂时没用
inline void check_required_room(const S_DayRequest& day_request, int32_t& required_cpu, int32_t & required_mem, uint32_t cur_idx){
	uint32_t total_request_cpu = 0;
	uint32_t total_request_mem = 0;
	uint32_t max_cpu = 0;
	uint32_t max_mem = 0;
	for(uint32_t i = cur_idx; i != day_request.request_num; ++i){
		const S_VM& vm = VMList[day_request.day_request[i].vm_type];
		if(day_request.day_request[i].is_add){
			if(vm.is_double_node){
				total_request_cpu += vm.half_cpu_num;
				total_request_mem += vm.half_mem_num;
			}
			else{
				total_request_mem += vm.memory_num;
				total_request_cpu += vm.cpu_num;
			}
		} 
		else{
			if(vm.is_double_node){
				total_request_cpu += vm.half_cpu_num;
				total_request_mem += vm.half_mem_num;
			}
			else{
				total_request_mem += -vm.memory_num;
				total_request_cpu += -vm.cpu_num;
			}
		}
		if(total_request_cpu >= max_cpu)max_cpu = total_request_cpu;
		if(total_request_mem >= max_mem)max_mem = total_request_mem;
	};
	required_mem = max_mem;
	required_cpu = max_cpu;
}

// //找到使用率最低且不为0的服务器
//  uint32_t get_least_used_server(){
// 	size_t size = My_servers.size();
// 	float min_used_rate = 3.0f;
// 	size_t min_idx = 0;	
// 	for(size_t i = 0; i != size; ++i){
// 		float cpu_used_rate, mem_used_rate, total_rate;
// 		const S_Server& server_info =  My_servers[i].server_info;
// 		cpu_used_rate = ((float)(server_info.cpu_num - My_servers[i].A.remaining_cpu_num - My_servers[i].B.remaining_cpu_num)) / server_info.cpu_num;
// 		mem_used_rate = ((float)(server_info.memory_num - My_servers[i].A.remaining_memory_num - My_servers[i].B.remaining_memory_num)) / server_info.memory_num;
// 		total_rate = cpu_used_rate + mem_used_rate;
// 		//不空置的服务器迁移才有意义
// 		if(total_rate > 0 and min_used_rate > total_rate){
// 			min_used_rate = total_rate;
// 			min_idx = i;
// 			}
// 	}
// 	assert(min_used_rate < 2.0f);
// 	return min_idx;
// }

//简单购买
inline C_BoughtServer buy_server(int32_t required_cpu, int32_t required_mem, map<string, vector<uint32_t>>& bought_server_kind) {

		//找到一台服务器
		size_t size = ServerList.size();
		int min_idx = 0;
		int min_dis = INT32_MAX;
		for(size_t i = 0; i != size; ++i){
			int cur_dis = pow(ServerList[i].cpu_num - required_cpu, 2) + pow(ServerList[i].memory_num - required_mem, 2);
			if(cur_dis < min_dis and(ServerList[i].cpu_num / 2 >= required_cpu) and (ServerList[i].memory_num / 2 >= required_mem)){
				min_dis = cur_dis;
				min_idx = i;
			}
		}

		S_Server server =  ServerList[min_idx];
		C_BoughtServer bought_server(server);
		//记录购买了哪种服务器，并令相应记录+1

		if (bought_server_kind.find(server.type) != bought_server_kind.end()) {
			bought_server_kind[server.type].push_back(bought_server.seq);
		}
		else {
			vector<uint32_t> bought_server_seq_nums;
			bought_server_seq_nums.push_back(bought_server.seq);
			bought_server_kind.insert(pair<string, vector<uint32_t>>(server.type, bought_server_seq_nums));
		}



	return bought_server;
}

//贪心算法购买
void cal_day_cpu_mem_requirement(const S_DayRequest &day_request, int32_t &required_cpu, int32_t & required_mem){
	if(day_request.request_num == 0)return;
	
	//当天需要的最多
	int32_t cpu_num = 0, mem_num = 0;
	size_t size = day_request.request_num;
	for(size_t i = 0; i != size; ++i){
		if(!day_request.day_request[i].is_add)continue;
		const S_VM& vm =  VMList[day_request.day_request[i].vm_type];

		//单节点视为双节点
		cpu_num += vm.is_double_node ? vm.cpu_num : vm.cpu_num + vm.cpu_num;
		mem_num += vm.is_double_node ? vm.memory_num : vm.memory_num + vm.memory_num;
	}

	size = My_servers.size();
	int32_t available_cpu = 0;
	int32_t available_mem = 0;
	for(size_t i = 0; i != size; ++i){
		const C_BoughtServer& cur_server = My_servers[i];
		
		available_cpu += cur_server.A.remaining_cpu_num > cur_server.B.remaining_cpu_num ? cur_server.B.remaining_cpu_num : cur_server.A.remaining_cpu_num;
		available_mem += cur_server.A.remaining_memory_num > cur_server.B.remaining_memory_num ? cur_server.B.remaining_memory_num : cur_server.A.remaining_memory_num;
	}
	required_cpu = cpu_num - available_cpu;
	required_mem = mem_num - available_mem;
}

//返回购买的服务器以及该服务器在serverList中的下标
pair<int32_t, int32_t> greedy_buy_day_servers(int32_t required_cpu, int32_t required_mem){
	
	float required_cm_div = (float)required_cpu / required_mem;

	//找到cpu内存比最接近当前需求的server型号
	int32_t min_seq = 0;
	float min_cm_div_dis = MAXFLOAT;
	float tmp = 0.0f;
	size_t size = ServerList.size();
	for(int32_t i = 0; i != size; ++i){

		tmp = (float)ServerList[i].cpu_num / ServerList[i].memory_num;
		float cur_div_dis = abs(required_cm_div - tmp);
		if(cur_div_dis	< min_cm_div_dis){
			min_cm_div_dis = cur_div_dis;
			min_seq = i;
		}
	}

	//最合适的服务器
	const S_Server &s = ServerList[min_seq];
	//如果按cpu 买多少台
	int32_t cpu_buy_num = required_cpu / s.cpu_num + 1;
	//如果按mem 买多少台
	int32_t mem_buy_num = required_mem / s.memory_num + 1;
	//两者取较大
	int32_t buy_num =  cpu_buy_num > mem_buy_num ? cpu_buy_num : mem_buy_num;
	return pair<int32_t, int32_t>(buy_num, min_seq);
}


//部署算法
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

inline bool first_fit(const S_Request & request, S_DeploymentInfo& one_deployment_info){
	if(My_servers.size() == 0)return false;
	
	size_t size = My_servers.size();
	const S_VM& vm = VMList[request.vm_type];
	for(size_t i = 0; i != size; ++ i){
		
		E_Deploy_status stat = My_servers[i].is_deployable(vm);
		if(stat != dep_No){
			My_servers[i].deployVM(stat, vm, request.vm_id, one_deployment_info);
			return true;
		}
	}
	return false;
	
}

inline bool best_fit(const S_Request & request, S_DeploymentInfo & one_deployment_info){
	size_t size = My_servers.size();
	if(size == 0)return false;
	
	const S_VM& vm = VMList[request.vm_type];
	int32_t dis = 0;
	int32_t min_dis = INT32_MAX;
	int32_t min_idx = 0;
	E_Deploy_status stat;
	for(size_t i = 0; i != size; ++i){
		//找剩余容量最接近的
		stat = My_servers[i].is_deployable(vm);
		if(stat != dep_No){
			dis = pow((vm.cpu_num - My_servers[i].A.remaining_cpu_num - My_servers[i].B.remaining_cpu_num), 2)+
					pow((vm.memory_num - My_servers[i].A.remaining_memory_num - My_servers[i].B.remaining_memory_num), 2);
			
			if(dis < min_dis){
				min_dis = dis;
				min_idx = i;
			}
		}
	}
	if(min_dis	== INT32_MAX)
	return false;
	stat = My_servers[min_idx].is_deployable(vm);
	assert(stat != dep_No);
	My_servers[min_idx].deployVM(stat, vm, request.vm_id, one_deployment_info);
	return true;
}

inline bool worst_fit(const S_Request & request, S_DeploymentInfo & one_deployment_info){
	if(My_servers.size() == 0)return false;
		
	size_t size = My_servers.size();
	const S_VM& vm = VMList[request.vm_type];
	int32_t dis = 0;
	int32_t max_dis = INT32_MIN;
	int32_t max_idx = 0;
	E_Deploy_status stat;
	for(size_t i = 0; i != size; ++i){
		//找剩余容量最不接近的
		stat = My_servers[i].is_deployable(vm);
		if(stat != dep_No){
			dis = pow((vm.cpu_num - My_servers[i].A.remaining_cpu_num - My_servers[i].B.remaining_cpu_num), 2)+
					pow((vm.memory_num - My_servers[i].A.remaining_memory_num - My_servers[i].B.remaining_memory_num), 2);
			if(dis > max_dis){
				max_dis = dis;
				max_idx = i;
			}
		}
	}
	if(max_dis	== INT32_MIN)return false;
	stat = My_servers[max_idx].is_deployable(vm);
	My_servers[max_idx].deployVM(stat, vm, request.vm_id, one_deployment_info);
	return true;
}



//迁移操作
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************

//返回当天最大可用迁移次数
inline int32_t get_max_migrate_num(){
	size_t num = GlobalVMDeployTable.size();
	return (int32_t)(num * 0.005);
}

//基本迁移操作
inline void migrate_vm(E_Deploy_status status ,uint32_t vm_id, uint32_t in_server_seq, S_MigrationInfo& one_migration_info){
	// assert(GlobalVMDeployTable.find(vm_id) != GlobalVMDeployTable.end());
	// assert(GlobalVMDeployTable[vm_id] < My_servers.size());
	// assert(0 <= in_server_seq < My_servers.size());

	//原服务器删除此虚拟机
	My_servers[GlobalVMDeployTable[vm_id]].removeVM(vm_id, GlobalVMRequestInfo[vm_id].type);

	//新服务器增加此虚拟机
	My_servers[in_server_seq].deployVM(status, GlobalVMRequestInfo[vm_id], vm_id, one_migration_info);

}

//对服务器按利用率升序排序
bool com_used_rate( C_BoughtServer& A,  C_BoughtServer& B){
	return A < B;
}

//根据两个节点使用率均衡程度对服务器排序，差值越大越不均衡
bool com_node_used_balance_rate(C_BoughtServer& s1,  C_BoughtServer& s2){
	return abs(s1.A_used_rate - s1.B_used_rate) < abs(s2.A_used_rate - s2.B_used_rate);
}

//迁移主流程，只进行服务器间迁移，适配最佳适应算法，将服务器资源利用率拉满
void full_loaded_migrate_vm(S_DayTotalDecisionInfo & day_decision){

	register int32_t remaining_migrate_vm_num = get_max_migrate_num();//当天可用迁移量

	if(remaining_migrate_vm_num == 0) return;


	//对服务器按使用率进行升序排列
	vector<C_BoughtServer> tmp_my_servers(My_servers);
	sort(tmp_my_servers.begin(), tmp_my_servers.end(), com_used_rate);	

	//查看的迁出服务器窗口大小
	int32_t max_out = (int32_t)(My_servers.size() * MAX_MIGRATE_OUT_SERVER_RATIO);
	if(max_out < 1)return;
	register int32_t out = 0;

	
	
	//迁移主循环
	do{	
		//迁出服务器信息
		uint32_t least_used_server_seq = tmp_my_servers[out].seq;
		C_BoughtServer &out_server = My_servers[least_used_server_seq];

		//遍历当前迁出服务器所有已有的虚拟机
		unordered_map<uint32_t, S_DeploymentInfo>::const_iterator it = out_server.deployed_vms.begin();		
		unordered_map<uint32_t, S_DeploymentInfo>::const_iterator end = out_server.deployed_vms.end();

		// //初始化使用率高的服务器剩余容量与当前迁出虚拟机需要容量的距离
		// uint32_t min_dis = UINT32_MAX;
		// int32_t min_idx = 0;

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
					day_decision.VM_migrate_vm_record.push_back(one_migration_info);
					if(--remaining_migrate_vm_num == 0)return;
					break;
					
				}


			}
			if(stat != dep_No)continue;
			//最好是能把使用率低的服务器腾出来，如果腾不出来，就要考虑换次低的服务器进行尝试
			++it;
		}
		++out;
	}while(out != max_out);
}

//todo 服务器内节点间迁移
void mirgrate_inside_server(S_DayTotalDecisionInfo & day_decision, bool is_balance_op){
	//进行服务器内节点的迁移
	//两种选择
	//1.让节点更均衡
	//2.让节点更不均衡
	register int32_t remaining_migrate_vm_num = get_max_migrate_num();//当天可用迁移量

	if(remaining_migrate_vm_num == 0) return;

	//对服务器按节点不均衡程度进行升序排列
	vector<C_BoughtServer> tmp_my_servers(My_servers);
	sort(tmp_my_servers.begin(), tmp_my_servers.end(), com_node_used_balance_rate);	

	//进行内部迁移的服务器窗口大小
	int32_t max_inner_migration_server_num = (int32_t)(My_servers.size() * MAX_MIGRATE_INNER_SERVER_RATIO);
	if(max_inner_migration_server_num < 1)return;
	register int32_t mirgrated_server_num = 0;

	if(!is_balance_op){
		for(mirgrated_server_num = 0; mirgrated_server_num != max_inner_migration_server_num; ++mirgrated_server_num){
			
			C_BoughtServer & s = My_servers[tmp_my_servers[mirgrated_server_num].seq];
		}

	}
}


//PSO
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************
//*****************************************************************************************************



struct PSOPara
{
	int dim_;							// 参数维度（position和velocity的维度）
	int particle_num_;					// 粒子个数
	int max_iter_num_;					// 最大迭代次数

	double *dt_ = nullptr;							// 时间步长
	double *wstart_ = nullptr;						// 初始权重
	double *wend_ = nullptr;						// 终止权重
	double *C1_ = nullptr;							// 加速度因子
	double *C2_ = nullptr;							// 加速度因子

	double *upper_bound_ = nullptr;					// position搜索范围上限
	double *lower_bound_ = nullptr;					// position搜索范围下限
	double *range_interval_ = nullptr;				// position搜索区间长度
	
	int results_dim_ = 0;								// results的维度

	PSOPara(){}

	PSOPara(int dim, bool hasBound = false)
	{
		dim_ = dim;

		dt_ = new double[dim_];
		wstart_ = new double[dim_];
		wend_ = new double[dim_];
		C1_ = new double[dim_];
		C2_ = new double[dim_];
		if (hasBound)
		{
			upper_bound_ = new double[dim_];
			lower_bound_ = new double[dim_];
			range_interval_ = new double[dim_];
		}
	}

	// 析构函数：释放堆内存
	~PSOPara()
	{
		if (upper_bound_) { delete[]upper_bound_; }
		if (lower_bound_) { delete[]lower_bound_; }
		if (range_interval_) { delete[]range_interval_; }
		if (dt_) { delete[]dt_; }
		if (wstart_) { delete[]wstart_; }
		if (wend_) { delete[]wend_; }
		if (C1_) { delete[]C1_; }
		if (C2_) { delete[]C2_; }
	}
};

struct Particle
{
	int dim_;							// 参数维度（position和velocity的维度）
	double fitness_;
	double *position_ = nullptr;
	double *velocity_ = nullptr;

	double *best_position_ = nullptr;
	double best_fitness_;
	double *results_ = nullptr;			// 一些需要保存出的结果
	int results_dim_ = 0;				// results_的维度

	Particle(){}

	~Particle()
	{
		if (position_) { delete[]position_; }
		if (velocity_) { delete[]velocity_; }
		if (best_position_) { delete[]best_position_; }
		if (results_) { delete[]results_; }
	}

	Particle(int dim, double * position, double * velocity, double * best_position, double best_fitness)
{
	dim_ = dim;
	//position_ = new double[dim];
	//velocity_ = new double[dim];
	//best_position_ = new double[dim];
	position_ = position;
	velocity_ = velocity;
	best_position_ = best_position;
	best_fitness_ = best_fitness;
}

};

typedef double(*ComputeFitness)(Particle& particle, int32_t required_cpu, int32_t required_mem);


class PSOOptimizer
{
public:
	int particle_num_;					// 粒子个数
	int max_iter_num_;					// 最大迭代次数
	int curr_iter_;						// 当前迭代次数

	int dim_;							// 参数维度（position和velocity的维度）

	Particle *particles_ = nullptr;		// 所有粒子
	
	double *upper_bound_ = nullptr;					// position搜索范围上限
	double *lower_bound_ = nullptr;					// position搜索范围下限
	double *range_interval_ = nullptr;				// position搜索区间长度

	double *dt_ = nullptr;							// 时间步长
	double *wstart_ = nullptr;						// 初始权重
	double *wend_ = nullptr;						// 终止权重
	double *w_ = nullptr;							// 当前迭代权重
	double *C1_ = nullptr;							// 加速度因子
	double *C2_ = nullptr;							// 加速度因子

	double all_best_fitness_;						// 全局最优粒子的适应度值
	double *all_best_position_ = nullptr;			// 全局最优粒子的poistion
	double *results_ = nullptr;						// 一些需要保存出的结果
	int results_dim_ = 0;							// results的维度
	int required_cpu, required_mem;
	ComputeFitness fitness_fun_ = nullptr;			// 适应度函数

public:
	// 默认构造函数
	PSOOptimizer() {}

	// 构造函数
 PSOOptimizer(PSOPara* pso_para, ComputeFitness fitness_fun, int32_t required_cpu, int32_t required_mem)
{
	particle_num_ = pso_para->particle_num_;
	max_iter_num_ = pso_para->max_iter_num_;
	dim_ = pso_para->dim_;
	curr_iter_ = 0;

	dt_ = new double[dim_];
	wstart_ = new double[dim_];
	wend_ = new double[dim_];
	C1_ = new double[dim_];
	C2_ = new double[dim_];

	for (int i = 0; i < dim_; i++)
	{
		dt_[i] = pso_para->dt_[i];
		wstart_[i] = pso_para->wstart_[i];
		wend_[i] = pso_para->wend_[i];
		C1_[i] = pso_para->C1_[i];
		C2_[i] = pso_para->C2_[i];
	}

	if (pso_para->upper_bound_ && pso_para->lower_bound_)
	{
		upper_bound_ = new double[dim_];
		lower_bound_ = new double[dim_];
		range_interval_ = new double[dim_];

		for (int i = 0; i < dim_; i++)
		{
			upper_bound_[i] = pso_para->upper_bound_[i];
			lower_bound_[i] = pso_para->lower_bound_[i];
			//range_interval_[i] = pso_para.range_interval_[i];
			range_interval_[i] = upper_bound_[i] - lower_bound_[i];
		}
	}

	particles_ = new Particle[particle_num_];
	w_ = new double[dim_];
	all_best_position_ = new double[dim_];

	results_dim_ = pso_para->results_dim_;

	if (results_dim_)
	{
		results_ = new double[results_dim_];
	}

	fitness_fun_ = fitness_fun;
	required_cpu = required_cpu;
	required_mem = required_mem;
}

 ~PSOOptimizer()
{
	if (particles_) { delete[]particles_; }
	if (upper_bound_) { delete[]upper_bound_; }
	if (lower_bound_) { delete[]lower_bound_; }
	if (range_interval_) { delete[]range_interval_; }
	if (dt_) { delete[]dt_; }
	if (wstart_) { delete[]wstart_; }
	if (wend_) { delete[]wend_; }
	if (w_) { delete[]w_; }
	if (C1_) { delete[]C1_; }
	if (C2_) { delete[]C2_; }
	if (all_best_position_) { delete[]all_best_position_; }
	if (results_) { delete[]results_; }
}

// 初始化所有粒子
void  InitialAllParticles()
{
	// 初始化第一个粒子参数并设置最优值
	InitialParticle(0);
	all_best_fitness_ = particles_[0].best_fitness_;
	for (int j = 0; j < dim_; j++)
	{
		all_best_position_[j] = particles_[0].best_position_[j];
	}

	// 初始化其他粒子，并更新最优值
	for (int i = 1; i < particle_num_; i++)
	{
		InitialParticle(i);
#ifdef MAXIMIZE_FITNESS
		if (particles_[i].best_fitness_ > all_best_fitness_)
#else
		if (particles_[i].best_fitness_ < all_best_fitness_)
#endif
		{
			all_best_fitness_ = particles_[i].best_fitness_;
			for (int j = 0; j < dim_; j++)
			{
				all_best_position_[j] = particles_[i].best_position_[j];
			}

			// 如果需要保存出一些结果
			if (particles_[i].results_dim_ && results_dim_ == particles_[i].results_dim_)
			{
				for (int k = 0; k < results_dim_; k++)
				{
					results_[k] = particles_[i].results_[k];
				}
			}
			else if (results_dim_)
			{
				std::cout << "WARNING: the dimension of your saved results for every particle\nis not match with the dimension you specified for PSO optimizer ant no result is saved!" << std::endl;
			}
		}
	}
}

// 获取双精度随机数
double  GetDoubleRand(int N = 9999)
{
	double temp = rand() % (N + 1) / (double)(N + 1);
	return temp;
}

double  GetFitness(Particle & particle)
{
	return fitness_fun_(particle, required_cpu, required_mem);
}

void  UpdateAllParticles()
{
	GetInertialWeight();
	for (int i = 0; i < particle_num_; i++)
	{
		UpdateParticle(i);
#ifdef MAXIMIZE_FITNESS
		if (particles_[i].best_fitness_ > all_best_fitness_)
#else
		if (particles_[i].best_fitness_ < all_best_fitness_)
#endif
		{
			all_best_fitness_ = particles_[i].best_fitness_;
			for (int j = 0; j < dim_; j++)
			{
				all_best_position_[j] = particles_[i].best_position_[j];
			}
			
			// 如果需要保存出一些参数
			if (particles_[i].results_dim_ && results_dim_ == particles_[i].results_dim_)
			{
				for (int k = 0; k < results_dim_; k++)
				{
					results_[k] = particles_[i].results_[k];
				}
			}
			else if (results_dim_)
			{
				std::cout << "WARNING: the dimension of your saved results for every particle\nis not match with the dimension you specified for PSO optimizer ant no result is saved!" << std::endl;
			}
		}
	}
	curr_iter_++;
}

void  UpdateParticle(int i)
{
	// 计算当前迭代的权重
	for (int j = 0; j < dim_; j++)
	{
		// 保存上一次迭代结果的position和velocity
		//double last_velocity = particles_[i].velocity_[j];
		double last_position = particles_[i].position_[j];

		particles_[i].velocity_[j] = w_[j] * particles_[i].velocity_[j] +
			C1_[j] * GetDoubleRand() * (particles_[i].best_position_[j] - particles_[i].position_[j]) +
			C2_[j] * GetDoubleRand() * (all_best_position_[j] - particles_[i].position_[j]);
		particles_[i].position_[j] += dt_[j] * particles_[i].velocity_[j];

		// 如果搜索区间有上下限限制
		if (upper_bound_ && lower_bound_)
		{
			if (particles_[i].position_[j] > upper_bound_[j])
			{
				double thre = GetDoubleRand(99);
				if (last_position == upper_bound_[j])
				{
					particles_[i].position_[j] = GetDoubleRand() * range_interval_[j] + lower_bound_[j];
				}
				else if (thre < 0.5)
				{
					particles_[i].position_[j] = upper_bound_[j] - (upper_bound_[j] - last_position) * GetDoubleRand();
				}
				else
				{
					particles_[i].position_[j] = upper_bound_[j];
				}		
			}
			if (particles_[i].position_[j] < lower_bound_[j])
			{
				double thre = GetDoubleRand(99);
				if (last_position == lower_bound_[j])
				{
					particles_[i].position_[j] = GetDoubleRand() * range_interval_[j] + lower_bound_[j];
				}
				else if (thre < 0.5)
				{
					particles_[i].position_[j] = lower_bound_[j] + (last_position - lower_bound_[j]) * GetDoubleRand();
				}
				else
				{
					particles_[i].position_[j] = lower_bound_[j];
				}
			}
		}
	}
	particles_[i].fitness_ = GetFitness(particles_[i]);

#ifdef MAXIMIZE_FITNESS
	if (particles_[i].fitness_ > particles_[i].best_fitness_)
#else
	if (particles_[i].fitness_ < particles_[i].best_fitness_)
#endif
	{
		particles_[i].best_fitness_ = particles_[i].fitness_;
		for (int j = 0; j < dim_; j++)
		{
			particles_[i].best_position_[j] = particles_[i].position_[j];
		}
	}
}


void  GetInertialWeight()
{
	double temp = curr_iter_ / (double)max_iter_num_;
	temp *= temp;
	for (int i = 0; i < dim_; i++)
	{
		w_[i] = wstart_[i] - (wstart_[i] - wend_[i]) * temp;
	}
}


void  InitialParticle(int i)
{
	// 为每个粒子动态分配内存
	particles_[i].position_ = new double[dim_];
	particles_[i].velocity_ = new double[dim_];
	particles_[i].best_position_ = new double[dim_];

	//if (results_dim_)
	//{
	//	particles_[i].results_ = new double[results_dim_];
	//}

	// 初始化position/veloctiy值
	for (int j = 0; j < dim_; j++)
	{
		// if defines lower bound and upper bound
		if (range_interval_)
		{
			particles_[i].position_[j] = GetDoubleRand() * range_interval_[j] + lower_bound_[j];
			particles_[i].velocity_[j] = GetDoubleRand() * range_interval_[j] / 300;
			//std::cout << particles_[i].position_[j] << std::endl;
		}
		else
		{
			particles_[i].position_[j] = GetDoubleRand() * 2;
			particles_[i].velocity_[j] = GetDoubleRand() * 0.5;
		}
	}

	// 设置初始化最优适应度值
	particles_[i].fitness_ = GetFitness(particles_[i]);

	for (int j = 0; j < dim_; j++)
	{
		particles_[i].best_position_[j] = particles_[i].position_[j];
	}
	particles_[i].best_fitness_ = particles_[i].fitness_;
}

};


double FitnessFunction(Particle& particle,int required_cpu,int required_mem)
{
	double sumCpu=0;
	double sumMem=0;
	double sumCost=0;
	for(int i=0;i<ServerList.size();i++)
	{
		sumCpu+=ServerList[i].cpu_num*particle.position_[i];
		sumMem+=ServerList[i].memory_num*particle.position_[i];
		sumCost+=(ServerList[i].purchase_cost+ServerList[i].maintenance_cost)*particle.position_[i];
	}

	if(sumCpu<required_cpu||sumMem<required_mem) return 0;

	double f1=1/sumCost;
	double f2=required_mem/sumMem;
	double f3=required_cpu/sumCpu;
	double f=f1+f2+f3;

	return f;


}


void runPSO(int required_cpu, int required_mem)
{
	PSOPara psopara(ServerList.size(), true);
	psopara.particle_num_ = 20;		// 粒子个数
	psopara.max_iter_num_ = 300;	// 最大迭代次数

	double *dtArr=new double[ServerList.size()];
	for(int i=0;i<ServerList.size();i++)
	{
		*(dtArr+i)=1.0;
	}
	psopara.dt_=dtArr;
	double *wstartArr=new double[ServerList.size()];
	for(int i=0;i<ServerList.size();i++)
	{
		*(wstartArr+i)=0.9;
	}
	psopara.wstart_=wstartArr;
	double *wendArr=new double[ServerList.size()];
	for(int i=0;i<ServerList.size();i++)
	{
		*(wendArr+i)=0.4;
	}
	psopara.wend_=wendArr;
	double *C1Arr=new double[ServerList.size()];
	for(int i=0;i<ServerList.size();i++)
	{
		*(C1Arr+i)=1.49445;
	}
	psopara.C1_=C1Arr;
	double *C2Arr=new double[ServerList.size()];
	for(int i=0;i<ServerList.size();i++)
	{
		*(C2Arr+i)=1.49445;
	}
	psopara.C2_=C2Arr;

	for(int i=0;i<ServerList.size();i++)
	{
		psopara.lower_bound_[i] = 0;
		psopara.upper_bound_[i] = 100;
	}


	PSOOptimizer psooptimizer(&psopara, FitnessFunction, required_cpu, required_mem);

	std::srand((unsigned int)time(0));
	psooptimizer.InitialAllParticles();
	double fitness = psooptimizer.all_best_fitness_;
	double *result = new double[psooptimizer.max_iter_num_];

	for (int i = 0; i < psooptimizer.max_iter_num_; i++)
	{
		psooptimizer.UpdateAllParticles();
		result[i] = psooptimizer.all_best_fitness_;
		std::cout << "第" << i << "次迭代结果：";
		cout <<"[";
		for(int j = 0; j != psopara.dim_; ++j){
			std::cout<<psooptimizer.all_best_position_[j] << " ";
		}
		std::cout << "] , fitness = " << result[i] << std::endl;

	}	

}


//主流程
void process() {
	
	for (int32_t t = 0; t != T; ++t) {
		#ifdef BALANCE_NODE
		GLOBAL_BALANCE_SCORE = get_day_balance_score(t);
		#endif

		S_DayTotalDecisionInfo day_decision;
		#ifndef SUBMIT
		cout<<"process"<<t<<" day"<<endl;
		#endif

		#ifdef MIGRATE
		//对存量虚拟机进行迁移
		full_loaded_migrate_vm(day_decision);
		day_decision.W = day_decision.VM_migrate_vm_record.size();
		#endif

		#ifdef PSO
		int required_cpu, required_mem;
		cal_day_cpu_mem_requirement(Requests[t], required_cpu, required_mem);
		//如果是负数，说明已有服务器可以满足现在的需求,不需要购买
		if(required_mem < 0 or required_mem < 0){}
		else{
			runPSO(required_cpu, required_mem);
			cout<<endl;
		}
		#endif

		#ifdef BUY_SERVER_GREEDY
			int required_cpu, required_mem;
			cal_day_cpu_mem_requirement(Requests[t], required_cpu, required_mem);
			//如果是负数，说明已有服务器可以满足现在的需求,不需要购买
			if(required_mem < 0 or required_cpu < 0){}
			else{			
				//存储购买序列号	
				vector<uint32_t> seqs;
				pair<int32_t, int32_t> purchase_info = greedy_buy_day_servers(required_cpu, required_mem);
			
				for(int i = 0; i != purchase_info.first; ++i){
					C_BoughtServer server = ServerList[purchase_info.second];
				
					seqs.push_back(server.seq);
					My_servers.push_back(server);
				}
				day_decision.server_bought_kind.insert(pair<string, vector<uint32_t> >(ServerList[purchase_info.second].type, seqs));
			}
		#endif

		for (uint32_t i = 0; i != Requests[t].request_num; ++i) {
		//不断处理请求，直至已有服务器无法满足
		S_DeploymentInfo one_request_deployment_info;
			
			//删除虚拟机
			if (!Requests[t].day_request[i].is_add) {
				assert(GlobalVMDeployTable.find(Requests[t].day_request[i].vm_id) != GlobalVMDeployTable.end());
				int32_t cur_server_seq = GlobalVMDeployTable[Requests[t].day_request[i].vm_id];
				My_servers[cur_server_seq].removeVM(
					Requests[t].day_request[i].vm_id, Requests[t].day_request[i].vm_type);
				continue;
			}

			//如果是增加虚拟机
			//如果可以满足条件
			#ifndef BUY_SERVER_GREEDY
			#ifndef PSO
			if (best_fit(Requests[t].day_request[i], one_request_deployment_info)) {
				day_decision.request_deployment_info.push_back(one_request_deployment_info);
				continue;
			};
			#endif
			#endif

			#ifdef BUY_SERVER_GREEDY
			//有可能仍出现虚拟机部署不下的情况，转化为普通贪心
			if(!best_fit(Requests[t].day_request[i], one_request_deployment_info)){
				const S_VM& vm = VMList[Requests[t].day_request[i].vm_type];
				//根据所需cpu and mem,购买服务器
				My_servers.push_back(buy_server(vm.cpu_num, vm.memory_num, day_decision.server_bought_kind));
				//再次处理此请求
				bool status = best_fit(Requests[t].day_request[i], one_request_deployment_info);
				assert(status == true);
			}
			//到这一步保证当前虚拟机已经部署
			day_decision.request_deployment_info.push_back(one_request_deployment_info);
			continue;
			#endif


 			#ifndef BUY_SERVER_GREEDY 
			 #ifndef PSO
			//check_required_room(Requests[t], required_cpu, required_mem, i);
			const S_VM& vm = VMList[Requests[t].day_request[i].vm_type];

			//根据所需cpu and mem,购买服务器
			My_servers.push_back(buy_server(vm.cpu_num, vm.memory_num, day_decision.server_bought_kind));
			--i;//购买服务器后重新处理当前请求
			#endif
			#endif
		}
		day_decision.Q = day_decision.server_bought_kind.size();
		server_seq_to_id(day_decision);
		
		Decisions.push_back(day_decision);
	}
	/*
	freopen("bought_server_ids.txt", "w", stdout);
	
	for (int32_t i = 0; i != T; ++i) {
		cout << "(purchase, " << Decisions[i].Q << ")" << endl;
		for (map<string, vector<uint32_t>>::iterator iter = Decisions[i].server_bought_kind.begin(); iter != Decisions[i].server_bought_kind.end(); ++iter) {
			cout << "(" << iter->first << ", " << iter->second.size() << ")" << endl;
			for (int j = 0; j != iter->second.size(); ++j) {
				cout << "server type:" << iter->first << "	" << "server seq:" << iter->second[j] << "   " << "server id:" << GlobalServerSeq2IdMapTable[iter->second[j]] << endl;
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
			results.push_back(s);
			i = pos;
		}
	}
	return results;
}

void read_standard_input() {
#ifndef SUBMIT
freopen("./training-1.txt", "r", stdin);
#endif // !SUBMIT

	
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
	cin >> N ;
	cin.get();
	S_Server server;
	for (int32_t i = 0; i != N; ++i) {
		getline(cin, line);

		vector <string> results = split(line, ',');
		server.type = results[0];
		server.cpu_num = stoi(results[1]);
		server.memory_num = stoi(results[2]);
		server.purchase_cost = stoi(results[3]);
		server.maintenance_cost = stoi(results[4]);

		ServerList.push_back(server);
	}

	cin >> M;
	cin.get();
	S_VM vm;
	for (int32_t i = 0; i != M; ++i) {
		getline(cin, line);
		vector<string> results = split(line, ',');
		vm.type = results[0];
		vm.cpu_num = stoi(results[1]);
		vm.half_cpu_num = vm.cpu_num / 2;
		vm.memory_num = stoi(results[2]);
		vm.half_mem_num = vm.memory_num / 2;
		vm.is_double_node = stoi(results[3]);

		VMList.insert(pair<string, S_VM>(vm.type, vm));
	}

	cin >> T;
	cin.get();
	for (int32_t i = 0; i != T; ++i) {
		int32_t day_request_num;
		cin >> day_request_num;
		cin.get();
		S_DayRequest day_request;
		day_request.request_num = day_request_num;

		for (int32_t j = 0; j != day_request_num; ++j) {
			getline(cin, line);
			S_Request one_request;
			vector<string>results = split(line, ',');
			if (results[0] == "add") {
				one_request.is_add = true;
				one_request.vm_type = results[1].erase(0, 1);
				one_request.vm_id = stoi(results[2]);
				GlobalVMRequestInfo.insert(pair<uint32_t,  S_VM&>(one_request.vm_id, VMList[one_request.vm_type]));
			}
			else {
				one_request.is_add = false;
				one_request.vm_id = stoi(results[1]);
				one_request.vm_type = GlobalVMRequestInfo[one_request.vm_id].type;
				day_request.delete_op_idxs.push_back(j);
			}

			day_request.day_request.push_back(one_request);
		}

		Requests.push_back(day_request);
	}
}

void write_standard_output() {
#ifndef SUBMIT
	freopen("out.txt", "w", stdout);
#endif
	for (int32_t i = 0; i != T; ++i) {
		cout << "(purchase, " << Decisions[i].Q << ")" << endl;
		for (map<string, vector<uint32_t>>::iterator iter = Decisions[i].server_bought_kind.begin(); iter != Decisions[i].server_bought_kind.end(); ++iter) {
			cout << "(" << iter->first << ", " << iter->second.size() << ")" << endl;
		}

		cout << "(migration, " << Decisions[i].W << ")" << endl;
		for (int32_t j = 0; j != Decisions[i].W; ++j) {
			if (Decisions[i].VM_migrate_vm_record[j].is_double_node) {
				cout << "(" << Decisions[i].VM_migrate_vm_record[j].vm_id << ", " << GlobalServerSeq2IdMapTable[Decisions[i].VM_migrate_vm_record[j].server_seq] << ")" << endl;
			}
			else {
				cout << "(" << Decisions[i].VM_migrate_vm_record[j].vm_id << ", " << GlobalServerSeq2IdMapTable[Decisions[i].VM_migrate_vm_record[j].server_seq] <<
					", " << Decisions[i].VM_migrate_vm_record[j].node_name << ")" << endl;
			}
		}

		for (size_t k = 0; k != Decisions[i].request_deployment_info.size(); ++k) {
			if (Decisions[i].request_deployment_info[k].is_double_node) {
				cout << "(" << GlobalServerSeq2IdMapTable[Decisions[i].request_deployment_info[k].server_seq] << ")";
			}
			else {
				cout << "(" << GlobalServerSeq2IdMapTable[Decisions[i].request_deployment_info[k].server_seq] << ", " << Decisions[i].request_deployment_info[k].node_name
					<< ")";
			}
			if ((i == T - 1) and (k == Decisions[i].request_deployment_info.size() - 1))break;
			cout << endl;
		}

	}
}


int main()
{	
	//int start = clock();
	//int end = 0;
	read_standard_input();
	//end = clock();
	//cout<<"read input cost: ";
	//cout<< (double)(end - start) / CLOCKS_PER_SEC << endl;
	process();
	//start = clock();
	//cout<<"process cost: ";
	//cout<< (double)(start - end) / CLOCKS_PER_SEC <<endl;
	write_standard_output();
	//end = clock();
	//cout<< "write output cost: ";
	//cout<< (double)(end - start) / CLOCKS_PER_SEC <<endl;
	fflush(stdout);
	return 0;
}
