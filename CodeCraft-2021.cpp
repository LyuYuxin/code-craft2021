#define _CRT_SECURE_NO_WARNINGS
#define SUBMIT
#include <iostream>
#include<string>
#include<vector>
#include<unordered_map>
#include<map>
#include<cmath>
#include<time.h>
#include<cassert>
#include<algorithm>

//可调参数列表

//最大迁出服务器比例
const float MAX_MiGRATE_OUT_SERVER_RATIO = 0.3;

using namespace std;

void read_standard_input();

void process();

void write_standard_output();

//接下来是输入
//*********************************************************************************************************

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


int32_t N;//可以采购的服务器类型
int32_t M;//虚拟机类型数量
int32_t T;//T天

vector<S_Server> ServerList;//用于存储所有可买的服务器种类信息
unordered_map<string, S_VM> VMList;//用于存储所有虚拟机种类信息
vector<S_DayRequest> Requests;//用于存储用户所有的请求信息
vector<S_DayTotalDecisionInfo> Decisions;//所有的决策信息
unordered_map<uint32_t, uint32_t> GlobalVMDeployTable;//全局虚拟机部署表，记录虚拟机id和部署的服务器序列号
unordered_map<uint32_t, S_VM> GlobalVMRequestInfo;//全局虚拟机信息表，记录虚拟机id和对应的虚拟机其他信息
unordered_map<uint32_t, uint32_t> GlobalServerSeq2IdMapTable;//全局服务器id表，用于从购买序列号到输出id的映射


class C_BoughtServer {
public:
	C_BoughtServer(const S_Server& server) :server_info(server)
	{
		A.remaining_cpu_num = server.cpu_num / 2;
		A.remaining_memory_num = server.memory_num / 2;
		B = A;
		seq = purchase_seq_num++;
		resource_used_rate = 0.f;
	}
	
	//迁移用
	inline void deployVM(const S_VM& vm, int vm_id, S_MigrationInfo& one_migration_info){
		//输出用迁移记录
		one_migration_info.vm_id = vm_id;
		one_migration_info.server_seq = seq;
		one_migration_info.is_double_node = vm.is_double_node;

		//更新全局虚拟机部署表
		GlobalVMDeployTable.insert(pair<uint32_t, uint32_t>(vm_id, seq));
		//维护服务器部署信息表
		S_DeploymentInfo one_deployment_info;
		one_deployment_info.is_double_node = vm.is_double_node;
		one_deployment_info.server_seq = seq;
		one_deployment_info.vm_type = vm.type;
		//双节点部署
		if(vm.is_double_node){
			A.remaining_cpu_num -= vm.half_cpu_num;
			A.remaining_memory_num -= vm.half_mem_num;
			B.remaining_memory_num -= vm.half_mem_num;
			B.remaining_cpu_num -= vm.half_cpu_num;
		}
		//单节点部署
		else{
			if((A.remaining_cpu_num >= vm.cpu_num )&& (A.remaining_memory_num >= vm.memory_num)){
				one_migration_info.node_name = "A";
				one_deployment_info.node_name = "A";
				A.remaining_cpu_num -= vm.cpu_num;
				A.remaining_memory_num -= vm.memory_num;
			}
			else if((B.remaining_cpu_num >= vm.cpu_num) && (B.remaining_memory_num >= vm.memory_num)){
				one_migration_info.node_name = "B";
				one_deployment_info.node_name = "B";
				B.remaining_memory_num -= vm.memory_num;
				B.remaining_cpu_num -= vm.cpu_num;
			}
		}
		assert(A.remaining_cpu_num >= 0);
		assert(A.remaining_memory_num >= 0);
		assert(B.remaining_cpu_num >= 0);
		assert(B.remaining_memory_num >= 0);
		deployed_vms.insert(pair<uint32_t, S_DeploymentInfo>(vm_id, one_deployment_info));
	}
	
