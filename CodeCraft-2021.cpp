#define SUBMIT
#include <iostream>
#include<string>
#include<vector>
#include<unordered_map>
#include<map>
#include<cmath>
#include<cassert>
using namespace std;

//������������
//*********************************************************************************************************

//server��Ϣ
typedef struct  {
	int32_t cpu_num;
	int32_t memory_num;
	int32_t purchase_cost;
	int32_t maintenance_cost;
	string type;
} S_Server;

//virtual machine��Ϣ
typedef struct {
	int32_t cpu_num;
	int32_t memory_num;
	bool is_double_node;
	string type;
}S_VM;

//һ��������Ϣ
typedef struct  {
	bool is_add;
	string vm_type;
	uint32_t vm_id;
}S_Request;

//һ���������Ϣ
typedef struct  {
	uint32_t request_num;
	vector<S_Request> day_request;
	vector<int> delete_op_idxs;//���ڼ�¼ɾ������������
}S_DayRequest;

//���������
//********************************************************************************************************

//���������Ǩ����Ϣ
typedef struct {
	uint32_t vm_id;//�����id
	uint32_t server_seq;//Ŀ�������id
	bool is_double_node;//�Ƿ�˫�ڵ㲿��
	string node_name;//������ڵ㲿�𣬲���Ľڵ�����
}S_MigrationInfo;

//���������������Ϣ
typedef struct {
	uint32_t server_seq;//����ķ��������к�
	bool is_double_node;//�Ƿ�˫�ڵ㲿��
	string node_name; //����Ľڵ�����
	string vm_type; //��������������
}S_DeploymentInfo;
	
//һ��ľ�����Ϣ, ���ڼ�¼���в��������
typedef struct DayTotalDecisionInfo{
	int32_t Q;//���ݹ���ķ�����������
	map<string, vector<uint32_t>> server_bought_kind;//����Ĳ�ͬ������

	int32_t W;//��ҪǨ�Ƶ������������
	vector<S_MigrationInfo> VM_transfer_record;//�����Ǩ�Ƽ�¼

	vector<S_DeploymentInfo> request_deployment_info;//���������Ĵ�����¼
	DayTotalDecisionInfo(){
		Q = 0;
		W = 0;
	};
}S_DayTotalDecisionInfo;

//�������ڵ���Ϣ
typedef struct node{
	int32_t remaining_cpu_num;
	int32_t remaining_memory_num;
	node(){
		remaining_cpu_num = 0;
		remaining_memory_num = 0;
	}
}S_node;
	
//todo
//inline void TransferVM(uint32_t vm_id, uint32_t server_seq);
//inline void TransferVM(uint32_t vm_id, uint32_t server_seq, const string& node_name);


//ȫ�ֱ�������
int32_t N;//���Բɹ��ķ���������
int32_t M;//�������������
int32_t T;//T��

vector<S_Server> ServerList;//���ڴ洢���п���ķ�����������Ϣ
unordered_map<string, S_VM> VMList;//���ڴ洢���������������Ϣ
vector<S_DayRequest> Requests;//���ڴ洢�û����е�������Ϣ
vector<S_DayTotalDecisionInfo> Decisions;//���еľ�����Ϣ
unordered_map<uint32_t, uint32_t> GlobalVMDeployTable;//ȫ����������������¼�����id�Ͳ���ķ��������к�
unordered_map<uint32_t, S_VM> GlobalVMRequestInfo;//ȫ���������Ϣ������¼�����id�Ͷ�Ӧ�������������Ϣ

unordered_map<uint32_t, uint32_t> GlobalServerSeq2IdMapTable;//�ѹ�����������к���id��ӳ���

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
		//����ȫ������������
		GlobalVMDeployTable.insert(pair<uint32_t, uint32_t>(vm_id, seq));

		//˫�ڵ㲿��
		if(vm.is_double_node){
			one_deployment_info.is_double_node = true;
			A.remaining_cpu_num -= vm.cpu_num / 2;
			A.remaining_memory_num -= vm.memory_num / 2;
			B.remaining_memory_num -= vm.memory_num / 2;
			B.remaining_cpu_num -= vm.cpu_num / 2;
		}
		//���ڵ㲿��
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
		
		//����Ҫɾ����vm���ڴ˷������ϲ���
		assert(deployed_vms.find(vm_id) != deployed_vms.end());

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

	S_Server server_info;//�˷������Ļ�����Ϣ����ʱ�ò���
	S_node A, B;//�����ڵ����Ϣ
	uint32_t seq;//�˷��������кţ�Ψһ��ʶ
	unordered_map<uint32_t, S_DeploymentInfo> deployed_vms;//�����id�Ͷ�Ӧ�Ĳ�����Ϣ
	static int32_t purchase_seq_num;//��̬��Ա�����ڴ洢��ǰ�ѹ������������

	//��ʱû��
	static uint32_t total_remaining_cpus;//��̬��Ա�����ڴ洢��ǰ��ʣ��cpu
	static uint32_t total_remaining_mems;//��̬��Ա�����ڴ洢��ǰ��ʣ���ڴ�
};

//�����0��ʼ����Ϊ�������My_servers��ʱ�����õ��±�
int32_t C_BoughtServer::purchase_seq_num = 0;//��ʼʱ��û���κη���������Ŵ�0��ʼ����һ̨�������������к�Ϊ0

