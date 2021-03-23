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
using namespace std;

void read_standard_input();

//优化算法求解过程
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
	int32_t memory_num;
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
	vector<S_MigrationInfo> VM_transfer_record;//虚拟机迁移记录

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

//todo
//inline void TransferVM(uint32_t vm_id, uint32_t server_id);
//inline void TransferVM(uint32_t vm_id, uint32_t server_id, const string& node_name);


int32_t N;//可以采购的服务器类型
int32_t M;//虚拟机类型数量
int32_t T;//T天

vector<S_Server> ServerList;//用于存储所有可买的服务器种类信息
unordered_map<string, S_VM> VMList;//用于存储所有虚拟机种类信息
vector<S_DayRequest> Requests;//用于存储用户所有的请求信息
vector<S_DayTotalDecisionInfo> Decisions;//所有的决策信息
unordered_map<uint32_t, uint32_t> GlobalVMDeployTable;//全局虚拟机部署表，记录虚拟机id和部署的服务器id
unordered_map<uint32_t, S_VM> GlobalVMRequestInfo;//全局虚拟机信息表，记录虚拟机id和对应的虚拟机其他信息
unordered_map<uint32_t, uint32_t> GlobalServerSeq2IdMapTable;
class C_BoughtServer {
public:
	C_BoughtServer(const S_Server& server) :server_info(server)
	{
		A.remaining_cpu_num = server.cpu_num / 2;
		A.remaining_memory_num = server.memory_num / 2;
		B = A;
		seq = purchase_seq_num++;
	}