	//正常部署用
	inline void deployVM(const S_VM& vm, int vm_id, S_DeploymentInfo& one_deployment_info){
		one_deployment_info.server_seq = seq;
		one_deployment_info.vm_type = vm.type;
		one_deployment_info.is_double_node = vm.is_double_node;
		//更新全局虚拟机部署表
		GlobalVMDeployTable.insert(pair<uint32_t, uint32_t>(vm_id, seq));

		//双节点部署
		if(vm.is_double_node){
			A.remaining_cpu_num -= vm.half_cpu_num;
			A.remaining_memory_num -= vm.half_mem_num;
			B.remaining_memory_num -= vm.half_mem_num;
			B.remaining_cpu_num -= vm.half_cpu_num;
		}
		//单节点部署
		else{
			if((A.remaining_cpu_num >= vm.cpu_num )&& (A.remaining_memory_num >= vm.memory_num)){
				one_deployment_info.node_name = "A";
				A.remaining_cpu_num -= vm.cpu_num;
				A.remaining_memory_num -= vm.memory_num;
			}
			else if((B.remaining_cpu_num >= vm.cpu_num) && (B.remaining_memory_num >= vm.memory_num)){
				one_deployment_info.node_name = "B";
				B.remaining_memory_num -= vm.memory_num;
				B.remaining_cpu_num -= vm.cpu_num;
			}
		}
		assert(A.remaining_cpu_num >= 0);
		assert(A.remaining_memory_num >= 0);
		assert(B.remaining_cpu_num >= 0);
		assert(B.remaining_memory_num >= 0);
		deployed_vms.insert(pair<uint32_t, S_DeploymentInfo>(vm_id, one_deployment_info));
	} 
	