vector<C_BoughtServer> My_servers;//�ѹ���ķ������б�

//���㵱�������Ҫ�ĵ��ڵ�cpu��mem����
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

 void server_seq_to_id( const S_DayTotalDecisionInfo& day_decision){
	static uint32_t idx = 0;
	map<string, vector<uint32_t>>::const_iterator it = day_decision.server_bought_kind.begin();
	
	for(; it != day_decision.server_bought_kind.end(); ++it){
		for(uint32_t j = 0; j != it->second.size(); ++j){
			GlobalServerSeq2IdMapTable.insert(pair<uint32_t, uint32_t>(it->second[j], idx));
			++idx;
		}
	}
}

//������ʵķ�����
inline vector<C_BoughtServer> buy_server(int32_t required_cpu, int32_t required_mem, map<string, vector<uint32_t>>& bought_server_kind){
	vector<C_BoughtServer> bought_servers;
	do{
	//��һ̨������
	S_Server server = evaluate_servers(required_cpu, required_mem);
	C_BoughtServer bought_server(server);
	//��¼���������ַ�����������¼��Ӧ�Ĺ������к�
	if(bought_server_kind.find(server.type) == bought_server_kind.end()){
		vector<uint32_t> bought_server_seq_nums;//��������������Ĺ������к��б�
		bought_server_seq_nums.push_back(bought_server.purchase_seq_num);
		bought_server_kind.insert(pair<string, vector<uint32_t>>(server.type, bought_server_seq_nums));
	}else{
		bought_server_kind[server.type].push_back(bought_server.purchase_seq_num);
	}
	
	bought_servers.push_back(bought_server);
	//��������һ̨��������ʣ������Ҫ��cpu��mem��Ŀ��ֱ����Ϊ0 ˵����������
	required_mem = required_mem > server.memory_num ? required_mem - server.memory_num : 0;
	required_cpu = required_cpu > server.cpu_num ? required_cpu - server.cpu_num : 0;
	}
	while((required_cpu != 0) or (required_mem != 0));
	
	return bought_servers;
	}

//ʹ��ff�㷨������������
//v1.0 ������Ǩ��
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

//����������
void process(){
	S_DayTotalDecisionInfo day_decison;
	
	for(int32_t t = 0; t != T; ++t){


		
		S_DayTotalDecisionInfo day_decison;
		
		for(uint32_t i = 0; i != Requests[t].request_num; ++i){
		//����ʹ��FF�㷨��������ֱ�����з������޷�����
			S_DeploymentInfo one_request_deployment_info;
			
			if(Requests[t].day_request[i].vm_id == 757464103){
				cout<<endl;
			}
			//����ɾ�������
			if(!Requests[t].day_request[i].is_add){
				My_servers[GlobalVMDeployTable[Requests[t].day_request[i].vm_id]].removeVM(
					Requests[t].day_request[i].vm_id, Requests[t].day_request[i].vm_type);
				continue;
			}

			//��������������
			//���������������
			if(first_fit(Requests[t].day_request[i], one_request_deployment_info)){
				day_decison.request_deployment_info.push_back(one_request_deployment_info);
				continue;
			};
			//��������ʱ������ʣ���������ռ�
			int32_t required_cpu, required_mem;
			//check_required_room(Requests[t], required_cpu, required_mem, i);
			const S_VM &vm = VMList[Requests[t].day_request[i].vm_type];
			required_mem = vm.memory_num;
			required_cpu = vm.cpu_num;
			
			//��������cpu and mem,���������
			vector<C_BoughtServer> bought_servers = buy_server(required_cpu, required_mem, day_decison.server_bought_kind);
			My_servers.insert(My_servers.end(), bought_servers.begin(), bought_servers.end());

			//todo
			//��¼Ǩ����־
			--i;//��������������´�����ǰ����
		}
		//��¼�����������������
		day_decison.Q = day_decison.server_bought_kind.size();
		server_seq_to_id(day_decison);
		Decisions.push_back(day_decison);
	}
}

//���ڴ��������ַ���
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
freopen("/home/ubuntu/Desktop/training-1.txt", "r", stdin);
#endif // !SUBMIT
	
	//�Ż�ioЧ��
	std::ios::sync_with_stdio(false);
    std::cin.tie(0);

	string line;

	/*VMList.reserve(1000);
	VMList.rehash(1000);
	GlobalVMRequestInfo.reserve(1000000);
	GlobalVMRequestInfo.rehash(1000000);
	GlobalVMDeployTable.reserve(1000000);
	GlobalVMDeployTable.rehash(1000000);*/
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
			else{
				one_request.is_add = false;
				one_request.vm_id = stoi(results[1]);

				//ɾ��ʱ��֮ǰ�Ѿ������
				assert(GlobalVMRequestInfo.find(one_request.vm_id) != GlobalVMRequestInfo.end());
				one_request.vm_type = GlobalVMRequestInfo[one_request.vm_id].type;
				day_request.delete_op_idxs.push_back(j);
			}

			day_request.day_request.push_back(one_request);
		}

		Requests.push_back(day_request);
	}
}

void write_standard_output(){

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
			if((i == T - 1) and (k == Decisions[i].request_deployment_info.size() - 1))break;
			cout<<endl;
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