	inline void deployVM(const S_VM& vm, int vm_id, S_DeploymentInfo& one_deployment_info){
		one_deployment_info.server_seq = seq;
		one_deployment_info.vm_type = vm.type;

		//更新全局虚拟机部署表
		GlobalVMDeployTable.insert(pair<uint32_t, uint32_t>(vm_id, seq));

		//双节点部署
		if(vm.is_double_node){
			one_deployment_info.is_double_node = true;
			A.remaining_cpu_num -= vm.cpu_num / 2;
			A.remaining_memory_num -= vm.memory_num / 2;
			B.remaining_memory_num -= vm.memory_num / 2;
			B.remaining_cpu_num -= vm.cpu_num / 2;
		}
		//单节点部署
		else{
			one_deployment_info.is_double_node = false;
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
	bool is_accomodated(const S_VM& vm){
		if(vm.is_double_node){
			if((A.remaining_cpu_num >= vm.cpu_num / 2) &&
			 (B.remaining_cpu_num >= vm.cpu_num / 2) && 
			 (A.remaining_memory_num >= vm.memory_num / 2) &&
			  (B.remaining_memory_num >= vm.memory_num / 2)){
			return true;
			}
		}
		else{
			if(((A.remaining_memory_num >= vm.memory_num) && (A.remaining_cpu_num >= vm.cpu_num))
			 or
			((B.remaining_memory_num >= vm.memory_num) && (B.remaining_cpu_num >= vm.cpu_num))){
				return true;
			}
		}
		return false;
	}	

	S_Server server_info;//此服务器的基本信息，暂时用不到
	S_node A, B;//两个节点的信息
	uint32_t seq;//此服务器序列号，唯一标识
	unordered_map<uint32_t, S_DeploymentInfo> deployed_vms;//虚拟机id和对应的部署信息
	static int32_t purchase_seq_num;//静态成员，用于存储当前已购买服务器总数，也用于给新买的服务器赋予序列号

	//暂时没用
	static uint32_t total_remaining_cpus;//静态成员，用于存储当前总剩余cpu
	static uint32_t total_remaining_mems;//静态成员，用于存储当前总剩余内存
};
int32_t C_BoughtServer::purchase_seq_num = 0;//初始时，没有任何服务器，编号从0开始，第一台服务器编号为0

vector<C_BoughtServer> My_servers;//已购买的服务器列表
void server_seq_to_id(const S_DayTotalDecisionInfo& day_decision) {
	static uint32_t idx = 0;
	map<string, vector<uint32_t>>::const_iterator it = day_decision.server_bought_kind.begin();

	for (; it != day_decision.server_bought_kind.end(); ++it) {
		for (uint32_t j = 0; j != it->second.size(); ++j) {
			GlobalServerSeq2IdMapTable.insert(pair<uint32_t, uint32_t>(it->second[j], idx));
			++idx;
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


//计算当天最多需要的单节点cpu和mem数量
inline void check_required_room(const S_DayRequest& day_request, int32_t& required_cpu, int32_t & required_mem, uint32_t cur_idx){
	uint32_t total_request_cpu = 0;
	uint32_t total_request_mem = 0;
	uint32_t max_cpu = 0;
	uint32_t max_mem = 0;
	for(uint32_t i = cur_idx; i != day_request.request_num; ++i){
		const S_VM& vm = VMList[day_request.day_request[i].vm_type];
		if(day_request.day_request[i].is_add){
			if(vm.is_double_node){
				total_request_cpu += vm.cpu_num / 2;
				total_request_mem += vm.memory_num / 2;
			}
			else{
				total_request_mem += vm.memory_num;
				total_request_cpu += vm.cpu_num;
			}
		} 
		else{
			if(vm.is_double_node){
				total_request_cpu += vm.cpu_num / 2;
				total_request_mem += vm.memory_num / 2;
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

inline const S_Server& evaluate_servers(const int32_t required_cpu, const int32_t required_mem){
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
	S_Server &best_server = ServerList[min_idx];
	return best_server;
}

//购买合适的服务器
inline vector<C_BoughtServer> buy_server(int32_t required_cpu, int32_t required_mem, map<string, vector<uint32_t>>& bought_server_kind) {
	vector<C_BoughtServer> bought_servers;
	do {
		//买一台服务器
		S_Server server = evaluate_servers(required_cpu, required_mem);
		C_BoughtServer bought_server(server);
		//记录购买了哪种服务器，并令相应记录+1
		if (bought_server_kind.find(server.type) == bought_server_kind.end()) {
			vector<uint32_t> bought_server_seq_nums;
			bought_server_seq_nums.push_back(bought_server.seq);
			bought_server_kind.insert(pair<string, vector<uint32_t>>(server.type, bought_server_seq_nums));
		}
		else {
			bought_server_kind[server.type].push_back(bought_server.seq);
		}

		bought_servers.push_back(bought_server);
		//更新买了一台服务器后剩余所需要的cpu和mem数目，直到都为0 说明需求满足
		required_mem = required_mem > server.memory_num ? required_mem - server.memory_num : 0;
		required_cpu = required_cpu > server.cpu_num ? required_cpu - server.cpu_num : 0;
	} while ((required_cpu != 0) or (required_mem != 0));

	return bought_servers;
}

//使用ff算法处理请求主流程
//v1.0 不考虑迁移
inline bool first_fit(const S_Request & request, S_DeploymentInfo& one_deployment_info){
	if(My_servers.size() == 0)return false;
	
	size_t size = My_servers.size();
	const S_VM& vm = VMList[request.vm_type];
	for(size_t i = 0; i != size; ++ i){
		
		if(My_servers[i].is_accomodated(vm)){
			My_servers[i].deployVM(vm, request.vm_id, one_deployment_info);
			return true;
		}
	}
	return false;
	
}

void process() {
	S_DayTotalDecisionInfo day_decision;

	for (int32_t t = 0; t != T; ++t) {
#ifndef SUBMIT
		cout << "开始处理第" << t << "天的请求" << endl;
#endif // #ifndef SUBMIT

		S_DayTotalDecisionInfo day_decision;

		for (int32_t i = 0; i != Requests[t].request_num; ++i) {
			//不断使用FF算法处理请求，直至已有服务器无法满足
			S_DeploymentInfo one_request_deployment_info;

			//对于删除的情况
			if (!Requests[t].day_request[i].is_add) {
				My_servers[GlobalVMDeployTable[Requests[t].day_request[i].vm_id]].removeVM(
					Requests[t].day_request[i].vm_id, Requests[t].day_request[i].vm_type);
				continue;
			}

			//如果是增加虚拟机
			//如果可以满足条件
			if (first_fit(Requests[t].day_request[i], one_request_deployment_info)) {
				day_decision.request_deployment_info.push_back(one_request_deployment_info);
				continue;
			};
			//不能满足时，计算剩余最大所需空间
			int32_t required_cpu, required_mem;
			//check_required_room(Requests[t], required_cpu, required_mem, i);
			const S_VM& vm = VMList[Requests[t].day_request[i].vm_type];
			required_mem = vm.memory_num;
			required_cpu = vm.cpu_num;
			//根据所需cpu and mem,购买服务器
			vector<C_BoughtServer> bought_servers = buy_server(required_cpu, required_mem, day_decision.server_bought_kind);
			My_servers.insert(My_servers.end(), bought_servers.begin(), bought_servers.end());


			//todo
			//记录迁移日志

			--i;//购买服务器后重新处理当前请求

		}
		day_decision.Q = day_decision.server_bought_kind.size();
		Decisions.push_back(day_decision);
		server_seq_to_id(day_decision);

		
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
	//line.erase(0, 1);
	//line.erase(line.size() - 1, 1);

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
freopen("C:\\Users\\lvyuxin\\Desktop\\华为软挑\\training-data\\training-2.txt", "r", stdin);
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
		vm.memory_num = stoi(results[2]);
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

	for (int32_t i = 0; i != T; ++i) {
		cout << "(purchase, " << Decisions[i].Q << ")" << endl;
		for (map<string, vector<uint32_t>>::iterator iter = Decisions[i].server_bought_kind.begin(); iter != Decisions[i].server_bought_kind.end(); ++iter) {
			cout << "(" << iter->first << ", " << iter->second.size() << ")" << endl;
		}

		cout << "(migration, " << Decisions[i].W << ")" << endl;
		for (int32_t j = 0; j != Decisions[i].W; ++j) {
			if (Decisions[i].VM_transfer_record[j].is_double_node) {
				cout << "(" << Decisions[i].VM_transfer_record[j].vm_id << ", " << Decisions[i].VM_transfer_record[j].server_seq << ")" << endl;
			}
			else {
				cout << "(" << Decisions[i].VM_transfer_record[j].vm_id << ", " << Decisions[i].VM_transfer_record[j].server_seq <<
					", " << Decisions[i].VM_transfer_record[j].node_name << ")" << endl;
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