	inline void removeVM(uint32_t vm_id, const string & vm_type){
		S_DeploymentInfo deployment_info = deployed_vms[vm_id];
		S_VM vm_info = VMList[vm_type];
		if(deployment_info.is_double_node){
			A.remaining_cpu_num += vm_info.cpu_num / 2;
			B.remaining_cpu_num += vm_info.cpu_num / 2;
			A.remaining_memory_num += vm_info.memory_num / 2;
			B.remaining_memory_num += vm_info.memory_num / 2;
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
	
	bool is_deployable(const S_VM& vm)const{
		if(vm.is_double_node){
			return (A.remaining_cpu_num >= vm.half_cpu_num) &&
			 (B.remaining_cpu_num >= vm.half_cpu_num) && 
			 (A.remaining_memory_num >= vm.half_mem_num) &&
			  (B.remaining_memory_num >= vm.half_mem_num);
		}
		else{
			return ((A.remaining_memory_num >= vm.memory_num) && (A.remaining_cpu_num >= vm.cpu_num))
			 or
			((B.remaining_memory_num >= vm.memory_num) && (B.remaining_cpu_num >= vm.cpu_num));

		}
		return false;
	}	

	bool operator<(C_BoughtServer& bought_server){
		return this->get_cpu_mem_used_rate() < bought_server.get_cpu_mem_used_rate();
	}

	inline float get_cpu_mem_used_rate(){
		float cpu_used_rate = ((float)(server_info.cpu_num - A.remaining_cpu_num - B.remaining_cpu_num)) / server_info.cpu_num;
		float mem_used_rate = ((float)(server_info.memory_num - A.remaining_memory_num - B.remaining_memory_num)) / server_info.memory_num;
		float total_rate = cpu_used_rate + mem_used_rate;
		resource_used_rate = total_rate;
		return total_rate;
	}

	S_Server server_info;//此服务器的基本参数
	S_node A, B;//两个节点的信息
	uint32_t seq;//此服务器序列号，唯一标识
	float resource_used_rate;//cpu和mem总使用率
	unordered_map<uint32_t, S_DeploymentInfo> deployed_vms;//部署在本服务器上的虚拟机id和对应的部署信息
	static int32_t purchase_seq_num;//静态成员，用于存储当前已购买服务器总数，也用于给新买的服务器赋予序列号

	//暂时没用
	static uint32_t total_remaining_cpus;//静态成员，用于存储当前总剩余cpu
	static uint32_t total_remaining_mems;//静态成员，用于存储当前总剩余内存
};
int32_t C_BoughtServer::purchase_seq_num = 0;//初始时，没有任何服务器，序列号从0开始，第一台服务器序列号为0

vector<C_BoughtServer> My_servers;//已购买的服务器列表

//返回当天最大可用迁移次数
inline int32_t get_max_migrate_num(){
	size_t num = GlobalVMDeployTable.size();
	return (int32_t)(num * 0.005);
}

//基本迁移操作
inline void migrate_vm(uint32_t vm_id, uint32_t in_server_seq, S_MigrationInfo& one_migration_info){
	// assert(GlobalVMDeployTable.find(vm_id) != GlobalVMDeployTable.end());
	// assert(GlobalVMDeployTable[vm_id] < My_servers.size());
	// assert(0 <= in_server_seq < My_servers.size());

	//原服务器删除此虚拟机
	My_servers[GlobalVMDeployTable[vm_id]].removeVM(vm_id, GlobalVMRequestInfo[vm_id].type);

	//新服务器增加此虚拟机
	My_servers[in_server_seq].deployVM(GlobalVMRequestInfo[vm_id], vm_id, one_migration_info);

}

//排序函数
bool com( C_BoughtServer& A,  C_BoughtServer& B){
	return A < B;
}

//迁移主流程，适配最佳适应算法，将服务器资源利用率拉满
void full_loaded_migrate_vm(S_DayTotalDecisionInfo & day_decision){

	register int32_t remaining_migrate_vm_num = get_max_migrate_num();//当天可用迁移量

	if(remaining_migrate_vm_num == 0) return;


	//对服务器按使用率进行升序排列
	vector<C_BoughtServer> tmp_my_servers(My_servers);
	sort(tmp_my_servers.begin(), tmp_my_servers.end(), com);	

	//查看的迁出服务器窗口大小
	int32_t max_out = (int32_t)(My_servers.size() * MAX_MiGRATE_OUT_SERVER_RATIO);
	if(max_out < 1)return;
	register int32_t out = 0;

	//迁移信息记录
	S_MigrationInfo one_migration_info;
	
	
	//迁移主循环
	do{	
		//迁出服务器信息
		uint32_t least_used_server_seq = tmp_my_servers[out].seq;
		C_BoughtServer &out_server = My_servers[least_used_server_seq];

		//遍历当前迁出服务器所有已有的虚拟机
		unordered_map<uint32_t, S_DeploymentInfo>::const_iterator it = out_server.deployed_vms.begin();		
		unordered_map<uint32_t, S_DeploymentInfo>::const_iterator end = out_server.deployed_vms.end();

		//初始化使用率高的服务器剩余容量与当前迁出虚拟机需要容量的距离
		uint32_t min_dis = UINT32_MAX;
		int32_t min_idx = 0;

		for(; it != end;){
			const S_VM & vm =  VMList[it->second.vm_type];
		
			min_dis = UINT32_MAX;
			min_idx = 0;

			//i只是当前迁入服务器遍历序号，并不是服务器序列号
			for(int32_t in = tmp_my_servers.size() - 1; in > out; --in){
				uint32_t most_used_server_seq = tmp_my_servers[in].seq;
				const C_BoughtServer &in_server = My_servers[most_used_server_seq];

				if(in_server.is_deployable(vm)){
					uint32_t dis = (in_server.A.remaining_cpu_num + in_server.B.remaining_cpu_num - vm.cpu_num) + (in_server.A.remaining_memory_num + in_server.B.remaining_memory_num - vm.memory_num);
					
					min_dis = dis < min_dis ? dis: min_dis;
					min_idx = dis < min_dis ? in : min_idx;
				}
			}

			if(min_dis != UINT32_MAX){//一次成功迁移
				unordered_map<uint32_t, S_DeploymentInfo>::const_iterator tmp_it = it;
				++it;
				migrate_vm(tmp_it->first, tmp_my_servers[min_idx].seq, one_migration_info);
				day_decision.VM_migrate_vm_record.push_back(one_migration_info);
				
				if(--remaining_migrate_vm_num == 0)break;

			}

			//最好是能把使用率低的服务器腾出来，如果腾不出来，就要考虑换次低的服务器进行尝试
			else ++it;
		}
		if(remaining_migrate_vm_num == 0)break;
		++out;
	}while(out < max_out);
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

//购买一台服务器
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

inline bool first_fit(const S_Request & request, S_DeploymentInfo& one_deployment_info){
	if(My_servers.size() == 0)return false;
	
	size_t size = My_servers.size();
	const S_VM& vm = VMList[request.vm_type];
	for(size_t i = 0; i != size; ++ i){
		
		if(My_servers[i].is_deployable(vm)){
			My_servers[i].deployVM(vm, request.vm_id, one_deployment_info);
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
	for(size_t i = 0; i != size; ++i){
		//找剩余容量最接近的
		if(My_servers[i].is_deployable(vm)){
			dis = pow((vm.cpu_num - My_servers[i].A.remaining_cpu_num - My_servers[i].B.remaining_cpu_num), 2)+
					pow((vm.memory_num - My_servers[i].A.remaining_memory_num - My_servers[i].B.remaining_memory_num), 2);
			min_dis = dis < min_dis ? dis: min_dis;
			min_idx = dis < min_dis ? i : min_idx;
		}
	}
	if(min_dis	== INT32_MAX)return false;
	My_servers[min_idx].deployVM(vm, request.vm_id, one_deployment_info);
	return true;
}

inline bool worst_fit(const S_Request & request, S_DeploymentInfo & one_deployment_info){
	if(My_servers.size() == 0)return false;
		
	size_t size = My_servers.size();
	const S_VM& vm = VMList[request.vm_type];
	int32_t dis = 0;
	int32_t max_dis = INT32_MIN;
	int32_t max_idx = 0;
	for(size_t i = 0; i != size; ++i){
		//找剩余容量最不接近的
		if(My_servers[i].is_deployable(vm)){
			dis = pow((vm.cpu_num - My_servers[i].A.remaining_cpu_num - My_servers[i].B.remaining_cpu_num), 2)+
					pow((vm.memory_num - My_servers[i].A.remaining_memory_num - My_servers[i].B.remaining_memory_num), 2);
			if(dis > max_dis){
				max_dis = dis;
				max_idx = i;
			}
		}
	}
	if(max_dis	== INT32_MIN)return false;
	My_servers[max_idx].deployVM(vm, request.vm_id, one_deployment_info);
	return true;
}


void process() {

	for (int32_t t = 0; t != T; ++t) {
		S_DayTotalDecisionInfo day_decision;

		//对存量虚拟机进行迁移
		full_loaded_migrate_vm(day_decision);
		day_decision.W = day_decision.VM_migrate_vm_record.size();
		
		//不断处理请求，直至已有服务器无法满足
		S_DeploymentInfo one_request_deployment_info;
		for (uint32_t i = 0; i != Requests[t].request_num; ++i) {

			//删除虚拟机
			if (!Requests[t].day_request[i].is_add) {
				int32_t cur_server_seq = GlobalVMDeployTable[Requests[t].day_request[i].vm_id];
				My_servers[cur_server_seq].removeVM(
					Requests[t].day_request[i].vm_id, Requests[t].day_request[i].vm_type);
				continue;
			}

			//如果是增加虚拟机
			//如果可以满足条件
			if (best_fit(Requests[t].day_request[i], one_request_deployment_info)) {
				day_decision.request_deployment_info.push_back(one_request_deployment_info);
				continue;
			};

			//check_required_room(Requests[t], required_cpu, required_mem, i);
			const S_VM& vm = VMList[Requests[t].day_request[i].vm_type];

			//根据所需cpu and mem,购买服务器
			My_servers.push_back(buy_server(vm.cpu_num, vm.memory_num, day_decision.server_bought_kind));
			--i;//购买服务器后重新处理当前请求

		}
		day_decision.Q = day_decision.server_bought_kind.size();
		day_decision.W = day_decision.VM_migrate_vm_record.size();
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
		vm.half_cpu_num = vm.half_cpu_num;
		vm.memory_num = stoi(results[2]);
		vm.half_mem_num = vm.half_mem_num;
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
