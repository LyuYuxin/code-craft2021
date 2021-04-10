#include"head.h"
			
extern int32_t N;//可以采购的服务器类型
extern int32_t M;//虚拟机类型数量
extern int32_t T;//总共T天
extern int32_t K;//先给K天

extern vector<S_Server> ServerList;//用于存储所有可买的服务器种类信息
extern unordered_map<string, S_VM> VMList;//用于存储所有虚拟机种类信息
extern vector<S_DayRequest> Requests;//用于存储用户所有的请求信息



extern vector<C_BoughtServer*> My_servers;//已购买的服务器列表
extern map<C_BoughtServer*, uint32_t, less_DoubleNode> DoubleNodeTable;//将所有服务器组织成一个双节点表，值为服务器seq
extern map<C_node*, uint32_t, less_SingleNode> SingleNodeTable;//将所有服务器的节点组织成一个单节点表， 值为服务器seq
extern unordered_map<uint32_t, S_DeploymentInfo> GlobalVMDeployTable;//全局虚拟机部署表，记录虚拟机id和相应的部署信息
extern unordered_map<uint32_t, uint32_t> GlobalServerSeq2IdMapTable;//全局服务器id表，用于从购买序列号到输出id的映射
extern unordered_map<uint32_t, S_VM> GlobalVMRequestInfo;//全局VMadd请求表，用于从虚拟机id映射到虚拟机信息

int32_t C_BoughtServer::purchase_seq_num = 0;//初始时，没有任何服务器，序列号从0开始，第一台服务器序列号为0

C_BoughtServer::C_BoughtServer(const S_Server& server) :server_info(server)
{
	seq = purchase_seq_num++;
	A = new(C_node)(server);
	B = new(C_node)(server);
}

C_BoughtServer::C_BoughtServer(const S_VM& vm) {
		A = new(C_node)(vm, true);
		B = new(C_node)(vm, true);
	}

C_BoughtServer::~C_BoughtServer()
	{
		delete A;
		delete B;
	}

float C_BoughtServer::cal_node_similarity(int m1, int c1, int m2, int c2) {
	return float(pow((m1 - m2), 2) + pow((c1 - c2), 2));
}

bool C_BoughtServer::operator<(C_BoughtServer& bought_server) {
	return this->cal_total_resource_used_rate() < bought_server.cal_total_resource_used_rate();
}

float C_BoughtServer::get_double_node_avail_resources()const {
	int32_t remaining_cpu = A->remaining_cpu_num > B->remaining_cpu_num ? B->remaining_cpu_num : A->remaining_cpu_num;
	int32_t remaining_mem = A->remaining_mem_num > B->remaining_mem_num ? B->remaining_mem_num : A->remaining_mem_num;
	return  remaining_cpu + remaining_mem;
}

float C_BoughtServer :: cal_total_resource_used_rate() {
	float total_rate =  static_cast<float>(server_info.cpu_num + server_info.mem_num - A->remaining_cpu_num - A->remaining_mem_num - B->remaining_cpu_num - B->remaining_mem_num) / (server_info.cpu_num + server_info.mem_num);
	total_resource_used_rate = total_rate;
	return total_rate;
}
